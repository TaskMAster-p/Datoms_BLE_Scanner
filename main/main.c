
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

#include "services/gap/ble_svc_gap.h"

static const char *TAG = "BLE_SCANNER";

// Function Prototypes
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);
static void scan_init(void);
static int gap_event(struct ble_gap_event *event, void *arg);

// Host Reset Callback 

static void on_stack_reset(int reason)
{
    ESP_LOGI(TAG,"Stack Reset : %d",reason);
}

// Host Sync Callback

static void on_stack_sync(void)
{
    ESP_LOGI(TAG,"Host Synced");
    scan_init();
}

// Configuring Nimble Host

static void nimble_host_config_init(void)
{
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
}

// Nimble Host Stack

static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG,"NimBLE Host Started");
    nimble_port_run();
    vTaskDelete(NULL);
}

// Scanning for advertisements

static void scan_init(void)
{
    uint8_t own_addr_type;
    int rc;
    rc = ble_hs_id_infer_auto(0,&own_addr_type);

    if(rc != 0)
    {
        ESP_LOGE(TAG,"Address Type Failed");
        return;
    }

    struct ble_gap_disc_params scan_params;

    memset(&scan_params, 0, sizeof(scan_params));

    scan_params.passive = 0;
    scan_params.itvl = 0x0010;
    scan_params.window = 0x0010;
    scan_params.filter_duplicates = 0;
    ESP_LOGI(TAG,"Starting Scan");

    rc = ble_gap_disc(own_addr_type,BLE_HS_FOREVER,&scan_params,gap_event,NULL);
    if(rc != 0)
    {
        ESP_LOGE(TAG,"Scan Failed : %d",rc);
    }
}

void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();

    if(ret == ESP_ERR_NVS_NO_FREE_PAGES ||ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG,"NVS Initialized");

    // Initialize Nimble

    ret = nimble_port_init();

    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG,"NimBLE Init Failed");
        return;
    }
    ESP_LOGI(TAG,"NimBLE Initialized");

    ble_svc_gap_init();

    nimble_host_config_init();

    xTaskCreate( nimble_host_task, "NimBLE Host",4096,NULL,5, NULL);
}

// GAP Event Callback

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch(event->type)
    {
        case BLE_GAP_EVENT_DISC:
        {
            /* T-One MAC Address (Little Endian) */

            static const uint8_t tone_mac[6] =
            {
                0x39,
                0x55,
                0x32,
                0x9E,
                0x06,
                0xE1
            };

            static const uint8_t tsense_mac[6] =
            {
                0x31,
                0x66,
                0xCD,
                0x99,
                0x4C,
                0xD3
            };
            /* Ignore all other devices */

            if ((memcmp(event->disc.addr.val, tsense_mac, 6) != 0) &&(memcmp(event->disc.addr.val, tone_mac, 6) != 0))
            {
                return 0;
            }

            struct ble_hs_adv_fields fields;

            memset(&fields, 0, sizeof(fields));

            ble_hs_adv_parse_fields(&fields,event->disc.data,event->disc.length_data);


            /* Telemetry Advertisement */

            if(event->disc.length_data >= 25)
            {
                const uint8_t *pkt = event->disc.data;

                if(memcmp(event->disc.addr.val, tsense_mac, 6) == 0)
                {
                    /* Temperature = bytes 19,20 */

                    int16_t temp_raw = (pkt[19] << 8) | pkt[20];
                    // float temperature = temp_raw / 100.0f;

                    /* Door Status = byte 19 */
                    /* Sign bit = Bit 15 */
                    bool negative = (temp_raw & 0x8000);

                    /* Remove sign bit */
                    temp_raw &= 0x7FFF;

                    float temperature = temp_raw / 100.0f;

                    if (negative)
                    {
                        temperature = -temperature;
                    }
                    uint8_t door = pkt[23];
                    int16_t voltage = (pkt[16] << 8) | pkt[17];
                    int8_t battery = pkt[18];
                    const char *door_str = "UNKNOWN";
                    const char *alarm_str = "NONE";

                    if (door == 0x01)
                        door_str = "OPEN";
                    else if (door == 0x00)
                        door_str = "CLOSED";

                    printf("\n----------------------------------------------------------\n");
                    printf("{\n");
                    printf("  \"sensor_mac\"      : \"%02X:%02X:%02X:%02X:%02X:%02X\",\n",
                        event->disc.addr.val[5],
                        event->disc.addr.val[4],
                        event->disc.addr.val[3],
                        event->disc.addr.val[2],
                        event->disc.addr.val[1],
                        event->disc.addr.val[0]);

                    printf("  \"manufacturer\"    : \"TopFlyTech\",\n");
                    printf("  \"model\"           : \"T-SENSE\",\n");
                    printf("  \"battery_level\"   : %d,\n", battery);
                    printf("  \"battery_voltage\" : %.2f,\n", voltage / 1000.0f);
                    printf("  \"temperature_c\"   : %.2f,\n", temperature);
                    printf("  \"door_status\"     : \"%s\",\n", door_str);
                    printf("  \"alarm_status\"    : \"%s\",\n", alarm_str);
                    printf("  \"rssi\"            : %d\n", event->disc.rssi);
                    printf("}\n");
                    printf("----------------------------------------------------------\n\n");
                }
                else
                {
                    /* Temperature = bytes 18,19 */

                    int16_t temp_raw = (pkt[22] << 8) | pkt[23];

                    bool negative = (temp_raw & 0x8000);

                    /* Remove sign bit */
                    temp_raw &= 0x7FFF;

                    float temperature = temp_raw / 100.0f;

                    if (negative)
                    {
                        temperature = -temperature;
                    }
                    // float temperature = temp_raw / 100.0f;

                    /* Humidity = byte 17 */

                    uint8_t humidity = pkt[21];
                    int16_t voltage = (pkt[16] << 8) | pkt[17];
                    int8_t battery = pkt[18];
                    printf("\n----------------------------------------------------------\n");
                    printf("{\n");
                    printf("  \"sensor_mac\"       : \"%02X:%02X:%02X:%02X:%02X:%02X\",\n",
                        event->disc.addr.val[5],
                        event->disc.addr.val[4],
                        event->disc.addr.val[3],
                        event->disc.addr.val[2],
                        event->disc.addr.val[1],
                        event->disc.addr.val[0]);

                    printf("  \"manufacturer\"     : \"TopFlyTech\",\n");
                    printf("  \"model\"            : \"T-ONE\",\n");
                    printf("  \"battery_level\"    : %d,\n", battery);
                    printf("  \"battery_voltage\"  : %.2f,\n", voltage / 1000.0f);
                    printf("  \"temperature_c\"    : %.2f,\n", temperature);
                    printf("  \"humidity_percent\" : %u,\n", humidity);
                    printf("  \"rssi\"             : %d\n", event->disc.rssi);
                    printf("}\n");
                    printf("----------------------------------------------------------\n\n");
                }
            }
                break;
        }   

    case BLE_GAP_EVENT_DISC_COMPLETE:

        ESP_LOGI(TAG, "Scan Complete");
        break;

    case BLE_GAP_EVENT_CONNECT:

        ESP_LOGI(TAG,"Connection Event");
        break;

    case BLE_GAP_EVENT_DISCONNECT:

        ESP_LOGI(TAG,"Disconnected");
        break;

    default:
        break;
    }

    return 0;
}

