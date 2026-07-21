/**
 * @file    power_monitor.c
 * @brief   PVD掉电检测模块实现
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/20
 */
#include "power_monitor.h"
#include "param_manager.h"
#include "main.h"

/**
 * @brief 配置PVD外设
 * @note  PVDLevel/Mode的具体取值请根据实际电源设计和CubeMX里PWR配置界面
 *        核对，这里给的是示例值：
 *          - PWR_PVDLEVEL_7 大约对应2.9V阈值，需要结合PVD触发点
 *            和外接电容能撑住的时间来选，阈值选得太低，
 *            触发时留给Param_Save()的电压余量就不够
 *          - PWR_MODE_IT_FALLING 表示电压从上往下跌破阈值时触发中断
 *            （正常掉电的方向），不是上升沿
 */
void Power_Monitor_Init(void)
{
    PWR_PVDTypeDef pvd_config = {0};

    // PVD外设挂在PWR总线下，用之前必须先使能PWR的时钟
    __HAL_RCC_PWR_CLK_ENABLE();

    pvd_config.PVDLevel = PWR_PVDLEVEL_7;
    pvd_config.Mode     = PWR_MODE_IT_FALLING;
    HAL_PWR_ConfigPVD(&pvd_config);

    // 优先级设为最高(0)，保证掉电中断不会被任何其它中断打断或延迟
    HAL_NVIC_SetPriority(PVD_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(PVD_IRQn);

    // 前面只是配置好了参数，这一步才真正打开PVD检测
    HAL_PWR_EnablePVD();
}

/**
 * @brief 重写HAL提供的PVD中断弱回调函数，真正掉电时执行的逻辑
 * @note  这里没有任何RTOS API调用、没有延时、没有非必要的分支判断——
 *        整个函数要在外接电容放电耗尽之前跑完，代码必须尽量短、
 *        尽量确定性，不能有任何可能阻塞或耗时不可控的操作
 */
void HAL_PWR_PVDCallback(void)
{
    // 立刻关闭全局中断，防止其它外设中断在这个节骨眼上打断保存流程，抢占电容仅剩的供电时间
    __disable_irq();

    // 整个模块唯一一处调用Param_Save()的地方，把当前RAM里的阈值落盘
    Param_Save();

    // 保存完成后原地死循环，等电容彻底放电、系统失电重启，
    // 不再做任何事——已禁用全局中断，也没有必要再返回主循环
    while (1)
    {
    }
}

/**
 * 因为本代码不是在CubeMX配置的PVD，没有生成相关代码
 *
 * 在 stm32f1xx_it.c 中添加
 *
 *  void PVD_IRQHandler(void)
 *  {
 *      HAL_PWR_PVD_IRQHandler();
 *  }
 *  放在文件里其它类似XXX_IRQHandler函数附近，
 *  同样也要写在对应的USER CODE区域里，避免以后CubeMX重新生成时被覆盖）
 *
 *
 */