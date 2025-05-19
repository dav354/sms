#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mpu6050.h"
#include <stdio.h>

#define I2C_MASTER_SCL_IO 20
#define I2C_MASTER_SDA_IO 21

static mpu6050_t dev;

void app_main(void) {
  i2c_dev_t i2c_dev = {0};
  ESP_ERROR_CHECK(i2cdev_init());

  ESP_ERROR_CHECK(mpu6050_init_desc(&dev, 0x68, I2C_NUM_0, I2C_MASTER_SDA_IO,
                                    I2C_MASTER_SCL_IO));
  ESP_ERROR_CHECK(mpu6050_init(&dev));

  ESP_ERROR_CHECK(mpu6050_set_acce_fs(&dev, MPU6050_ACCE_FS_2G));
  ESP_ERROR_CHECK(mpu6050_set_gyro_fs(&dev, MPU6050_GYRO_FS_250DPS));

  mpu6050_acce_value_t acc;
  mpu6050_gyro_value_t gyro;

  while (1) {
    ESP_ERROR_CHECK(mpu6050_get_acce(&dev, &acc));
    ESP_ERROR_CHECK(mpu6050_get_gyro(&dev, &gyro));

    printf("Accel (g): X=%.2f Y=%.2f Z=%.2f | Gyro (deg/s): X=%.2f Y=%.2f "
           "Z=%.2f\n",
           acc.x, acc.y, acc.z, gyro.x, gyro.y, gyro.z);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
