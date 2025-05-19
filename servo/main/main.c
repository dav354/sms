#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define SERVO_GPIO 18               // Choose a PWM-capable GPIO
#define SERVO_MIN_PULSEWIDTH_US 500 // Minimum pulse width for the servo (500us)
#define SERVO_MAX_PULSEWIDTH_US                                                \
  2500                       // Maximum pulse width for the servo (2500us)
#define SERVO_MAX_DEGREE 180 // Maximum angle for the servo

// Helper function to convert degree to pulse width
static uint32_t servo_angle_to_duty_us(uint32_t angle) {
  uint32_t duty_us =
      SERVO_MIN_PULSEWIDTH_US +
      ((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * angle) /
          SERVO_MAX_DEGREE;
  return duty_us;
}

void app_main(void) {
  // 1. Configure the LEDC PWM timer
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .timer_num = LEDC_TIMER_0,
      .duty_resolution = LEDC_TIMER_14_BIT, // High resolution for fine control
      .freq_hz = 50, // Standard servo PWM frequency is 50Hz (20ms period)
      .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&ledc_timer);

  // 2. Configure the LEDC channel
  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                        .channel = LEDC_CHANNEL_0,
                                        .timer_sel = LEDC_TIMER_0,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = SERVO_GPIO,
                                        .duty = 0,
                                        .hpoint = 0};
  ledc_channel_config(&ledc_channel);

  while (1) {
    // Move to 180 degrees
    uint32_t duty_us_180 = servo_angle_to_duty_us(180);
    uint32_t period_us = 1000000 / 50; // 50Hz -> 20000us period
    uint32_t max_duty = (1 << 14) - 1; // Use 14-bit as in your timer config
    uint32_t duty_180 = (duty_us_180 * max_duty) / period_us;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_180);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a second

    // Move to 0 degrees
    uint32_t duty_us_0 = servo_angle_to_duty_us(0);
    uint32_t duty_0 = (duty_us_0 * max_duty) / period_us;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait a second
  }
}
