/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* 头文件包含 */
#include "gap.h"
#include "common.h"
#include "gatt_svc.h"

/* 私有函数声明 */
inline static void format_addr(char *addr_str, uint8_t addr[]);
static void print_conn_desc(struct ble_gap_conn_desc *desc);
static void start_advertising(void);
static int gap_event_handler(struct ble_gap_event *event, void *arg);

/* 私有变量 */
static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};
static uint8_t esp_uri[] = {BLE_GAP_URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'};

/* 私有函数 */
inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

static void print_conn_desc(struct ble_gap_conn_desc *desc) {
    /* 局部变量 */
    char addr_str[18] = {0};

    /* 连接句柄 */
    ESP_LOGI(TAG, "连接句柄: %d", desc->conn_handle);

    /* 本地 ID 地址 */
    format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "本地 ID 地址: 类型=%d, 值=%s",
             desc->our_id_addr.type, addr_str);

    /* 对端 ID 地址 */
    format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "对端 ID 地址: 类型=%d, 值=%s", desc->peer_id_addr.type,
             addr_str);

    /* 连接信息 */
    ESP_LOGI(TAG,
             "连接间隔=%d, 连接延迟=%d, 监督超时=%d, "
             "加密=%d, 认证=%d, 绑定=%d\n",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
             desc->sec_state.encrypted, desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

static void start_advertising(void) {
    /* 局部变量 */
    int rc = 0;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    /* 设置广播标志位 */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* 设置设备名称 */
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    /* 设置设备发射功率 */
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;

    /* 设置设备外观 */
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    /* 设置设备的 LE 角色 */
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    /* 设置广播字段 */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "设置广播数据失败，错误码: %d", rc);
        return;
    }

    /* 设置设备地址 */
    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr_type;
    rsp_fields.device_addr_is_present = 1;

    /* 设置 URI */
    rsp_fields.uri = esp_uri;
    rsp_fields.uri_len = sizeof(esp_uri);

    /* 设置广播间隔 */
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
    rsp_fields.adv_itvl_is_present = 1;

    /* 设置扫描响应字段 */
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "设置扫描响应数据失败，错误码: %d", rc);
        return;
    }

    /* 设置为可连接和常规可发现模式，作为 beacon */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* 设置广播间隔 */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    /* 开始广播 */
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "开始广播失败，错误码: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "广播已开始！");
}

/*
 * NimBLE 采用事件驱动模型来维持 GAP 服务。
 * gap_event_handler 在调用 ble_gap_adv_start API 时进行注册，
 * 当 GAP 事件到来时便会调用此回调函数。
 */
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    /* 局部变量 */
    int rc = 0;
    struct ble_gap_conn_desc desc;

    /* 处理不同的 GAP 事件 */
    switch (event->type) {

    /* 连接事件 */
    case BLE_GAP_EVENT_CONNECT:
        /* 新连接已建立或连接尝试失败 */
        ESP_LOGI(TAG, "连接%s；状态=%d",
                 event->connect.status == 0 ? "成功" : "失败",
                 event->connect.status);

        /* 连接成功 */
        if (event->connect.status == 0) {
            /* 检查连接句柄 */
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG,
                         "通过句柄查找连接失败，错误码: %d",
                         rc);
                return rc;
            }

            /* 打印连接描述符 */
            print_conn_desc(&desc);

            /* 尝试更新连接参数 */
            struct ble_gap_upd_params params = {.itvl_min = desc.conn_itvl,
                                                .itvl_max = desc.conn_itvl,
                                                .latency = 3,
                                                .supervision_timeout =
                                                    desc.supervision_timeout};
            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0) {
                ESP_LOGE(
                    TAG,
                    "更新连接参数失败，错误码: %d",
                    rc);
                return rc;
            }
        }
        /* 连接失败，重新开始广播 */
        else {
            start_advertising();
        }
        return rc;

    /* 断开事件 */
    case BLE_GAP_EVENT_DISCONNECT:
        /* 连接终止，打印连接描述符 */
        ESP_LOGI(TAG, "与对端断开连接；原因=%d",
                 event->disconnect.reason);

        /* 重新开始广播 */
        start_advertising();
        return rc;

    /* 连接参数更新事件 */
    case BLE_GAP_EVENT_CONN_UPDATE:
        /* 中心端已更新连接参数 */
        ESP_LOGI(TAG, "连接参数更新；状态=%d",
                 event->conn_update.status);

        /* 打印连接描述符 */
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc != 0) {
            ESP_LOGE(TAG, "通过句柄查找连接失败，错误码: %d",
                     rc);
            return rc;
        }
        print_conn_desc(&desc);
        return rc;

    /* 广播完成事件 */
    case BLE_GAP_EVENT_ADV_COMPLETE:
        /* 广播完成后，重新开始广播 */
        ESP_LOGI(TAG, "广播已完成；原因=%d",
                 event->adv_complete.reason);
        start_advertising();
        return rc;

    /* 通知发送事件 */
    case BLE_GAP_EVENT_NOTIFY_TX:
        if ((event->notify_tx.status != 0) &&
            (event->notify_tx.status != BLE_HS_EDONE)) {
            /* 出现错误时打印通知信息 */
            ESP_LOGI(TAG,
                     "通知事件；conn_handle=%d attr_handle=%d "
                     "status=%d is_indication=%d",
                     event->notify_tx.conn_handle, event->notify_tx.attr_handle,
                     event->notify_tx.status, event->notify_tx.indication);
        }
        return rc;

    /* 订阅事件 */
    case BLE_GAP_EVENT_SUBSCRIBE:
        /* 打印订阅信息到日志 */
        ESP_LOGI(TAG,
                 "订阅事件；conn_handle=%d attr_handle=%d "
                 "原因=%d prevn=%d curn=%d previ=%d curi=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle,
                 event->subscribe.reason, event->subscribe.prev_notify,
                 event->subscribe.cur_notify, event->subscribe.prev_indicate,
                 event->subscribe.cur_indicate);

        /* GATT 订阅事件回调 */
        gatt_svr_subscribe_cb(event);
        return rc;

    /* MTU 更新事件 */
    case BLE_GAP_EVENT_MTU:
        /* 打印 MTU 更新信息到日志 */
        ESP_LOGI(TAG, "MTU 更新事件；conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle, event->mtu.channel_id,
                 event->mtu.value);
        return rc;
    }

    return rc;
}

/* 公有函数 */
void adv_init(void) {
    /* 局部变量 */
    int rc = 0;
    char addr_str[18] = {0};

    /* 确保拥有正确的 BT 身份地址（偏好随机地址） */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "设备没有可用的蓝牙地址！");
        return;
    }

    /* 推断要用的蓝牙地址类型（此处不使用隐私） */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "推断地址类型失败，错误码: %d", rc);
        return;
    }

    /* 打印地址 */
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "拷贝设备地址失败，错误码: %d", rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "设备地址: %s", addr_str);

    /* 开始广播 */
    start_advertising();
}

int gap_init(void) {
    /* 局部变量 */
    int rc = 0;

    /* 调用 NimBLE GAP 初始化 API */
    ble_svc_gap_init();

    /* 设置 GAP 设备名称 */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "设置设备名称为 %s 失败，错误码: %d",
                 DEVICE_NAME, rc);
        return rc;
    }
    return rc;
}