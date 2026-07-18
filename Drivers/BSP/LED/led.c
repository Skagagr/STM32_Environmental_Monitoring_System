/**
 * @file    led.c
 * @brief   LED 驱动(报警指示)
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/17
 */
#include "led.h"

/**
 * @brief 打开 LED
 */
void LED_On(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

/**
 * @brief 关闭 LED
 */
void LED_Off(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

/**
 * @brief 翻转 LED
 */
void LED_Toggle(void)
{
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
}