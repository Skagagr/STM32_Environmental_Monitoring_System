/**
 * @file    alarm_ctrl.c
 * @brief   报警判断模块实现
 *
 * @details 根据传入的当前温湿度值与上下限阈值，判断是否需要报警
 *          本模块只做纯逻辑判断，不采集数据、不管理阈值、不操作任何硬件
 *          与 env_monitor、param_manager 均无耦合，可独立编译和测试
 *
 * @version 1.0.0
 * @date    2026/7/22
 */
#include "alarm_ctrl.h"

uint8_t AlarmCtrl_Update(float temp, float humi, float temp_low, float temp_high, float humi_low, float humi_high)
{
    if (temp > temp_high || temp < temp_low || humi > humi_high || humi < humi_low)
        return 1;

    return 0;
}
