extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
}

#include "lcd.hpp"
#include "shtc3.hpp"

extern "C" void app_main(void)
{
    // Create and init LCD object
    RGBLCD1602 lcd(
        I2C_PORT_USED,
        LCD_I2C_ADDR_DEFAULT,  // 0x3E from i2cdetect
        RGB_I2C_ADDR_DEFAULT,  // 0x2D from i2cdetect
        16,
        2
    );

    lcd.init();

    // pick a readable backlight color
    lcd.setRGB(0x20, 0x40, 0x80);

    // create and init sensor object using SAME i2c bus
    SHTC3 sensor(lcd.getBusHandle());
    sensor.init(); // wake and attach dev at 0x70

    char line0[17];
    char line1[17];

    while (1) {
        float temp_c = 0.0f;
        float hum = 0.0f;

        // read sensor
        if (sensor.read(&temp_c, &hum) == ESP_OK) {
            // format strings to fit 16 columns
            // Example: "T=23.4C"
            //          "H=41.2%"
            snprintf(line0, sizeof(line0), "Temp:%.1fC", temp_c);
            snprintf(line1, sizeof(line1), "Hum :%.1f%%", hum);

            // write to LCD
            lcd.setCursor(0, 0);
            lcd.printstr("               "); // clear line
            lcd.setCursor(0, 0);
            lcd.printstr(line0);

            lcd.setCursor(0, 1);
            lcd.printstr("               "); // clear line
            lcd.setCursor(0, 1);
            lcd.printstr(line1);
        } else {
            // sensor failed to read
            lcd.setCursor(0, 0);
            lcd.printstr("SHTC3 ERROR     ");
            lcd.setCursor(0, 1);
            lcd.printstr("                ");
        }

        // update every ~1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


