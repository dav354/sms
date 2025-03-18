#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

// Define some note frequencies (in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

static const char *TAG = "MUSIC";

// Helper function to set the current frequency by reconfiguring the LEDC timer
void set_frequency(int freq_hz)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = freq_hz,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);
}

// Plays one tone at a given frequency for a specified duration (milliseconds)
void play_tone(int freq, int duration_ms)
{
    ESP_LOGI(TAG, "Playing frequency %d Hz for %d ms", freq, duration_ms);

    // Set the PWM frequency
    set_frequency(freq);

    // Set ~50% duty (for a 13-bit resolution, half of 8192 is 4096)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4096);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    // Wait for the tone to finish
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    // Turn the tone off
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    // Small delay between notes
    vTaskDelay(pdMS_TO_TICKS(100));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Configuring LEDC channel for buzzer on GPIO18");

    // Configure a default channel (GPIO18)
    ledc_channel_config_t ledc_channel = {
        .gpio_num   = 18,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ledc_channel_config(&ledc_channel);

    // Example melody: C, D, E, F, G, A, B, C (all 500 ms)
    int melody[] = {
        NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
        NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5
    };
    int durations_ms[] = {
        500, 500, 500, 500,
        500, 500, 500, 500
    };
    int length = sizeof(melody) / sizeof(melody[0]);

    // Loop forever playing the melody repeatedly
    while (true) {
        for (int i = 0; i < length; i++) {
            play_tone(melody[i], durations_ms[i]);
        }

        // Optional pause between repetitions
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
