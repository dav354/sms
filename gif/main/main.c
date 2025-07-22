#include "esp_log.h"   // Für Logging-Funktionen (ESP_LOGI, ESP_LOGE, etc.)
#include "esp_timer.h" // Für hochauflösende Timer (wird für LVGL Ticks benötigt)
#include "freertos/FreeRTOS.h" // FreeRTOS Basis-Header
#include "freertos/task.h"     // FreeRTOS Task-Management

// ===== LVGL und Display Treiber Includes =====
#include "driver/gpio.h" // Für die GPIO (General Purpose Input/Output) Steuerung
#include "esp_heap_caps.h" // Für erweiterte Speicherallokierungsfunktionen (DMA-fähiger Speicher)
#include "esp_lcd_ili9341.h" // Spezifischer Treiber für den ILI9341 LCD-ControllerR
#include "esp_lcd_panel_io.h" // Generische LCD Panel I/O Funktionen
#include "esp_lcd_panel_ops.h" // Operationen für LCD Panels (wie initialisieren, zeichnen)
#include "esp_lcd_panel_vendor.h" // Hersteller-spezifische Panel-Konfigurationen (hier ILI9341)
#include "lvgl.h"                 // Haupt-Header für die LVGL Grafikbibliothek

// ===== SD-Karten und Dateisystem Includes =====
#include "driver/spi_master.h" // Für die SPI Master-Treiberfunktionen (SD-Karte im SPI-Modus)
#include "esp_vfs_fat.h" // Für das FAT-Dateisystem auf der SD-Karte (Virtual File System)
#include "sdmmc_cmd.h" // Befehle und Definitionen für SD/MMC Karten
#include <sys/stat.h> // Für Dateistatistiken (z.B. Dateigröße prüfen mit stat())
#include <sys/unistd.h> // Für Unix-ähnliche Systemaufrufe (hier für Dateisystemoperationen)

// Tag für Log-Ausgaben, um Nachrichten dieser Komponente im seriellen Monitor
// zu identifizieren.
static const char *TAG = "FINAL_GIF_APP";

// ===== Display und LVGL Globale Definitionen und Variablen =====
#define LCD_H_RES 240 // Horizontale Auflösung des LCDs in Pixeln
#define LCD_V_RES 320 // Vertikale Auflösung des LCDs in Pixeln
#define LVGL_TICK_PERIOD_MS                                                    \
  10 // Periode für den LVGL-Systemtick in Millisekunden
#define BUF_LINES                                                              \
  80 // Anzahl der Zeilen, die für einen der LVGL-Grafikpuffer verwendet werden.
#define COLOR_SIZE                                                             \
  sizeof(lv_color_t) // Größe eines Pixels in Bytes, abhängig von der LVGL
                     // Farbkonfiguration

// Struktur für den LVGL-Zeichenpuffer (Display Buffer). Enthält Zeiger auf die
// eigentlichen Puffer.
static lv_disp_draw_buf_t disp_buf;
// LVGL Display-Treiberstruktur.
static lv_disp_drv_t disp_drv;
// Handle (Zeiger auf eine Struktur) für das LCD-Panel, wird von den esp_lcd
// Funktionen verwendet.
static esp_lcd_panel_handle_t panel_handle = NULL;

// ===== Display Hardware Pinbelegung (ILI9341 - 8080 Parallel) - PINBELEGUNG
#define PIN_RST 15 // Reset-Pin des LCDs
#define PIN_BLK 13 // Backlight (Hintergrundbeleuchtung) Pin des LCDs
#define PIN_CS 7   // Chip Select Pin des LCDs
#define PIN_DC 8   // Data/Command (auch als "RS" Register Select) Pin des LCDs
#define PIN_WR 16  // Write Strobe Pin des LCDs
#define PIN_RD                                                                 \
  9 // Read Strobe Pin des LCDs (oft nicht aktiv genutzt bei reinen
    // Schreiboperationen)
// Array der GPIO-Pins für die 8 Datenleitungen D0-D7 des Parallelinterface
static const int data_pins[8] = {36, 35, 38, 39, 40, 41, 42, 37};

// ===== SD-Karten SPI Pinbelegung - PINBELEGUNG ÜBERPRÜFEN! =====
#define PIN_SD_SS 45  // Chip Select (Slave Select) für SD-Karte
#define PIN_SD_DI 48  // Data In (MOSI - Master Out Slave In) für SD-Karte
#define PIN_SD_DO 47  // Data Out (MISO - Master In Slave Out) für SD-Karte
#define PIN_SD_SCK 21 // Serial Clock (SCK) für SD-Karte

