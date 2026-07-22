/**
 * @file    flash_bsp.h
 * @brief   Flash底层读写驱动
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/18
 */
#ifndef FLASH_BSP_H
#define FLASH_BSP_H

#include "main.h"

/* 已废弃：不使用 FLASH_BANK1_END 宏
 * 原因：该宏在 stm32f103xb.h 中是 C8/CB 共用值（对应128KB），
 *       部分 C8T6 实际物理Flash只有64KB，直接使用会指向不存在的Flash区域
 * #define FLASH_USER_PAGE_ADDR (FLASH_BANK1_END - FLASH_PAGE_SIZE + 1)
 */

#define FLASH_USER_PAGE_ADDR 0x0800FC00     // Flash 地址，使用的是最后一页的地址，防止与程序代码冲突

/**
 * @brief 擦除一整页
 *
 * @details STM32 的 Flash 写入前必须先擦除，擦除后的内容全上 0xFF
 *          这是 Flash 硬件特性决定的（不是 HAL 库设计的）
 *          写入操作只能把 bit 从 1 改成 0，不能反过来
 *          所以改数据必须先擦干净
 *
 * @param addr  Flash 地址
 * @return 1    成功
 * @return 0    失败
 */
uint8_t Flash_BSP_ErasePage(uint32_t addr);

/**
 * @brief 按半字（16bit）写入数据
 *
 * @details STM32F1 的 Flash 编程最小单位是半字（16bit = 2 字节）
 *          不能按单字节写，这也是硬件限制
 *
 * @param addr          Flash 地址
 * @param data          写入的数据
 * @param len_halfword  半字个数（每个半字 = 16bit = 2字节，总写入字节数 = len_halfword * 2）
 * @return 1            成功
 * @return 0            失败
 */
uint8_t Flash_BSP_Write(uint32_t addr, uint16_t *data, uint16_t len_halfword);

/**
 * @brief 读取 Flash 数据
 *
 * @details Flash 可以像普通内存一样直接寻址读取，不需要特殊操作
 *
 * @param addr          Flash 地址
 * @param buf           用于接收读取数据的缓冲区指针
 * @param len_halfword  半字个数（每个半字 = 16bit = 2字节，总写入字节数 = len_halfword * 2）
 */
void Flash_BSP_Read(uint32_t addr, uint16_t *buf, uint16_t len_halfword);

#endif
