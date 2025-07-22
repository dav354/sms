#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "hal/uart_ll.h"
#include "soc/uart_struct.h"
#include "string.h"

// Hardware-Schnittstelle: UART Port 0 wird für die serielle Kommunikation verwendet.
#define UART_PORT UART_NUM_0
// Hardware-Schnittstelle: GPIO 2 ist mit einer LED verbunden.
#define LED_GPIO GPIO_NUM_2

static const char *TAG = "uart_interrupt";
static const char *RESPONSE = "LED toggled!\r\n";

// Dies ist eine Low-Level UART Interrupt Service Routine (ISR).
// IRAM_ATTR stellt sicher, dass der Code im schnellen IRAM liegt.
static void IRAM_ATTR uart_rx_isr(void *arg) {
    uint8_t dummy;
    // Direkter Zugriff auf die Hardware-Register des UART-Peripherals.
    uart_dev_t *hw = UART_LL_GET_HW(UART_PORT);

    // Leert den RX-FIFO-Puffer, um den Interrupt-Status zurückzusetzen.
    // uart_ll_* Funktionen sind Low-Level-Funktionen, die direkt auf die Hardware zugreifen.
    while (uart_ll_get_rxfifo_len(hw)) {
        uart_ll_read_rxfifo(hw, &dummy, 1);
    }

    // Löscht manuell die Interrupt-Flags in den Hardware-Registern.
    uart_ll_clr_intsts_mask(hw, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);

    // Logik, die bei Empfang von Daten ausgeführt wird.
    int current = gpio_get_level(LED_GPIO);
    gpio_set_level(LED_GPIO, !current);

    // ACHTUNG: Das Senden von Daten innerhalb einer ISR ist eine schlechte Praxis.
    // Die `while`-Schleife unten ist eine "busy-wait"-Schleife, die den Prozessor
    // blockiert, bis Platz im Sende-Puffer ist. Wenn der Empfänger die Daten
    // nicht schnell genug liest, kann dies das gesamte System zum Absturz bringen,
    // da keine anderen Interrupts (z.B. vom Betriebssystem-Timer) mehr bearbeitet werden.
    for (int i = 0; i < strlen(RESPONSE); i++) {
        // Blockierendes Warten auf freien Platz im TX-FIFO.
        while (hw->status.txfifo_cnt >= SOC_UART_FIFO_LEN) {
            ;
        }
        // Direkter Schreibzugriff auf das TX-FIFO-Register.
        uart_ll_write_txfifo(hw, (const uint8_t *)&RESPONSE[i], 1);
    }
}

void app_main(void) {
    // Direkter Zeiger auf die UART-Hardware-Register.
    uart_dev_t *hw = UART_LL_GET_HW(UART_PORT);

    // Konfiguration des LED-Pins als Ausgang.
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // Standard-Konfiguration des UART-Treibers von ESP-IDF.
    uart_config_t uart_config = {
        .baud_rate = 1152200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT, &uart_config);
    // Installiert den Treiber, aber wir werden sein Event-System umgehen.
    uart_driver_install(UART_PORT, 1024, 0, 0, NULL, 0);

    // Manuelle Konfiguration der Interrupts auf Hardware-Ebene.
    // Dies umgeht die Abstraktionen des UART-Treibers.
    uart_disable_intr_mask(UART_PORT, UART_LL_INTR_MASK);
    uart_clear_intr_status(UART_PORT, UART_LL_INTR_MASK);

    // Aktiviert nur die Interrupts, auf die wir reagieren wollen (RX FIFO voll oder Timeout).
    uart_ll_clr_intsts_mask(hw, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);
    uart_ll_ena_intr_mask(hw, UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT);

    // Registriert unsere ISR direkt beim Interrupt-Controller des ESP32.
    // Dies ist eine Low-Level-Schnittstelle des ESP-IDF.
    esp_intr_alloc(uart_periph_signal[UART_PORT].irq, ESP_INTR_FLAG_IRAM, uart_rx_isr, NULL, NULL);

    ESP_LOGI(TAG, "UART interrupt initialized. Send data to toggle LED.");
}

