#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
  const char *TAG = "BEEP";
  ESP_LOGI(TAG, "Starting beep configuration");

  // Configure LEDC timer for PWM (2 kHz beep tone)
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_13_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 2000, // 2 kHz frequency for the beep
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_LOGI(TAG, "Configuring LEDC timer");
  ledc_timer_config(&ledc_timer);
  ESP_LOGI(TAG, "LEDC timer configured");

  // Configure LEDC channel (using GPIO18 for the buzzer)
  ledc_channel_config_t ledc_channel = {
      .gpio_num = 18,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_0,
      .timer_sel = LEDC_TIMER_0,
      .duty = 0,
      .hpoint = 0,
      .intr_type = LEDC_INTR_DISABLE,
  };
  ESP_LOGI(TAG, "Configuring LEDC channel");
  ledc_channel_config(&ledc_channel);
  ESP_LOGI(TAG, "LEDC channel configured");

  // Loop forever to beep repeatedly
  while (1) {
    // Start the beep: set duty cycle to approximately 50%
    ESP_LOGI(TAG, "Starting beep");
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 4096);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

    // Beep on for 500 ms
    vTaskDelay(pdMS_TO_TICKS(500));

    // Stop the beep
    ESP_LOGI(TAG, "Stopping beep");
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

    // Pause for 500 ms before the next beep
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
