/**
 * @file    key.h
 * @brief
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/17
 */
#ifndef KEY_H
#define KEY_H

/**
 * @brief 按键事件类型
 *        Key_Scan() 每次调用只会返回"这一刻"检测到的事件，
 *        没有按键动作时返回 KEY_EVENT_NONE
 */
typedef enum
{
    KEY_EVENT_NONE = 0,   // 无按键事件
    KEY_EVENT_MENU,       // PA0 - 菜单（进入/切换设置项）
    KEY_EVENT_PLUS,       // PA1 - 阈值+
    KEY_EVENT_MINUS,      // PA2 - 阈值-
    KEY_EVENT_ENTER       // PA3 - 确定（保存/退出）
} KeyEvent_t;

/**
 * @brief 扫描4个按键，返回检测到的按键事件（内部已做消抖+边沿检测）
 * @note  非阻塞函数，需要在主循环 while(1) 里每次循环都调用一次
 * @return 检测到"刚刚被按下"的按键事件；如果没有按键动作，返回 KEY_EVENT_NONE
 */
KeyEvent_t Key_Scan(void);

#endif
