/**
* @file    sht30.c
 * @brief   SHT30 温湿度传感器驱动实现(阻塞模式)
 *
 * @details
 *  基于 I2C 总线抽象层, 实现 SHT30 单次测量模式下的温湿度读取
 *  发送测量命令后延时 15ms 等待采集完成,读取 6 字节原始数据并换算
 *
 * @version 1.2.0
 * @date    2026-07-17
 */
#include "sht30.h"

/**
 * @brief SHT30 初始化
 *
 * @param ctx   SHT30_Ctx 句柄指针
 * @param bus   I2C_Bus_Ctx 句柄指针
 * @return 1    成功
 * @return 0    失败
 */
uint8_t SHT30_Init(SHT30_Ctx *ctx, I2C_Bus_Ctx *bus)
{
    if (ctx == NULL || bus == NULL) return 0;

    ctx->bus = bus;

    return 1;
}

/**
 * @brief 发送测量命令
 *
 * @param ctx   SHT30_Ctx 句柄指针
 * @return 1    成功
 * @return 0    失败
 */
static uint8_t SHT30_StartMeasure(SHT30_Ctx *ctx)
{
    uint8_t buf[2] = {0x2C, 0x06};

    return I2C_Bus_Write(ctx->bus, SHT30_ADDR, buf, 2);
}

/**
 * @brief 读取 6 字节原始数据
 *
 * @param ctx   SHT30_Ctx 句柄指针
 * @return 1    成功
 * @return 0    失败
 */
static uint8_t SHT30_ReadRaw(SHT30_Ctx *ctx)
{
    return I2C_Bus_Read(ctx->bus, SHT30_ADDR, ctx->buf, 6);
}

/**
 * @brief 进行 CRC 校验
 *
 * @param data  数据
 * @param len   长度
 * @return      计算得到的 CRC-8 校验值
 */
static uint8_t SHT30_CRC(uint8_t *data, uint8_t len)
{
    /*
     * 算法   CRC-8
     * 多项式  0x31
     * 初始值  0xFF
     * 数据   每次校验两个字节
     *
     * CRC-8 计算流程
     * 初始值 crc = 0xFF
     * 对每个字节
     *  crc = crc XOR 当前字节
     *      重复 8 次
     *          if crc 最高位是 1
     *              crc = (crc 左移 1 位) XOR 0x31
     *          else
     *              crc = crc 左移 1 位
     * 对照数据手册的粒子验证 CRC(0xBE, 0xEF) = 0x92
     */
    uint8_t crc = 0xFF;

    for (int i = 0; i < len; i++)
    {
        crc = crc ^ data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = crc << 1;
        }
    }

    return crc;
}

/**
 * @brief 读取温度与湿度
 *
 * @param ctx   SHT30_Ctx 句柄指针
 * @param temp  温度
 * @param humi  湿度
 * @return 1    成功
 * @return 0    失败
 */
uint8_t SHT30_Read(SHT30_Ctx *ctx, float *temp, float *humi)
{
    uint8_t data[2];
    float SRH, ST;

    // 发测量命令
    if (!SHT30_StartMeasure(ctx))   return 0;
    // 延时等待采集完成(高重复精度约需 15ms)
    HAL_Delay(15);
    // 读 6 字节
    if (!SHT30_ReadRaw(ctx))   return 0;

    // buf[0] buf[1] buf[2]     // 温度高字节 温度低字节 温度 CRC
    // buf[3] buf[4] buf[5]     // 湿度高字节 湿度低字节 湿度 CRC
    // ----------CRC 校验----------
    // 温度校验
    data[0] = ctx->buf[0];
    data[1] = ctx->buf[1];
    if (SHT30_CRC(data, 2) != ctx->buf[2])
        return 0;
    // 湿度校验
    data[0] = ctx->buf[3];
    data[1] = ctx->buf[4];
    if (SHT30_CRC(data, 2) != ctx->buf[5])
        return 0;

    // 换算温度
    ST = (ctx->buf[0] << 8) | ctx->buf[1];
    *temp = -45 + 175 * (float)ST / 65535;
    // 换算湿度
    SRH = (ctx->buf[3] << 8) | ctx->buf[4];
    *humi = 100 * (float)SRH / 65535;

    return 1;
}

/**
 * -----------------------------------------------------------------------------
 * CubeMX 配置说明
 * -----------------------------------------------------------------------------
 *
 * Connectivity -> I2C1
 *  I2C Speed Mode : Standard Mode(100 Khz)
 *  其余保持默认
 *
 * 接线说明
 *  PB6     -> SCL(需接 4.7kΩ 上拉至 3.3V)
 *  PB7     -> SDA(需接 4.7kΩ 上拉至 3.3V)
 *  VCC     -> 3.3V
 *  GND     -> GND
 *  ADDR    -> GND 地址 0x44, 8 位格式 0x88
 *
 * 注意事项
 *  SHT30_Init 必须在 I2C_Bus_Init 之后调用
 *  SHT30_Read 内含 15ms 阻塞延时, 调用频率不宜过高
 */