#include "driver/i2c.h"
#include <stdio.h>
#include <time.h>

// MPU6050 I2C Adresse
#define MPU6050_ADDR 0x68

// Register-Adressen
#define ACCEL_XOUT_H 0x3B
#define ACCEL_YOUT_H 0x3D
#define ACCEL_ZOUT_H 0x3F
#define GYRO_XOUT_H 0x43
#define GYRO_YOUT_H 0x45
#define GYRO_ZOUT_H 0x47
#define PWR_MGMT_1 0x6B

// I2C Konfiguration
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM 0
#define I2C_MASTER_FREQ_HZ 400000

void wait_ms(int delay_ms) {
  clock_t start_time = clock();
  while ((clock() - start_time) * 1000 / CLOCKS_PER_SEC < delay_ms) {
    // Aktiv warten, bis die Zeit abgelaufen ist
  }
}

// I2C Initialisierung
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

// MPU6050 Register schreiben
void mpu6050_write_reg(uint8_t reg_addr, uint8_t data) {
  uint8_t write_buf[2] = {reg_addr, data};
  i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR, write_buf,
                             sizeof(write_buf), 1000);
}

// MPU6050 Register lesen
void mpu6050_read_reg(uint8_t reg_addr, uint8_t *data, size_t len) {
  i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, &reg_addr, 1, data,
                               len, 1000);
}

// 16-bit Wert aus zwei 8-bit Registern lesen
int16_t read_16bit_value(uint8_t reg_addr) {
  uint8_t data[2];
  mpu6050_read_reg(reg_addr, data, 2);
  return data[0] << 8 | data[1];
}

void app_main(void) {
  i2c_master_init();
  printf("I2C initialisiert\n");
  mpu6050_write_reg(PWR_MGMT_1, 0x00);
  printf("MPU6050 initialisiert\n");

  while (1) {
    // Beschleunigungsdaten lesen
    int16_t accel_x = read_16bit_value(ACCEL_XOUT_H);
    int16_t accel_y = read_16bit_value(ACCEL_YOUT_H);
    int16_t accel_z = read_16bit_value(ACCEL_ZOUT_H);

    // Gyroskop-Daten lesen
    int16_t gyro_x = read_16bit_value(GYRO_XOUT_H);
    int16_t gyro_y = read_16bit_value(GYRO_YOUT_H);
    int16_t gyro_z = read_16bit_value(GYRO_ZOUT_H);

    // Daten ausgeben
    printf("Beschleunigung: X=%d, Y=%d, Z=%d\n", accel_x, accel_y, accel_z);
    printf("Gyroskop: X=%d, Y=%d, Z=%d\n", gyro_x, gyro_y, gyro_z);

    wait_ms(500);
  }
}
