/**
 * @file    i2c_bus.c
 * @brief   通用 I2C 总线抽象层实现(阻塞模式)
 * 
 * @details
 *  封装 HAL_I2C_Master_Transmit / Receive,提供统一收发接口
 *  支持多总线实例(I2C1 / I2C2)同时工作
 *  I2C_Bus_Write 内含总线忙超时复位,解决 STM32F103 I2C 连续传输死锁问题
 * 
 * @version 1.3.0
 * @date    2026-07-17
 */
#include "i2c_bus.h"

//------------------------------------初始化------------------------------------

/**
 * @brief 初始化 I2C 总线模块
 * 
 * @details
 *  绑定 HAL I2C 句柄,记录初始化状态
 *  必须在 CubeMX 生成的 MX_I2Cx_Init() 之后调用
 *
 * @param ctx   I2C 总线上下文指针
 * @param hi2c  CubeMX  生成的 HAL I2C 句柄指针
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t I2C_Bus_Init(I2C_Bus_Ctx *ctx, I2C_HandleTypeDef *hi2c)
{
    if (ctx == NULL || hi2c == NULL) return 0;

    ctx->hi2c = hi2c;
    ctx->inited = 1;
    return 1;
}

//------------------------------内部辅助: 总线恢复------------------------------

/**
 * @brief 检测总线 BUSY 标志,超时则强制复位
 *
 * @details
 *  STM32F103 I2C 已知 bug: 通信中途异常中断(如线路瞬断)后,
 *  总线可能卡在 BUSY 状态无法自行恢复。
 *  在每次收发操作前调用此函数,可以在下一次通信时自动检测并恢复。
 *
 * @param ctx   I2C 总线上下文指针
 */
static void I2C_Bus_RecoverIfBusy(I2C_Bus_Ctx *ctx)
{
    uint32_t t = HAL_GetTick();
    while (__HAL_I2C_GET_FLAG(ctx->hi2c, I2C_FLAG_BUSY))
    {
        if (HAL_GetTick() - t > 10)
        {
            HAL_I2C_DeInit(ctx->hi2c);
            HAL_Delay(5);
            HAL_I2C_Init(ctx->hi2c);
            break;
        }
    }
}

//-------------------------------------发送-------------------------------------

/**
 * @brief 向指定设备地址写入数据
 * 
 * @param ctx       I2C 总线上下文指针
 * @param dev_addr  设备 I2C 地址(8位格式)
 * @param pData     待发送数据缓冲区指针
 * @param size      发送字节数
 * @return 1        成功
 * @return 0        发送失败或参数非法
 */
uint8_t I2C_Bus_Write(I2C_Bus_Ctx *ctx, uint8_t dev_addr, const uint8_t *pData, uint16_t size)
{
    if (ctx == NULL || !ctx->inited || pData == NULL || size == 0) return 0;

    I2C_Bus_RecoverIfBusy(ctx);

    // 总线忙检测块
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit
    (
        ctx->hi2c,
        dev_addr,
        (uint8_t *)pData,
        size,
        I2C_BUS_TIMEOUT_MS
    );

    return (ret == HAL_OK) ? 1 : 0;
}

//-------------------------------------接收-------------------------------------

/**
 * @brief 从指定设备地址读取数据
 * 
 * @details
 *  冲 dev_addr 设备直接读取 size 字节到 pData
 * 
 * @param ctx       I2C 总线上下文指针
 * @param dev_addr  设备 I2C 地址(8 位格式)
 * @param pData     接收缓冲区指针
 * @param size      读取字节数
 * @return 1        成功
 * @return 0        读取失败或参数非法
 */
uint8_t I2C_Bus_Read(I2C_Bus_Ctx *ctx, uint8_t dev_addr, uint8_t *pData, uint16_t size)
{
    if (ctx == NULL || !ctx->inited || pData == NULL || size == 0) return 0;

    I2C_Bus_RecoverIfBusy(ctx);
    
    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive
    (
        ctx->hi2c,
        dev_addr,
        pData,
        size,
        I2C_BUS_TIMEOUT_MS
    );

    return (ret == HAL_OK) ? 1 : 0;
}

