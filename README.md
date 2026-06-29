# ESP32 BLE Scan Application (NimBLE Stack)

## 🧠 Overview: What is the ESP32?
The **ESP32** is a feature-rich System-on-a-Chip (SoC) designed by Espressif Systems. It embeds dual Xtensa 32-bit LX6 processing cores, integrated Wi-Fi, and a dedicated **Bluetooth Low Energy (BLE)** radio subsystem. 

Unlike conventional Bluetooth Classic, which maintains an open, high-power stream for continuous audio or high-speed data, **BLE** is highly optimized for ultra-low power applications. It operates by sending short, discrete packets known as advertisements over a network. This particular application configures the ESP32 to run as a central scanner (an Observer), listening continuously for incoming broadcast packets sent by battery-powered IoT telemetry sensors.

---

## 🚀 Docker Setup & Compilation Instructions
This project runs entirely inside an isolated **Docker container** using a standardized development environment image (`env-esp-idf`). By using Docker, you eliminate the need to manually download or maintain cross-compilation toolchains, system paths, or native environment variable overlays on your local workstation.

### 1. Prerequisites
* Download and run [Docker Desktop](https://www.docker.com/products/docker-desktop/).
* Make sure that the Docker Daemon is actively running in your background system tray.

### 2. Set the Hardware Target
Before compiling the codebase, you must specify the hardware architecture targets. Open your host terminal in this repository's root folder and run:

```bash
docker run --rm -v $PWD:/project -w /project env-esp-idf idf.py set-target esp32