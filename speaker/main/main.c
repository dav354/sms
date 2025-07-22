/**
 * @file main.c
 * @brief Example of generating a tone on a speaker using a hardware timer
 * (gptimer).
 *
 * This application configures a general-purpose timer to trigger an interrupt
 * at a specific frequency. The interrupt service routine (ISR) then toggles a
 * GPIO pin, creating a square wave that can drive a simple speaker or buzzer.
 */

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- Definitions ---
#define SPEAKER_PIN 21      // GPIO pin connected to the speaker
#define DESIRED_FREQ_HZ 100 // Desired frequency of the tone in Hertz

// --- Global Variables ---
// Holds the current state of the speaker pin (HIGH or LOW).
// `volatile` is used because it's modified in an ISR and could be read
// elsewhere, preventing unsafe compiler optimizations.
static volatile bool speaker_state = false;

/**
 * @brief Timer ISR callback function, called when the timer alarm triggers.
 *
 * This function is the core of the tone generation. It flips the state of the
 * speaker pin, creating the square wave.
 *
 * @param timer The handle of the timer that triggered the alarm.
 * @param edata Event data, not used here.
 * @param user_data User data, not used here.
 * @return `true` if a high-priority task was woken up, `false` otherwise.
 *         Here, we always return `false`.
 */
static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer,
                                        const gptimer_alarm_event_data_t *edata,
                                        void *user_data) {
  // Toggle the state (true -> false, false -> true)
  speaker_state = !speaker_state;
  // Apply the new state to the GPIO pin
  gpio_set_level(SPEAKER_PIN, speaker_state);
  return false;
}

/**
 * @brief Main entry point of the application.
 */
void app_main(void) {
  // --- 1. Configure the Speaker GPIO Pin ---
  gpio_reset_pin(SPEAKER_PIN);
  gpio_set_direction(SPEAKER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(SPEAKER_PIN, 0); // Start with the speaker off

  // --- 2. Configure the General Purpose Timer (gptimer) ---
  gptimer_handle_t gptimer = NULL;
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Use the default clock source
      .direction = GPTIMER_COUNT_UP,     // Count upwards
      .resolution_hz = 1 * 1000 * 1000,  // Set resolution to 1 MHz (1 tick = 1 µs)
  };
  gptimer_new_timer(&timer_config, &gptimer);

  // --- 3. Configure the Timer Alarm ---
  // To create a square wave of frequency F, we need to toggle the pin every
  // half-period. The period is 1/F. The half-period is 1/(2*F).
  // Since our timer resolution is 1µs, the alarm count is (1,000,000 / (F *
  // 2)).
  gptimer_alarm_config_t alarm_config = {
      .alarm_count = 1000000 / (DESIRED_FREQ_HZ * 2), // Alarm value in ticks (µs)
      .reload_count = 0, // Not used when auto-reload is enabled
      .flags.auto_reload_on_alarm = true, // Automatically reload alarm on trigger
  };
  gptimer_set_alarm_action(gptimer, &alarm_config);

  // --- 4. Register the ISR Callback ---
  // This tells the timer which function to call when the alarm event occurs.
  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_on_alarm_cb,
  };
  gptimer_register_event_callbacks(gptimer, &cbs, NULL);

  // --- 5. Start the Timer ---
  gptimer_enable(gptimer);
  gptimer_start(gptimer);

  // --- 6. Keep the Main Task Alive ---
  // The main task doesn't need to do anything else; the interrupts handle the
  // work. We put it in an endless loop with a delay to prevent it from exiting.
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
