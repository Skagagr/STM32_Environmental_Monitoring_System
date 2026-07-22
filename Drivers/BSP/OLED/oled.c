/**
* @file    oled.c
 * @brief   SSD1315 OLED 驱动实现（128×64，I2C 阻塞模式）
 *
 * @details
 *  所有绘图操作写入内部 GRAM 缓冲区，调用 OLED_Refresh() 后批量上屏。
 *  底层每字节独立一帧发送，与 SSD1315 I2C 时序完全兼容。
 *
 * @version 1.3.0
 * @date    2026-07-15
 */
#include "oled.h"
#include "font.h"
#include <stdio.h>

//--------------------------------私有: 底层发送--------------------------------

/**
 * @brief 发送单条命令(独立 I2C 帧)
 *
 * @details
 *  帧格式: START | addr | 0x00(Co=0, D/C#=0) | cmd | STOP
 *  每条命令独立一帧, 防止 SSD1315 把参数字节误判为新命令
 *
 * @param ctx   OLED 上下文指针
 * @param cmd   命令
 * @return 1    成功
 * @return 0    发送失败或参数非法
 */
static uint8_t oled_cmd(OLED_Ctx *ctx, uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return I2C_Bus_Write(ctx->bus, OLED_I2C_ADDR, buf, 2);
}

/**
 * @brief 发送单字节数据(独立 I2C 帧)
 *
 * @details
 *  帧格式: START | addr | 0x40(Co=0, D/C#=1) | data | STOP
 *
 * @param ctx   OLED 上下文指针
 * @param data  数据
 * @return 1    成功
 * @return 0    发送失败或参数非法
 */
static uint8_t oled_data(OLED_Ctx *ctx, uint8_t data)
{
    uint8_t buf[2] = {0x40, data};
    return I2C_Bus_Write(ctx->bus, OLED_I2C_ADDR, buf, 2);
}

//------------------------------------初始化------------------------------------

/**
 * @brief 初始化 OLED 屏幕
 *
 * @details
 *  发送 SSD1315 初始化命令序列, 清空 GRAM 并刷新屏幕
 *
 * @param ctx   OLED 上下文指针
 * @param bus   已初始化的 I2C 总线上下文指针
 * @return 1    成功
 * @return 0    失败或参数非法
 */
uint8_t OLED_Init(OLED_Ctx *ctx, I2C_Bus_Ctx *bus)
{
    if (ctx == NULL || bus == NULL) return 0;

    ctx->bus = bus;
    ctx->inited = 0;
    memset(ctx->gram, 0, sizeof(ctx->gram));

    HAL_Delay(100);     // 上电稳定延时

    // 逐条发送初始化命令, 每条独立一帧
    #define CMD(c) oled_cmd(ctx, (c))

        CMD(0xAE);          // 关闭显示
        CMD(0xD5);          // 时钟分频
        CMD(0x80);
        CMD(0xA8);          // 多路复用率
        CMD(0x3F);          // 64 行
        CMD(0xD3);          // 显示偏移
        CMD(0x00);
        CMD(0x40);          // 起始行 = 0
        CMD(0x8D);          // 电荷泵
        CMD(0x14);          // 开启
        CMD(0x20);          // 页寻址模式
        CMD(0x02);
        CMD(0xA1);          // 列地址重映射(正向)
        CMD(0xC8);          // COM 扫描方向
        CMD(0xDA);          // COM 引脚配置
        CMD(0x12);
        CMD(0x81);          // 对比度
        CMD(0xCF);
        CMD(0xD9);          // 预充电周期
        CMD(0xF1);
        CMD(0xDB);          // VCOMH 电平
        CMD(0x30);
        CMD(0xA4);          // 显示跟随 GRAM
        CMD(0xA6);          // 正常显示(不反色)
        CMD(0xAF);          // 开启显示

    #undef CMD

    ctx->inited = 1;

    OLED_Clear(ctx);
    OLED_Refresh(ctx);

    return 1;

}

