/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* 头文件包含 */
#include "gatt_svc.h"
#include "common.h"
#include "EnGet.h"

/* 私有函数声明 */
static int chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);

/* 私有变量 */
static const ble_uuid16_t temp_humi_svc_uuid = BLE_UUID16_INIT(0x181A);

static uint8_t temperature_chr_val[2] = {0};
static uint8_t humidity_chr_val[2] = {0};
static uint16_t temperature_chr_val_handle;
static uint16_t humidity_chr_val_handle;
static char percentage_chr_val[8] = {0}; 
static uint16_t percentage_chr_val_handle;

static const ble_uuid16_t temperature_chr_uuid = BLE_UUID16_INIT(0x2A6E);
static const ble_uuid16_t humidity_chr_uuid = BLE_UUID16_INIT(0x2A6F);
static const ble_uuid16_t battery_svc_uuid = BLE_UUID16_INIT(0x180F);         // 电量服务
static const ble_uuid16_t percentage_chr_uuid = BLE_UUID16_INIT(0x2A1B);      // 电量百分比属性

static uint16_t temp_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t humi_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t battery_chr_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool percentage_ind_status = false;
static bool temp_ind_status = false;
static bool humi_ind_status = false;

/* GATT 服务表 */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* 温湿度服务 */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &temp_humi_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {/* 温度特性 */
                 .uuid = &temperature_chr_uuid.u,
                 .access_cb = chr_access,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                 .val_handle = &temperature_chr_val_handle},
                {/* 湿度特性 */
                 .uuid = &humidity_chr_uuid.u,
                 .access_cb = chr_access,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                 .val_handle = &humidity_chr_val_handle},
                {0}},
    },
    /* 电量服务 */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &battery_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {/* 电量百分比特性 */
                 .uuid = &percentage_chr_uuid.u,
                 .access_cb = chr_access,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                 .val_handle = &percentage_chr_val_handle},
                {0}},
    },
    {0},
};

