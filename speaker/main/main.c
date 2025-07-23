#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

#define SPEAKER_PIN 21
#define DESIRED_FREQ_HZ 100 // Ton

static volatile bool speaker_state = false;

// Gibt true zurück, wenn eine höhere Task aufgeweckt werden muss
static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t *edata,
                                        void *user_data) {
  // Lautsprecher-Pin umschalten
  speaker_state = !speaker_state;
  gpio_set_level(SPEAKER_PIN, speaker_state);
  // Keine höhere Task muss aufgeweckt werden
  return false;
}

void app_main(void) {
  // Konfiguriere den Lautsprecher-Pin
  gpio_reset_pin(SPEAKER_PIN);
  gpio_set_direction(SPEAKER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(SPEAKER_PIN, 0);

  gptimer_handle_t gptimer = NULL;
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1 * 1000 * 1000, // 1 MHz, 1 Tick = 1 µs
  };
  gptimer_new_timer(&timer_config, &gptimer);

  // Alarm-Konfiguration
  gptimer_alarm_config_t alarm_config = {
      .alarm_count =
          1000000 / (DESIRED_FREQ_HZ * 2), // Halbe Periode in Ticks (µs)
      .reload_count =
          0, // Nicht neu laden, da auto_reload in der Hauptkonfig ist
      .flags.auto_reload_on_alarm = true,
  };
  gptimer_set_alarm_action(gptimer, &alarm_config);

  // Registriere die Callback-Funktion für den Alarm
  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_on_alarm_cb,
  };
  gptimer_register_event_callbacks(gptimer, &cbs, NULL);

  // Aktiviere und starte den Timer
  gptimer_enable(gptimer);
  gptimer_start(gptimer);

  // Endlosschleife, damit das Programm aktiv bleibt
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
