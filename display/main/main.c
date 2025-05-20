#include "driver/gpio.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define LCD_PIN_NUM_CS 11
#define LCD_PIN_NUM_DC 14 // RS
#define LCD_PIN_NUM_RST 10
#define LCD_PIN_NUM_WR 12
#define LCD_PIN_NUM_D0 18
#define LCD_PIN_NUM_D1 8
#define LCD_PIN_NUM_D2 5
#define LCD_PIN_NUM_D3 6
#define LCD_PIN_NUM_D4 7
#define LCD_PIN_NUM_D5 15
#define LCD_PIN_NUM_D6 16
#define LCD_PIN_NUM_D7 17

#define LCD_H_RES 240
#define LCD_V_RES 320

void app_main(void) {
  esp_lcd_i80_bus_handle_t i80_bus = NULL;
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_handle_t panel_handle = NULL;

  // Configure i80 bus
  esp_lcd_i80_bus_config_t bus_config = {
      .dc_gpio_num = LCD_PIN_NUM_DC,
      .wr_gpio_num = LCD_PIN_NUM_WR,
      .clk_src = LCD_CLK_SRC_DEFAULT,
      .data_gpio_nums = {LCD_PIN_NUM_D0, LCD_PIN_NUM_D1, LCD_PIN_NUM_D2,
                         LCD_PIN_NUM_D3, LCD_PIN_NUM_D4, LCD_PIN_NUM_D5,
                         LCD_PIN_NUM_D6, LCD_PIN_NUM_D7},
      .bus_width = 8,
      .max_transfer_bytes =
          LCD_H_RES * LCD_V_RES * sizeof(uint16_t), // 153600 bytes

  };
  ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

  // Configure IO
  esp_lcd_panel_io_i80_config_t io_config = {
      .cs_gpio_num = LCD_PIN_NUM_CS,
      .pclk_hz = 10 * 1000 * 1000, // 10 MHz is safe; try higher if stable
      .trans_queue_depth = 10,
      .on_color_trans_done = NULL,
      .user_ctx = NULL,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

  // Configure ILI9341 panel
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = LCD_PIN_NUM_RST,
      .color_space = ESP_LCD_COLOR_SPACE_RGB,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

  // Reset and init panel
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  // Fill the screen with a color
  uint16_t *color_buf = heap_caps_malloc(
      LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
  for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
    color_buf[i] = 0xF800; // Red
  }
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES,
                                            LCD_V_RES, color_buf));
  free(color_buf);

  // Draw text, etc. (for more advanced graphics use LVGL with esp_lcd)

  while (1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
