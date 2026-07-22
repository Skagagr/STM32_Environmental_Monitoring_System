/**
 * @file    key.c
 * @brief   按键扫描（消抖 + 边沿检测）
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/17
 */
#include "key.h"
#include "main.h"

/**
 * @brief 按键映射表的每一项：一个引脚 对应 一个按键事件
 *        这样把"硬件是什么"和"扫描逻辑怎么做"分开，
 *        以后加按键只需要在 key_map[] 里加一行，下面的 for 循环逻辑不用改
 */
typedef struct
{
    GPIO_TypeDef *port;     // GPIO端口，比如 GPIOA
    uint16_t      pin;      // 具体引脚号，比如 GPIO_PIN_0
    KeyEvent_t    event;    // 这个引脚对应哪个按键事件
} KeyMap_t;

// 4个按键的映射表，顺序无所谓，扫描时会按顺序一个个查
static const KeyMap_t key_map[] =
{
    {GPIOA, GPIO_PIN_0, KEY_EVENT_MENU},
    {GPIOA, GPIO_PIN_1, KEY_EVENT_PLUS},
    {GPIOA, GPIO_PIN_2, KEY_EVENT_MINUS},
    {GPIOA, GPIO_PIN_3, KEY_EVENT_ENTER},
};

// 自动计算按键数量 = 整个数组占的字节数 / 单个元素占的字节数 = 4
// 这样以后往 key_map[] 里加减按键，这里不用手动改数字，不会漏改出bug
#define KEY_COUNT           (sizeof(key_map) / sizeof(key_map[0]))

// 消抖+扫描间隔：机械按键按下瞬间电平会抖动几毫秒到十几毫秒，
// 每隔20ms才读一次电平，可以跳过这段抖动的时间窗口
#define KEY_SCAN_INTERVAL   20

// 上一次调用 Key_Scan() 的时间戳，用来判断"是否已经过了20ms"
static uint32_t s_last_tick = 0;

// 记录每个按键"上一次扫描到的电平状态"，用于边沿检测
// 数组大小 = 按键数量，每个按键各自独立记录一份
// 初始值全部设为 GPIO_PIN_SET，假设开机时按键都是"未按下"状态
static GPIO_PinState last_state[KEY_COUNT] = {GPIO_PIN_SET};

/**
 * @brief 扫描4个按键，返回检测到的按键事件（内部已做消抖+边沿检测）
 * @note  非阻塞函数，需要在主循环 while(1) 里每次循环都调用一次
 * @return 检测到"刚刚被按下"的按键事件；如果没有按键动作，返回 KEY_EVENT_NONE
 */
KeyEvent_t Key_Scan(void)
{
    uint32_t now_tick = HAL_GetTick();

    // ------- 第一步：限流，控制多久才真正扫描一次 -------
    // 如果距离上次扫描还没到20ms，直接返回"无事件"，不做任何电平读取
    // 目的：给按键足够的时间让抖动结束，避免抖动期间反复触发
    if (now_tick - s_last_tick < KEY_SCAN_INTERVAL)
        return KEY_EVENT_NONE;
    // 到了20ms，更新时间戳，准备进行本次扫描
    s_last_tick = now_tick;

    // ------- 第二步：依次检查4个按键 -------
    for (uint8_t i = 0; i < KEY_COUNT; i++)
    {
        // 读取第i个按键当前的电平状态
        // 例如 i=0 时，读的是 GPIOA 的 PIN_0（菜单键）
        GPIO_PinState cur = HAL_GPIO_ReadPin(key_map[i].port, key_map[i].pin);

        // ------- 第三步：边沿检测（核心逻辑）-------
        // 只有"这一次是按下(RESET)，但上一次记录的是没按下(SET)"
        // 才算是一次真正的"刚刚按下"事件
        //
        // 为什么要这样判断，而不是"只要是RSSET就触发"？
        // 因为如果用户手指按住不放，接下来每隔20ms扫描都会读到RSSET，
        // 如果不做这个"变化"判断，一次按键会在按住的1秒内触发50次事件
        // （1000ms / 20ms = 50次），阈值会被疯狂加减，体验很差
        //
        // 加上这个判断后：只有电平"从没按下变成按下"的那一瞬间才返回事件，
        // 之后哪怕一直按住不放，也不会重复触发，必须先松开再按一次才会再触发一次
        if (cur == GPIO_PIN_RESET && last_state[i] == GPIO_PIN_SET)
        {
            // 先更新状态记录，再返回事件
            // （一定要在return之前更新，否则下次调用这个状态还是旧的）
            last_state[i] = cur;
            return key_map[i].event;
        }

        // 不管这次有没有触发事件，都要把"这次读到的电平"存起来，
        // 作为下一次调用时的"上一次状态"参考
        last_state[i] = cur;
    }

    // 4个按键都检查完了，没有任何一个是"刚刚按下"，返回无事件
    return KEY_EVENT_NONE;
}
