#include "driver/gpio.h" // Für die GPIO (General Purpose Input/Output) Steuerung
#include "esp_heap_caps.h" // Für erweiterte Speicherallokierungsfunktionen (DMA-fähiger Speicher)
#include "esp_lcd_ili9341.h" // Spezifischer Treiber für den ILI9341 LCD-Controller
#include "esp_lcd_panel_io.h" // Generische LCD Panel I/O Funktionen
#include "esp_lcd_panel_ops.h" // Operationen für LCD Panels (wie initialisieren, zeichnen)
#include "esp_lcd_panel_vendor.h" // Hersteller-spezifische Panel-Konfigurationen (hier ILI9341)
#include "esp_log.h"   // Für Logging-Funktionen (ESP_LOGI, ESP_LOGE, etc.)
#include "esp_timer.h" // Für hochauflösende Timer (wird für LVGL Ticks benötigt)
#include "freertos/FreeRTOS.h" // FreeRTOS Basis-Header
#include "freertos/task.h"     // FreeRTOS Task-Management
#include "lvgl.h"              // Haupt-Header für die LVGL Grafikbibliothek

/* 
 * Allgemeine Panel-Parameter
 */
#define LCD_H_RES 240 // Horizontale Auflösung des LCDs in Pixeln
#define LCD_V_RES 320 // Vertikale Auflösung des LCDs in Pixeln
#define BUF_LINES                                                              \
  80 // Anzahl der Zeilen, die für einen der LVGL-Grafikpuffer verwendet werden.
     // Bei Double Buffering (2 Puffer) sind das 2 * 80 Zeilen.
     // Das entspricht hier der Hälfte des Bildschirms (320/2 = 160, also 2*80
     // Zeilen passen gut). Lässt genügend RAM für andere Dinge übrig.
#define COLOR_SIZE                                                             \
  sizeof(lv_color_t) // Größe eines Pixels in Bytes, abhängig von der LVGL
                     // Farbkonfiguration (z.B. 2 Bytes für RGB565)

/* 
 * Deine Pinbelegung (8080-8 Bit Parallelinterface)
 */
#define PIN_RST 15 // Reset-Pin des LCDs
#define PIN_BLK 13 // Backlight (Hintergrundbeleuchtung) Pin des LCDs
#define PIN_CS 7   // Chip Select Pin des LCDs
#define PIN_DC                                                                 \
  8 // Data/Command (auch als "RS" Register Select auf dem Shield bezeichnet)
    // Pin des LCDs
#define PIN_WR                                                                 \
  16 // Write Strobe Pin des LCDs (Daten werden bei steigender/fallender Flanke
     // geschrieben)
#define PIN_RD                                                                 \
  9 // Read Strobe Pin des LCDs (wird hier nicht aktiv genutzt, aber
    // initialisiert)

// Array der GPIO-Pins für die 8 Datenleitungen D0-D7 des Parallelinterface
static const int data_pins[8] = {36, 35, 38, 39, 40, 41, 42, 37};

/* 
 * Globale Variablen
 */
// Handle (Zeiger auf eine Struktur) für das LCD-Panel, wird von den esp_lcd
// Funktionen verwendet.
static esp_lcd_panel_handle_t panel = NULL;
// Struktur für den LVGL-Zeichenpuffer (Display Buffer). Enthält Zeiger auf die
// eigentlichen Puffer.
static lv_disp_draw_buf_t draw_buf;
// Zeiger auf die beiden Framebuffer (für Double Buffering, um Tearing-Effekte
// zu vermeiden).
static lv_color_t *buf_a, *buf_b;
// Tag für Log-Ausgaben, um Nachrichten dieser Komponente im seriellen Monitor
// zu identifizieren.
static const char *TAG = "LCD_DEMO";

/* 
 * Kleine Hilfsfunktionen
 */

