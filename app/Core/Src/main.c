/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "esp32_at.h"
#include "application_config.h"
#include "mqtt_helper.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { SESSION_IDLE, SESSION_SITTING, SESSION_GRACE } session_state_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HEARTBEAT_INTERVAL_MS (5 * 60 * 1000) // 5 mins
#define GRACE_PERIOD_MS (3 * 60 * 1000)       // 3 mins
#define MQTT_PAYLOAD_MAX_LEN 64
#define MS_TO_MIN(ms) ((ms) / 60000UL)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_uart4_rx;

/* USER CODE BEGIN PV */
volatile uint8_t pir_motion_detected = 0;
uint32_t boot_epoch_s = 0;
uint32_t boot_tick_ms = 0;

QueueHandle_t mqtt_tx_queue;
QueueHandle_t mqtt_rx_queue;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_UART4_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* PIR sitting session state machine - queues MQTT publishes to mqtt_tx_queue */
void mqtt_publish_task(void *parameters) {
	uint32_t now_s = 0;
	uint32_t hh = 0;
	uint32_t mm = 0;
	uint32_t ss = 0;

	session_state_t session_state = SESSION_IDLE;
	uint32_t session_start_ms = 0;
	uint32_t last_heartbeat_ms = 0;
	uint32_t grace_start_ms = 0;
	uint32_t current_time_ms = 0;
	uint32_t session_duration_mins = 0;

	mqtt_queue_item_t item = {
	    .operation = MQTT_OPERATION_PUBLISH,
	    .topic = MOTION_TOPIC,
	    .topic_length = strlen(MOTION_TOPIC),
	};

	while (1) {
		current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
		now_s = (boot_epoch_s + (current_time_ms - boot_tick_ms) / 1000) % 86400;
		hh = now_s / 3600;
		mm = (now_s % 3600) / 60;
		ss = now_s % 60;

		switch (session_state) {
			case SESSION_IDLE:
				if (pir_motion_detected) {
					session_start_ms = current_time_ms;
					last_heartbeat_ms = current_time_ms; // every 5 mins
					size_t payload_len =
					    snprintf((char *)item.payload, sizeof(item.payload), "{\"at_desk\":true,\"duration_mins\": 0}");

					item.payload_length = payload_len;

					if (xQueueSend(mqtt_tx_queue, &item, portMAX_DELAY) == pdPASS) {
						LogInfo(("Queued MQTT publish: Topic='%s'", MOTION_TOPIC));
					} else {
						LogError(("Failed to queue MQTT publish: Topic='%s'", MOTION_TOPIC));
					}

					LogDebug(("PIR triggered - session started at %02lu:%02lu:%02lu", hh, mm, ss));
					session_state = SESSION_SITTING;
				}
				break;

			case SESSION_SITTING: // pir HIGH
				if (!pir_motion_detected) {
					grace_start_ms = current_time_ms;
					LogDebug(("Motion lost - entering grace period at %02lu:%02lu:%02lu", hh, mm, ss));
					session_state = SESSION_GRACE;
				} else {
					// if i am still sitting right now
					// check if enough time has passed for a heart beat (5 mins)
					// if so, publish a message with the current session duration
					if (current_time_ms - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
						session_duration_mins = MS_TO_MIN(current_time_ms - session_start_ms);
						size_t payload_len = snprintf(
						    (char *)item.payload,
						    sizeof(item.payload),
						    "{\"at_desk\":true,\"duration_mins\": %lu}",
						    session_duration_mins
						);
						item.payload_length = payload_len;

						if (xQueueSend(mqtt_tx_queue, &item, portMAX_DELAY) == pdPASS) {
							LogInfo(("Queued MQTT publish: Topic='%s'", MOTION_TOPIC));
						} else {
							LogError(("Failed to queue MQTT publish: Topic='%s'", MOTION_TOPIC));
						}

						LogDebug(
						    ("Heartbeat publish - duration_mins: %lu at %02lu:%02lu:%02lu",
						     session_duration_mins,
						     hh,
						     mm,
						     ss)
						);

						last_heartbeat_ms = current_time_ms;
					}
				}
				break;

			case SESSION_GRACE: // grace timer running
				if (pir_motion_detected) {
					LogDebug(("Motion resumed in grace period at %02lu:%02lu:%02lu - back to SITTING", hh, mm, ss));
					session_state = SESSION_SITTING;
				} else {
					// check if 60s passed (task will be checked twice in this period)
					if (current_time_ms - grace_start_ms >= GRACE_PERIOD_MS) {
						session_duration_mins = MS_TO_MIN(current_time_ms - session_start_ms);
						size_t payload_len = snprintf(
						    (char *)item.payload,
						    sizeof(item.payload),
						    "{\"session_end\":true,\"total_duration_mins\": %lu}",
						    session_duration_mins
						);
						item.payload_length = payload_len;

						if (xQueueSend(mqtt_tx_queue, &item, portMAX_DELAY) == pdPASS) {
							LogInfo(("Queued MQTT publish: Topic='%s'", MOTION_TOPIC));
						} else {
							LogError(("Failed to queue MQTT publish: Topic='%s'", MOTION_TOPIC));
						}

						LogDebug(
						    ("Grace period expired at %02lu:%02lu:%02lu - session ended, total_mins: %lu",
						     hh,
						     mm,
						     ss,
						     session_duration_mins)
						);
						session_state = SESSION_IDLE;
					}
				}
				break;
		}
		vTaskDelay(pdMS_TO_TICKS(MQTT_PUBLISH_TIME_BETWEEN_MS));
	}
}

