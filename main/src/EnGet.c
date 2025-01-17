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
#include "esp_adc/adc_oneshot.h"

#define I2C_MASTER_SCL_IO           5         // SCL引脚
#define I2C_MASTER_SDA_IO           4         // SDA引脚
#define I2C_MASTER_NUM              I2C_NUM_0  // I2C端口号
#define I2C_MASTER_FREQ_HZ          100000     // I2C时钟频率
#define I2C_MASTER_TX_BUF_DISABLE   0          // 禁用TX缓冲区
#define I2C_MASTER_RX_BUF_DISABLE   0          // 禁用RX缓冲区
#define SHT40_SENSOR_ADDR           0x44       // SHT40默认I2C地址
#define SHT40_MEASURE_CMD           0xFD       // 测量命令
float temperature,humidity,batteryVoltage,batteryPercentage;
float ftem,fhum;
adc_oneshot_unit_handle_t adc1_handle;
static int adc_raw_value;

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
    int rc1 = i2c_master_write_to_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
    if (rc1 != ESP_OK){
        ESP_LOGE(TAG, "试图测量I2C时出错: %d ", rc1);
        return rc1;
    }

    // 延时等待测量完成
    vTaskDelay(pdMS_TO_TICKS(10));

    // 读取传感器数据
    int rc2 = i2c_master_read_from_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, data, 6, pdMS_TO_TICKS(100));
    if (rc2 != ESP_OK){
        ESP_LOGE(TAG, "试图读取I2C时出错: %d ", rc2);
        return rc2;
    }

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

void InitADC(void){
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,  // 使用 ADC1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 配置 ADC1 IO3（ADC1_CHANNEL_3）
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // 默认的位宽
        .atten = ADC_ATTEN_DB_12,       // 配置衰减
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &config));
}

void UpDataBattry(void){
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &adc_raw_value));
    ESP_LOGI("ADC", "Raw ADC Value from IO3 (ADC1 Channel 3): %d", adc_raw_value);
    batteryVoltage = ((float)adc_raw_value/ 1000) * 2;
    batteryPercentage = (batteryVoltage - 3.7) * 100 / (4.2 - 3.7);
    ESP_LOGI("Battery", "电压: %f 电量: %f",batteryVoltage,batteryPercentage);
}

float GetTemp(void){
    return temperature;
}

float GetHumi(void){
    return humidity;
}

float GetVoltage(void){
    return batteryVoltage;
}

float GetBatteryPercentage(void){
    return batteryPercentage;
}