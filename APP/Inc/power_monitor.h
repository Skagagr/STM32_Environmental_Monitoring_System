/**
 * @file    power_monitor.h
 * @brief   PVD掉电检测配置模块对外接口
 * @details
 *   本模块只负责开机时把PVD外设配置好（时钟使能、检测电压阈值、
 *   中断触发方式、NVIC优先级），供 app.h 的初始化流程调用一次。
 *
 *   掉电真正发生时的处理逻辑（关中断、保存阈值、死循环等断电）
 *   写在 power_monitor.c 里，通过重写HAL库提供的弱函数
 *   HAL_PWR_PVDCallback() 实现——这个函数的原型已经在HAL的头文件里
 *   声明过了，不需要、也不应该在本文件里再声明一遍。
 *
 * @note    PVD中断服务函数设计上不会调用任何RTOS API（如osDelay/
 *          osMutexAcquire），避免调度器已冻结/未运行时死锁
 *
 * @version 1.0.0
 * @date    2026/7/20
 */
#ifndef __POWER_MONITOR_H
#define __POWER_MONITOR_H

/**
 * @brief  配置PVD外设：使能PWR时钟、设置检测电压阈值、配置中断触发方式、
 *         设置NVIC最高优先级并使能中断
 * @details 只需要在app启动流程里调用一次，之后PVD会在后台持续监测电压，
 *          掉电时自动触发 HAL_PWR_PVDCallback()，不需要轮询
 */
void Power_Monitor_Init(void);

#endif