/* 私有函数 */
static int chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc;
    
    if (attr_handle == temperature_chr_val_handle){
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ESP_LOGI(TAG, "读取温度特性；conn_handle=%d attr_handle=%d", conn_handle, attr_handle);

            if (attr_handle == temperature_chr_val_handle) {
                uint16_t temp_value = (int16_t)(GetTemp() * 100);
                // 分解为两个字节
                temperature_chr_val[0] = temp_value & 0xFF;         // 低字节;
                temperature_chr_val[1] = (temp_value >> 8) & 0xFF;  // 高字节
                rc = os_mbuf_append(ctxt->om, &temperature_chr_val, sizeof(temperature_chr_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }

        ESP_LOGE(TAG, "对温度特性的访问操作异常，操作码: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }else if (attr_handle == humidity_chr_val_handle){
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ESP_LOGI(TAG, "读取湿度特性；conn_handle=%d attr_handle=%d", conn_handle, attr_handle);

            if (attr_handle == humidity_chr_val_handle) {
                uint16_t humi_value = (int16_t)(GetHumi() * 100);
                // 分解为两个字节
                humidity_chr_val[0] = humi_value & 0xFF;         // 低字节;
                humidity_chr_val[1] = (humi_value >> 8) & 0xFF;  // 高字节
                rc = os_mbuf_append(ctxt->om, &humidity_chr_val, sizeof(humidity_chr_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }

        ESP_LOGE(TAG, "对湿度特性的访问操作异常，操作码: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }else if (attr_handle == percentage_chr_val_handle){
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            ESP_LOGI(TAG, "读取电量百分比特性；conn_handle=%d attr_handle=%d", conn_handle, attr_handle);

            if (attr_handle == percentage_chr_val_handle) {
                uint8_t percentage_value = (uint8_t)GetBatteryPercentage(); // 获取电量百分比
                snprintf(percentage_chr_val, sizeof(percentage_chr_val), "%d%%", percentage_value); // 转换为字符串形式

                rc = os_mbuf_append(ctxt->om, percentage_chr_val, strlen(percentage_chr_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }

        ESP_LOGE(TAG, "对电量百分比特性的访问操作异常，操作码: %d", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }else{
        return 0;
    }
}

/* 公有函数 */
void send_indication(void) {
    if (temp_ind_status && temp_chr_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        uint16_t temp_value = (int16_t)(GetTemp() * 100);
        // 分解为两个字节
        temperature_chr_val[0] = temp_value & 0xFF;         // 低字节;
        temperature_chr_val[1] = (temp_value >> 8) & 0xFF;  // 高字节

        // 发送指示
        int rc = ble_gatts_indicate(temp_chr_conn_handle, temperature_chr_val_handle);
        if (rc == 0) {
            ESP_LOGI(TAG, "温度指示已发送！温度=%d.%02d °C", temp_value / 100, temp_value % 100);
        } else {
            ESP_LOGE(TAG, "发送温度指示失败，错误码=%d", rc);
        }
    } else {
        //ESP_LOGW(TAG, "未订阅温度指示或无效连接句柄！");
    }

    if (humi_ind_status && humi_chr_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        uint16_t humi_value = (int16_t)(GetHumi() * 100);
        // 分解为两个字节
        humidity_chr_val[0] = humi_value & 0xFF;         // 低字节;
        humidity_chr_val[1] = (humi_value >> 8) & 0xFF;  // 高字节
        // 调用 ble_gatts_indicate
        int rc = ble_gatts_indicate(humi_chr_conn_handle, humidity_chr_val_handle);
        if (rc == 0) {
            ESP_LOGI(TAG, "湿度指示已发送！ 湿度=%d.%02d %%", humi_value / 100, humi_value % 100);
        } else {
            ESP_LOGE(TAG, "发送湿度指示失败，错误码=%d", rc);
        }
    } else {
        //ESP_LOGW(TAG, "未订阅湿度指示或无效连接句柄！");
    }

    if (percentage_ind_status && battery_chr_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        uint8_t percentage_value = (uint8_t)GetBatteryPercentage();
        snprintf(percentage_chr_val, sizeof(percentage_chr_val), "%d%%", percentage_value); // 转换为字符串形式

        int rc = ble_gatts_indicate(battery_chr_conn_handle, percentage_chr_val_handle);
        if (rc == 0) {
            ESP_LOGI(TAG, "电量百分比指示已发送！电量=%s", percentage_chr_val);
        } else {
            ESP_LOGE(TAG, "发送电量百分比指示失败，错误码=%d", rc);
        }
    } else {
        //ESP_LOGW(TAG, "未订阅电量百分比指示或无效连接句柄！");
    }
}

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == temperature_chr_val_handle) {
        temp_chr_conn_handle = event->subscribe.conn_handle;
        temp_ind_status = event->subscribe.cur_indicate;
        ESP_LOGI(TAG, "温度订阅事件；conn_handle=%d, 当前状态=%d", temp_chr_conn_handle, temp_ind_status);
    } else if (event->subscribe.attr_handle == humidity_chr_val_handle) {
        humi_chr_conn_handle = event->subscribe.conn_handle;
        humi_ind_status = event->subscribe.cur_indicate;
        ESP_LOGI(TAG, "湿度订阅事件；conn_handle=%d, 当前状态=%d", humi_chr_conn_handle, humi_ind_status);
    } else if (event->subscribe.attr_handle == percentage_chr_val_handle) {
        battery_chr_conn_handle = event->subscribe.conn_handle;
        percentage_ind_status = event->subscribe.cur_indicate;
        ESP_LOGI(TAG, "电量百分比订阅事件；conn_handle=%d, 当前状态=%d", battery_chr_conn_handle, percentage_ind_status);
    }
}

/* GATT 服务器初始化 */
int gatt_svc_init(void) {
    int rc;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