// Schaltet die Hintergrundbeleuchtung des LCDs ein.
static void backlight_on(void) {
  // GPIO-Konfigurationsstruktur
  gpio_config_t io = {
      .pin_bit_mask = 1ULL << PIN_BLK, // Bitmaske für den BLK-Pin
      .mode = GPIO_MODE_OUTPUT         // Pin als Ausgang konfigurieren
  };
  gpio_config(&io);           // GPIO-Konfiguration anwenden
  gpio_set_level(PIN_BLK, 1); // BLK-Pin auf High setzen, um die Beleuchtung
                              // einzuschalten (Annahme: High = An)
}

// Callback-Funktion für den LVGL-Tick-Timer.
// Diese Funktion wird periodisch aufgerufen, um LVGL über die vergangene Zeit
// zu informieren.
static void lv_tick_cb(void *arg) {
  (void)arg;       // Argument wird nicht verwendet, Cast zu void um
                   // Compiler-Warnungen zu vermeiden.
  lv_tick_inc(10); // LVGL mitteilen, dass 10 Millisekunden vergangen sind
                   // (entsprechend der Timer-Periode).
}

// Callback-Funktion, die von LVGL aufgerufen wird, um Daten auf das Display zu
// schreiben ("flushen"). drv: Zeiger auf den LVGL Display-Treiber. a:   Der
// Bereich (Area) des Displays, der aktualisiert werden soll. map: Zeiger auf
// den Puffer mit den Farbdaten (Pixeln), die gezeichnet werden sollen.
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *map) {
  // Sendet die Bitmap-Daten (Pixeldaten) an das LCD-Panel für den angegebenen
  // Bereich.
  esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, map);
  // LVGL mitteilen, dass der Flush-Vorgang abgeschlossen ist und der Puffer
  // wiederverwendet werden kann.
  lv_disp_flush_ready(drv);
}

/* 
 * Hauptfunktion (Entry Point der Applikation)
 */
