#include <stdio.h>
#include "driver/i2c.h" 

#define I2C_SCL 6
#define I2C_SDA 5
#define EEPROM_ADDR 0xA0
#define FREQ 100000
#define TIMEOUT_MS 1000

// function prototypes
void init_i2c();
void write_byte(uint8_t, uint8_t );
void read_byte(uint8_t, uint8_t *);

int app_main(void)
{
    // data to be written to the eeprom
    const char *write_data = "Hallo World. Hello World. Hello World. Hello World. Hello World.";
    const uint8_t full_mem_addr = 0x00;

    uint8_t addr = full_mem_addr;
    uint8_t read_data[100] = {0};

    init_i2c();

    // write the data
    while(*write_data != '\0'){
        write_byte(addr, *write_data);
        write_data++;
        addr++;
    }
    //write_byte(addr, *byte); // wozu?
    printf("Finished writing.\n");
    
    // read the data
    addr = full_mem_addr;
    for(int i = 0; i < 65; i++){
        read_byte(addr, &read_data[i]);
        addr++;
    }
    printf("%s\n", read_data);
    return 0; //normal termination
}

// initialise i2c
void init_i2c(void){
     // configure i2c
     i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = FREQ,
    };

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    printf("Init completed\n");
}

// write a byte to a specific address
void write_byte(uint8_t mem_addr, uint8_t data){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, EEPROM_ADDR | I2C_MASTER_WRITE, true);  //address calculation changed
    i2c_master_write_byte(cmd, mem_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    vTaskDelay(pdMS_TO_TICKS(10));
}

// reads bytes from the eeprom
void read_byte(uint8_t mem_addr, uint8_t *data){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, EEPROM_ADDR | I2C_MASTER_WRITE, true); //address calculation changed
    i2c_master_write_byte(cmd, mem_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, EEPROM_ADDR  | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
}