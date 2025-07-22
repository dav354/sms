#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

// Hardware-Schnittstelle: GPIO 1 wird als Ausgang für den Trigger-Impuls des Sensors verwendet.
#define TRIGGER_GPIO 1
// Hardware-Schnittstelle: GPIO 2 wird als Eingang für das Echo-Signal des Sensors verwendet.
#define ECHO_GPIO 2

#define SPEED_OF_SOUND_CM_PER_US 0.0343f

static const char *TAG = "ULTRASONIC";

// Diese Variablen werden von einer Interrupt Service Routine (ISR) geändert und von einem
// normalen Task gelesen. 'volatile' stellt sicher, dass der Compiler den Wert
// immer aus dem Speicher liest und keine veralteten, zwischengespeicherten Werte
// in Registern verwendet.
static volatile int64_t echo_start_time = 0;
static volatile int64_t echo_end_time = 0;
static volatile bool pulse_detected = false;

// Dies ist die Interrupt Service Routine (ISR), die vom GPIO-Treiber aufgerufen wird.
// IRAM_ATTR sorgt dafür, dass der Code der Funktion im schnellen IRAM liegt,
// was für die Ausführung von Interrupts zwingend erforderlich ist.
static void IRAM_ATTR gpio_isr_handler(void *arg) {
  // Prüft den aktuellen Pegel des Echo-Pins, um zwischen steigender und fallender Flanke zu unterscheiden.
  if (gpio_get_level(ECHO_GPIO)) {
    // Steigende Flanke: Das Echo-Signal beginnt. Wir speichern den Zeitstempel.
    // esp_timer_get_time() ist ein hochauflösender Timer des ESP-IDF, ideal für solche Zeitmessungen.
    echo_start_time = esp_timer_get_time();
  } else {
    // Fallende Flanke: Das Echo-Signal ist beendet. Wir speichern den Endzeitstempel.
    echo_end_time = esp_timer_get_time();
    // Setzen eines Flags, um den Haupt-Task zu informieren, dass eine neue Messung bereitsteht.
    // Dies ist eine einfache und schnelle Methode zur Synchronisation zwischen ISR und Task.
    pulse_detected = true;
  }
}

// Konfiguriert die GPIOs mithilfe des ESP-IDF GPIO-Treibers.
static void ultrasonic_gpio_init(void) {
  // Konfiguration für den Trigger-Pin als Ausgang.
  gpio_config_t trig_io_conf = {
      .pin_bit_mask = (1ULL << TRIGGER_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .intr_type = GPIO_INTR_DISABLE, // Kein Interrupt für den Trigger-Pin.
  };
  gpio_config(&trig_io_conf);

  // Konfiguration für den Echo-Pin als Eingang.
  gpio_config_t echo_io_conf = {
      .pin_bit_mask = (1ULL << ECHO_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      // Der Interrupt soll bei JEDER Flanke (steigend und fallend) auslösen.
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  gpio_config(&echo_io_conf);

  // ESP-IDF spezifisch: Installiert den globalen ISR-Handler-Service des GPIO-Treibers.
  // Dies ist eine Voraussetzung, um ISRs für einzelne Pins registrieren zu können.
  gpio_install_isr_service(0);
  // Verbindet unsere Funktion `gpio_isr_handler` mit dem Interrupt des ECHO_GPIO Pins.
  gpio_isr_handler_add(ECHO_GPIO, gpio_isr_handler, NULL);

  ESP_LOGI(TAG, "GPIOs configured.");
}

// Sendet den 10µs langen Trigger-Impuls, um eine Messung zu starten.
static void send_trigger_pulse(void) {
  gpio_set_level(TRIGGER_GPIO, 0);
  // esp_rom_delay_us ist eine einfache, blockierende Warteschleife aus der ESP-ROM.
  // Sie ist ungenau, aber für kurze Delays wie hier ausreichend.
  esp_rom_delay_us(2);
  gpio_set_level(TRIGGER_GPIO, 1);
  esp_rom_delay_us(10);
  gpio_set_level(TRIGGER_GPIO, 0);
}

// Dies ist ein FreeRTOS-Task, der die Hauptlogik des Programms enthält.
void ultrasonic_test_task(void *pvParameters) {
  ultrasonic_gpio_init();

  while (1) {
    pulse_detected = false;
    send_trigger_pulse();

    // vTaskDelay ist die Standard-FreeRTOS-Funktion für Wartezeiten.
    // pdMS_TO_TICKS wandelt Millisekunden in System-Ticks um.
    // Dies ist ein nicht-blockierendes Warten, d.h. der Task wird in den Ruhezustand
    // versetzt und andere Tasks können in der Zwischenzeit laufen.
    // Wir nutzen es hier als Timeout, falls kein Echo empfangen wird.
    vTaskDelay(pdMS_TO_TICKS(100));

    if (pulse_detected) {
      // Die ISR hat das Flag gesetzt, also können wir die Zeitdifferenz berechnen.
      int64_t pulse_duration_us = echo_end_time - echo_start_time;
      float distance_cm = (pulse_duration_us / 2.0f) * SPEED_OF_SOUND_CM_PER_US;

      if (distance_cm > 2 && distance_cm < 400) {
        ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);
      } else {
        ESP_LOGI(TAG, "Out of range (%.2f cm)", distance_cm);
      }
    } else {
      ESP_LOGW(TAG, "No echo received (timeout).");
    }

    // Warte eine Sekunde, bevor die nächste Messung gestartet wird.
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) {
  // ESP-IDF Programme basieren auf FreeRTOS. Die Hauptlogik wird typischerweise
  // in einen oder mehrere Tasks ausgelagert. xTaskCreate startet einen neuen Task.
  xTaskCreate(ultrasonic_test_task, "ultrasonic_test_task", 4096, NULL, 5,
              NULL);
}
