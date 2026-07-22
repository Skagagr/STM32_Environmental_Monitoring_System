/**
 * @file    alarm_ctrl.h
 * @brief   报警判断模块声明
 *
 * @details 根据传入的当前温湿度值与上下限阈值，判断是否需要报警
 *          本模块只做纯逻辑判断，不采集数据、不管理阈值、不操作任何硬件
 *          与 env_monitor、param_manager 均无耦合，可独立编译和测试
 *
 *
 * @version 1.0.0
 * @date    2026/7/22
 */
#ifndef ALARM_CTRL_H
#define ALARM_CTRL_H

#include <stdint.h>

/**
 * @brief 根据当前温湿度与阈值判断是否需要报警
 *
 * @param temp      当前温度值
 * @param humi      当前湿度值
 * @param temp_low  温度下限阈值，低于此值视为异常
 * @param temp_high 温度上限阈值，高于此值视为异常
 * @param humi_low  湿度下限阈值，低于此值视为异常
 * @param humi_high 湿度上限阈值，高于此值视为异常
 * @return uint8_t  1 表示需要报警（任一项超出阈值），0 表示一切正常
 */
uint8_t AlarmCtrl_Update(float temp, float humi, float temp_low, float temp_high, float humi_low, float humi_high);

#endif