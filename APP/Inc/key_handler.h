/**
* @file    key_handler.h
 * @brief   按键状态机模块：处理菜单/加/减/确定键，维护当前选中的阈值字段
 *
 * @details 四个物理按键分别对应:
 *            - MENU  : 在 temp_low -> temp_high -> humi_low -> humi_high
 *                      之间循环切换选中字段(不含"未选中"状态，只在4个字段间转圈)
 *            - PLUS  : 对当前选中字段执行 +3 调节，未选中任何字段时不响应
 *            - MINUS : 对当前选中字段执行 -3 调节，未选中任何字段时不响应
 *            - ENTER : 退出调节模式，选中字段重置为 PARAM_FIELD_NONE
 *
 *          开机默认选中字段为 PARAM_FIELD_NONE(未选中，不进入调节模式)。
 *          本模块只依赖 key.h(按键事件来源) 和 param_manager.h(字段类型定义
 *          与 Param_AdjustXxx 调节接口)，不直接操作 OLED；当前选中字段通过
 *          KeyHandler_GetSelectedField() 对外暴露，供 main.c 传给 env_monitor
 *          决定高亮显示哪个区域，模块之间不直接耦合。
 *
 * @version 1.0.0
 * @date    2026/7/22
 */
#ifndef KEY_HANDLER_H
#define KEY_HANDLER_H

#include "param_manager.h"

/**
 * @brief 按键状态机更新入口，由主循环周期调用
 *
 * @details 内部调用 Key_Scan() 获取一次按键事件并驱动状态机：
 *            - KEY_EVENT_NONE  : 无按键，直接返回，不做任何处理
 *            - KEY_EVENT_MENU  : 选中字段前进一格，到达 HUMI_HIGH 后
 *                                折返回 TEMP_LOW(循环，不会回到 NONE)
 *            - KEY_EVENT_ENTER : 选中字段重置为 PARAM_FIELD_NONE，退出调节模式
 *            - KEY_EVENT_PLUS  : 对当前选中字段对应的阈值执行 +3 调节
 *                                (若当前为 PARAM_FIELD_NONE 则不响应)
 *            - KEY_EVENT_MINUS : 对当前选中字段对应的阈值执行 -3 调节
 *                                (若当前为 PARAM_FIELD_NONE 则不响应)
 */
void Key_Update(void);

/**
 * @brief 查询当前选中的阈值字段
 *
 * @details 供 main.c 调用，将结果传给 EnvMonitor_Update()，
 *          用于决定 OLED 上哪个字段区域需要反色高亮显示。
 *
 * @return ParamField_t 当前选中字段，PARAM_FIELD_NONE 表示未进入调节模式
 */
ParamField_t KeyHandler_GetSelectedField(void);

#endif