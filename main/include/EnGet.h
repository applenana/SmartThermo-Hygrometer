/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef ENGET_H
#define ENGET_H

/* Includes */
/* ESP APIs */
#include "esp_random.h"

/* Defines */
#define ENGET_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

/* Public function declarations */
float GetTemp(void);
float GetHumi(void);
esp_err_t i2c_master_init(void);
esp_err_t sht40_read_measurement(float *temperature, float *humidity);
void UpDateTH(void);
extern float temperature; // 声明全局变量
extern float humidity;    // 声明全局变量

#endif // ENGET_H
