#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"

#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// 50 HZ Signal, 20ms Periode
// 0,7-1,2ms -> move to left (1000 ticks)
// 2,0-2,3ms -> move to right (2000 ticks)
// 1,5 ms -> move to middle (1500 ticks)

// Pin for the signal
#define SIGNALPIN 4

// UUIDs for services (try out the  other addresses)
static const ble_uuid128_t UART_SERVICE_UUID =
    BLE_UUID128_INIT(0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9,
                     0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);

static const ble_uuid128_t UART_CHAR_UUID_RX =
    BLE_UUID128_INIT(0x6e, 0x40, 0x00, 0x02, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9,
                     0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);

static const ble_uuid128_t UART_CHAR_UUID_TX =
    BLE_UUID128_INIT(0x6e, 0x40, 0x00, 0x03, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9,
                     0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);

// defines the length of the signal -> direction
int signal_length_ticks = 1500;

// handle for rx and tx
static uint16_t rx_handle;
static uint16_t tx_handle;

// idea: timer_20ms runs for 20ms, turns on signal and starts the timer_signal,
// which turns off the signal after the specified time -> creates the signal
gptimer_handle_t timer_20ms;
gptimer_handle_t timer_signal;

uint8_t ble_addr_type;

// struct that defines the services of our gatt server
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &UART_SERVICE_UUID.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {.uuid = &UART_CHAR_UUID_RX.u,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              .access_cb = ble_get_data,
              .val_handle = &rx_handle},
             {.uuid = &UART_CHAR_UUID_TX.u,
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &tx_handle,
              .access_cb = device_read},

             {0}}},
    {0}};

// Interrupt function, turns signal off
static bool IRAM_ATTR timer_alarm_off(gptimer_handle_t timer,
                                      const gptimer_alarm_event_data_t *edata,
                                      void *user_data) {
  gpio_set_level(SIGNALPIN, false);
  return true;
}

// Interrupt function, turns signal on and starts the timer_signal
static bool IRAM_ATTR timer_alarm_on(gptimer_handle_t timer,
                                     const gptimer_alarm_event_data_t *edata,
                                     void *user_data) {
  gpio_set_level(SIGNALPIN, true);
  resetSignalAlarm(signal_length_ticks);

  return true;
}

// starts timer_20ms and sets global singal_length to moveServo
static void moveServo(int signal_length) {
  signal_length_ticks = signal_length;
  gptimer_start(timer_20ms);
  vTaskDelay(pdMS_TO_TICKS(400));
  gptimer_stop(timer_20ms);
}

// function that creates and initalizes the timers
static void setupTimers() {

  // timer configuration, runs at 1MHZ
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,

  };

  gptimer_new_timer(&timer_config, &timer_20ms);
  gptimer_new_timer(&timer_config, &timer_signal);

  // set the event callback for 20ms timer
  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_alarm_on,
  };
  gptimer_register_event_callbacks(timer_20ms, &cbs, NULL);

  // set the event callback for the signal timer
  gptimer_event_callbacks_t cbs2 = {
      .on_alarm = timer_alarm_off,
  };
  gptimer_register_event_callbacks(timer_signal, &cbs2, NULL);

  gptimer_enable(timer_20ms);
  gptimer_enable(timer_signal);

  // set up alarm to trigger interrupt at 0.02ms
  gptimer_alarm_config_t alarm_config = {
      .alarm_count = 20000,
      .reload_count = 0,
      .flags.auto_reload_on_alarm = true,
  };

  gptimer_set_alarm_action(timer_20ms, &alarm_config);
}

// resets the signal Alarm
static void resetSignalAlarm(int alarm_count) {

  gptimer_stop(timer_signal);
  gptimer_set_raw_count(timer_signal, 0);

  gptimer_alarm_config_t alarm_config_signal = {
      .alarm_count = alarm_count,
      .reload_count = 0,
      .flags.auto_reload_on_alarm = false,
  };
  gptimer_set_alarm_action(timer_signal, &alarm_config_signal);
  gptimer_start(timer_signal);
}

// ble gatt server functions

// triggered by gap events
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {

  // device attempts to connect
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status != 0) {
      // if connection failed, start advertising
      ble_app_advertise();
    }
    break;

    // advertising complete
  case BLE_GAP_EVENT_ADV_COMPLETE:
    // start advertising again
    ble_app_advertise();
    break;
  default:
    break;
  }
  return 0;
}

// ble service function, receives data from the client
static int ble_get_data(uint16_t con_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  char value = ctxt->om->om_data[0];

  // moving from 0 to 90 degree in 1 is 10°, 2 is 20° and so on
  if (value >= '0' && value <= '9') {
    int angle = (value - '0') * 10;
    int pwm_value = 1000 + ((angle * 1000) / 90);
    moveServo(pwm_value);
  }
  return 0;
}

// ble characteristic function,
// a read function is needed for the connection to the app, but its not used in
// practice we'll leave it empty
static int device_read(uint16_t con_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {

  return 0;
}

// give ble devices information about device and services
static void ble_app_advertise(void) {
  // define information about this device
  struct ble_hs_adv_fields fields;
  const char *device_name;
  memset(&fields, 0, sizeof(fields));
  device_name = ble_svc_gap_device_name();
  fields.name = (uint8_t *)device_name;
  fields.name_len = strlen(device_name);
  fields.name_is_complete = 1;
  ble_gap_adv_set_fields(&fields);

  // define connection and discovery types
  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  // start advertising
  ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                    ble_gap_event, NULL);
}

// automatically infer address type of BLE device (ESP32)
static void ble_app_on_sync(void) {
  ble_hs_id_infer_auto(0, &ble_addr_type);
  ble_app_advertise();
}

// continously run the nimble task that handles GATT and GAP
// the nimble task is blocking, so it runs in its own thread
static void host_task(void *param) { nimble_port_run(); }

void app_main(void) {

  // set up GPIO Pin
  gpio_set_direction(SIGNALPIN, GPIO_MODE_OUTPUT);

  // setup timers for the servo function
  setupTimers();

  // setup nimble gatt server
  nvs_flash_init();
  nimble_port_init();
  ble_svc_gap_device_name_set("ESP32 Servo Controls");
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_gatts_count_cfg(gatt_svcs);
  ble_gatts_add_svcs(gatt_svcs);
  ble_hs_cfg.sync_cb = ble_app_on_sync;
  nimble_port_freertos_init(host_task);
}