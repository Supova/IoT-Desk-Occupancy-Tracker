# ESP32 + STM32 MQTT on AWS IoT Core

STM32F407 talking to AWS IoT Core over MQTT, using an ESP32 as a WiFi/TLS bridge via AT commands.

## Hardware

- STM32F407G-DISC1
- ESP32 module (AT firmware)
- HC-SR501 PIR motion sensor

## How it works

The STM32 sends AT commands to the ESP32 over UART. The ESP32 handles WiFi, TLS, and MQTT — the STM32 just calls `AT+MQTTPUB` and `AT+MQTTSUB`. Certificates live on the ESP32 side.

## Setup

1. Download your AWS IoT certificates and flash them to the ESP32
2. Rename `IOT_SDK/config/application_config_example.h` → `application_config.h`
3. Fill in your WiFi credentials, AWS endpoint, and thing name
4. Build and flash with STM32CubeIDE

## Topics

| Topic | Direction |
|-------|-----------|
| `sensors/room1/motion` | Publish |