// ===== Pfade für SD-Karte und GIF-Datei =====
#define SD_MOUNT_POINT                                                         \
  "/sdcard" // Einhängepunkt im VFS (Virtual File System) für die SD-Karte
#define GIF_LVGL_PATH                                                          \
  "S:/anim.gif" // Pfad zur GIF-Datei, wie LVGL ihn erwartet (mit
                // Laufwerksbuchstabe "S:" für SD-Karte).
#define GIF_VFS_PATH                                                           \
  "/sdcard/anim.gif" // Vollständiger Pfad zur GIF-Datei im VFS. \
  //  (z.B. "anim.gif" nicht "animation_bild.gif")

// ===== LVGL Log-Funktion =====
#if LV_USE_LOG // Nur kompilieren, wenn LV_USE_LOG in lv_conf.h aktiviert ist
// Callback-Funktion, um LVGL Log-Nachrichten über das ESP-IDF Logging-System
// auszugeben.
static void lvgl_log_cb(const char *buf) {
  ESP_LOGI("LVGL_LOG", "%s",
           buf); // Gibt die LVGL-Nachricht mit dem Tag "LVGL_LOG" aus
}
#endif

// ===== LVGL Flush Callback =====
// Diese Funktion wird von LVGL aufgerufen, wenn ein Bereich des Bildschirms
// aktualisiert werden muss. Sie überträgt die Pixeldaten aus dem LVGL-Puffer an
// das Display.
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                          lv_color_t *color_p) {
  // Debug-Ausgabe, um zu sehen, wann und welcher Bereich geflusht wird.
  ESP_LOGI(TAG, "Flush callback called! Area: x1=%d, y1=%d, x2=%d, y2=%d",
           area->x1, area->y1, area->x2, area->y2);

  // Panel-Handle aus den Benutzerdaten des Treibers holen.
  esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
  // Bitmap-Daten (Pixeldaten) an das LCD-Panel für den angegebenen Bereich
  // senden. Beachte: x2 und y2 müssen oft um +1 erhöht werden, da die esp_lcd
  // API exklusive Endpunkte erwartet.
  esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1,
                            area->y2 + 1, color_p);
  // LVGL mitteilen, dass der Flush-Vorgang abgeschlossen ist und der Puffer
  // wiederverwendet werden kann.
  lv_disp_flush_ready(drv);
}

// ===== LVGL Tick Callback =====
// Diese Funktion wird periodisch von einem ESP-Timer aufgerufen.
// Sie informiert LVGL über die vergangene Zeit, was für Animationen und Events
// wichtig ist.
static void lvgl_tick_cb(void *arg) {
  (void)arg; // Argument wird nicht verwendet, Cast zu void um
             // Compiler-Warnungen zu vermeiden.
  lv_tick_inc(LVGL_TICK_PERIOD_MS); // LVGL mitteilen, dass LVGL_TICK_PERIOD_MS
                                    // Millisekunden vergangen sind.
}

