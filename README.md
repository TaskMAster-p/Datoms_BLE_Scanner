# ESP32 BLE Advertisement Scanner using NimBLE

A Bluetooth Low Energy (BLE) advertisement scanner implemented using the **ESP32** and the **ESP-IDF** framework. The ESP32 operates as a **BLE Observer**, continuously scanning nearby BLE advertisement packets without establishing a connection.

The application currently supports advertisement packet decoding for **TopFly Tech T-SENSE** and **TopFly Tech T-ONE** sensors. Advertisement packets are parsed according to the vendor-specific broadcast protocol to extract telemetry such as:

- Temperature
- Humidity (T-ONE)
- Battery Voltage
- Battery Percentage
- Door Status (T-SENSE)
- RSSI
- Sensor MAC Address

The project is designed to be modular, allowing support for additional BLE sensors (such as ELA Blue Puck or other beacon-based devices) by simply implementing new advertisement parsers while reusing the same scanning framework.

---

# Introduction

## ESP32

The **ESP32** is a low-power System-on-Chip (SoC) developed by **Espressif Systems** for IoT and embedded applications.
Its integrated BLE controller and low-power operation make it an ideal platform for developing BLE gateways capable of receiving telemetry from battery-powered wireless sensors.

---

## ESP-IDF

The **Espressif IoT Development Framework (ESP-IDF)** is the official software development framework for ESP32 devices.
This project uses the **NimBLE Host Stack** provided by ESP-IDF for BLE scanning and advertisement parsing.

### Useful Documentation

- ESP-IDF Programming Guide  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/

- NimBLE Programming Guide  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/nimble/index.html

---

# Project Objective

The objective of this project is to configure the ESP32 as a **BLE Observer** that continuously scans for nearby BLE advertisements and extracts telemetry from supported BLE sensors.

Unlike a conventional BLE client, this application **does not establish a BLE connection** with the sensors. Instead, it listens to advertisement packets broadcast periodically by the sensors and decodes the payload according to the manufacturer's advertisement protocol.

The current implementation supports:

| Sensor | Advertisement Type | Extracted Parameters |
|---------|--------------------|----------------------|
| TopFly T-SENSE | Vendor Specific (Eddystone-based) | Temperature, Battery Voltage, Battery Percentage, Door Status |
| TopFly T-ONE | Vendor Specific (Eddystone-based) | Temperature, Humidity, Battery Voltage, Battery Percentage |

The scanning framework has been intentionally designed to be modular so that additional BLE advertisement formats can be integrated by implementing new packet parsers without modifying the core scanning logic.

---

## High-Level Workflow

```text
                BLE Sensor
                     │
         Advertisement Packet
                     │
                     ▼
              ESP32 Scanner
                     │
                     ▼
         Advertisement Parser
                     │
                     ▼
      Decode Sensor Telemetry
                     │
                     ▼
          Display Parsed Data
```

The remainder of this document explains the implementation, advertisement packet formats, Docker setup, and execution workflow in detail.

# Project Workflow

The application configures the ESP32 as a **BLE Observer**. It continuously scans for BLE advertisement packets transmitted by nearby sensors without establishing a BLE connection.

Whenever an advertisement is received, the packet is inspected to determine the transmitting sensor. If the advertisement belongs to a supported sensor (currently **TopFly T-SENSE** or **TopFly T-ONE**), the payload is parsed according to the corresponding broadcast protocol and the extracted telemetry is displayed.

The complete execution flow is illustrated below.

```text
                    app_main()
                         │
                         ▼
              Initialize NVS Flash
                         │
                         ▼
             Initialize NimBLE Stack
                         │
                         ▼
          Configure NimBLE Host Callbacks
                         │
                         ▼
            Start NimBLE Host Task
                         │
                         ▼
             Wait for Host Synchronization
                         │
                         ▼
               Configure BLE Scanner
                         │
                         ▼
             Start Continuous Scanning
                         │
                         ▼
         Advertisement Packet Received
                         │
                         ▼
        BLE_GAP_EVENT_DISC Callback
                         │
                         ▼
         Check Sensor MAC Address
                         │
        ┌────────────────┴───────────────┐
        │                                │
        ▼                                ▼
   T-SENSE Packet                  T-ONE Packet
        │                                │
        ▼                                ▼
 Parse Advertisement              Parse Advertisement
        │                                │
        └────────────────┬───────────────┘
                         ▼
              Display Parsed Telemetry
                         │
                         ▼
             Continue Scanning Forever
```

