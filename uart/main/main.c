#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "hal/uart_ll.h"
#include "soc/uart_struct.h"
#include "soc/uart_reg.h"
#include "soc/uart_periph.h"
#include "string.h"

/*
This program lets the LED on the ESP32-S3 blink when a message via UART is received.
*/

#define UART_PORT UART_NUM_0
#define LED_GPIO GPIO_NUM_2

static const char *TAG = "uart_interrupt";
static const char *RESPONSE = "LED toggled!\r\n";

// UART RX interrupt handler
static void IRAM_ATTR uart_rx_isr(void *arg) {
    uint8_t dummy;
    uart_dev_t *hw = UART_LL_GET_HW(UART_PORT);

    // Read all bytes from RX FIFO to clear the interrupt.
    while (uart_ll_get_rxfifo_len(hw)) {
        uart_ll_read_rxfifo(hw, &dummy, 1);
    }

    // Clear RX FIFO full and timeout interrupt flags.
    uart_ll_clr_intsts_mask(hw, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);

    // Toggle LED.
    int current = gpio_get_level(LED_GPIO);
    gpio_set_level(LED_GPIO, !current);

    // Send a response message back over UART.
    for (int i = 0; i < strlen(RESPONSE); i++) {
        // Wait until there is space in the TX FIFO.
        while (hw->status.txfifo_cnt >= SOC_UART_FIFO_LEN) {
            ;
        }
        uart_ll_write_txfifo(hw, (const uint8_t *)&RESPONSE[i], 1);
    }
}

void app_main() {
    uart_dev_t *hw = UART_LL_GET_HW(UART_PORT);

    // Initialize LED GPIO.
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // Configure UART.
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_PORT, &uart_config);
    uart_driver_install(UART_PORT, 1024, 0, 0, NULL, 0);

    // Disable and clear all UART interrupts using driver functions.
    uart_disable_intr_mask(UART_PORT, UART_LL_INTR_MASK);
    uart_clear_intr_status(UART_PORT, UART_LL_INTR_MASK);

    // Clear any pending RX interrupts and enable RX FIFO full and timeout interrupts.
    uart_ll_clr_intsts_mask(hw, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);
    uart_ll_ena_intr_mask(hw, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);

    // Register our ISR using esp_intr_alloc with the UART peripheral's IRQ.
    esp_intr_alloc(uart_periph_signal[UART_PORT].irq, ESP_INTR_FLAG_IRAM, uart_rx_isr, NULL, NULL);

    ESP_LOGI(TAG, "UART interrupt initialized. Send data to toggle LED.");
}
