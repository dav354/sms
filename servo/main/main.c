#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"

// ESP-IDF-Komponenten für den NimBLE Host Stack (Bluetooth Low Energy)
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// Hardware-Schnittstelle: GPIO 4 wird als Signalausgang für den Servo verwendet.
#define SIGNAL_PIN 4

// UUIDs definieren die eindeutigen Adressen für den BLE-Service und seine Charakteristiken.
// Dies ist die Schnittstelle, die eine Client-App (z.B. auf einem Handy) verwendet, um den Servo zu finden und zu steuern.
static const ble_uuid128_t UART_SERVICE_UUID =
    BLE_UUID128_INIT(0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9,
                     0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
// Diese Charakteristik ist zum EMPFANGEN von Daten (aus Sicht des ESP32). Eine App SCHREIBT auf diese Charakteristik.
static const ble_uuid128_t UART_CHAR_UUID_RX =
    BLE_UUID128_INIT(0x6e, 0x40, 0x00, 0x02, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9,
                     0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);
// Diese Charakteristik ist zum SENDEN von Daten (ungenutzt in diesem Beispiel).
static const ble_uuid128_t UART_CHAR_UUID_TX =
    BLE_UUID128_INIT(0x6e, 0x40, 0x00, 0x03, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9,
                     0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e);

// Globale Variable für die aktuelle Pulsbreite des Servosignals in Mikrosekunden.
int signal_length_ticks = 1500;

// Handles, die vom NimBLE-Stack intern zur Verwaltung der Charakteristiken verwendet werden.
static uint16_t rx_handle;
static uint16_t tx_handle;

// ESP-IDF gptimer Handles. Zwei Timer werden verwendet, um ein PWM-Signal zu erzeugen.
gptimer_handle_t timer_20ms;   // Erzeugt die 20ms (50Hz) Periode.
gptimer_handle_t timer_signal; // Erzeugt die variable Pulsbreite (z.B. 1-2ms).

uint8_t ble_addr_type; // Speichert den Adresstyp des BLE-Geräts.

// Forward-Deklarationen für die Callback-Funktionen, die in der Service-Definition verwendet werden.
static int ble_get_data(uint16_t con_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);
static int device_read(uint16_t con_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_app_advertise(void);

// Definition der GATT-Services und Charakteristiken. Dies ist die "API" des BLE-Servers.
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &UART_SERVICE_UUID.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             // RX-Charakteristik: Ein Client kann hier Daten schreiben.
             {.uuid = &UART_CHAR_UUID_RX.u,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              // Wichtige Schnittstelle: Diese Funktion wird aufgerufen, wenn ein Client Daten schreibt.
              .access_cb = ble_get_data,
              .val_handle = &rx_handle},
             // TX-Charakteristik: Wir könnten hier Daten an einen Client senden (ungenutzt).
             {.uuid = &UART_CHAR_UUID_TX.u,
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &tx_handle,
              .access_cb = device_read},
             {0}}},
    {0}};

// ISR für den Signal-Timer. Wird aufgerufen, um den GPIO-Pin auszuschalten.
static bool IRAM_ATTR timer_alarm_off(gptimer_handle_t timer,
                                      const gptimer_alarm_event_data_t *edata,
                                      void *user_data) {
  gpio_set_level(SIGNAL_PIN, false);
  return true;
}

// ISR für den 20ms-Perioden-Timer. Wird alle 20ms aufgerufen, um den GPIO-Pin einzuschalten.
static bool IRAM_ATTR timer_alarm_on(gptimer_handle_t timer,
                                     const gptimer_alarm_event_data_t *edata,
                                     void *user_data) {
  gpio_set_level(SIGNAL_PIN, true);
  // Startet den zweiten Timer, der das Signal nach `signal_length_ticks` Mikrosekunden wieder ausschaltet.
  resetSignalAlarm(signal_length_ticks);
  return true;
}

// Bewegt den Servo zu einer bestimmten Position.
static void moveServo(int signal_length) {
  signal_length_ticks = signal_length;
  // Startet den 20ms-Timer, der das PWM-Signal erzeugt.
  gptimer_start(timer_20ms);
  // Blockiert für 400ms, um dem Servo Zeit zu geben, die Position zu erreichen.
  vTaskDelay(pdMS_TO_TICKS(400));
  // Stoppt das Signal, damit der Servo nicht unter Last brummt.
  gptimer_stop(timer_20ms);
}

// Konfiguriert den Signal-Timer neu mit der gewünschten Pulsbreite.
static void resetSignalAlarm(int alarm_count) {
  gptimer_stop(timer_signal);
  gptimer_set_raw_count(timer_signal, 0);

  // Konfiguriert den Alarm für den Signal-Timer. Dieser ist "one-shot" (auto_reload = false),
  // da er bei jedem 20ms-Zyklus neu gestartet wird.
  gptimer_alarm_config_t alarm_config_signal = {
      .alarm_count = alarm_count,
      .reload_count = 0,
      .flags.auto_reload_on_alarm = false,
  };
  gptimer_set_alarm_action(timer_signal, &alarm_config_signal);
  gptimer_start(timer_signal);
}