---

# BLE Concepts Used in this Project

This project only uses the **Generic Access Profile (GAP)** portion of the Bluetooth Low Energy stack.

Unlike applications that establish BLE connections and exchange data through GATT services and characteristics, this project simply listens to advertisement packets broadcast periodically by nearby BLE devices.

No pairing, bonding or connection establishment takes place.

---

## Generic Access Profile (GAP)

The **Generic Access Profile (GAP)** defines how BLE devices discover each other and establish communication.

For this project, the ESP32 operates in the **Observer** role.

```text
              TopFly Sensor
                  │
         Broadcast Advertisement
                  │
                  ▼
            ESP32 Observer
                  │
          Receive Advertisement
                  │
                  ▼
            Parse Telemetry
```

The ESP32 never sends connection requests to the sensor.

Instead, it continuously scans for advertisements transmitted over the BLE advertising channels.

---

## BLE Observer

An Observer is a BLE device whose sole purpose is to scan and receive advertisement packets.

Characteristics of the Observer role:

- Continuously scans nearby BLE devices.
- Does not establish a BLE connection.
- Receives advertisement packets only.
- Suitable for BLE gateways and monitoring applications.
- Lower software complexity than a BLE Central device.

This project configures the ESP32 exclusively as an Observer.

---

## Advertisement Packet

BLE sensors periodically broadcast small packets of information known as **Advertisement Packets**.

These packets are transmitted without requiring any connection between the sensor and the receiver.

Each advertisement contains a sequence of bytes representing information such as:

- Advertisement Flags
- Service UUIDs
- Local Device Name
- Manufacturer Specific Data
- Service Data
- Vendor-defined telemetry

The ESP32 receives these packets as a raw byte stream.

Example:

```text
18 16 AA FE 08 12 20 08 00 D3 4C 99 CD 66 31 00 0B B8 64 09 E3 FF 00 01 00
```

The received bytes have no meaning until they are interpreted according to the sensor manufacturer's broadcast protocol.

---

## Advertisement Reception

Whenever a BLE advertisement is received, the NimBLE stack generates a GAP event:

```c
BLE_GAP_EVENT_DISC
```

This callback provides the application with:

- Advertiser MAC Address
- Address Type
- RSSI
- Raw Advertisement Data
- Advertisement Length

Inside this callback, the application:

1. Checks whether the advertisement belongs to a supported sensor.
2. Determines the sensor type.
3. Parses the advertisement payload.
4. Extracts telemetry values.
5. Displays the decoded information.

---

# Source Code Explanation

This section explains the implementation of the BLE advertisement scanner in detail. The execution begins from the `app_main()` function, which is the application entry point for every ESP-IDF project.

The following figure illustrates the software execution flow.

```text
                    app_main()
                         │
        ┌────────────────┴────────────────┐
        │                                 │
        ▼                                 ▼
 Initialize NVS                  Initialize NimBLE
        │                                 │
        └────────────────┬────────────────┘
                         ▼
               Initialize GAP Service
                         │
                         ▼
          Configure NimBLE Host Callbacks
                         │
                         ▼
             Create NimBLE Host Task
                         │
                         ▼
                 nimble_port_run()
                         │
                         ▼
                 Host Synchronization
                         │
                         ▼
                  scan_init()
                         │
                         ▼
               Start BLE Scanning
                         │
                         ▼
              Advertisement Received
```

---



---

# GAP Event Callback

```c
static int gap_event(struct ble_gap_event *event,
                     void *arg)
```

Every advertisement received by the ESP32 enters this callback.

The project currently handles:

```c
BLE_GAP_EVENT_DISC
BLE_GAP_EVENT_DISC_COMPLETE
BLE_GAP_EVENT_CONNECT
BLE_GAP_EVENT_DISCONNECT
```