//----------------------------------刷屏  清屏----------------------------------

/**
 * @brief 将 GRAM 缓冲区内容刷新到屏幕
 *
 * @details
 *  遍历 8 个页, 逐页逐字节写入 SSD1315 DDRAM
 *  所有绘图操作完成后必须调用此函数才会实际显示
 *
 * @param ctx   OLED 上下文指针
 * @return 1    成功
 * @return 0    失败或未初始化
 */
uint8_t OLED_Refresh(OLED_Ctx *ctx)
{
    if (ctx == NULL || !ctx->inited) return 0;

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        oled_cmd(ctx, (uint8_t)(0xB0 + page));  // 页地址
        oled_cmd(ctx, 0x10);                    // 列高 4 位 = 0
        oled_cmd(ctx, 0x00);                    // 列低 4 位 = 0
        for (uint8_t col = 0; col < OLED_WIDTH; col++)
            oled_data(ctx, ctx->gram[page][col]);
    }

    return 1;
}

/**
 * @brief 清空 GRAM 缓冲区(不立即刷屏)
 *
 * @details
 *  将内部GRAM 全部置 0, 需配合 OLED_Refresh() 才会清空屏幕显示
 *
 * @param ctx   OLED 上下文指针
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_Clear(OLED_Ctx *ctx)
{
    if (ctx == NULL) return 0;

    memset(ctx->gram, 0, sizeof(ctx->gram));

    return 1;
}

//-----------------------------------屏幕控制-----------------------------------

/**
 * @brief 设置屏幕亮度(对比度)
 *
 * @param ctx       OLED 上下文指针
 * @param contrast  亮度值, 0x00(最暗) ~ 0xFF(最亮), 默认 0xCF
 * @return 1        成功
 * @return 0        失败
 */
uint8_t OLED_SetContrast(OLED_Ctx *ctx, uint8_t contrast)
{
    if (ctx == NULL || !ctx->inited) return 0;

    oled_cmd(ctx, 0x81);

    return oled_cmd(ctx, contrast);
}

/**
 * @brief 开启 / 关闭显示
 *
 * @param ctx       OLED 上下文指针
 * @param enable    1=开显示, 0=关显示(不断电, 保留 GRAM 内容)
 * @return 1        成功
 * @return 0        失败
 */
uint8_t OLED_SetDisplay(OLED_Ctx *ctx, uint8_t enable)
{
    if (ctx == NULL || !ctx->inited) return 0;

    return oled_cmd(ctx, enable ? 0xAF : 0xAE);
}

//-------------------------------------绘图-------------------------------------

/**
 * @brief 在指定坐标画一个像素点
 *
 * @param ctx   OLED 上下文指针
 * @param x     列坐标, 0~127
 * @param y     行坐标, 0~63
 * @param color 1=点亮, 0=熄灭
 * @return 1    成功
 * @return 0    参数越界或非法
 */
uint8_t OLED_DrawPixel(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t color)
{
    if (ctx == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) return 0;

    uint8_t page = y / 8;
    uint8_t bit = y % 8;

    if (color)
        ctx->gram[page][x] |= (uint8_t)(1u << bit);
    else
        ctx->gram[page][x] &= ~(uint8_t)(1u << bit);

    return 1;
}

