# IoT Desk Occupancy Tracker

Tracks desk sitting sessions and publishes duration data to AWS IoT Core over MQTT/TLS. Includes an A/B bootloader for OTA firmware updates.

![Hardware overview](app/Docs/system.jpg)

## Repository structure

* `app/` contains the PIR sitting session tracker. Publishes session data to AWS IoT Core over MQTT/TLS via an ESP32 Wi-Fi bridge running FreeRTOS. See `app/README.md` for setup and configuration.

* `bootloader/` contains the A/B bootloader. Reads a boot descriptor from flash, selects the active firmware slot, and jumps to it. See `bootloader/README.md` for the flash layout and boot flow.

## First-time setup after cloning

**1. Activate git hooks** (blocks accidental credential commits):
```sh
git config core.hooksPath scripts/hooks
```

**2. Create your local config file** (contains credentials — never committed):
```sh
cp app/IOT_SDK/config/application_config_example.h app/IOT_SDK/config/application_config.h
# Edit application_config.h with your WiFi credentials, MQTT broker, and Thing name
```
