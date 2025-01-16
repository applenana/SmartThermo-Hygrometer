/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef GATT_SVR_H
#define GATT_SVR_H

/* Includes */
/* NimBLE GATT APIs */
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

/* NimBLE GAP APIs */
#include "host/ble_gap.h"

/* Public function declarations */
/* 初始化 GATT 服务 */
int gatt_svc_init(void);

/* 注册回调 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

/* 订阅事件回调 */
void gatt_svr_subscribe_cb(struct ble_gap_event *event);

/* 通知函数 */
void send_temperature_indication(void);
void send_humidity_indication(void);

#endif // GATT_SVR_H