// Initialisiert die beiden Timer mit der gptimer-API von ESP-IDF.
static void setupTimers() {
  // Konfiguration für eine Timer-Auflösung von 1MHz (1 Tick = 1 Mikrosekunde).
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,
  };
  gptimer_new_timer(&timer_config, &timer_20ms);
  gptimer_new_timer(&timer_config, &timer_signal);

  // Registriert die ISR `timer_alarm_on` als Callback für den 20ms-Timer.
  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_alarm_on,
  };
  gptimer_register_event_callbacks(timer_20ms, &cbs, NULL);

  // Registriert die ISR `timer_alarm_off` als Callback für den Signal-Timer.
  gptimer_event_callbacks_t cbs2 = {
      .on_alarm = timer_alarm_off,
  };
  gptimer_register_event_callbacks(timer_signal, &cbs2, NULL);

  gptimer_enable(timer_20ms);
  gptimer_enable(timer_signal);

  // Konfiguriert den Alarm für den 20ms-Timer. Er wird automatisch neu geladen,
  // um ein kontinuierliches 50Hz-Signal zu erzeugen.
  gptimer_alarm_config_t alarm_config = {
      .alarm_count = 20000, // 20.000 µs = 20ms
      .reload_count = 0,
      .flags.auto_reload_on_alarm = true,
  };
  gptimer_set_alarm_action(timer_20ms, &alarm_config);
}

// Callback für GAP-Events (z.B. Verbindungsaufbau).
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status != 0) {
      // Wenn die Verbindung fehlschlägt, starte das Advertising erneut.
      ble_app_advertise();
    }
    break;
  case BLE_GAP_EVENT_ADV_COMPLETE:
    // Wenn das Advertising endet, starte es erneut, um sichtbar zu bleiben.
    ble_app_advertise();
    break;
  default:
    break;
  }
  return 0;
}

// Callback, der aufgerufen wird, wenn ein Client auf die RX-Charakteristik schreibt.
// Dies ist die Kernlogik der BLE-Schnittstelle.
static int ble_get_data(uint16_t con_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // Nimmt das erste Byte der gesendeten Daten.
  char value = ctxt->om->om_data[0];

  // Wandelt das Zeichen '0'-'9' in einen Winkel und dann in einen PWM-Wert um.
  if (value >= '0' && value <= '9') {
    int angle = (value - '0') * 10;
    int pwm_value = 1000 + ((angle * 1000) / 90); // Lineare Umrechnung
    moveServo(pwm_value);
  }
  return 0;
}

// Dummy-Callback für Lesezugriffe. Wird von manchen Apps benötigt, um die Verbindung herzustellen.
static int device_read(uint16_t con_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return 0;
}

// Startet das BLE-Advertising.
static void ble_app_advertise(void) {
  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));
  // Setzt den Gerätenamen, der in BLE-Scans sichtbar sein wird.
  fields.name = (uint8_t *)ble_svc_gap_device_name();
  fields.name_len = strlen(ble_svc_gap_device_name());
  fields.name_is_complete = 1;
  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  // Konfiguriert das Gerät als verbindungsfähig und allgemein sichtbar.
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  // Startet das Advertising. `ble_gap_event` wird als Callback für Events registriert.
  ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                    ble_gap_event, NULL);
}

// Callback, der aufgerufen wird, wenn der NimBLE-Stack bereit ist.
static void ble_app_on_sync(void) {
  // Lässt den Stack automatisch den besten Adresstyp für den ESP32 bestimmen.
  ble_hs_id_infer_auto(0, &ble_addr_type);
  // Startet das Advertising, sobald der Stack bereit ist.
  ble_app_advertise();
}

// Der Haupt-Task für den NimBLE-Stack. `nimble_port_run` ist eine blockierende Funktion.
static void host_task(void *param) {
  nimble_port_run();
}

void app_main(void) {
  // Konfiguriert den GPIO-Pin für den Servo als Ausgang.
  gpio_set_direction(SIGNAL_PIN, GPIO_MODE_OUTPUT);

  // Initialisiert die gptimer für die PWM-Erzeugung.
  setupTimers();

  // Initialisiert den NVS (Non-Volatile Storage), wird von BLE zum Speichern von Schlüsseln benötigt.
  nvs_flash_init();
  // Initialisiert den NimBLE-Port für ESP-IDF.
  nimble_port_init();

  // Konfiguriert die grundlegenden BLE-Dienste.
  ble_svc_gap_device_name_set("ESP32 Servo Controls");
  ble_svc_gap_init();
  ble_svc_gatt_init();
  // Registriert unsere oben definierte GATT-Service-Struktur beim Stack.
  ble_gatts_count_cfg(gatt_svcs);
  ble_gatts_add_svcs(gatt_svcs);

  // Setzt den Callback, der aufgerufen wird, wenn der Stack synchronisiert ist.
  ble_hs_cfg.sync_cb = ble_app_on_sync;

  // Startet den NimBLE-Stack in einem eigenen FreeRTOS-Task.
  nimble_port_freertos_init(host_task);
}
