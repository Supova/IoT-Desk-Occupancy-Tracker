/*
 * ota_application.c
 *
 *  Created on: Mar 25, 2025
 *      Author: Shreyas Acharya, BHARATI SOFTWARE
 *  Modified by Supova: target slot derived from boot descriptor for A/B OTA
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ota_application.h"
#include "custom_job_doc.h"
#include "custom_job_parser.h"

#include "ota_flash.h"
#include "application_config.h"
#include "MQTTFileDownloader_config.h"
#include "mqtt_helper.h"
#include "MQTTFileDownloader.h"
#include "jobs.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "ota_job_processor.h"
#include "boot_descriptor.h"

#define MAX_THING_NAME_SIZE 128U
#define MAX_JOB_ID_LENGTH 64U
#define START_JOB_MSG_LENGTH 147U
#define UPDATE_JOB_MSG_LENGTH 48U

MqttFileDownloaderContext_t mqtt_file_downloader_context = {0};
static uint32_t num_of_blocks_remaining = 0;
static uint32_t current_block_offset = 0;
static uint8_t current_file_id = 0;
static uint32_t total_bytes_received = 0;
static uint32_t ota_target_address = 0;
char global_job_id[MAX_JOB_ID_LENGTH] = {0};

static void handle_mqtt_streams_block_arrived(uint8_t *data, size_t data_length);
static void finish_download();
static bool job_metadata_handler_chain(char *topic, size_t topic_length);
static void request_data_block(void);
static bool job_handler_chain(char *message, size_t message_length);

extern QueueHandle_t mqtt_tx_queue;

/*=====================================================================================================================*/
/**
 * @brief Starts the OTA process by publishing a
 *        StartNextPendingJobExecution request to AWS IoT Jobs.
 */
OTA_Status_t ota_start(void) {

	char topic_buf[TOPIC_BUFFER_SIZE + 1] = {0};
	char msg_buf[START_JOB_MSG_LENGTH] = {0};
	size_t topic_len, msg_len;
	mqtt_queue_item_t pub_item = {0};

	if (Jobs_StartNext(topic_buf, sizeof(topic_buf), CLIENT_ID, strlen(CLIENT_ID), &topic_len) != JobsSuccess) {

		LogError(("Jobs_StartNext failed"));
		return OTA_ERR_TOPIC_BUILD;
	}

	msg_len = Jobs_StartNextMsg(OTA_CLIENT_TOKEN, sizeof(OTA_CLIENT_TOKEN) - 1U, msg_buf, sizeof(msg_buf));

	if (msg_len > sizeof(msg_buf)) {

		LogError(("StartNextMsg truncated"));
		return OTA_ERR_MSG_TRUNC;
	}

	pub_item.operation = MQTT_OPERATION_PUBLISH;
	pub_item.topic_length = topic_len;
	pub_item.payload_length = msg_len;
	memcpy(pub_item.topic, topic_buf, topic_len + 1U);
	memcpy(pub_item.payload, msg_buf, msg_len);

	if (xQueueSend(mqtt_tx_queue, &pub_item, 0) != pdPASS) {
		LogError(("PUBLISH enqueue failed"));
		return OTA_ERR_QUEUE_FULL;
	}

	return OTA_SUCCESS;
}

/**
 * @brief Handles incoming MQTT messages related to OTA jobs and data blocks.
 */
bool ota_handle_incoming_mqtt_message(char *topic, size_t topic_length, char *message, size_t message_length) {

	bool handled = false;
	int32_t file_id = 0;
	int32_t block_id = 0;
	int32_t block_size = 0;

	handled = job_metadata_handler_chain(topic, topic_length);

	if (!handled) {

		handled = Jobs_IsStartNextAccepted(topic, topic_length, CLIENT_ID, strlen(CLIENT_ID));

		if (handled) {

			handled = job_handler_chain((char *)message, message_length);
		} else {

			handled = mqttDownloader_isDataBlockReceived(&mqtt_file_downloader_context, topic, topic_length);

			if (handled) {

				uint8_t decoded_data[mqttFileDownloader_CONFIG_BLOCK_SIZE];
				size_t decoded_data_length = 0;

				handled = mqttDownloader_processReceivedDataBlock(
				    &mqtt_file_downloader_context,
				    message,
				    message_length,
				    &file_id,
				    &block_id,
				    &block_size,
				    decoded_data,
				    &decoded_data_length
				);

				handle_mqtt_streams_block_arrived(decoded_data, decoded_data_length);
			}
		}
	}

	if (!handled) {
		LogError(
		    ("Unrecognized incoming MQTT message received on topic: "
		     "%.*s\nMessage: %.*s\n",
		     (unsigned int)topic_length,
		     topic,
		     (unsigned int)message_length,
		     (char *)message)
		);
	}

	return handled;
}