However, since this project performs advertisement scanning only, the most important event is

```c
BLE_GAP_EVENT_DISC
```

---

# BLE_GAP_EVENT_DISC

This event indicates that a BLE advertisement has been received.

The callback receives a structure containing information about the advertisement.

The most important members used in this project are:

| Field | Description |
|--------|-------------|
| event->disc.addr | Advertiser BLE Address |
| event->disc.addr.type | Address Type |
| event->disc.rssi | Signal Strength |
| event->disc.data | Raw Advertisement Bytes |
| event->disc.length_data | Advertisement Length |

These fields provide everything required to identify the transmitting sensor and decode its telemetry.

---

# Sensor Identification

The application currently supports two TopFly sensors.

```c
static const uint8_t tone_mac[6]
```

```c
static const uint8_t tsense_mac[6]
```

The advertiser MAC address is compared against these known sensor addresses.

```c
memcmp(event->disc.addr.val,
       tsense_mac,
       6)
```

If the advertisement originates from any other BLE device, it is ignored immediately.

```c
if(...)
{
    return 0;
}
```

This significantly reduces unnecessary packet processing when multiple BLE devices are present nearby.

---

# Parsing Advertisement Fields

Once a supported sensor is identified, the raw advertisement packet is parsed using

```c
ble_hs_adv_parse_fields()
```

```c
struct ble_hs_adv_fields fields;

ble_hs_adv_parse_fields(
    &fields,
    event->disc.data,
    event->disc.length_data
);
```

This function decodes standard BLE advertisement fields such as:

- Complete Local Name
- Manufacturer Specific Data
- Service UUIDs
- Service Data
- TX Power
- Appearance

In this project, it is primarily used to obtain the sensor's advertised name:

```c
fields.name
```

For example,

```
T-sense
```

if the sensor includes its local name inside the advertisement packet.

The remaining telemetry values are extracted directly from the raw advertisement bytes according to the TopFly advertisement protocol, which is explained in the next section.

# Advertisement Packet Parsing

After identifying the transmitting sensor, the application decodes the raw advertisement packet according to the broadcast protocol defined by **TopFly Tech**.

Although both **T-SENSE** and **T-ONE** use BLE advertisements, the position of each telemetry field inside the packet differs. Therefore, separate parsers are implemented for each sensor.

The following sections describe the packet structure used by each sensor and explain how the application extracts the telemetry values.

---

# T-SENSE Advertisement Packet

A typical advertisement packet received from the T-SENSE sensor is shown below.

```text
18 16 AA FE 08 12 20 08 00 D3 4C 99 CD 66 31 00 0B B8 64 09 E3 FF 00 01 00
```

The application interprets the packet as follows.

| Byte Index | Length | Description |
|------------|--------|-------------|
| 0 – 5 | 6 Bytes | Advertisement Header |
| 6 | 1 Byte | Hardware Version |
| 7 | 1 Byte | Firmware Version |
| 8 – 13 | 6 Bytes | Sensor MAC Address |
| 14 | 1 Byte | Reserved |
| 15 | 1 Byte | Reserved |
| 16 – 17 | 2 Bytes | Battery Voltage (mV) |
| 18 | 1 Byte | Battery Percentage |
| **19 – 20** | **2 Bytes** | **Temperature (Signed Integer)** |
| 21 | 1 Byte | Reserved |
| 22 | 1 Byte | Reserved |
| **23** | **1 Byte** | **Door Status** |
| **24** | **1 Byte** | **Alarm Status** |

---

## Battery Voltage

The battery voltage is obtained from bytes **16** and **17**.

```c
int16_t voltage = (pkt[16] << 8) | pkt[17];
```

Example

```text
0B B8

0x0BB8

3000 mV

3.0 V
```

---

## Battery Percentage

Battery percentage occupies byte **18**.

```c
int8_t battery = pkt[18];
```

Example

```text
64

0x64

100 %
```

---

## Temperature

Temperature is stored using bytes **19** and **20**.

```c
int16_t temp_raw =
        (pkt[19] << 8) | pkt[20];
```

