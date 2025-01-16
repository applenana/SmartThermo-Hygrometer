# 智能温湿度计

## 概览Overview

使用ESPIDF开发的温湿度器的固件仓库,包含三大功能:SHT40温湿度采集,墨水屏驱动,BLE低功耗蓝牙管理

1. 蓝牙部分内核代码基本从乐鑫的官方示例[Here](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/ble/get-started/ble-introduction.html)抄的,基于其加入了蓝牙和墨水屏模块
2. 与HA连接
    1. 条件:HA服务器有可以自定义的蓝牙网关/有蓝牙适配器
    2. 实现:BLE广播后可被发现,连接后订阅相关属性即可

## FirmWare固件

1. 发布的固件只在ESP32C2上进行了测试

## ToDo

- [x] 完成SHT40温度采集代码
- [x] 完成BLE蓝牙管理框架
- [] 完成墨水屏驱动
- [] 完成与RTC芯片的通讯
- [] 完成电池电量采集部分
- [] 完成与HA对接测试