/**
 * @brief 画水平线
 *
 * @param ctx   OLED 上下文指针
 * @param x     起始列, 0~127
 * @param y     行坐标, 0~63
 * @param len   线长(像素数)
 * @param color 1=点亮, 0=熄灭
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_DrawHLine(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t len, uint8_t color)
{
    if (ctx == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) return 0;

    for (uint8_t i = 0; i < len && (x + i) < OLED_WIDTH; i++)
        OLED_DrawPixel(ctx, (uint8_t)(x + i), y, color);

    return 1;
}

/**
 * @brief 画垂直线
 *
 * @param ctx   OLED 上下文指针
 * @param x     列坐标, 0~127
 * @param y     起始行, 0~63
 * @param len   线长(像素数)
 * @param color 1=点亮,0=熄灭
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_DrawVLine(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t len, uint8_t color)
{
    if (ctx == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) return 0;

    for (uint8_t i = 0; i < len && (y + i) < OLED_HEIGHT; i++)
        OLED_DrawPixel(ctx, x, (uint8_t)(y + i), color);

    return 1;
}

/**
 * @brief 画矩形边框
 *
 * @param ctx   OLED 上下文指针
 * @param x     左上角列, 0~127
 * @param y     左上角行, 0~63
 * @param w     宽度(像素)
 * @param h     高度(像素)
 * @param color 1=点亮, 0=熄灭
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_DrawRect(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (ctx == NULL) return 0;

    OLED_DrawHLine(ctx, x, y, w, color);
    OLED_DrawHLine(ctx, x, (uint8_t)(y + h - 1), w, color);
    OLED_DrawVLine(ctx, x, y, h, color);
    OLED_DrawVLine(ctx, (uint8_t)(x + w - 1), y, h, color);

    return 1;
}

/**
 * @brief 画填充矩形
 *
 * @param ctx   OLED 上下文指针
 * @param x     左上角列，0~127
 * @param y     左上角行，0~63
 * @param w     宽度（像素）
 * @param h     高度（像素）
 * @param color 1=点亮，0=熄灭
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_FillRect(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (ctx == NULL) return 0;

    for (uint8_t row = 0; row < h && (y + row) < OLED_HEIGHT; row++)
        OLED_DrawHLine(ctx, x, (uint8_t)(y + row), w, color);

    return 1;
}

//-----------------------------ASCII 字符显示(8x16)-----------------------------

/**
 * @brief 显示单个 ASCII 字符
 *
 * @param ctx   OLED 上下文指针
 * @param x     列坐标, 0~127
 * @param y     行坐标, 0~63, 建议为 8 的倍数
 * @param ch    ASII 字符(0x20 ~ 0x7E)
 * @return 1    成功
 * @return 0    参数非法或字符超范围
 */
uint8_t OLED_ShowChar(OLED_Ctx *ctx, uint8_t x, uint8_t y, char ch)
{
    if (ctx == NULL || !ctx->inited || ch < 0x20 || ch > 0x7E || x >= OLED_WIDTH)
        return 0;

    uint8_t idx = (uint8_t)(ch - 0x20);
    uint8_t page = y / 8;
    uint8_t boff = y % 8;
    const uint8_t *font = g_font_ascii_8x16[idx];

    // 字模格式: 行优先, 高位在左
    // font[0..7] = 上半部分每行, font[8..15] = 下半部分每行
    // 转换为列优先写入 GRAM
    for (uint8_t col = 0; col < 8 && (x + col) < OLED_WIDTH; col++)
    {
        uint16_t col_data = 0;
        for (uint8_t row = 0; row < 8; row++)
        {
            if (font[row] & (0x80 >> col))
                col_data |= (uint16_t)(1u << row);
            if (font[row + 8] & (0x80 >> col))
                col_data |= (uint16_t)(1u << (row + 8));
        }
        col_data <<= boff;
        if (page < OLED_PAGES)
            ctx->gram[page][x + col] |= (uint8_t)(col_data & 0xFF);
        if ((page+1) < OLED_PAGES)
            ctx->gram[page + 1][x + col] |= (uint8_t)(col_data >> 8);
    }

    return 1;
}

