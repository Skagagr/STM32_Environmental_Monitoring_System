/**
 * @file    param_manager.h
 * @brief   阈值参数管理模块对外接口
 *
 * @details
 *  本模块维护温湿度的报警阈值参数：
 *      - 正常运行阶段：按键调整只修改 RAM 中的阈值副本，不写 Flash，保护 Flash 寿命
 *      - 仅在 PVD 掉电中断触发时调用一次 Param_Save()，将当前阈值以追加写入
 *        的方式记录到 Flash 用户页的下一个空闲位置，避免每次调整阈值都整页擦除重写
 *      - 开机时调用 Param_Init()，顺序扫描 Flash 用户页恢复最后一条有效记录到 RAM
 *        并确定下一次写入地址
 *
 * @note    当前处于裸机开发阶段，模块内部暂不加锁
 *
 * @version 1.0.0
 * @date    2026/7/19
 */
#ifndef PARAM_MANAGER_H
#define PARAM_MANAGER_H

#include <stdint.h>

/* ==================== 默认阈值（无有效Flash记录时使用） ==================== */
/* PARAM = Parameter(参数) DEFAULT = 默认值 TEMP = Temperature(温度) HUMI = Humidity(湿度) */
/* TODO: 以下均为占位示例数值，请根据实际应用场景确认后再使用 */
#define PARAM_DEFAULT_TEMP_HIGH ((int8_t)35)    // 默认温度上限报警阈值，单位 °C
#define PARAM_DEFAULT_TEMP_LOW  ((int8_t)0)     // 默认温度下限报警阈值，单位 °C
#define PARAM_DEFAULT_HUMI_HIGH ((uint8_t)80)   // 默认湿度上限报警阈值，单位 %RH
#define PARAM_DEFAULT_HUMI_LOW  ((uint8_t)20)   // 默认湿度下限报警阈值，单位 %RH

/**
 * @brief 阈值参数逻辑视图（RAM中使用，供 alarm_ctrl / key_handler 等模块读取）
 * @note  这是"逻辑视图"，跟Flash里实际的物理存储布局（含magic/checksum，
 *        每个字段各占一个完整uint16_t）是两回事，物理布局细节封装在
 *        param_manager.c 内部，不在这里出现
 */
typedef struct
{
    int8_t  temp_high;  // temperature high(温度上限) —— 当前生效的温度上限报警阈值，单位 °C
    int8_t  temp_low;   // temperature low(温度下限)  —— 当前生效的温度下限报警阈值，单位 °C
    uint8_t humi_high;  // humidity high(湿度上限)    —— 当前生效的湿度上限报警阈值，单位 %RH
    uint8_t humi_low;   // humidity low(湿度下限)     —— 当前生效的湿度下限报警阈值，单位 %RH
} ThresholdParam_t;

/**
 * @brief 模块初始化，扫描 Flash 用户页恢复最后一条有效阈值记录到 RAM
 * @details 扫描规则：
 *          - 逐条记录顺序向后扫描，magic == 0xFFFF 视为空白，停止扫描
 *          - magic 正确但 checksum 校验失败，视为掉电时写到一半的损坏记录，
 *            丢弃该条，保留上一条已确认有效的记录
 *          - 全部记录均无效（含整页空白）时，使用 PARAM_DEFAULT_* 默认值
 *          本函数还会把第一个空白记录的地址记录为下一次 Param_Save() 的写入地址，
 *          该地址天然会跳过任何中间损坏的记录，无需额外的 +1 运算
 * @return 1 找到并恢复了有效记录
 * @return 0 未找到有效记录，已使用默认值
 */
uint8_t Param_Init(void);

/**
 * @brief 将当前 RAM 中的阈值以追加写入方式保存为一条新记录
 * @details 仅供 PVD 掉电中断服务函数调用一次。若当前页已写满（下一写入地址超出页范围），
 *          会先整页擦除、复位写入地址到页头，再写入这条记录
 * @return 1 写入成功
 * @return 0 写入失败（Flash 操作出错）
 */
uint8_t Param_Save(void);

/**
 * @brief 获取当前 RAM 中阈值参数的只读句柄
 * @return 指向内部阈值副本的常量指针，调用者不应尝试修改其指向内容
 */
const ThresholdParam_t *Param_GetHandle(void);

/**
 * @brief 设置温度上限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的温度上限阈值
 */
void Param_SetTempHigh(int8_t value);

/**
 * @brief 设置温度下限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的温度下限阈值
 */
void Param_SetTempLow(int8_t value);

/**
 * @brief 设置湿度上限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的湿度上限阈值
 */
void Param_SetHumiHigh(uint8_t value);

/**
 * @brief 设置湿度下限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的湿度下限阈值
 */
void Param_SetHumiLow(uint8_t value);

/**
 * @brief 在当前温度上限阈值基础上增减一个偏移量（仅修改RAM，不触碰Flash）
 * @param delta 偏移量，正数增大、负数减小
 * @note  未做上下限裁剪，如果delta过大，理论上可能让temp_high超出
 *        int8_t范围(-128~127)发生环绕；业务上是否需要限定一个更小的
 *        合理范围（比如传感器实际量程），需要你自己确认后再决定要不要加
 */
void Param_AdjustTempHigh(int8_t delta);

/**
 * @brief 在当前湿度上限阈值基础上增减一个偏移量（仅修改RAM，不触碰Flash）
 * @param delta 偏移量，正数增大、负数减小
 * @note  湿度物理意义上只能是0~100%RH，内部已做裁剪，超出范围会被
 *        夹到0或100，不会发生uint8_t下溢/上溢环绕
 */
void Param_AdjustHumiHigh(int8_t delta);

/**
 * @brief 在当前温度下限阈值基础上增减一个偏移量（仅修改RAM，不触碰Flash）
 * @param delta 偏移量，正数增大、负数减小
 * @note  未做上下限裁剪，理由同 Param_AdjustTempHigh
 */
void Param_AdjustTempLow(int8_t delta);

/**
 * @brief 在当前湿度下限阈值基础上增减一个偏移量（仅修改RAM，不触碰Flash）
 * @param delta 偏移量，正数增大、负数减小
 * @note  裁剪到0~100，理由同 Param_AdjustHumiHigh
 */
void Param_AdjustHumiLow(int8_t delta);

#endif