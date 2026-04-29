/*
 * application_config.h.example
 *
 * Copy this file to application_config.h and fill in your values.
 * DO NOT commit application_config.h to version control.
 */

#error "This is the example config. Copy this file to application_config.h, fill in your values, and remove this line."

#ifndef INC_APPLICATION_CONFIG_H_
#define INC_APPLICATION_CONFIG_H_


/**************************************************/
/******* DO NOT CHANGE the following order ********/
/**************************************************/

#include "logging_levels.h"

#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME               "MAIN_APP"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL              LOG_INFO
#endif

#include "logging_stack.h"

/* WiFi Credentials */
#define WIFI_SSID                          "your-wifi-ssid"
#define WIFI_PASSWORD                      "your-wifi-password"

/* AWS IoT Core — found in AWS Console → IoT Core → Settings → Device data endpoint */
#define MQTT_BROKER                        "xxxxxxxxxx.iot.ap-south-1.amazonaws.com"
#define MQTT_PORT                          8883

/* AWS IoT Thing name */
#define CLIENT_ID                          "your-thing-name"

/* Timezone offset from UTC in hours (e.g. 5 for PKT, 8 for CST) */
#define UTC_OFFSET                         X

/* MQTT Topics */
#define MOTION_TOPIC                       "sensors/room1/motion"

#define MQTT_PUBLISH_TIME_BETWEEN_MS       ( 10000U )

#define MAX_MQTT_PAYLOAD_SIZE              512
#define MAX_MQTT_TOPIC_SIZE                80

/* OTA Application: Define flash memory regions for OTA update operations */
#define USER_FLASH_FIRST_SECTOR_ADDRESS    0x08020000
#define USER_FLASH_SECOND_SECTOR_ADDRESS   0x08040000

#define USER_FLASH_LAST_SECTOR_ADDRESS     0x081C0000
#define USER_FLASH_END_ADDRESS             0x081FFFFF

#endif /* INC_APPLICATION_CONFIG_H_ */