Unlike conventional signed integers, TopFly uses the **Most Significant Bit (Bit 15)** as the sign indicator.

```
Bit15 = 0

Positive Temperature

Bit15 = 1

Negative Temperature
```

The application first determines the sign.

```c
bool negative =
        (temp_raw & 0x8000);
```

The sign bit is then removed.

```c
temp_raw &= 0x7FFF;
```

Finally,

```c
temperature =
        temp_raw / 100.0f;
```

If the sign bit was set,

```c
temperature =
        -temperature;
```

Example

```
09 E3

0x09E3

2531

2531 /100

25.31 °C
```

---

## Door Status

Door status occupies byte **23**.

```c
uint8_t door = pkt[23];
```

The parser interprets the value as

| Value | Status |
|-------|---------|
| 0x00 | CLOSED |
| 0x01 | OPEN |

---

## Alarm Status

The final byte (24) contains the alarm state.

Current implementation does not decode this field, but it is reserved for future use.

---

# T-ONE Advertisement Packet

A typical advertisement packet from the T-ONE sensor is shown below.

```text
18 16 AA FE 0A 11 20 09 00 E1 06 9E 32 55 39 00 0B B8 64 00 00 33 09 BB 3F
```

The packet layout differs slightly from T-SENSE.

| Byte Index | Length | Description |
|------------|--------|-------------|
| 0 – 5 | 6 Bytes | Advertisement Header |
| 6 | 1 Byte | Hardware Version |
| 7 | 1 Byte | Firmware Version |
| 8 – 13 | 6 Bytes | Sensor MAC Address |
| 14 | 1 Byte | Reserved |
| 15 | 1 Byte | Reserved |
| 16 – 17 | 2 Bytes | Battery Voltage (mV) |
| 18 | 1 Byte | Battery Percentage |
| 19 – 20 | 2 Bytes | Reserved |
| **21** | **1 Byte** | **Humidity** |
| **22 – 23** | **2 Bytes** | **Temperature** |
| 24 | 1 Byte | Reserved |

---

## Battery Voltage

```c
int16_t voltage =
        (pkt[16] << 8) | pkt[17];
```

Example

```
0B B8

3000 mV

3.0 V
```

---

## Battery Percentage

```c
int8_t battery =
        pkt[18];
```

Example

```
64

100 %
```

---

## Humidity

Humidity is stored in byte **21**.

```c
uint8_t humidity =
        pkt[21];
```

Example

```
33

0x33

51 %
```

---

## Temperature

Temperature is stored using bytes **22** and **23**.

```c
int16_t temp_raw =
        (pkt[22] << 8) | pkt[23];
```

The sign bit is interpreted exactly the same way as the T-SENSE sensor.

Example

```
09 BB

0x09BB

2491

24.91 °C
```

---

# Packet Parsing Workflow

The complete advertisement parsing process implemented in the application is illustrated below.

```text
BLE Advertisement Received
            │
            ▼
Check Advertiser MAC Address
            │
            ├──────────────┐
            │              │
            ▼              ▼
        T-SENSE         T-ONE
            │              │
            ▼              ▼
Extract Bytes       Extract Bytes
19–20              22–23
Temperature         Temperature
            │              │
Extract Bytes       Extract Byte
23                 21
Door Status        Humidity
            │              │
Extract Bytes       Extract Bytes
16–18              16–18
Battery            Battery
            │              │
            └──────┬───────┘
                   ▼
          Display Parsed Telemetry
```

This modular parsing approach allows additional BLE sensors to be supported easily by implementing a new parser corresponding to the manufacturer's advertisement packet format while reusing the existing BLE scanning framework.

# Sample Output

After flashing the firmware onto the ESP32 and powering the supported BLE sensors, the scanner continuously receives advertisement packets and decodes the embedded telemetry.

---

## T-SENSE Advertisement

```text
----------------------------------------------------------
{
  "sensor_mac"      : "D3:4C:99:CD:66:31",
  "manufacturer"    : "TopFlyTech",
  "model"           : "T-SENSE",
  "battery_level"   : 100,
  "battery_voltage" : 3.00,
  "temperature_c"   : 25.31,
  "door_status"     : "OPEN",
  "alarm_status"    : "NONE",
  "rssi"            : -48
}
----------------------------------------------------------
```