/**
 * @brief Processes OTA job metadata messages and clears global job ID
 *        if the job update status is accepted or rejected.
 */
static bool job_metadata_handler_chain(char *topic, size_t topic_length) {

	bool handled = false;

	if (global_job_id[0] != 0) {

		handled = Jobs_IsJobUpdateStatus(
		    topic,
		    topic_length,
		    (const char *)&global_job_id,
		    strnlen(global_job_id, MAX_JOB_ID_LENGTH),
		    CLIENT_ID,
		    strlen(CLIENT_ID),
		    JobUpdateStatus_Accepted
		);

		if (handled) {

			LogInfo(("Job was accepted! Clearing Job ID."));
			global_job_id[0] = 0;
		} else {

			handled = Jobs_IsJobUpdateStatus(
			    topic,
			    topic_length,
			    (const char *)&global_job_id,
			    strnlen(global_job_id, MAX_JOB_ID_LENGTH),
			    CLIENT_ID,
			    strlen(CLIENT_ID),
			    JobUpdateStatus_Rejected
			);
		}

		if (handled) {

			LogInfo(("Job was rejected! Clearing Job ID."));
			global_job_id[0] = 0;
		}
	}

	return handled;
}

/**
 * @brief Processes job file details extracted from the job document.
 */
static void process_job_file(custom_job_doc_fields_t *params) {
	num_of_blocks_remaining = params->file_size / mqttFileDownloader_CONFIG_BLOCK_SIZE;
	num_of_blocks_remaining += (params->file_size % mqttFileDownloader_CONFIG_BLOCK_SIZE > 0) ? 1 : 0;

	current_file_id = params->file_id;
	current_block_offset = 0;
	total_bytes_received = 0;

	mqttDownloader_init(
	    &mqtt_file_downloader_context,
	    params->image_ref,
	    params->image_ref_len,
	    CLIENT_ID,
	    strlen(CLIENT_ID),
	    DATA_TYPE_JSON
	);

	mqtt_queue_item_t queue_item = {0};
	queue_item.operation = MQTT_OPERATION_SUBSCRIBE;
	queue_item.topic_length = mqtt_file_downloader_context.topicStreamDataLength;
	memcpy(
	    queue_item.topic,
	    mqtt_file_downloader_context.topicStreamData,
	    mqtt_file_downloader_context.topicStreamDataLength
	);

	if (xQueueSend(mqtt_tx_queue, &queue_item, portMAX_DELAY) != pdPASS) {
		LogError(("Failed to queue MQTT subscribe: Topic='%s'", mqtt_file_downloader_context.topicStreamData));
	}

	LogInfo(("Starting The Download."));

	boot_descriptor_t desc;
	boot_descriptor_read(&desc);
	ota_target_address =
	    (desc.active_slot == SLOT_A) ? USER_FLASH_SECOND_SECTOR_ADDRESS : USER_FLASH_FIRST_SECTOR_ADDRESS;
	flash_write_address = ota_target_address;
	flash_erase(ota_target_address);

	request_data_block();
}

/**
 * @brief Handles parsing and processing of the OTA job document.
 */
static bool job_handler_chain(char *message, size_t message_length) {

	char *job_doc;
	size_t job_doc_length = 0U;
	char *jobId;
	size_t job_id_length = 0U;
	int8_t file_index = 0;

	job_doc_length = Jobs_GetJobDocument(message, message_length, (const char **)&job_doc);

	job_id_length = Jobs_GetJobId(message, message_length, (const char **)&jobId);

	if (global_job_id[0] == 0) {

		strncpy(global_job_id, jobId, job_id_length);
	}

	if ((job_doc_length != 0U) && (job_id_length != 0U)) {

		custom_job_doc_fields_t job_fields = {0};

		do {
			file_index = custom_parser_parse_job_doc_file(job_doc, job_doc_length, &job_fields);

			if (file_index >= 0) {

				LogInfo(("Received Custom Job Document"));

				process_job_file(&job_fields);
				LogInfo(("Successfully processed job document.\n"));
			}
		} while (file_index > 0);

	} else {

		LogInfo(("No job available."));
	}

	return file_index == 0;
}

/**
 * @brief Sends a request to AWS IoT Core for the next OTA data block.
 */
