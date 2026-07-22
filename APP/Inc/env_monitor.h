/**
 * @file    env_monitor.h
 * @brief   环境监测模块：周期性采集温湿度并刷新OLED显示
 *
 * @details 内部按1秒周期非阻塞刷新，通过EnvMonitor_Update(now_tick)
 *          由主循环驱动；采集到的最新数据可通过EnvMonitor_GetData()
 *          获取，供报警等其他模块使用，模块之间不直接耦合。
 *
 * @version 1.0.0
 * @date    2026/7/22
 */
#ifndef ENV_MONITOR_H
#define ENV_MONITOR_H

#include "sht30.h"
#include "oled.h"


/**
 * @brief 一次环境监测数据快照
 */
typedef struct
{
    float temp;
    float humi;
} EnvData_t;

/**
 * @brief 初始化环境监测模块
 * @param sht30_ctx 已完成初始化的SHT30句柄指针
 * @param oled_ctx  已完成初始化的OLED句柄指针
 */
void EnvMonitor_Init(SHT30_Ctx *sht30_ctx, OLED_Ctx *oled_ctx);

/**
 * @brief 读取温湿度并刷新OLED
 * @param now_tick 当前系统tick，用于判断是否到达1秒刷新周期
 * @return 1 本次真正执行了刷新
 * @return 0 未到刷新周期，本次未刷新
 */
uint8_t EnvMonitor_Update(uint32_t now_tick);

/**
 * @brief 获取最新温湿度
 * @return 返回EnvData_t的地址
 */
const EnvData_t *EnvMonitor_GetData(void);

#endif
