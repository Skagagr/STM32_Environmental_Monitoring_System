/**
 * @file    env_monitor.c
 * @brief   环境监测模块：周期性采集温湿度并刷新OLED显示
 *
 * @details 内部按1秒周期非阻塞刷新，通过EnvMonitor_Update(now_tick)
 *          由主循环驱动；采集到的最新数据可通过EnvMonitor_GetData()
 *          获取，供报警等其他模块使用，模块之间不直接耦合。
 *
 * @version 1.0.0
 * @date    2026/7/22
 */
#include "env_monitor.h"
#include "param_manager.h"

static EnvData_t s_data = {0};
static OLED_Ctx *s_oled_ctx = NULL;
static SHT30_Ctx *s_sht30_ctx = NULL;
static uint32_t s_last_tick = 0;

void EnvMonitor_Init(SHT30_Ctx *sht30_ctx, OLED_Ctx *oled_ctx)
{
    s_sht30_ctx = sht30_ctx;
    s_oled_ctx = oled_ctx;
}

uint8_t EnvMonitor_Update(uint32_t now_tick, ParamField_t field)
{
    if (now_tick - s_last_tick < 1000)
        return 0;
    s_last_tick = now_tick;

    // 每次刷新前都重新取一次句柄，而不是缓存成外层长期持有的变量
    // key_handler按键调阈值会实时改动s_param，每次显示前重新取，
    // 保证OLED上显示的永远是当前最新值，不是开机那一刻的旧值
    const ThresholdParam_t *param = Param_GetHandle();

    SHT30_Read(s_sht30_ctx, &s_data.temp, &s_data.humi);    // 运行时间 16 ms

    OLED_Clear(s_oled_ctx);

    OLED_ShowString(s_oled_ctx, 0, 0, "T:");
    OLED_ShowInt(s_oled_ctx, 16, 0, param->temp_low, 0);
    OLED_ShowString(s_oled_ctx, 40, 0, "~");
    OLED_ShowInt(s_oled_ctx, 56, 0, param->temp_high, 0);

    OLED_ShowString(s_oled_ctx, 0, 16, "H:");
    OLED_ShowNum(s_oled_ctx, 16, 16, param->humi_low, 0);
    OLED_ShowString(s_oled_ctx, 40, 16, "~");
    OLED_ShowNum(s_oled_ctx, 56, 16, param->humi_high, 0);

    OLED_ShowString(s_oled_ctx, 0, 32, "Temp:");
    OLED_ShowFloat(s_oled_ctx, 40, 32, s_data.temp, 2);
    OLED_ShowString(s_oled_ctx, 0, 48, "Humi:");
    OLED_ShowFloat(s_oled_ctx, 40, 48, s_data.humi, 2);

    // 阈值选择反色
    if (field == PARAM_FIELD_TEMP_LOW)
        OLED_InvertRect(s_oled_ctx, 16, 0, 24, 16);
    else if (field == PARAM_FIELD_TEMP_HIGH)
        OLED_InvertRect(s_oled_ctx, 56, 0, 24, 16);
    else if (field == PARAM_FIELD_HUMI_LOW)
        OLED_InvertRect(s_oled_ctx, 16, 16, 24, 16);
    else if (field == PARAM_FIELD_HUMI_HIGH)
        OLED_InvertRect(s_oled_ctx, 56, 16, 24, 16);



    // -------运行时间 306 ms-------
    // 原因 每字节独立一帧发送
    // 刷新一屏总共要发起 8页 × (3条命令 + 128个数据字节) = 8 × 131 = 1048 次独立的 I2C 传输
    // 每次只发2个字节，但每次都要完整走一遍 START→地址+ACK→数据→ACK→STOP 的完整I2C帧
    // -------未来预计把逐字节发送改成一次性打包发送一整页-------
    OLED_Refresh(s_oled_ctx);

    return 1;
}

const EnvData_t *EnvMonitor_GetData(void)
{
    return &s_data;
}