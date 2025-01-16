/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "EnGet.h"
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           5         // SCL引脚
#define I2C_MASTER_SDA_IO           4         // SDA引脚
#define I2C_MASTER_NUM              I2C_NUM_0  // I2C端口号
#define I2C_MASTER_FREQ_HZ          100000     // I2C时钟频率
#define I2C_MASTER_TX_BUF_DISABLE   0          // 禁用TX缓冲区
#define I2C_MASTER_RX_BUF_DISABLE   0          // 禁用RX缓冲区
#define SHT40_SENSOR_ADDR           0x44       // SHT40默认I2C地址
#define SHT40_MEASURE_CMD           0xFD       // 测量命令
float temperature, humidity;
float ftem,fhum;

esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t sht40_read_measurement(float *temperature, float *humidity) {
    uint8_t data[6];
    uint8_t cmd = SHT40_MEASURE_CMD;

    // 发送测量命令
    int rc = i2c_master_write_to_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
    if (rc != ESP_OK){
        ESP_LOGE(TAG, "试图测量I2C时出错: %d ", rc);
    }

    // 延时等待测量完成
    vTaskDelay(pdMS_TO_TICKS(10));

    // 读取传感器数据
    ESP_ERROR_CHECK(i2c_master_read_from_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, data, 6, pdMS_TO_TICKS(100)));

    // 数据解码
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_humi = (data[3] << 8) | data[4];

    // 转换为实际值
    *temperature = -45 + 175 * ((float)raw_temp / 65535.0f);
    *humidity = 100 * ((float)raw_humi / 65535.0f);

    return ESP_OK;
}

void UpDateTH(void){
    if (sht40_read_measurement(&ftem, &fhum) == ESP_OK) {
        ESP_LOGI(TAG, "温度: %f °C, 湿度: %f %%", ftem, fhum);
        temperature = ftem;
        humidity = fhum;
    } else {
        ESP_LOGE(TAG, "Failed to read from SHT40");
    }
}

float GetTemp(void){
    return temperature;
}

float GetHumi(void){
    return humidity;
}