/**
 * @brief 写寄存器地址后读取数据(先写后读)
 * 
 * @details
 *  先发送 1 字节寄存器地址 reg_addr, 再重复起始条件读取数据
 *  适用于大多数传感器的寄存器读取操作
 * 
 * @param ctx       I2C 总线上下文指针
 * @param dev_addr  设备 I2C 地址(8位格式)
 * @param reg_addr  寄存器地址
 * @param pData     接收缓冲区指针
 * @param size      读取字节数
 * @return 1        成功
 * @return 0        失败或参数非法
 */
uint8_t I2C_Bus_ReadReg(I2C_Bus_Ctx *ctx, uint8_t dev_addr, uint8_t reg_addr, uint8_t *pData, uint16_t size)
{
    if (ctx == NULL || !ctx->inited || pData == NULL || size == 0) return 0;

    I2C_Bus_RecoverIfBusy(ctx);

    // 发送寄存器地址
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit
    (
        ctx->hi2c,
        dev_addr,
        &reg_addr,
        1,
        I2C_BUS_TIMEOUT_MS
    );

    if (ret != HAL_OK) return 0;

    // 读取数据
    ret = HAL_I2C_Master_Receive
    (
        ctx->hi2c,
        dev_addr,
        pData,
        size,
        I2C_BUS_TIMEOUT_MS
    );

    return (ret == HAL_OK) ? 1 : 0;
}

//-----------------------------------设备检测-----------------------------------

/**
 * @brief 检测总线上指定地址的设备是否存在
 * 
 * @details
 *  发送地址后检测 ACK,可用于设备在线检测和地址扫描
 * 
 * @param ctx       I2C 总线上下文指针
 * @param dev_addr  设备 I2C 地址(8位格式)
 * @return 1        成功
 * @return 0        设备不存在或总线错误
 */
uint8_t I2C_Bus_IsDeviceReady(I2C_Bus_Ctx *ctx, uint8_t dev_addr)
{
    if (ctx == NULL || !ctx->inited) return 0;
    
    HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady
    (
        ctx->hi2c,
        dev_addr,
        3,
        I2C_BUS_TIMEOUT_MS
    );

    return (ret == HAL_OK) ? 1 : 0;
}

/**
 * -----------------------------------------------------------------------------
 * CubeMX 配置说明
 * -----------------------------------------------------------------------------
 * 
 * Connectivity -> I2C1
 *  I2C Speed Mode : Standard Mode(100 Khz)
 *      可改 Fast Mode(400 kHz), 需确认上拉电阻和设备支持
 *  其余保持默认
 * 
 * NVIC Settings
 *  阻塞模式无需开启 I2C 中断
 * 
 * 接线说明
 *  PB6 -> I2C1_SCL(需接 4.7kΩ 上拉至 3.3V)
 *  PB7 -> I2C1_SDA(需接 4.7kΩ 上拉至 3.3V)
 * 
 * MX_I2C1_Init 必须加复位代码(STMF103 I2C 上电 bug)
 *  在 HAL_I2C_Init() 之前加入:
 *      __HAL_RCC_I2C1_FORCE_RESET();
 *      HAL_Delay(10);
 *      __HAL_RCC_I2C1_RELEASE_RESET();
 *      HAL_Delay(10);
 * 
 * 注意事项
 *  dev_addr 使用 8 位格式: 7 位地址左移 1 位
 *      例如: 7 位地址 0x3c -> 传入 0x78; 0x3d -> 传入 0x7a
 *  I2C_Bus_Init 必须在 MX_I2C1_Init 之后调用
 *  I2C_Bus_IsDeviceReady 必须在 I2C_Bus_Init 之后调用
 */