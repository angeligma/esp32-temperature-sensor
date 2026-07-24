#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

#define SHTC3_I2C_ADDR 0x70  // from your i2cdetect

class SHTC3 {
public:
    SHTC3(i2c_master_bus_handle_t bus);

    esp_err_t init();
    esp_err_t read(float *temp_c_out, float *humidity_out);

private:
    esp_err_t wakeup();
    esp_err_t sleep();
    esp_err_t measure_raw(uint16_t *raw_temp, uint16_t *raw_rh);

    i2c_master_dev_handle_t m_dev;
    i2c_master_bus_handle_t m_bus;
};