// ===== Funktion zur Initialisierung und zum Mounten der SD-Karte =====
static esp_err_t init_sd_card(void) {
  ESP_LOGI(TAG, "Initialisiere SD-Karte...");
  esp_err_t ret; // Variable für Rückgabewerte von ESP-IDF Funktionen

  // Konfiguration für das Mounten des FAT-Dateisystems auf der SD-Karte
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed =
          false,      // SD-Karte nicht formatieren, wenn Mount fehlschlägt
      .max_files = 5, // Maximale Anzahl gleichzeitig geöffneter Dateien
      .allocation_unit_size =
          16 * 1024 // Größe der Allokationseinheit ( beeinflusst Performance
                    // und Speicherplatznutzung)
  };
  sdmmc_card_t *card; // Zeiger auf eine Struktur, die Informationen über die
                      // SD-Karte enthält

  // Konfiguration für den SPI-Bus, an dem die SD-Karte angeschlossen ist
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = PIN_SD_DI,  // GPIO-Pin für MOSI (Master Out Slave In)
      .miso_io_num = PIN_SD_DO,  // GPIO-Pin für MISO (Master In Slave Out)
      .sclk_io_num = PIN_SD_SCK, // GPIO-Pin für SCLK (Serial Clock)
      .quadwp_io_num =
          -1, // Nicht verwendet für Standard SPI SD-Karten (Quad Write Protect)
      .quadhd_io_num =
          -1, // Nicht verwendet für Standard SPI SD-Karten (Quad Hold)
      .max_transfer_sz = 4092 // Maximale Übertragungsgröße in Bytes
  };

  // SPI-Bus initialisieren (hier SPI2_HOST)
  // SDSPI_DEFAULT_DMA bedeutet, dass DMA (Direct Memory Access) für die
  // Übertragung genutzt wird, wenn möglich.
  ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI Bus Initialisierung fehlgeschlagen (%s)",
             esp_err_to_name(ret));
    return ret; // Fehler zurückgeben
  }

  // Konfiguration für das SPI-Gerät (die SD-Karte)
  sdspi_device_config_t slot_config =
      SDSPI_DEVICE_CONFIG_DEFAULT(); // Standardkonfiguration laden
  slot_config.gpio_cs = PIN_SD_SS;   // GPIO-Pin für Chip Select der SD-Karte
  slot_config.host_id = SPI2_HOST;   // ID des SPI-Hosts (hier SPI2_HOST)

  // Host-Konfiguration für SD-Karte im SPI-Modus
  sdmmc_host_t host =
      SDSPI_HOST_DEFAULT(); // Standardkonfiguration für SDSPI-Host laden
  host.slot = SPI2_HOST;    // Zuordnung zum SPI-Host (obwohl SDSPI_HOST_DEFAULT
                         // dies oft schon korrekt setzt, explizit ist besser)

  // SD-Karte mounten mit dem FAT-Dateisystemtreiber für SPI
  ret = esp_vfs_fat_sdspi_mount(
      SD_MOUNT_POINT, // Einhängepunkt im VFS
      &host,          // Host-Konfiguration
      &slot_config,   // Geräte (Slot) Konfiguration
      &mount_config,  // Mount-Konfiguration
      &card);         // Zeiger, um Karteninformationen zu speichern

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Mounten der SD-Karte fehlgeschlagen (%s)",
             esp_err_to_name(ret));
    spi_bus_free(SPI2_HOST); // SPI-Bus wieder freigeben, wenn Mount fehlschlägt
    return ret;              // Fehler zurückgeben
  }

  ESP_LOGI(TAG, "SD-Karte erfolgreich gemountet.");
  sdmmc_card_print_info(stdout,
                        card); // Gibt Detailinformationen über die Karte aus
  return ESP_OK;               // Erfolg zurückgeben
}

