#include "lcd.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

static const char *TAG = "lcd";

// HD44780-style commands
#define LCD_CMD_CLEAR_DISPLAY    0x01
#define LCD_CMD_RETURN_HOME      0x02
#define LCD_CMD_ENTRY_MODE_SET   0x04
#define LCD_CMD_DISPLAY_CTRL     0x08
#define LCD_CMD_CURSOR_SHIFT     0x10
#define LCD_CMD_FUNCTION_SET     0x20
#define LCD_CMD_SET_CGRAM_ADDR   0x40
#define LCD_CMD_SET_DDRAM_ADDR   0x80

// display control flags
#define LCD_FLAG_DISPLAY_ON      0x04
#define LCD_FLAG_CURSOR_OFF      0x00
#define LCD_FLAG_BLINK_OFF       0x00

// entry mode flags
#define LCD_FLAG_ENTRY_LEFT              0x02
#define LCD_FLAG_ENTRY_SHIFT_DECREMENT   0x00

// function set flags
#define LCD_FLAG_FUNC_8BIT       0x10
#define LCD_FLAG_FUNC_2LINE      0x08
#define LCD_FLAG_FUNC_5x8DOTS    0x00

// row base addresses
#define LCD_LINE0_ADDR 0x00
#define LCD_LINE1_ADDR 0x40

// -------------------------------------------------------
// Constructor
// -------------------------------------------------------
RGBLCD1602::RGBLCD1602(
    int i2c_port,
    uint8_t lcd_addr,
    uint8_t rgb_addr,
    uint8_t cols,
    uint8_t rows
)
: m_port(i2c_port),
  m_lcd_addr(lcd_addr),
  m_rgb_addr(rgb_addr),
  m_cols(cols),
  m_rows(rows),
  m_bus(NULL),
  m_lcd_dev(NULL),
  m_rgb_dev(NULL),
  m_reg_red(0x04),
  m_reg_green(0x03),
  m_reg_blue(0x02),
  m_disp_ctrl(0),
  m_entry_mode(0),
  m_function(0)
{
}

// -------------------------------------------------------
// I2C setup
// -------------------------------------------------------
esp_err_t RGBLCD1602::i2c_bus_init() {
    // Your wiring: SDA = GPIO10, SCL = GPIO8
    i2c_master_bus_config_t bus_config = {
        .i2c_port = m_port,
        .sda_io_num = (gpio_num_t)10,
        .scl_io_num = (gpio_num_t)8,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,            // default ISR priority
        .trans_queue_depth = 0,        // 0 = default queue depth
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = false,
        },
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &m_bus));
    return ESP_OK;
}

esp_err_t RGBLCD1602::add_lcd_device() {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = m_lcd_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
        .scl_wait_us     = 0,
        .flags = {
            .disable_ack_check = false,
        },
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(m_bus, &dev_cfg, &m_lcd_dev));
    return ESP_OK;
}

esp_err_t RGBLCD1602::add_rgb_device() {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = m_rgb_addr,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
        .scl_wait_us     = 0,
        .flags = {
            .disable_ack_check = false,
        },
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(m_bus, &dev_cfg, &m_rgb_dev));
    return ESP_OK;
}

// -------------------------------------------------------
// Low-level write helpers
// -------------------------------------------------------
esp_err_t RGBLCD1602::write_cmd(uint8_t cmd)
{
    // control byte 0x80 = "next byte is a command"
    uint8_t payload[2] = {0x80, cmd};
    return i2c_master_transmit(m_lcd_dev, payload, 2, I2C_TIMEOUT_MS);
}

esp_err_t RGBLCD1602::write_data(uint8_t data)
{
    // control byte 0x40 = "next byte is data"
    uint8_t payload[2] = {0x40, data};
    return i2c_master_transmit(m_lcd_dev, payload, 2, I2C_TIMEOUT_MS);
}

esp_err_t RGBLCD1602::rgb_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t payload[2] = { reg, val };
    return i2c_master_transmit(m_rgb_dev, payload, 2, I2C_TIMEOUT_MS);
}

esp_err_t RGBLCD1602::lcd_set_ddram_addr(uint8_t addr)
{
    return write_cmd(LCD_CMD_SET_DDRAM_ADDR | addr);
}