/**
 * @brief 显示 ASCII 字符串(8×16)
 *
 * @details
 *  每字符宽 8 像素, 超出屏幕宽度自动截断
 *
 * @param ctx   OLED 上下文指针
 * @param x     起始列坐标
 * @param y     起始行坐标
 * @param str   以 '\0' 结尾的字符串指针
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_ShowString(OLED_Ctx *ctx, uint8_t x, uint8_t y, const char *str)
{
    if (ctx == NULL || str == NULL) return 0;

    uint8_t cx = x;

    while (*str)
    {
        if (cx + 8 > OLED_WIDTH) break;
        OLED_ShowChar(ctx, cx, y, *str++);
        cx += 8;
    }

    return 1;
}

//-----------------------------------数值显示-----------------------------------

/**
 * @brief 显示无符号整数
 *
 * @param ctx   OLED 上下文指针
 * @param x     起始列坐标
 * @param y     起始行坐标
 * @param num   待显示的无符号整数
 * @param width 显示宽度（位数），0 表示自动宽度
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_ShowNum(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint32_t num, uint8_t width)
{
    if (ctx == NULL) return 0;

    char buf[12];

    if (width == 0)
        snprintf(buf, sizeof(buf), "%lu",  (unsigned long)num);
    else
        snprintf(buf, sizeof(buf), "%*lu", width, (unsigned long)num);

    return OLED_ShowString(ctx, x, y, buf);
}

/**
 * @brief 显示有符号整数
 *
 * @param oled.cOLED 上下文指针
 * @param x     起始列坐标
 * @param y     起始行坐标
 * @param num   待显示的有符号整数
 * @param width 显示宽度（含符号位），0 表示自动宽度
 * @return 1    成功
 * @return 0    参数非法
 */
uint8_t OLED_ShowInt(OLED_Ctx *ctx, uint8_t x, uint8_t y, int32_t num, uint8_t width)
{
    if (ctx == NULL) return 0;

    char buf[13];

    if (width == 0)
        snprintf(buf, sizeof(buf), "%ld",  (long)num);
    else
        snprintf(buf, sizeof(buf), "%*ld", width, (long)num);

    return OLED_ShowString(ctx, x, y, buf);
}

/**
 * @brief 显示浮点数
 *
 * @details
 *  依赖 printf 浮点支持，CMakeLists.txt 需加：
 *  target_link_options(${PROJECT_NAME}.elf PRIVATE -u _printf_float)
 *
 * @param ctx       OLED 上下文指针
 * @param x         起始列坐标
 * @param y         起始行坐标
 * @param num       待显示的浮点数
 * @param decimal   小数位数，0~4
 * @return 1        成功
 * @return 0        参数非法
 */
uint8_t OLED_ShowFloat(OLED_Ctx *ctx, uint8_t x, uint8_t y, float num, uint8_t decimal)
{
    if (ctx == NULL || decimal > 4) return 0;

    char buf[20];

    snprintf(buf, sizeof(buf), "%.*f", decimal, (double)num);

    return OLED_ShowString(ctx, x, y, buf);
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
 *  PB6 -> SCL(需接 4.7kΩ 上拉至 3.3V)
 *  PB7 -> SDA(需接 4.7kΩ 上拉至 3.3V)
 *  VCC -> 3.3V
 *  GND -> GND
 *
 *  SSD1315 I2C 地址
 *      SA0 接 GND -> 0x3C(8 位格式 0x78)默认
 *      SA0 接 VCC -> 0x3D(8 位格式 0x7A)需修改 oled.h 中 OLED_I2C_ADDR 为 0x7A
 *
 * MX_I2C1_Init 必须加复位代码(STMF103 I2C 上电 bug)
 *  在 HAL_I2C_Init() 之前加入:
 *      __HAL_RCC_I2C1_FORCE_RESET();
 *      HAL_Delay(10);
 *      __HAL_RCC_I2C1_RELEASE_RESET();
 *      HAL_Delay(10);
 *
 * CMakeLists.txt(浮点支持 sprintf)
 *  target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
 *      -u _printf_float        # ← 添加这行，开启 sprintf 浮点支持
 *  )
 *
 * 常见问题异常排查
 *  全屏雪花    -> 初始化命令未送达, 检查 I2C 地址和接线
 *  全黑无显示  -> I2C 通信失败, I2C_Bus_IsDeviceReady 确认设备在线
 *  上下颠倒    -> 将 0xA1 改 0xA0, 0xC8 改 0xC0
 *  字符旋转    -> OLED_ShowChar 中行列转换方向有误
 */