// ===== Hauptfunktion der Applikation =====
void app_main(void) {
  ESP_LOGI(TAG, "--- STARTE FINALES GIF DEMO ---");

  // --- 1. Initialisiere Display Hardware ---
  ESP_LOGI(TAG, "1. Initialisiere Display Hardware...");
  esp_lcd_i80_bus_handle_t i80_bus = NULL; // Handle für den parallelen I80 Bus
  // Konfiguration des I80 Busses
  esp_lcd_i80_bus_config_t bus_config = {
      .dc_gpio_num = PIN_DC,          // GPIO-Pin für Data/Command
      .wr_gpio_num = PIN_WR,          // GPIO-Pin für Write Strobe
      .clk_src = LCD_CLK_SRC_DEFAULT, // Taktquelle (Standard verwenden)
      .data_gpio_nums = {data_pins[0], data_pins[1], data_pins[2],
                         data_pins[3], // Array der Datenpins
                         data_pins[4], data_pins[5], data_pins[6],
                         data_pins[7]},
      .bus_width = 8, // Busbreite ist 8 Bit
      .max_transfer_bytes =
          LCD_H_RES * BUF_LINES *
          COLOR_SIZE, // Max. Übertragungsgröße (Größe eines LVGL-Puffers)
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_i80_bus(&bus_config, &i80_bus)); // Neuen I80 Bus erstellen

  esp_lcd_panel_io_handle_t io_handle =
      NULL; // Handle für das Panel I/O Interface
  // Konfiguration des I80 Panel I/O Interfaces
  esp_lcd_panel_io_i80_config_t io_config = {
      .cs_gpio_num = PIN_CS,       // GPIO-Pin für Chip Select
      .pclk_hz = 10 * 1000 * 1000, // Taktfrequenz für das Panel: 10 MHz
      .trans_queue_depth = 10,     // Tiefe der Transaktions-Warteschlange
      .dc_levels = {.dc_data_level = 1,
                    .dc_cmd_level = 0,
                    .dc_dummy_level =
                        0}, // Pegel für D/C Pin (Daten, Befehl, Dummy)
      .lcd_cmd_bits = 8,    // Anzahl Bits für LCD-Befehle
      .lcd_param_bits = 8,  // Anzahl Bits für LCD-Parameter
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(
      i80_bus, &io_config, &io_handle)); // Neues I/O Interface erstellen

  // Konfiguration des ILI9341 Panel-Treibers
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = PIN_RST,        // GPIO-Pin für Reset
      .rgb_endian = LCD_RGB_ENDIAN_RGB, // Farbreihenfolge (Byte Order)
      .bits_per_pixel = 16,             // Bits pro Pixel (z.B. 16 für RGB565)
  };
  // Neuen Panel-Treiber für ILI9341 erstellen und panel_handle zuweisen
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle)); // Panel resetten
  ESP_ERROR_CHECK(esp_lcd_panel_init(
      panel_handle)); // Panel initialisieren (sendet Initialisierungssequenz)
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(
      panel_handle,
      true)); // Farben invertieren (kann je nach Display nötig sein)
  ESP_ERROR_CHECK(
      esp_lcd_panel_disp_on_off(panel_handle, true)); // Display einschalten

  // Hintergrundbeleuchtung (Backlight) konfigurieren und einschalten
  gpio_config_t bk_gpio_config = {
      .mode = GPIO_MODE_OUTPUT,       // Pin als Ausgang
      .pin_bit_mask = 1ULL << PIN_BLK // Bitmaske für den BLK-Pin
  };
  ESP_ERROR_CHECK(gpio_config(&bk_gpio_config)); // GPIO-Konfiguration anwenden
  gpio_set_level(PIN_BLK, 1); // BLK-Pin auf High setzen (Annahme: High = An)
  ESP_LOGI(TAG, "Display Initialisiert.");

  // --- 2. Initialisiere SD-Karte ---
  if (init_sd_card() !=
      ESP_OK) { // Aufruf der SD-Karten Initialisierungsfunktion
    ESP_LOGE(TAG, "Initialisierung der SD-Karte fehlgeschlagen! Programm wird "
                  "angehalten.");
    while (1) { // Endlosschleife, um das Programm anzuhalten, wenn SD-Karte
                // nicht initialisiert werden kann
      vTaskDelay(pdMS_TO_TICKS(1000)); // Warte 1 Sekunde
    }
  }

  // --- 3. Initialisiere LVGL ---
  ESP_LOGI(TAG, "3. Initialisiere LVGL...");
  lv_init(); // LVGL Bibliothek initialisieren

#if LV_USE_LOG // Wenn LVGL-Logging aktiviert ist
  lv_log_register_print_cb(
      lvgl_log_cb); // Registriere die Log-Callback Funktion
