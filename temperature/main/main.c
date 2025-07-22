/**
 * @file main.c
 * @brief Example for reading a DS18B20 temperature sensor using the 1-Wire
 * protocol.
 *
 * This application initializes a 1-Wire bus, searches for a DS18B20 sensor,
 * and then periodically reads and prints the temperature. It uses a 3rd-party
 * component for the 1-Wire bus communication.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire_bus.h"
#include "onewire_device.h"
#include <stdio.h>
#include <string.h>

// --- Definitions ---
#define ONEWIRE_GPIO 18 // GPIO pin connected to the 1-Wire data line

// --- Global Handles ---
onewire_bus_handle_t bus = NULL; // Handle for the 1-Wire bus
// Stores the unique 64-bit address of the found DS18B20 sensor
onewire_device_address_t device_address = 0;

/**
 * @brief Main entry point of the application.
 */
void app_main(void) {
  // --- 1. Initialize the 1-Wire Bus ---
  onewire_bus_config_t bus_config = {.bus_gpio_num = ONEWIRE_GPIO};
  // RMT (Remote Control) is a peripheral used here to create the precise
  // timing required for the 1-Wire protocol.
  onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 10};
  onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);

  // --- 2. Search for a DS18B20 Device on the Bus ---
  onewire_device_iter_handle_t iter;
  onewire_device_t dev;
  // Create an iterator to search for all devices on the bus
  onewire_new_device_iter(bus, &iter);
  // Loop through all found devices
  while (onewire_device_iter_get_next(iter, &dev) == ESP_OK) {
    // The family code for a DS18B20 sensor is 0x28.
    // For other sensors like DS18S20 it is 0x10.
    // We check the first byte of the 64-bit address.
    if ((dev.address & 0xFF) == 0x28) { // Check for DS18B20 family code
      device_address = dev.address;
      char addr_str[17];
      sprintf(addr_str, "%016llX", device_address);
      printf("Found DS18B20 sensor with address: %s\n", addr_str);
      break; // Stop searching once we've found one
    }
  }
  // Clean up the iterator
  onewire_del_device_iter(iter);

  if (device_address == 0) {
    printf("No DS18B20 sensor found.\n");
    return; // Stop if no sensor is present
  }

  // --- 3. Main Measurement Loop ---
  while (1) {
    // --- Step A: Start Temperature Conversion ---
    // This command is sent to all devices on the bus.
    uint8_t start_conversion_cmd[] = {0xCC, 0x44}; // 0xCC: SKIP ROM, 0x44: CONVERT T
    onewire_bus_reset(bus); // Reset the bus before every command
    onewire_bus_write_bytes(bus, start_conversion_cmd, 2);

    // The conversion takes time (up to 750ms for 12-bit resolution).
    // We wait here to ensure the measurement is complete.
    vTaskDelay(pdMS_TO_TICKS(750));

    // --- Step B: Read the Scratchpad (Memory) of our specific sensor ---
    // First, we must select our device using its unique address.
    uint8_t select_device_cmd[9] = {0x55}; // 0x55: MATCH ROM command
    memcpy(&select_device_cmd[1], &device_address, 8); // Copy the 8-byte address

    uint8_t read_scratchpad_cmd = 0xBE; // 0xBE: READ SCRATCHPAD command
    uint8_t scratchpad_data[9] = {0};   // Buffer to store the 9 bytes of memory

    onewire_bus_reset(bus);
    onewire_bus_write_bytes(bus, select_device_cmd, 9); // Select our device
    onewire_bus_write_bytes(bus, &read_scratchpad_cmd, 1); // Send read command
    onewire_bus_read_bytes(bus, scratchpad_data, 9); // Read the 9-byte scratchpad

    // --- Step C: Calculate Temperature ---
    // The temperature is in the first two bytes of the scratchpad (LSB, MSB).
    // We combine them into a 16-bit signed integer.
    int16_t raw_temp = (int16_t)((scratchpad_data[1] << 8) | scratchpad_data[0]);
    // According to the DS18B20 datasheet, we divide the raw value by 16.0
    // to get the temperature in degrees Celsius.
    float temp_c = raw_temp / 16.0;
    printf("Temperature: %.2f°C\n", temp_c);

    // Wait for 2 seconds before the next measurement
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
