#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LVGL and Display Driver Includes
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "lvgl.h"

// SD Card and Filesystem Includes
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <sys/stat.h>
#include <sys/unistd.h>

static const char *TAG = "FINAL_GIF_APP";

// Display and LVGL Globals
#define LCD_H_RES 240
#define LCD_V_RES 320
#define LVGL_TICK_PERIOD_MS 10
#define BUF_LINES 80
#define COLOR_SIZE sizeof(lv_color_t)

static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static esp_lcd_panel_handle_t panel_handle = NULL;

// Display Hardware Pinout (ILI9341 - 8080 Parallel) - VERIFY YOUR PINS
#define PIN_RST 15
#define PIN_BLK 13
#define PIN_CS 7
#define PIN_DC 8
#define PIN_WR 16
#define PIN_RD 9
static const int data_pins[8] = {36, 35, 38, 39, 40, 41, 42, 37};

// SD Card SPI Pinout - VERIFY YOUR PINS
#define PIN_SD_SS 45
#define PIN_SD_DI 48
#define PIN_SD_DO 47
#define PIN_SD_SCK 21

#define SD_MOUNT_POINT "/sdcard"
#define GIF_LVGL_PATH "S:/anim.gif"     // CHANGED to 8.3 filename
#define GIF_VFS_PATH "/sdcard/anim.gif" // CHANGED to 8.3 filename

// LVGL Log Function
#if LV_USE_LOG
static void lvgl_log_cb(const char *buf) { ESP_LOGI("LVGL_LOG", "%s", buf); }
#endif

// LVGL Flush Callback
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_p) {
  ESP_LOGI(TAG, "Flush callback called! Area: x1=%d, y1=%d, x2=%d, y2=%d",
           area->x1, area->y1, area->x2, area->y2);

  esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
  esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1,
                            area->y2 + 1, color_p);
  lv_disp_flush_ready(drv);
}

// LVGL Tick Callback
static void lvgl_tick_cb(void *arg) {
  (void)arg;
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// Function to initialize and mount SD card
static esp_err_t init_sd_card(void) {
  ESP_LOGI(TAG, "Initializing SD card...");
  esp_err_t ret;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};
  sdmmc_card_t *card;
  spi_bus_config_t bus_cfg = {.mosi_io_num = PIN_SD_DI,
                              .miso_io_num = PIN_SD_DO,
                              .sclk_io_num = PIN_SD_SCK,
                              .quadwp_io_num = -1,
                              .quadhd_io_num = -1,
                              .max_transfer_sz = 4092};
  ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI bus init failed (%s)", esp_err_to_name(ret));
    return ret;
  }
  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = PIN_SD_SS;
  slot_config.host_id = SPI2_HOST;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI2_HOST;
  ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config,
                                &mount_config, &card);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SD card mount failed (%s)", esp_err_to_name(ret));
    spi_bus_free(SPI2_HOST);
    return ret;
  }
  ESP_LOGI(TAG, "SD card mounted successfully.");
  return ESP_OK;
}

void app_main(void) {
  ESP_LOGI(TAG, "--- STARTING FINAL GIF DEMO ---");

  // --- 1. Initialize Display Hardware ---
  ESP_LOGI(TAG, "1. Initializing Display Hardware...");
  esp_lcd_i80_bus_handle_t i80_bus = NULL;
  esp_lcd_i80_bus_config_t bus_config = {
      .dc_gpio_num = PIN_DC,
      .wr_gpio_num = PIN_WR,
      .clk_src = LCD_CLK_SRC_DEFAULT,
      .data_gpio_nums = {data_pins[0], data_pins[1], data_pins[2], data_pins[3],
                         data_pins[4], data_pins[5], data_pins[6],
                         data_pins[7]},
      .bus_width = 8,
      .max_transfer_bytes = LCD_H_RES * BUF_LINES * COLOR_SIZE,
  };
  ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_i80_config_t io_config = {
      .cs_gpio_num = PIN_CS,
      .pclk_hz = 10 * 1000 * 1000,
      .trans_queue_depth = 10,
      .dc_levels = {.dc_data_level = 1, .dc_cmd_level = 0, .dc_dummy_level = 0},
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = PIN_RST,
      .rgb_endian = LCD_RGB_ENDIAN_RGB,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
  gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                  .pin_bit_mask = 1ULL << PIN_BLK};
  ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
  gpio_set_level(PIN_BLK, 1);
  ESP_LOGI(TAG, "Display Initialized.");

  // --- 2. Initialize SD Card ---
  if (init_sd_card() != ESP_OK) {
    ESP_LOGE(TAG, "SD card init failed! Halting.");
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // --- 3. Initialize LVGL ---
  ESP_LOGI(TAG, "3. Initializing LVGL...");
  lv_init();
#if LV_USE_LOG
  lv_log_register_print_cb(lvgl_log_cb);
#endif
  lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * BUF_LINES * COLOR_SIZE,
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * BUF_LINES * COLOR_SIZE,
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  assert(buf1 && buf2);
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * BUF_LINES);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
  lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
  assert(disp);
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &lvgl_tick_cb, .name = "lvgl_tick"};
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
  lv_fs_stdio_init();
  ESP_LOGI(TAG, "LVGL Initialized.");

  // --- 4. Create LVGL UI ---
  ESP_LOGI(TAG, "4. Creating UI...");
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000),
                            LV_PART_MAIN); // Black background

  struct stat st;
  if (stat(GIF_VFS_PATH, &st) != 0) {
    ESP_LOGE(TAG, "!!! GIF file not found at %s. Check SD card.", GIF_VFS_PATH);
    lv_obj_t *err_label = lv_label_create(lv_scr_act());
    lv_label_set_text(err_label, "ERROR:\nanim.gif\nnot found!");
    lv_obj_set_style_text_color(err_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(err_label, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(err_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(err_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(err_label, 10, LV_PART_MAIN);
    lv_obj_set_width(err_label, lv_pct(80));
    lv_obj_align(err_label, LV_ALIGN_CENTER, 0, 0);
  } else {
    ESP_LOGI(TAG, "SUCCESS! Found %s. Size: %ld bytes.", GIF_VFS_PATH,
             (long)st.st_size);
    ESP_LOGI(TAG, "Creating GIF object...");
    lv_obj_t *gif_obj = lv_gif_create(lv_scr_act());
    if (gif_obj) {
      lv_gif_set_src(gif_obj, GIF_LVGL_PATH);
      lv_obj_align(gif_obj, LV_ALIGN_CENTER, 0, 0);
      ESP_LOGI(TAG, "GIF object created and source set.");
    } else {
      ESP_LOGE(TAG, "Failed to create LVGL GIF object!");
    }
  }

  ESP_LOGI(TAG, "--- Main loop starting ---");
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
    lv_timer_handler();
  }
}