#endif

  // Speicher für die beiden LVGL-Zeichenpuffer allokieren (Double Buffering)
  // MALLOC_CAP_DMA: Speicher, der für DMA (Direct Memory Access) geeignet ist.
  // MALLOC_CAP_INTERNAL: Speicher aus dem internen RAM des ESP32 (schneller).
  lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * BUF_LINES * COLOR_SIZE,
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * BUF_LINES * COLOR_SIZE,
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  assert(buf1 &&
         buf2); // Sicherstellen, dass die Puffer erfolgreich allokiert wurden

  // LVGL Display-Pufferstruktur initialisieren mit den beiden allokierten
  // Puffern
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * BUF_LINES);

  // LVGL Display-Treiber initialisieren
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;      // Horizontale Auflösung
  disp_drv.ver_res = LCD_V_RES;      // Vertikale Auflösung
  disp_drv.flush_cb = lvgl_flush_cb; // Callback-Funktion zum Übertragen der
                                     // Daten auf das Display
  disp_drv.draw_buf = &disp_buf;     // Zeiger auf die Display-Pufferstruktur
  disp_drv.user_data =
      panel_handle; // Benutzerdaten (hier das Panel-Handle für den flush_cb)
  // Den konfigurierten Treiber bei LVGL registrieren und ein Display-Objekt
  // erhalten
  lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
  assert(disp); // Sicherstellen, dass die Registrierung erfolgreich war

  // ESP-Timer für den LVGL-Tick erstellen und starten
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &lvgl_tick_cb, // Die oben definierte lvgl_tick_cb Funktion
      .name = "lvgl_tick"        // Name des Timers (für Debugging)
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args,
                                   &lvgl_tick_timer)); // Timer erstellen
  // Timer starten, sodass er periodisch alle LVGL_TICK_PERIOD_MS Millisekunden
  // auslöst
  ESP_ERROR_CHECK(esp_timer_start_periodic(
      lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000)); // Periode in Mikrosekunden

  // LVGL Dateisystem-Interface für stdio (Standard Input/Output)
  // initialisieren. Dies ermöglicht LVGL, Dateien über Standard C-Funktionen
  // (fopen, fread etc.) zu lesen, was hier für das Laden der GIF-Datei von der
  // SD-Karte genutzt wird (via VFS).
  lv_fs_stdio_init();
  ESP_LOGI(TAG, "LVGL Initialisiert.");

  // --- 4. Erstelle LVGL UI (Benutzeroberfläche) ---
  ESP_LOGI(TAG, "4. Erstelle UI...");
  // Hintergrundfarbe des aktiven Bildschirms (Screen) auf Schwarz setzen
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);

  struct stat st; // Struktur für Dateiinformationen
  // Überprüfen, ob die GIF-Datei auf der SD-Karte existiert
  if (stat(GIF_VFS_PATH, &st) != 0) { // stat() gibt 0 bei Erfolg zurück
    ESP_LOGE(TAG, "!!! GIF-Datei nicht gefunden unter %s. Überprüfe SD-Karte.",
             GIF_VFS_PATH);
    // Fehlermeldung auf dem Display anzeigen, wenn GIF nicht gefunden wurde
    lv_obj_t *err_label =
        lv_label_create(lv_scr_act()); // Label-Objekt erstellen
    lv_label_set_text(err_label,
                      "FEHLER:\nanim.gif\nnicht gefunden!"); // Text setzen
    lv_obj_set_style_text_color(err_label, lv_color_white(),
                                LV_PART_MAIN); // Textfarbe weiß
    lv_obj_set_style_bg_color(err_label, lv_color_hex(0xFF0000),
                              LV_PART_MAIN); // Hintergrund rot
    lv_obj_set_style_bg_opa(err_label, LV_OPA_COVER,
                            LV_PART_MAIN); // Hintergrund opak
    lv_obj_set_style_text_align(err_label, LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);             // Text zentrieren
    lv_obj_set_style_pad_all(err_label, 10, LV_PART_MAIN); // Innenabstand
    lv_obj_set_width(err_label, lv_pct(80)); // Breite auf 80% des Bildschirms
    lv_obj_align(err_label, LV_ALIGN_CENTER, 0, 0); // In der Mitte ausrichten
  } else {
    // GIF-Datei wurde gefunden
    ESP_LOGI(TAG, "ERFOLG! %s gefunden. Größe: %ld Bytes.", GIF_VFS_PATH,
             (long)st.st_size); // Dateigröße loggen
    ESP_LOGI(TAG, "Erstelle GIF-Objekt...");
    lv_obj_t *gif_obj =
        lv_gif_create(lv_scr_act()); // LVGL GIF-Objekt erstellen
    if (gif_obj) {
      // Quelle des GIF-Objekts auf den Pfad der Datei setzen (LVGL erwartet
      // "S:/..." für SD-Karte)
      lv_gif_set_src(gif_obj, GIF_LVGL_PATH);
      lv_obj_align(gif_obj, LV_ALIGN_CENTER, 0,
                   0); // GIF in der Mitte des Bildschirms ausrichten
      ESP_LOGI(TAG, "GIF-Objekt erstellt und Quelle gesetzt.");
    } else {
      ESP_LOGE(TAG, "Fehler beim Erstellen des LVGL GIF-Objekts!");
    }
  }

  ESP_LOGI(TAG, "--- Hauptschleife startet ---");
  // Endlosschleife für die Hauptverarbeitung
  while (1) {
    // Kurze Pause von 10 Millisekunden, um anderen Tasks (z.B. Systemtasks)
    // Rechenzeit zu geben.
    vTaskDelay(pdMS_TO_TICKS(10));
    // LVGL Timer-Handler aufrufen. Diese Funktion ist essentiell für LVGL,
    // da sie Animationen, Events und das Neuzeichnen von Objekten managed.
    lv_timer_handler();
  }
}