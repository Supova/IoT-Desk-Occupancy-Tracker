# Desk Occupancy Tracker

PIR-based sitting session tracker. On motion, a session starts. Heartbeat published every 5 minutes while seated. A 3-minute grace period runs after motion stops before the session ends.

Publishes session data to AWS IoT Core over MQTT/TLS using an ESP32 as a Wi-Fi bridge.

## Architecture

![PIR state machine](Docs/pir_state_machine.png)

![System architecture](Docs/system_architecture.jpg)



## Configuration

**AWS IoT Core:** Create a Thing, attach a policy for `iot:Connect/Publish/Subscribe/Receive`, download the certificates.

**ESP32:** Flash Espressif AT firmware with AWS certificates baked in. See the [Espressif AT compile guide](https://docs.espressif.com/projects/esp-at/en/latest/esp32/Compile_and_Develop/How_to_clone_project_and_compile_it.html).

**Credentials:** Copy `IOT_SDK/config/application_config_example.h` to `application_config.h` and fill in Wi-Fi and MQTT details. Do not commit this file.

## Build and Flash

Open in STM32CubeIDE, build, and flash via ST-LINK to 0x08020000.The linker script limits the app to 128 KB (slot A). 

Debug output on USART2 at 115200 baud.
