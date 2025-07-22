#include "driver/uart.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define TXD_PIN 17    // UART TX-Pin
#define RXD_PIN 16    // UART RX-Pin
#define UART_NUM 1    // UART-Portnummer
#define BUF_SIZE 1024 // Puffergröße für empfangene Daten

static const char *TAG = "UART_COMM";

void init_uart() {
  // UART-Konfiguration
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  // Initialisierung des UART-Treibers
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));

  ESP_LOGI(TAG, "UART initialized successfully");
}

void send_data(const char *data) {
  int len = uart_write_bytes(UART_NUM, data, strlen(data));
  if (len > 0) {
    ESP_LOGI(TAG, "Sent: %s", data);
  } else {
    ESP_LOGE(TAG, "Failed to send data");
  }
}

void receive_data() {
  uint8_t data[BUF_SIZE];
  int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
  if (len > 0) {
    data[len] = '\0'; // Nullterminierung für String
    ESP_LOGI(TAG, "Received: %s", (char *)data);
  } else {
    ESP_LOGW(TAG, "No data received");
  }
}

void app_main() {
  init_uart();

  while (1) {
    // String senden
    send_data("Hello World!\n");

    // String empfangen
    receive_data();

    vTaskDelay(pdMS_TO_TICKS(2000)); // Wartezeit von 2 Sekunden
  }
}