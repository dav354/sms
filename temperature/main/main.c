#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire_bus.h"
#include "onewire_device.h"

#define ONEWIRE_GPIO 18

onewire_bus_handle_t bus = NULL;
onewire_device_address_t device_address = 0;

void app_main(void) {
    // Init bus
    onewire_bus_config_t cfg = {.bus_gpio_num = ONEWIRE_GPIO};
    onewire_bus_rmt_config_t rmt = {.max_rx_bytes = 10};
    onewire_new_bus_rmt(&cfg, &rmt, &bus);

    // Search device
    onewire_device_iter_handle_t iter;
    onewire_device_t dev;
    onewire_new_device_iter(bus, &iter);
    while (onewire_device_iter_get_next(iter, &dev) == ESP_OK) {
        if ((dev.address & 0xFF) == 0x10) {
            device_address = dev.address;
            break;
        }
    }
    onewire_del_device_iter(iter);

    while (1) {
        // Start conversion (broadcast with SKIP ROM)
        uint8_t cmd[] = {0xCC, 0x44}; // SKIP ROM + CONVERT
        onewire_bus_reset(bus);
        onewire_bus_write_bytes(bus, cmd, 2);
        vTaskDelay(pdMS_TO_TICKS(750));

        // Read scratchpad
        uint8_t match[9] = {0x55}; // MATCH ROM
        memcpy(&match[1], &device_address, 8);
        uint8_t read_cmd = 0xBE;
        uint8_t data[9] = {0};

        onewire_bus_reset(bus);
        onewire_bus_write_bytes(bus, match, 9);
        onewire_bus_write_bytes(bus, &read_cmd, 1);
        onewire_bus_read_bytes(bus, data, 9);

        int16_t raw = (data[1] << 8) | data[0];
        float temp = raw / 2.0;
        printf("%.2f°C\n", temp);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
