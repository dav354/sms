#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

// === Pin Definitions ===
#define TRIGGER_GPIO 1
#define ECHO_GPIO 2

// === Constants ===
#define SPEED_OF_SOUND_CM_PER_US 0.0343f // Speed of sound in cm/µs

// === Logging Tag ===
static const char *TAG = "ULTRASONIC";

// === Global Variables for Interrupt Handling ===
// volatile is used as these variables are accessed by an ISR
static volatile int64_t echo_start_time = 0;
static volatile int64_t echo_end_time = 0;
static volatile bool pulse_detected = false;

/**
 * @brief GPIO interrupt handler to capture echo pulse timing.
 *
 * This ISR is triggered on both the rising and falling edge of the echo pin.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg) {
  if (gpio_get_level(ECHO_GPIO)) {
    // Rising edge: record the start time
    echo_start_time = esp_timer_get_time();
  } else {
    // Falling edge: record the end time and set the flag
    echo_end_time = esp_timer_get_time();
    pulse_detected = true;
  }
}

/**
 * @brief Configure and initialize GPIO pins for the sensor.
 */
static void ultrasonic_gpio_init(void) {
  // --- Configure Trigger Pin ---
  gpio_config_t trig_io_conf = {
      .pin_bit_mask = (1ULL << TRIGGER_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&trig_io_conf);

  // --- Configure Echo Pin ---
  gpio_config_t echo_io_conf = {
      .pin_bit_mask = (1ULL << ECHO_GPIO),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE, // Trigger on both rising and falling edge
  };
  gpio_config(&echo_io_conf);

  // Install GPIO ISR service
  gpio_install_isr_service(0);
  // Hook ISR handler for our specific GPIO pin
  gpio_isr_handler_add(ECHO_GPIO, gpio_isr_handler, NULL);

  ESP_LOGI(TAG, "GPIOs configured.");
}

/**
 * @brief Sends a 10 microsecond trigger pulse to the sensor.
 */
static void send_trigger_pulse(void) {
  gpio_set_level(TRIGGER_GPIO, 0);
  esp_rom_delay_us(2);
  gpio_set_level(TRIGGER_GPIO, 1);
  esp_rom_delay_us(10);
  gpio_set_level(TRIGGER_GPIO, 0);
}

/**
 * @brief Main task for measuring and printing the distance.
 */
void ultrasonic_test_task(void *pvParameters) {
  ultrasonic_gpio_init();

  while (1) {
    // Reset the pulse detection flag
    pulse_detected = false;

    // Send the trigger pulse to start a measurement
    send_trigger_pulse();

    // Wait for the pulse to be detected by the ISR.
    // We add a timeout to prevent the task from blocking forever if no echo is
    // received. A 100ms timeout is more than enough for the sensor's max range
    // (e.g., 400cm is ~24ms).
    vTaskDelay(pdMS_TO_TICKS(100));

    if (pulse_detected) {
      // Calculate the duration of the pulse in microseconds
      int64_t pulse_duration_us = echo_end_time - echo_start_time;

      // Calculate the distance in centimeters
      // distance = (duration / 2) * speed_of_sound
      float distance_cm = (pulse_duration_us / 2.0f) * SPEED_OF_SOUND_CM_PER_US;

      // Check for valid range
      if (distance_cm > 2 && distance_cm < 400) {
        ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);
      } else {
        ESP_LOGI(TAG, "Out of range (%.2f cm)", distance_cm);
      }
    } else {
      // This happens if the ISR didn't detect a full pulse within the timeout
      ESP_LOGW(TAG, "No echo received (timeout).");
    }

    // Wait for 1 second before the next measurement
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) {
  // Create the task that will handle the sensor readings
  xTaskCreate(ultrasonic_test_task, "ultrasonic_test_task", 4096, NULL, 5,
              NULL);
}