### Output Description

| Field | Description |
|---------|-------------|
| sensor_mac | BLE MAC address of the transmitting sensor |
| manufacturer | Sensor manufacturer |
| model | Sensor model |
| battery_level | Battery percentage |
| battery_voltage | Battery voltage |
| temperature_c | Temperature in degree Celsius |
| door_status | OPEN or CLOSED |
| alarm_status | Alarm state (reserved for future implementation) |
| rssi | Received Signal Strength Indicator |

---

## T-ONE Advertisement

```text
----------------------------------------------------------
{
  "sensor_mac"       : "E1:06:9E:32:55:39",
  "manufacturer"     : "TopFlyTech",
  "model"            : "T-ONE",
  "battery_level"    : 100,
  "battery_voltage"  : 3.00,
  "temperature_c"    : 24.91,
  "humidity_percent" : 51,
  "rssi"             : -69
}
----------------------------------------------------------
```

### Output Description

| Field | Description |
|---------|-------------|
| sensor_mac | BLE MAC address of the transmitting sensor |
| manufacturer | Sensor manufacturer |
| model | Sensor model |
| battery_level | Battery percentage |
| battery_voltage | Battery voltage |
| temperature_c | Temperature in degree Celsius |
| humidity_percent | Relative humidity |
| rssi | Received Signal Strength Indicator |

---

# 🚀 Docker Setup & Compilation Instructions

This project runs entirely inside an isolated **Docker container** using a standardized development environment image (`espressif/idf`).

By using Docker, the ESP-IDF toolchain, Python dependencies and build environment remain isolated from the host operating system, ensuring a consistent development workflow across different machines.

---

## 1. Prerequisites

Install **Docker Desktop** and ensure that the Docker daemon is running.

https://www.docker.com/products/docker-desktop/

---


## 2. Pull the latest image

Open a terminal inside the project directory.


```bash
docker pull espressif/idf:release-v5.4
```


## 3. Set the Target Device

Open a terminal inside the project directory.

Configure the target device.

```bash
docker run --rm -v "${PWD}:/project" -w /project espressif/idf:release-v5.4 idf.py set-target esp32
```

---

## 4. Build the Project

Compile the application.

```bash
docker run --rm -v "${PWD}:/project" -w /project espressif/idf:release-v5.4 idf.py build
```

After a successful build, the firmware binaries are generated inside the `build/` directory.

---

## 4. Install python libraries for flashing the device 


```bash
pip install esptool
```
```bash
pip install pyserial
```
---

## 5. Flash the ESP32

Connect the ESP32 through USB and determine the serial port.

Flash the firmware in other terminal outside container

```bash
python -m esptool `
--chip esp32 `
--port COM3 `
--baud 460800 `
write_flash `
0x1000 build\bootloader\bootloader.bin `
0x8000 build\partition_table\partition-table.bin `
0x10000 build\app.bin
```

Replace **COM3** with the appropriate serial port.

---

## 5. Open the Serial Monitor

To observe the decoded advertisement packets:

```bash
python -m serial.tools.miniterm COM3 115200
```

Terminate the serial monitor using

```text
Ctrl + ]
```

---


# Troubleshooting

| Problem | Possible Solution |
|----------|-------------------|
| Docker container does not start | Ensure Docker Desktop is running |
| ESP32 not detected | Verify USB cable and COM port |
| Build failure | Execute `idf.py fullclean` followed by `idf.py build` |
| No BLE advertisements received | Verify the sensor is powered and broadcasting |
| Sensor data not decoded | Confirm the sensor MAC address matches the parser configuration |

---

# References

## ESP-IDF

ESP-IDF Programming Guide

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/

---

## NimBLE

ESP-IDF NimBLE Documentation

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/nimble/index.html

Apache NimBLE Host Documentation

https://mynewt.apache.org/latest/network/index.html

---

## BLE GAP

Generic Access Profile (GAP)

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/nimble/index.html

---

## FreeRTOS

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html

---

## Docker

https://docs.docker.com/

---
