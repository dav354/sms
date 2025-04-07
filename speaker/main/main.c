/*
For ESP32-S3
Code by David Glaesle & ChatBro

This code plays some tones with a speaker.
The Speaker needs to be connected to GND and Pin 21.
*/

#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SPEAKER_PIN 21
#define TIMER_DIVIDER 80 // Annahme: APB-Clock = 80 MHz → Timer läuft mit 1 MHz
#define DESIRED_FREQ_HZ 100 // Gewünschte Frequenz (Ton)
#define HALF_PERIOD_US                                                         \
  (1000000 / (DESIRED_FREQ_HZ * 2)) // Halbe Periode in µs (bei 100 Hz: 5000 µs)

static volatile bool speaker_state = false;

// Timer-Interrupt-Routine (muss im IRAM liegen)
static void IRAM_ATTR timer_isr(void *arg) {
  // Interrupt-Status löschen
  TIMER_0.int_clr_timers.t0 = BIT((int)arg);
  // Alarm wieder aktivieren (Auto-Reload ist gesetzt)
  TIMER_0.hw_timer[(int)arg].config.alarm_en = TIMER_ALARM_EN;
  // Lautsprecher-Pin umschalten
  speaker_state = !speaker_state;
  gpio_set_level(SPEAKER_PIN, speaker_state);
}

void app_main(void) {
  // Konfiguriere den Lautsprecher-Pin
  gpio_reset_pin(SPEAKER_PIN);
  gpio_set_direction(SPEAKER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(SPEAKER_PIN, 0);

  // Timer-Konfiguration
  timer_config_t config = {
      .divider = TIMER_DIVIDER,
      .counter_dir = TIMER_COUNT_UP,
      .counter_en = TIMER_PAUSE, // zunächst pausieren
      .alarm_en = TIMER_ALARM_EN,
      .auto_reload = true, // automatisch neu laden
  };

  // Initialisiere Timer 0 in Timer-Gruppe 0
  timer_init(TIMER_GROUP_0, TIMER_0, &config);
  // Setze Alarmwert für den halben Perioden-Takt (z. B. 5000 µs für 100 Hz)
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, HALF_PERIOD_US);
  // Aktiviere den Interrupt für den Timer
  timer_enable_intr(TIMER_GROUP_0, TIMER_0);
  timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_isr, (void *)TIMER_0,
                     ESP_INTR_FLAG_IRAM, NULL);
  // Starte den Timer
  timer_start(TIMER_GROUP_0, TIMER_0);

  // Endlosschleife, damit das Programm aktiv bleibt
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