/* Logs incoming MQTT messages */
void mqtt_receive_task(void *parameters) {
	mqtt_queue_item_t item;

	while (1) {
		xQueueReceive(mqtt_rx_queue, &item, portMAX_DELAY);

		if (item.operation == MQTT_OPERATION_RECEIVE && item.topic_length > 0 && item.payload_length > 0) {
			LogInfo(("Topic: %.*s", (int)item.topic_length, item.topic));
			LogInfo(("Message: %.*s", (int)item.payload_length, item.payload));
		}
	}
}

/* Sole USART2 owner: drain the TX queue (send publishes) & poll for incoming MQTT RX data */
void at_cmd_dispatcher_task(void *parameters) {
	(void)parameters;

	mqtt_queue_item_t item, new_message = {0};
	mqtt_receive_t mqtt_data = {0};
	mqtt_status_t status = MQTT_ERROR;

	static char topic_buffer[MAX_MQTT_TOPIC_SIZE];
	static char payload_buffer[MAX_MQTT_PAYLOAD_SIZE];

	mqtt_data.p_payload = payload_buffer;
	mqtt_data.p_topic = topic_buffer;
	mqtt_data.topic_length = sizeof(topic_buffer);
	mqtt_data.payload_length = sizeof(payload_buffer);

	while (1) {

		// 1. De-queue MQTT TX queue
		if (xQueueReceive(mqtt_tx_queue, &item, 0) == pdPASS) {
			if (item.operation == MQTT_OPERATION_PUBLISH) {

				// 2. Send MQTT publish AT command using mqtt_publish()
				status = mqtt_publish(item.topic, item.topic_length, item.payload, item.payload_length);

				if (status == MQTT_SUCCESS) {
					LogInfo(
					    ("MQTT Publish successful: Topic='%.*s', Payload='%.*s'",
					     (int)item.topic_length,
					     item.topic,
					     (int)item.payload_length,
					     (char *)item.payload)
					);
				} else {
					LogError(("MQTT Publish failed: Topic='%.*s'", (int)item.topic_length, item.topic));
				}

			} else if (item.operation == MQTT_OPERATION_SUBSCRIBE) {
				// 2. Send MQTT subscribe AT command using mqtt_subscribe()
				status = mqtt_subscribe(item.topic, item.topic_length);
				if (status == MQTT_SUCCESS) {
					LogInfo(("MQTT Subscribe successful: Topic='%.*s'", (int)item.topic_length, item.topic));
				} else {
					LogError(("MQTT Subscribe failed: Topic='%.*s'", (int)item.topic_length, item.topic));
				}
			}
		}

		// 3. check for incoming MQTT data
		esp32_status_t rx_status = esp32_recv_mqtt_data(&mqtt_data);

		if (rx_status != ESP32_ERROR && mqtt_data.payload_length > 0) {
			new_message.operation = MQTT_OPERATION_RECEIVE;
			new_message.payload_length = mqtt_data.payload_length;
			new_message.topic_length = mqtt_data.topic_length;
			memcpy(new_message.payload, mqtt_data.p_payload, mqtt_data.payload_length);
			memcpy(new_message.topic, mqtt_data.p_topic, mqtt_data.topic_length);

			if (xQueueSend(mqtt_rx_queue, &new_message, 0) == pdPASS) {
				LogInfo(("Queued MQTT receive: Topic='%.*s'", (int)new_message.topic_length, new_message.topic));
			}
		}
	}
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_UART4_Init();
	MX_USART2_UART_Init();
	/* USER CODE BEGIN 2 */

	/* Initialize the ESP32 Wi-Fi module*/
	LogInfo(("Initializing Wi-Fi module..."));
	if (esp32_init() != ESP32_OK) {
		LogError(("Failed to initialize Wi-Fi module."));
		Error_Handler();
	}
	LogInfo(("Wi-Fi module initialized successfully!\n"));

	/* Connect to the Wi-Fi network*/
	LogInfo(("Joining Access Point: '%s' ...", WIFI_SSID));
	/* Keep attempting to connect to the specified Wi-Fi access point until
	 * successful */
	while (esp32_join_ap((uint8_t *)WIFI_SSID, (uint8_t *)WIFI_PASSWORD) != ESP32_OK) {
		LogInfo(("Retrying to join Access Point: %s", WIFI_SSID));
	}
	LogInfo(("Successfully joined Access Point: %s", WIFI_SSID));

	/* Configure SNTP for time synchronization */
	if (esp32_config_sntp(UTC_OFFSET) != ESP32_OK) {
		LogError(("Failed to configure SNTP."));
		Error_Handler();
	}
	LogInfo(("SNTP configured !"));

	/* Retrieve the current time from SNTP */
	sntp_time_t sntp_time;
	if (esp32_get_sntp_time(&sntp_time) != ESP32_OK) {
		LogError(("Failed to retrieve current time from SNTP."));
		Error_Handler();
	}

	boot_epoch_s = sntp_time.hour * 3600 + sntp_time.min * 60 + sntp_time.sec;
	boot_tick_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

	LogInfo(
	    ("SNTP time retrieved: %s, %02d %s %04d %02d:%02d:%02d",
	     sntp_time.day,
	     sntp_time.date,
	     sntp_time.month,
	     sntp_time.year,
	     sntp_time.hour,
	     sntp_time.min,
	     sntp_time.sec)
	);

	/* Configure the MQTT client for TLS */
	LogInfo(("Connecting to MQTT broker at %s:%d...", MQTT_BROKER, MQTT_PORT));
	if (mqtt_connect(CLIENT_ID, MQTT_BROKER, MQTT_PORT) != MQTT_SUCCESS) {
		LogError(("MQTT connection failed."));
		Error_Handler();
	}
	LogInfo(("Successfully connected to MQTT broker: %s", MQTT_BROKER));

	// loop back for subscribing to same topic it's publishing to
	LogInfo(("Subscribing to topic: %s", MOTION_TOPIC));
	if (mqtt_subscribe(MOTION_TOPIC, strlen(MOTION_TOPIC)) != MQTT_SUCCESS) {
		LogError(("Subscription to topic '%s' failed.", MOTION_TOPIC));
		Error_Handler();
	}
	LogInfo(("Successfully Subscribed to topic: %s", MOTION_TOPIC));

	mqtt_tx_queue = xQueueCreate(5, sizeof(mqtt_queue_item_t));
	mqtt_rx_queue = xQueueCreate(5, sizeof(mqtt_queue_item_t));

	if ((mqtt_tx_queue == NULL) || (mqtt_rx_queue == NULL)) {
		LogError(("Queue creation failed."));
		Error_Handler();
	} else {
		LogInfo(("MQTT RX and TX queues successfully created."));
	}

	BaseType_t status;
	status = xTaskCreate(mqtt_publish_task, "Publish sensor data", 2048, NULL, 2, NULL);
	configASSERT(status == pdPASS);

	status = xTaskCreate(mqtt_receive_task, "Receive sensor data", 2048, NULL, 2, NULL);
	configASSERT(status == pdPASS);

	status = xTaskCreate(at_cmd_dispatcher_task, "MQTT Send and Receive", 2048, NULL, 1, NULL);
	configASSERT(status == pdPASS);

	vTaskStartScheduler();
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief UART4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_UART4_Init(void) {

	/* USER CODE BEGIN UART4_Init 0 */

	/* USER CODE END UART4_Init 0 */

	/* USER CODE BEGIN UART4_Init 1 */

	/* USER CODE END UART4_Init 1 */
	huart4.Instance = UART4;
	huart4.Init.BaudRate = 115200;
	huart4.Init.WordLength = UART_WORDLENGTH_8B;
	huart4.Init.StopBits = UART_STOPBITS_1;
	huart4.Init.Parity = UART_PARITY_NONE;
	huart4.Init.Mode = UART_MODE_TX_RX;
	huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart4.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart4) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN UART4_Init 2 */

	/* USER CODE END UART4_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Stream2_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin : PD1 */
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_1) {
		pir_motion_detected = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1);
	}
}
/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM6) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
