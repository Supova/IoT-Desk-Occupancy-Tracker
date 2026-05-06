/*
 * ota_application.h
 *
 *  Created on: Mar 25, 2025
 *      Author: Shreyas Acharya, BHARATI SOFTWARE
 */

#ifndef SRC_OTA_APPLICATION_H_
#define SRC_OTA_APPLICATION_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  OTA_SUCCESS = 0,

  /* Buffer-building errors */
  OTA_ERR_TOPIC_TRUNC = -1,
  OTA_ERR_MSG_TRUNC   = -2,
  OTA_ERR_TOPIC_BUILD = -3,

  /* Queue / transport errors */
  OTA_ERR_QUEUE_FULL = -4,
  OTA_ERR_SUBSCRIBE  = -5,
  OTA_ERR_PUBLISH    = -6,

  /* Catch-all */
  OTA_ERR_UNKNOWN = -7
} OTA_Status_t;

#define OTA_CLIENT_TOKEN    "test"

OTA_Status_t ota_start(void);

bool ota_handle_incoming_mqtt_message(char *topic, size_t topic_length,
    char *message, size_t message_length);

#endif /* SRC_OTA_APPLICATION_H_ */
