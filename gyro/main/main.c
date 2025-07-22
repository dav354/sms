/**
 * @file main.c
 * @brief Example for reading data from an MPU6050 Gyro/Accelerometer sensor.
 *
 * This file contains an implementation to initialize the MPU6050 sensor via
 * I2C, wake it up, and then continuously read and print the raw values from
 * its accelerometer and gyroscope.
 */

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h" // Required for vTaskDelay
#include "freertos/task.h"     // Required for vTaskDelay
#include <stdio.h>

// --- MPU6050 Definitions ---
#define MPU6050_ADDR 0x68 // I2C address of the MPU6050 sensor

// MPU6050 Register Addresses
#define ACCEL_XOUT_H 0x3B // Accelerometer X-axis High Byte
#define ACCEL_YOUT_H 0x3D // Accelerometer Y-axis High Byte
#define ACCEL_ZOUT_H 0x3F // Accelerometer Z-axis High Byte
#define GYRO_XOUT_H 0x43  // Gyroscope X-axis High Byte
#define GYRO_YOUT_H 0x45  // Gyroscope Y-axis High Byte
#define GYRO_ZOUT_H 0x47  // Gyroscope Z-axis High Byte
#define PWR_MGMT_1 0x6B   // Power Management 1 Register

// --- I2C Configuration ---
#define I2C_MASTER_SCL_IO 9        // GPIO pin for I2C Clock (SCL)
#define I2C_MASTER_SDA_IO 8        // GPIO pin for I2C Data (SDA)
#define I2C_MASTER_NUM I2C_NUM_0   // I2C port number
#define I2C_MASTER_FREQ_HZ 400000  // I2C master clock frequency (400kHz)

/**
 * @brief Initializes the I2C master driver.
 */
void i2c_master_init(void) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  i2c_param_config(I2C_MASTER_NUM, &conf);
  i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * @brief Writes a single byte to a specific register of the MPU6050.
 *
 * @param reg_addr The address of the register to write to.
 * @param data The byte of data to write.
 */
void mpu6050_write_reg(uint8_t reg_addr, uint8_t data) {
  uint8_t write_buf[2] = {reg_addr, data};
  // The i2c_master_write_to_device function sends the register address
  // followed by the data byte in a single I2C transaction.
  i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR, write_buf,
                             sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

/**
 * @brief Reads a sequence of bytes from a specific register of the MPU6050.
 *
 * @param reg_addr The address of the starting register to read from.
 * @param data Pointer to the buffer where the read data will be stored.
 * @param len Number of bytes to read.
 */
void mpu6050_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
  // The i2c_master_write_read_device function first writes the register address
  // to point the MPU6050 to the correct location, and then reads `len` bytes.
  i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, &reg_addr, 1, data,
                               len, 1000 / portTICK_PERIOD_MS);
}

/**
 * @brief Reads a 16-bit value from the MPU6050.
 *
 * The sensor stores 16-bit values as two separate 8-bit registers (High and Low
 * byte). This function reads both and combines them.
 *
 * @param reg_addr The address of the High Byte register.
 * @return The combined 16-bit signed value.
 */
int16_t read_16bit_value(uint8_t reg_addr) {
  uint8_t data[2];
  // Read the High Byte and the Low Byte
  mpu6050_read_reg(reg_addr, data, 2);
  // Combine them into a single 16-bit value.
  // (data[0] << 8) shifts the high byte to the left, | data[1] adds the low
  // byte.
  return (int16_t)((data[0] << 8) | data[1]);
}

/**
 * @brief Main entry point of the application.
 */
void app_main(void) {
  // Initialize the I2C communication bus
  i2c_master_init();
  printf("I2C initialisiert\n");

  // Wake up the MPU6050 by writing 0x00 to the Power Management 1 register.
  mpu6050_write_reg(PWR_MGMT_1, 0x00);
  printf("MPU6050 initialisiert\n");

  // Main loop to continuously read and display sensor data
  while (1) {
    // Read accelerometer data for all three axes
    int16_t accel_x = read_16bit_value(ACCEL_XOUT_H);
    int16_t accel_y = read_16bit_value(ACCEL_YOUT_H);
    int16_t accel_z = read_16bit_value(ACCEL_ZOUT_H);

    // Read gyroscope data for all three axes
    int16_t gyro_x = read_16bit_value(GYRO_XOUT_H);
    int16_t gyro_y = read_16bit_value(GYRO_YOUT_H);
    int16_t gyro_z = read_16bit_value(GYRO_ZOUT_H);

    // Print the raw sensor values to the console
    printf("Beschleunigung: X=%d, Y=%d, Z=%d\n", accel_x, accel_y, accel_z);
    printf("Gyroskop: X=%d, Y=%d, Z=%d\n", gyro_x, gyro_y, gyro_z);

    // Wait for 500 milliseconds before the next reading.
    // vTaskDelay is the preferred method in FreeRTOS as it allows other tasks
    // to run, saving CPU time and power.
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

