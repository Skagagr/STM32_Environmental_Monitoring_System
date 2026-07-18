/**
 * @file    flash_bsp.c
 * @brief   Flash底层读写驱动
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/18
 */
#include "flash_bsp.h"

uint8_t Flash_BSP_ErasePage(uint32_t addr)
{
    HAL_FLASH_Unlock();     // 解锁 Flash

    // Flash 擦除结构体定义
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;    // 按页擦除
    eraseInit.PageAddress = addr;                   // 地址
    eraseInit.NbPages = 1;                          // 只擦 1 页

    uint32_t pageError = 0;     // 如果擦除过程中出错，HAL 会把出错的那一页地址放进这个变量里（这里用不上，但函数要求）
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &pageError);   // 擦除函数

    HAL_FLASH_Lock();       // 上锁 Flash

    return (status == HAL_OK) ? 1 : 0;
}

uint8_t Flash_BSP_Write(uint32_t addr, uint16_t *data, uint16_t len_halfword)
{
    HAL_FLASH_Unlock();     // 解锁 FLash

    for (uint16_t i = 0; i < len_halfword; i++)
    {
        // 每个半字占两个字节，所以每次写入两个字节。第 i 个半字相对起始地址要偏移 i * 2 字节
        // 1 字 = 2 半字 = 4 字节。1 字节 = 8 位。所以半字节等于 16 位，data的一个元素就是 16 位
        HAL_StatusTypeDef status =  HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i * 2, data[i]);

        // 如果写入中途失败直接跳出，因为继续写已经没有意义了
        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();   // 上锁 Flash
            return 0;
        }
    }

    HAL_FLASH_Lock();       // 上锁 Flash

    return 1;
}

void Flash_BSP_Read(uint32_t addr, uint16_t *buf, uint16_t len_halfword)
{
    for (uint16_t i = 0; i < len_halfword; i++)
    {
        // __IO 是 HAL 库里定义的一个宏，表示这是易变的、需要每次都从内存重新读取
        // 避免编译器优化时把这次读取跳过、用缓存的旧值
        // 这是在读寄存器或者 Flash 这种硬件相关的地址时的标准写法
        buf[i] =  *(__IO uint16_t *)(addr + i * 2);     // addr + i * 2 每次循环偏移量加 2 字节
    }
}