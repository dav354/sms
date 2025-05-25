
/*
idf.py add-dependency espressif/esp_lcd_ili9341
idf.py add-dependency lvgl/lvgl^8
Noah Raupold 5022097
Arduino Shield – Hello-World demo (8080, ESP32-S3)
*/

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

/* ────────────────────────────
 *  Generic panel parameters
 * ──────────────────────────── */
#define LCD_H_RES 240
#define LCD_V_RES 320
#define BUF_LINES 80 /* 2×80 lines = ½ screen – plenty of RAM left */
#define COLOR_SIZE sizeof(lv_color_t)

/* ────────────────────────────
 *  Your pin mapping (8080-8)
 * ──────────────────────────── */
#define PIN_RST 15
#define PIN_BLK 13
#define PIN_CS 7
#define PIN_DC 8 /* “RS” on the shield */
#define PIN_WR 16
#define PIN_RD 9

static const int data_pins[8] = {36, 35, 38, 39, 40, 41, 42, 37};

/* ────────────────────────────
 *  Globals
 * ──────────────────────────── */
static esp_lcd_panel_handle_t panel = NULL;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf_a, *buf_b;
static const char *TAG = "LCD_DEMO";

/* ────────────────────────────
 *  Small helpers
 * ──────────────────────────── */
static void backlight_on(void) {
  gpio_config_t io = {.pin_bit_mask = 1ULL << PIN_BLK,
                      .mode = GPIO_MODE_OUTPUT};
  gpio_config(&io);
  gpio_set_level(PIN_BLK, 1);
}

static void lv_tick_cb(void *arg) { lv_tick_inc(10); }

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *map) {
  esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, map);
  lv_disp_flush_ready(drv);
}

/* ────────────────────────────
 *  Main
 * ──────────────────────────── */
void app_main(void) {
  ESP_LOGI(TAG, "boot");

  /* 1 ─ back-light & RD-pin */
  backlight_on();
  gpio_set_direction(PIN_RD, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_RD, 1); /* keep RD inactive (high) */

  /* 2 ─ LVGL draw buffers (double-buffer avoids tearing) */
  size_t buf_bytes = LCD_H_RES * BUF_LINES * COLOR_SIZE;
  buf_a = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  buf_b = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!buf_a || !buf_b) {
    ESP_LOGE(TAG, "not enough memory for frame buffers");
    abort();
  }
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf_a, buf_b, LCD_H_RES * BUF_LINES);

  /* 3 ─ 8080 bus */
  esp_lcd_i80_bus_handle_t bus;
  esp_lcd_i80_bus_config_t bus_cfg = {
      .dc_gpio_num = PIN_DC,
      .wr_gpio_num = PIN_WR,
      .clk_src = LCD_CLK_SRC_DEFAULT,
      .data_gpio_nums = {data_pins[0], data_pins[1], data_pins[2], data_pins[3],
                         data_pins[4], data_pins[5], data_pins[6],
                         data_pins[7]},
      .bus_width = 8,
      .max_transfer_bytes = buf_bytes,
      .sram_trans_align = 4};
  ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &bus));

  /* 4 ─ panel IO */
  esp_lcd_panel_io_handle_t io;
  esp_lcd_panel_io_i80_config_t io_cfg = {.cs_gpio_num = PIN_CS,
                                          .pclk_hz = 10 * 1000 * 1000,
                                          .trans_queue_depth = 10,
                                          .dc_levels = {.dc_data_level = 1},
                                          .lcd_cmd_bits = 8,
                                          .lcd_param_bits = 8};
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));

  /* 5 ─ ILI9341 panel driver */
  esp_lcd_panel_dev_config_t panel_cfg = {.reset_gpio_num = PIN_RST,
                                          .rgb_endian = LCD_RGB_ENDIAN_RGB,
                                          .bits_per_pixel = 16};
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_cfg, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

  /* 6 ─ orientation tweak — EDIT HERE if needed */
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false)); /* mirror X  */
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));      /* no rotate */

  /* 7 ─ LVGL driver registration */
  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = LCD_H_RES;
  drv.ver_res = LCD_V_RES;
  drv.flush_cb = flush_cb;
  drv.draw_buf = &draw_buf;
  drv.full_refresh = 1; /* avoid “half-cut” artefacts */
  lv_disp_drv_register(&drv);

  /* 8 ─ LVGL tick */
  esp_timer_handle_t tick;
  const esp_timer_create_args_t tick_args = {.callback = lv_tick_cb,
                                             .name = "lv_tick"};
  esp_timer_create(&tick_args, &tick);
  esp_timer_start_periodic(tick, 10 * 1000); /* 10 ms */

  /* 9 ─ UI : three Hello-Worlds */
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

  lv_obj_t *lbl_red = lv_label_create(lv_scr_act());
  lv_obj_t *lbl_green = lv_label_create(lv_scr_act());
  lv_obj_t *lbl_blue = lv_label_create(lv_scr_act());

  lv_label_set_text(lbl_red, "Hello World");
  lv_label_set_text(lbl_green, "Hello World");
  lv_label_set_text(lbl_blue, "Hello World");

  lv_obj_set_style_text_color(lbl_red, lv_color_make(255, 0, 0), LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_green, lv_color_make(0, 255, 0),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_blue, lv_color_make(0, 0, 255), LV_PART_MAIN);

  lv_obj_align(lbl_red, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_align(lbl_green, LV_ALIGN_CENTER, 0, 0);
  lv_obj_align(lbl_blue, LV_ALIGN_BOTTOM_MID, 0, -40);

  /* 10 ─ main loop */
  ESP_LOGI(TAG, "running");
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10));
    lv_timer_handler();
  }
}