static void request_data_block(void) {

	char get_stream_request[GET_STREAM_REQUEST_BUFFER_SIZE];
	size_t get_stream_request_length = 0U;
	static mqtt_queue_item_t queue_item = {0};

	get_stream_request_length = mqttDownloader_createGetDataBlockRequest(
	    mqtt_file_downloader_context.dataType,
	    current_file_id,
	    mqttFileDownloader_CONFIG_BLOCK_SIZE,
	    current_block_offset,
	    1,
	    get_stream_request,
	    GET_STREAM_REQUEST_BUFFER_SIZE
	);

	queue_item.operation = MQTT_OPERATION_PUBLISH;
	queue_item.payload_length = get_stream_request_length;
	queue_item.topic_length = mqtt_file_downloader_context.topicGetStreamLength;
	memcpy(queue_item.payload, get_stream_request, get_stream_request_length);
	memcpy(
	    queue_item.topic,
	    mqtt_file_downloader_context.topicGetStream,
	    mqtt_file_downloader_context.topicGetStreamLength
	);

	if (xQueueSend(mqtt_tx_queue, &queue_item, portMAX_DELAY) != pdPASS) {
		LogError(
		    ("Failed to queue MQTT publish: Topic='%.*s', Payload='%.*s'",
		     (int)queue_item.topic_length,
		     queue_item.topic,
		     (int)queue_item.payload_length,
		     (char *)queue_item.payload)
		);
	}
}

/**
 * @brief Handles incoming OTA data block from MQTT Streams and writes it to flash.
 */
uint32_t flash_write_address = 0;
static void handle_mqtt_streams_block_arrived(uint8_t *data, size_t data_length) {

	uint32_t flash_length = data_length / 4;

	flash_status_t f_status = FLASH_OK;
	f_status = flash_write(&flash_write_address, (uint32_t *)data, flash_length);

	if (f_status != FLASH_OK) {
		LogError(("Flash write error"));
	}

	total_bytes_received += data_length;
	num_of_blocks_remaining--;

	LogInfo(("Downloaded block %lu of %lu.", current_block_offset, (current_block_offset + num_of_blocks_remaining)));

	if (num_of_blocks_remaining == 0) {
		finish_download();
	} else {
		current_block_offset++;
		request_data_block();
	}
}

/**
 * @brief Completes the OTA download, updates job status, and resets into new slot.
 */
static void finish_download() {

	char topic_buffer[TOPIC_BUFFER_SIZE + 1] = {0};
	size_t topic_buffer_length = 0U;
	char message_buffer[UPDATE_JOB_MSG_LENGTH] = {0};
	mqtt_queue_item_t queue_item = {0};

	Jobs_Update(
	    topic_buffer,
	    TOPIC_BUFFER_SIZE,
	    CLIENT_ID,
	    strlen(CLIENT_ID),
	    global_job_id,
	    strlen(global_job_id),
	    &topic_buffer_length
	);

	size_t message_buffer_length = Jobs_UpdateMsg(Succeeded, "2", 1U, message_buffer, UPDATE_JOB_MSG_LENGTH);

	queue_item.operation = MQTT_OPERATION_PUBLISH;
	queue_item.payload_length = message_buffer_length;
	queue_item.topic_length = topic_buffer_length;
	memcpy(queue_item.payload, message_buffer, message_buffer_length);
	memcpy(queue_item.topic, topic_buffer, topic_buffer_length);

	if (xQueueSend(mqtt_tx_queue, &queue_item, portMAX_DELAY) != pdPASS) {
		LogError(("Failed to queue MQTT publish: Topic='%s'", topic_buffer));
	}

	vTaskDelay(pdMS_TO_TICKS(1000));

	LogInfo(("\033[1;32mOTA download complete. Updating boot descriptor and resetting.\033[0m\n"));

	boot_descriptor_t desc;
	boot_descriptor_read(&desc);
	desc.active_slot    = (ota_target_address == USER_FLASH_SECOND_SECTOR_ADDRESS) ? SLOT_B : SLOT_A;
	desc.slot_b_valid   = (desc.active_slot == SLOT_B) ? SLOT_VALID_MAGIC : desc.slot_b_valid;
	desc.slot_a_valid   = (desc.active_slot == SLOT_A) ? SLOT_VALID_MAGIC : desc.slot_a_valid;
	desc.boot_try_count = 1;
	desc.slot_confirmed = 0;
	boot_descriptor_write(&desc);
	NVIC_SystemReset();
}
