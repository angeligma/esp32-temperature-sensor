#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// YOUR SCANNED I2C ADDRESSES:
#define LCD_I2C_ADDR_DEFAULT  0x3E  // text LCD controller
#define RGB_I2C_ADDR_DEFAULT  0x2D  // RGB backlight controller

// Bus settings
#define I2C_PORT_USED          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ     100000
#define I2C_TIMEOUT_MS         1000

class RGBLCD1602 {
public:
    RGBLCD1602(
        int i2c_port = I2C_PORT_USED,
        uint8_t lcd_addr = LCD_I2C_ADDR_DEFAULT,
        uint8_t rgb_addr = RGB_I2C_ADDR_DEFAULT,
        uint8_t cols = 16,
        uint8_t rows = 2
    );

    // bring up I2C, LCD, and backlight
    esp_err_t init();

    // move cursor (0-based col, row)
    esp_err_t setCursor(uint8_t col, uint8_t row);

    // write a null-terminated string at current cursor
    esp_err_t printstr(const char *s);

    // set backlight RGB color (0-255 each)
    esp_err_t setRGB(uint8_t r, uint8_t g, uint8_t b);

    // clear display
    esp_err_t clear();

public:
    i2c_master_bus_handle_t getBusHandle() const { return m_bus; }


private:
    // low-level helpers
    esp_err_t write_cmd(uint8_t cmd);
    esp_err_t write_data(uint8_t data);
    esp_err_t rgb_write_reg(uint8_t reg, uint8_t val);
    esp_err_t lcd_set_ddram_addr(uint8_t addr);

    // init helpers
    esp_err_t i2c_bus_init();
    esp_err_t add_lcd_device();
    esp_err_t add_rgb_device();
    void      detect_rgb_register_map();
    esp_err_t lcd_begin_sequence();

private:
    int      m_port;
    uint8_t  m_lcd_addr;
    uint8_t  m_rgb_addr;
    uint8_t  m_cols;
    uint8_t  m_rows;

    i2c_master_bus_handle_t  m_bus;
    i2c_master_dev_handle_t  m_lcd_dev;
    i2c_master_dev_handle_t  m_rgb_dev;

    uint8_t m_reg_red;
    uint8_t m_reg_green;
    uint8_t m_reg_blue;

    // cached LCD config flags
    uint8_t m_disp_ctrl;
    uint8_t m_entry_mode;
    uint8_t m_function;
};

