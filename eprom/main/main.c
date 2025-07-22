/**
 * @file main.c
 * @brief Example for writing to and reading from an external I2C EEPROM.
 *
 * This application demonstrates how to communicate with an I2C EEPROM memory
 * chip (like an AT24C series). It initializes the I2C bus, writes a string
 * byte-by-byte into the EEPROM, and then reads it back to verify.
 */

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h" // Required for vTaskDelay
#include "freertos/task.h"     // Required for vTaskDelay
#include <stdio.h>
#include <string.h>

// --- I2C and EEPROM Definitions ---
#define I2C_SCL 6                // GPIO pin for I2C Clock (SCL)
#define I2C_SDA 5                // GPIO pin for I2C Data (SDA)
#define EEPROM_ADDR 0x50         // I2C address of the EEPROM chip. Note: 0xA0 is often the 8-bit address, 0x50 is the 7-bit address. ESP-IDF uses the 7-bit address.
#define I2C_PORT I2C_NUM_0       // I2C port to use
#define I2C_FREQ 100000          // I2C clock frequency (100kHz)
#define I2C_TIMEOUT_MS 1000      // Timeout for I2C operations

// --- Function Prototypes ---
// Declaring functions before they are used, especially before app_main.
void init_i2c(void);
void write_byte(uint8_t mem_addr, uint8_t data);
void read_byte(uint8_t mem_addr, uint8_t *data);

/**
 * @brief Main entry point of the application.
 */
int app_main(void) {
  // The string to be written to the EEPROM
  const char *write_data =
      "Hallo World. Hello World. Hello World. Hello World. Hello World.";
  const uint8_t start_mem_addr = 0x00; // Start writing at address 0x00

  uint8_t current_addr = start_mem_addr;
  uint8_t read_buffer[100] = {0}; // Buffer to store the data read back

  // Initialize the I2C bus
  init_i2c();

  // --- Write the data to the EEPROM byte by byte ---
  printf("Writing data to EEPROM...\n");
  const char *ptr = write_data;
  while (*ptr != '\0') {
    write_byte(current_addr, *ptr);
    ptr++;
    current_addr++;
  }
  printf("Finished writing.\n");

  // --- Read the data back from the EEPROM ---
  printf("Reading data from EEPROM...\n");
  current_addr = start_mem_addr;
  // We read 65 bytes as an example
  for (int i = 0; i < 65; i++) {
    read_byte(current_addr, &read_buffer[i]);
    current_addr++;
  }
  printf("Data read: %s\n", read_buffer);

  return 0; // Normal termination (note: app_main should not return)
}

/**
 * @brief Initializes the I2C master driver.
 */
void init_i2c(void) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_SDA,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = I2C_SCL,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_FREQ,
  };
  i2c_param_config(I2C_PORT, &conf);
  i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
  printf("I2C driver initialized.\n");
}

/**
 * @brief Writes a single byte to a specific memory address in the EEPROM.
 *
 * @param mem_addr The internal memory address of the EEPROM to write to.
 * @param data The byte of data to write.
 */
void write_byte(uint8_t mem_addr, uint8_t data) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  // 1. Send the EEPROM's I2C address with the write bit.
  i2c_master_write_byte(cmd, (EEPROM_ADDR << 1) | I2C_MASTER_WRITE, true);
  // 2. Send the internal memory address to write to.
  i2c_master_write_byte(cmd, mem_addr, true);
  // 3. Send the data byte.
  i2c_master_write_byte(cmd, data, true);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
  i2c_cmd_link_delete(cmd);

  // IMPORTANT: EEPROMs need time to complete the internal write cycle.
  // A small delay is crucial after each write operation. 5-10ms is typical.
  vTaskDelay(pdMS_TO_TICKS(10));
}

/**
 * @brief Reads a single byte from a specific memory address in the EEPROM.
 *
 * @param mem_addr The internal memory address of the EEPROM to read from.
 * @param data Pointer to a byte where the read data will be stored.
 */
void read_byte(uint8_t mem_addr, uint8_t *data) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  // --- "Dummy Write" to set the memory address pointer ---
  i2c_master_start(cmd);
  // 1. Send EEPROM address with write bit.
  i2c_master_write_byte(cmd, (EEPROM_ADDR << 1) | I2C_MASTER_WRITE, true);
  // 2. Send the internal memory address we want to read from.
  i2c_master_write_byte(cmd, mem_addr, true);

  // --- Actual Read operation ---
  i2c_master_start(cmd); // Repeated start condition
  // 3. Send EEPROM address again, but this time with the read bit.
  i2c_master_write_byte(cmd, (EEPROM_ADDR << 1) | I2C_MASTER_READ, true);
  // 4. Read one byte of data from the EEPROM.
  //    NACK (Not Acknowledge) is sent because we are only reading one byte.
  i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
  i2c_cmd_link_delete(cmd);
}