// -------------------------------------------------------
// Backlight register mapping for your board
// (from DFRobot, we matched your scan: rgb addr = 0x2D)
// -------------------------------------------------------
void RGBLCD1602::detect_rgb_register_map()
{
    if (m_rgb_addr == 0x2D) {
        // DFRobot branch for 0x2D boards:
        // REG_RED=0x01, GREEN=0x02, BLUE=0x03
        m_reg_red   = 0x01;
        m_reg_green = 0x02;
        m_reg_blue  = 0x03;
    } else if (m_rgb_addr == 0x60) {
        m_reg_red   = 0x04;
        m_reg_green = 0x03;
        m_reg_blue  = 0x02;
    } else if (m_rgb_addr == (0x60 >> 1)) {
        m_reg_red   = 0x06;
        m_reg_green = 0x07;
        m_reg_blue  = 0x08;
    } else if (m_rgb_addr == 0x6B) {
        m_reg_red   = 0x06;
        m_reg_green = 0x05;
        m_reg_blue  = 0x04;
    } else {
        // fallback guess
        m_reg_red   = 0x04;
        m_reg_green = 0x03;
        m_reg_blue  = 0x02;
    }
}

// -------------------------------------------------------
// High-level API
// -------------------------------------------------------
esp_err_t RGBLCD1602::init()
{
    ESP_LOGI(TAG, "init(): starting I2C bus");
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(add_lcd_device());
    ESP_ERROR_CHECK(add_rgb_device());

    detect_rgb_register_map();

    ESP_LOGI(TAG, "init(): running LCD init sequence");
    ESP_ERROR_CHECK(lcd_begin_sequence());

    ESP_LOGI(TAG, "init(): done");
    return ESP_OK;
}

// This recreates the Arduino "begin()" logic using ESP-IDF
esp_err_t RGBLCD1602::lcd_begin_sequence()
{
    // function set: 8-bit, 2-line, 5x8 font
    m_function =
        LCD_FLAG_FUNC_8BIT |
        LCD_FLAG_FUNC_2LINE |
        LCD_FLAG_FUNC_5x8DOTS;

    // wait ~50ms after power-up
    vTaskDelay(pdMS_TO_TICKS(50));

    // send FUNCTION SET a few times, like the original library did
    ESP_ERROR_CHECK(write_cmd(LCD_CMD_FUNCTION_SET | m_function));
    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_ERROR_CHECK(write_cmd(LCD_CMD_FUNCTION_SET | m_function));
    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_ERROR_CHECK(write_cmd(LCD_CMD_FUNCTION_SET | m_function));
    vTaskDelay(pdMS_TO_TICKS(5));

    // display on, cursor off, blink off
    m_disp_ctrl = LCD_FLAG_DISPLAY_ON | LCD_FLAG_CURSOR_OFF | LCD_FLAG_BLINK_OFF;
    ESP_ERROR_CHECK(write_cmd(LCD_CMD_DISPLAY_CTRL | m_disp_ctrl));
    vTaskDelay(pdMS_TO_TICKS(5));

    // clear
    ESP_ERROR_CHECK(write_cmd(LCD_CMD_CLEAR_DISPLAY));
    vTaskDelay(pdMS_TO_TICKS(2));

    // entry mode: left to right, no shift
    m_entry_mode = LCD_FLAG_ENTRY_LEFT | LCD_FLAG_ENTRY_SHIFT_DECREMENT;
    ESP_ERROR_CHECK(write_cmd(LCD_CMD_ENTRY_MODE_SET | m_entry_mode));
    vTaskDelay(pdMS_TO_TICKS(2));

    // optional backlight init sequences:
    // your 0x2D branch in Arduino code setReg(...) differently,
    // we're just going to set a default color after init.
    setRGB(0x40, 0x40, 0x40);

    return ESP_OK;
}

esp_err_t RGBLCD1602::setCursor(uint8_t col, uint8_t row)
{
    uint8_t base = (row == 0) ? LCD_LINE0_ADDR : LCD_LINE1_ADDR;
    return lcd_set_ddram_addr(base + col);
}

esp_err_t RGBLCD1602::printstr(const char *s)
{
    while (*s) {
        ESP_ERROR_CHECK(write_data((uint8_t)*s));
        s++;
    }
    return ESP_OK;
}

esp_err_t RGBLCD1602::clear()
{
    ESP_ERROR_CHECK(write_cmd(LCD_CMD_CLEAR_DISPLAY));
    vTaskDelay(pdMS_TO_TICKS(2));
    return ESP_OK;
}

esp_err_t RGBLCD1602::setRGB(uint8_t r, uint8_t g, uint8_t b)
{
    ESP_ERROR_CHECK(rgb_write_reg(m_reg_red,   r));
    ESP_ERROR_CHECK(rgb_write_reg(m_reg_green, g));
    ESP_ERROR_CHECK(rgb_write_reg(m_reg_blue,  b));
    return ESP_OK;
}