void app_main(void) {
  ESP_LOGI(TAG, "Starte Applikation (boot)"); // Log-Nachricht beim Start

  /* 1 ─ Hintergrundbeleuchtung & RD-Pin initialisieren */
  backlight_on(); // Hintergrundbeleuchtung einschalten.
  gpio_set_direction(
      PIN_RD, GPIO_MODE_OUTPUT); // RD-Pin (Read) als Ausgang konfigurieren.
  gpio_set_level(PIN_RD, 1);     // RD-Pin auf High setzen (inaktiv), da wir nur
                                 // schreiben und nicht vom Display lesen.

  /* 2 ─ LVGL Zeichenpuffer (Double-Buffering vermeidet Tearing-Artefakte) */
  // Größe eines einzelnen Framebuffers in Bytes berechnen.
  size_t buf_bytes = LCD_H_RES * BUF_LINES * COLOR_SIZE;
  // Speicher für den ersten Framebuffer allokieren.
  // MALLOC_CAP_INTERNAL: Speicher aus dem internen RAM des ESP32.
  // MALLOC_CAP_DMA: Speicher, der für DMA (Direct Memory Access) geeignet ist,
  // was die Datenübertragung beschleunigt.
  buf_a = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  // Speicher für den zweiten Framebuffer allokieren.
  buf_b = heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  // Überprüfen, ob die Speicherallokierung erfolgreich war.
  if (!buf_a || !buf_b) {
    ESP_LOGE(TAG, "Nicht genügend Speicher für Framebuffer!");
    abort(); // Programm abbrechen, wenn kein Speicher verfügbar ist.
  }
  lv_init(); // LVGL Bibliothek initialisieren.
  // Die LVGL Display-Pufferstruktur initialisieren mit den beiden allokierten
  // Puffern. Der letzte Parameter ist die Größe eines Puffers in Pixeln (nicht
  // Bytes).
  lv_disp_draw_buf_init(&draw_buf, buf_a, buf_b, LCD_H_RES * BUF_LINES);

  /* 3 ─ Konfiguration des parallelen 8080 Busses */
  esp_lcd_i80_bus_handle_t bus; // Handle für den I80 Bus.
  // Konfigurationsstruktur für den I80 Bus.
  esp_lcd_i80_bus_config_t bus_cfg = {
      .dc_gpio_num = PIN_DC,          // GPIO-Pin für Data/Command (D/C).
      .wr_gpio_num = PIN_WR,          // GPIO-Pin für Write Strobe (WR).
      .clk_src = LCD_CLK_SRC_DEFAULT, // Taktquelle für den LCD-Controller
                                      // (Standardwert verwenden).
      .data_gpio_nums =
          {// Array der GPIO-Pins für die Datenleitungen D0-D7.
           data_pins[0], data_pins[1], data_pins[2], data_pins[3], data_pins[4],
           data_pins[5], data_pins[6], data_pins[7]},
      .bus_width = 8, // Busbreite ist 8 Bit.
      .max_transfer_bytes =
          buf_bytes, // Maximale Anzahl an Bytes, die in einer Transaktion
                     // übertragen werden können (Größe eines Framebuffers).
      .sram_trans_align = 4 // Speicherausrichtung für Transaktionen (optional,
                            // für Performance-Optimierung).
  };
  // Neuen I80 Bus erstellen und initialisieren mit der gegebenen Konfiguration.
  ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &bus));

  /* 4 ─ Konfiguration des Panel I/O Interfaces */
  esp_lcd_panel_io_handle_t io; // Handle für das Panel I/O Interface.
  // Konfigurationsstruktur für das I80 Panel I/O Interface.
  esp_lcd_panel_io_i80_config_t io_cfg = {
      .cs_gpio_num = PIN_CS,       // GPIO-Pin für Chip Select (CS).
      .pclk_hz = 10 * 1000 * 1000, // Taktfrequenz für das Panel: 10 MHz.
      .trans_queue_depth = 10,     // Tiefe der Transaktions-Warteschlange.
      .dc_levels =
          {
              // Konfiguration der Pegel für den D/C Pin.
              .dc_data_level = 1 // D/C Pin ist High für Daten.
          },
      .lcd_cmd_bits = 8, // Anzahl der Bits für LCD-Befehle (typischerweise 8).
      .lcd_param_bits =
          8 // Anzahl der Bits für LCD-Parameter (typischerweise 8).
  };
  // Neues Panel I/O Interface für den I80 Bus erstellen und initialisieren.
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));

  /* 5 ─ Initialisierung des ILI9341 Panel-Treibers */
  // Konfigurationsstruktur für den ILI9341 Panel-Treiber.
  esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = PIN_RST,        // GPIO-Pin für den Reset des Panels.
      .rgb_endian = LCD_RGB_ENDIAN_RGB, // Farbreihenfolge (Byte Order) für
                                        // RGB-Daten (hier RGB).
      .bits_per_pixel = 16 // Anzahl der Bits pro Pixel (z.B. 16 für RGB565).
  };
  // Neuen Panel-Treiber für den ILI9341 erstellen, unter Verwendung des
  // konfigurierten I/O Interfaces.
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_cfg, &panel));
  ESP_ERROR_CHECK(
      esp_lcd_panel_reset(panel)); // Hardware-Reset des LCD-Panels durchführen.
  ESP_ERROR_CHECK(
      esp_lcd_panel_init(panel)); // Panel initialisieren (sendet
                                  // Initialisierungssequenz an ILI9341).
  ESP_ERROR_CHECK(
      esp_lcd_panel_disp_on_off(panel, true)); // Display einschalten.

  /* 6 ─ Anpassung der Display-Orientierung – HIER EDITIEREN falls nötig */
  // Spiegeln der X-Achse (horizontale Spiegelung).
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
  // Vertauschen der X- und Y-Achsen (Rotation). 'false' bedeutet hier keine
  // Rotation.
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));

  /* 7 ─ Registrierung des LVGL-Display-Treibers */
  static lv_disp_drv_t drv; // Statische LVGL Display-Treiberstruktur.
  lv_disp_drv_init(
      &drv); // Initialisiert die Treiberstruktur mit Standardwerten.
  drv.hor_res = LCD_H_RES; // Horizontale Auflösung setzen.
  drv.ver_res = LCD_V_RES; // Vertikale Auflösung setzen.
  drv.flush_cb =
      flush_cb; // Die oben definierte flush_cb Funktion als Callback zuweisen.
  drv.draw_buf = &draw_buf; // Zeiger auf die LVGL-Zeichenpufferstruktur setzen.
  drv.full_refresh =
      1; // Wichtig für manche Setups: Erzwingt ein vollständiges Neuzeichnen
         // des Puffers, um "halb zerschnittene" Artefakte bei partiellem
         // Refresh zu vermeiden, besonders wenn Puffer kleiner als der
         // Bildschirm sind.
  lv_disp_drv_register(
      &drv); // Den konfigurierten Treiber bei LVGL registrieren.

  /* 8 ─ LVGL Tick-Timer konfigurieren und starten */
  esp_timer_handle_t tick; // Handle für den ESP-Timer.
  // Konfigurationsstruktur für den Timer.
  const esp_timer_create_args_t tick_args = {
      .callback =
          lv_tick_cb,   // Die oben definierte lv_tick_cb Funktion als Callback.
      .name = "lv_tick" // Name des Timers (für Debugging).
  };
  esp_timer_create(&tick_args, &tick); // Den Timer erstellen.
  // Den Timer starten, sodass er periodisch alle 10 Millisekunden (10 * 1000
  // Mikrosekunden) auslöst.
  esp_timer_start_periodic(tick, 10 * 1000);

  /* 9 ─ UI : Erstellung von drei "Hello-World" Labels */
  // Hintergrund des aktiven Bildschirms (Screen) konfigurieren:
  // Deckkraft auf voll (opak) setzen.
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
  // Hintergrundfarbe auf Schwarz setzen.
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

  // Drei LVGL Label-Objekte auf dem aktiven Bildschirm erstellen.
  lv_obj_t *lbl_red = lv_label_create(lv_scr_act());
  lv_obj_t *lbl_green = lv_label_create(lv_scr_act());
  lv_obj_t *lbl_blue = lv_label_create(lv_scr_act());

  // Text für die Labels setzen.
  lv_label_set_text(lbl_red, "Hello World");
  lv_label_set_text(lbl_green, "Hello World");
  lv_label_set_text(lbl_blue, "Hello World");

  // Textfarbe für die Labels setzen.
  lv_obj_set_style_text_color(lbl_red, lv_color_make(255, 0, 0),
                              LV_PART_MAIN); // Rot
  lv_obj_set_style_text_color(lbl_green, lv_color_make(0, 255, 0),
                              LV_PART_MAIN); // Grün
  lv_obj_set_style_text_color(lbl_blue, lv_color_make(0, 0, 255),
                              LV_PART_MAIN); // Blau

  // Labels auf dem Bildschirm ausrichten.
  lv_obj_align(lbl_red, LV_ALIGN_TOP_MID, 0,
               40); // Oben mittig, mit 40 Pixeln Abstand vom oberen Rand.
  lv_obj_align(lbl_green, LV_ALIGN_CENTER, 0,
               0); // Exakt in der Mitte des Bildschirms.
  lv_obj_align(lbl_blue, LV_ALIGN_BOTTOM_MID, 0,
               -40); // Unten mittig, mit 40 Pixeln Abstand vom unteren Rand
                     // (nach oben verschoben).

  /* 10 ─ Hauptschleife der Applikation */
  ESP_LOGI(TAG,
           "Applikation läuft (running)"); // Log-Nachricht, dass die
                                           // Initialisierung abgeschlossen ist.
  while (true) {
    // Kurze Pause von 10 Millisekunden, um anderen Tasks (z.B. Systemtasks)
    // Rechenzeit zu geben.
    vTaskDelay(pdMS_TO_TICKS(10));
    // LVGL Timer-Handler aufrufen. Diese Funktion ist essentiell für LVGL,
    // da sie Animationen, Events und das Neuzeichnen von Objekten managed.
    lv_timer_handler();
  }
}