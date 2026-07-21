/**
 * @file    param_manager.c
 * @brief   阈值参数管理模块实现
 *
 * @details
 *   本文件是 param_manager.h 声明接口的具体实现，核心是三件事：
 *     - Flash物理记录布局(ParamRecord_t)与RAM逻辑视图(ThresholdParam_t)
 *       之间的相互转换(Param_BuildRecord / Param_ParseRecord)
 *     - 开机顺序扫描Flash用户页，恢复最后一条有效记录(Param_Init)
 *     - 追加写入新记录，页满则整页擦除重置(Param_Save)
 *
 * @version 1.0.0
 * @date    2026/7/19
 */
#include "param_manager.h"
#include "flash_bsp.h"

// MAGIC：记录有效标志(哨兵值)，用来和Flash擦除后的空白态(0xFFFF)区分开
#define PARAM_RECORD_MAGIC      ((uint16_t)0xA55A)
// HALFWORDS：一条完整记录占用的半字(uint16_t)个数
// = magic(1) + temp_high(1) + temp_low(1) + humi_high(1) + humi_low(1) + checksum(1) = 6
#define PARAM_RECORD_HALFWORDS  (6u)
// BYTES：一条完整记录占用的字节数，由 HALFWORDS 推导而来（1 半字 = 2 字节）
// 目前 = 6 * 2 = 12 字节
#define PARAM_RECORD_BYTES      (PARAM_RECORD_HALFWORDS * 2u)

/**
 * @brief Flash 中一条记录的物理布局
 * @note  每个字段都用 uint16_t（哪怕逻辑上只是 1 字节的阈值）因为
 *        Flash_BSP_Write 是按半字粒度写入的，物理布局不能直接沿用
 *        ThresholdParam_t 里的 int8_t / uint8_t 类型
 */
typedef struct
{
    uint16_t magic;      // 记录有效标志，写入时=PARAM_RECORD_MAGIC，擦除后默认0xFFFF
    uint16_t temp_high;  // 温度上限阈值（低8位有效，高8位写入时补0）
    uint16_t temp_low;   // 温度下限阈值（低8位有效，高8位写入时补0）
    uint16_t humi_high;  // 湿度上限阈值（低8位有效，高8位写入时补0）
    uint16_t humi_low;   // 湿度下限阈值（低8位有效，高8位写入时补0）
    uint16_t checksum;   // 校验和，覆盖magic和4个阈值字段，不含checksum自身
} ParamRecord_t;

// s_ = static 静态/模块私有 前缀，只在本文件内可见
// 当前生效的阈值 RAM 副本：开机由Param_Init()填入一次，之后被Param_SetXxx()修改，
// Param_GetHandle()把它的地址以只读形式提供给外部，Param_Save()读它来组装写入记录
static ThresholdParam_t s_param;
// 下一次Param_Save()该写入的Flash地址：由Param_Init()扫描确定，
// 每次写入成功后往后推PARAM_RECORD_BYTES，推出页范围则被Param_Save()重置回页头
static uint32_t s_next_write_addr;

/**
 * @brief 计算记录的 checksum，覆盖 magic 和 4 个阈值字段，不含 checksum 自身
 * @param[in] rec 待计算校验和的记录，只读
 * @return 计算出的checksum值
 * @details 简单半字求和即可：断电导致的损坏是"顺序写入被打断"，尾部会停留在
 *          擦除态0xFFFF，跟写入时基于完整最终值算出的checksum必然对不上，
 *          这个场景不需要上CRC
 * @note   写入前(Param_BuildRecord)调用它算出checksum存入Flash；读出记录后
 *         (Param_Init)再调一次拿来比对，两处共用同一份实现，不要各自重复计算
 */
static uint16_t Param_CalcChecksum(const ParamRecord_t *rec)
{
    uint16_t sum = rec->magic;
    sum = (uint16_t)(sum + rec->temp_high);
    sum = (uint16_t)(sum + rec->temp_low);
    sum = (uint16_t)(sum + rec->humi_high);
    sum = (uint16_t)(sum + rec->humi_low);   // 修正：原来这里重复写了 humi_high，
                                              // 导致 humi_low 字段完全没被计入校验
    return sum;
}

/**
 * @brief 把RAM中的逻辑阈值组装成待写入Flash的物理记录
 * @param[out] rec   待填充的物理记录，输出参数，函数会写入它的每个字段
 * @param[in]  param 当前RAM中的阈值，输入参数，只读
 * @note  temp_high/temp_low是int8_t（有符号），不能直接赋值给uint16_t字段，
 *        否则编译器会先做符号扩展（比如-5会变成0xFFFB而不是0x00FB）。
 *        这里先转uint8_t把原始字节位模式截出来，再零扩展成uint16_t，
 *        跟"高位补0"的约定保持一致
 */
static void Param_BuildRecord(ParamRecord_t *rec, const ThresholdParam_t *param)
{
    rec->magic     = PARAM_RECORD_MAGIC;
    rec->temp_high = (uint16_t)(uint8_t)param->temp_high;
    rec->temp_low  = (uint16_t)(uint8_t)param->temp_low;
    rec->humi_high = (uint16_t)param->humi_high;  // uint8_t本来就无符号，零扩展没问题
    rec->humi_low  = (uint16_t)param->humi_low;
    rec->checksum  = Param_CalcChecksum(rec);     // 必须放最后：前面字段都填好了才能算
}

/**
 * @brief 把Flash中读出的物理记录解析回RAM逻辑阈值
 * @param[in]  rec   从Flash读出的物理记录（只读，不会被修改）
 * @param[out] param 待填充的RAM阈值（输出参数，函数会写入它的每个字段）
 * @note  只取每个字段的低8位重新解释为int8_t/uint8_t，高8位（写入时已被
 *        零扩展为0）直接丢弃即可
 */
static void Param_ParseRecord(const ParamRecord_t *rec, ThresholdParam_t *param)
{
    param->temp_high = (int8_t)(rec->temp_high & 0xFFu);
    param->temp_low  = (int8_t)(rec->temp_low  & 0xFFu);
    param->humi_high = (uint8_t)(rec->humi_high & 0xFFu);
    param->humi_low  = (uint8_t)(rec->humi_low  & 0xFFu);
}

/**
 * @brief  模块初始化，扫描Flash用户页恢复最后一条有效阈值记录到RAM
 * @details 扫描规则：
 *   -# 逐条记录顺序向后扫描，magic == 0xFFFF 视为空白，停止扫描
 *   -# magic正确但checksum校验失败，视为掉电时写到一半的损坏记录，
 *      丢弃该条，保留上一条已确认有效的记录
 *   -# 全部记录均无效（含整页空白）时，s_param 使用 PARAM_DEFAULT_* 默认值
 *   本函数还会把"第一个空白/损坏记录"的地址记录为下一次 Param_Save() 的
 *   写入地址，该地址天然会跳过任何中间损坏的记录，无需额外的+1运算
 * @return 1 = 找到并恢复了有效记录；0 = 未找到有效记录，已使用默认值
 */
uint8_t Param_Init(void)
{
    uint32_t addr = FLASH_USER_PAGE_ADDR;
    uint32_t last_valid_addr = 0;   // 目前找到的最后一条有效记录的地址
    uint8_t found_valid = 0;        // 是否找到过有效记录，不用addr是否为0来判断，
                                     // "有没有发生过"和"值是多少"是两件独立的事
    ParamRecord_t rec;

    // 循环条件用 addr + 一条记录的字节数 <= 页尾，问的是"完整读一条记录会不会越界"，
    // 而不是单纯判断 addr 本身在不在页内
    while (addr + PARAM_RECORD_BYTES <= FLASH_USER_PAGE_ADDR + FLASH_PAGE_SIZE)
    {
        Flash_BSP_Read(addr, (uint16_t *)&rec, PARAM_RECORD_HALFWORDS);

        if (rec.magic == 0xFFFFu)
        {
            // 空白记录，说明后面都没写过，停止扫描
            break;
        }

        if (rec.checksum == Param_CalcChecksum(&rec))
        {
            // magic和checksum都正确，记为目前最新的有效记录，继续往后扫
            last_valid_addr = addr;
            found_valid = 1;
        }
        else
        {
            // magic正确但checksum不对：写到一半就掉电了，这条记录损坏。
            // 写入是单向顺序的，损坏点之后不可能再有更早写入的有效数据，
            // 直接停止扫描，不做跳过式的继续查找
            break;
        }

        addr += PARAM_RECORD_BYTES;
    }

    // 循环退出时的addr，正好是第一个空白/损坏记录的地址，天然跳过中间的损坏块，
    // 不需要用"有效记录数+1"去反推
    s_next_write_addr = addr;

    if (found_valid)
    {
        // 正常路径：找到了有效记录，读出来解析、提前返回
        Flash_BSP_Read(last_valid_addr, (uint16_t *)&rec, PARAM_RECORD_HALFWORDS);
        Param_ParseRecord(&rec, &s_param);
        return 1;
    }

    // 兜底路径：没找到任何有效记录（含整页空白），使用默认值
    s_param.temp_high = PARAM_DEFAULT_TEMP_HIGH;
    s_param.temp_low  = PARAM_DEFAULT_TEMP_LOW;
    s_param.humi_high = PARAM_DEFAULT_HUMI_HIGH;
    s_param.humi_low  = PARAM_DEFAULT_HUMI_LOW;
    return 0;
}

/**
 * @brief  将当前RAM中的阈值以"追加写入"方式保存为一条新记录
 * @details 仅供PVD掉电中断服务函数调用一次。若当前页已写满(下一写入地址
 *          超出页范围)，会先整页擦除、复位写入地址到页头，再写入这条记录
 * @return 1 = 写入成功；0 = 写入失败（Flash操作出错）
 */
uint8_t Param_Save(void)
{
    ParamRecord_t rec;
    Param_BuildRecord(&rec, &s_param);

    if (s_next_write_addr + PARAM_RECORD_BYTES > FLASH_USER_PAGE_ADDR + FLASH_PAGE_SIZE)
    {
        // 页已写满，整页擦除后从页头重新开始追加
        if (!Flash_BSP_ErasePage(FLASH_USER_PAGE_ADDR))
        {
            return 0;   // 擦除失败立刻返回，不能继续往一片状态不明的Flash上写
        }
        s_next_write_addr = FLASH_USER_PAGE_ADDR;
    }

    if (!Flash_BSP_Write(s_next_write_addr, (uint16_t *)&rec, PARAM_RECORD_HALFWORDS))
    {
        return 0;   // 写入失败同样立刻返回，s_next_write_addr不能被提前推进
    }

    // 只有确认写入成功后，才更新"下次该写哪"，保证这个变量随时对应Flash真实状态
    s_next_write_addr += PARAM_RECORD_BYTES;
    return 1;
}

/**
 * @brief  获取当前RAM中阈值参数的只读句柄
 * @return 指向内部阈值副本的常量指针，调用者不应尝试修改其指向内容
 */
const ThresholdParam_t *Param_GetHandle(void)
{
    return &s_param;
}

/**
 * @brief 设置温度上限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的温度上限阈值
 */
void Param_SetTempHigh(int8_t value)
{
    s_param.temp_high = value;
}

/**
 * @brief 设置温度下限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的温度下限阈值
 */
void Param_SetTempLow(int8_t value)
{
    s_param.temp_low = value;
}

/**
 * @brief 设置湿度上限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的湿度上限阈值
 */
void Param_SetHumiHigh(uint8_t value)
{
    s_param.humi_high = value;
}

/**
 * @brief 设置湿度下限报警阈值（仅修改RAM，不触碰Flash）
 * @param value 新的湿度下限阈值
 */
void Param_SetHumiLow(uint8_t value)
{
    s_param.humi_low = value;
}

void Param_AdjustTempHigh(int8_t delta)
{
    // 直接相加、强转回int8_t——delta通常来自按键，一次幅度不大，
    // 实际使用中溢出概率很低，但严格来说这里没有防护。
    // 如果之后发现有必要限定合理范围（比如传感器实际量程），
    // 可以在这一行加min/max裁剪，写法可以照抄下面Param_AdjustHumiHigh
    // 里裁剪的思路
    s_param.temp_high = (int8_t)(s_param.temp_high + delta);
}

void Param_AdjustHumiHigh(int8_t delta)
{
    // 用int16_t做中间计算，避免delta为负、结果本该是负数时，
    // 直接对uint8_t相减发生下溢环绕（比如80-90在uint8_t下会
    // 变成246这种荒谬值，而不是我们想要的"夹到0"
    int16_t result = (int16_t)s_param.humi_high + delta;

    // 湿度只有0~100这个物理意义上的合理区间，超出的直接夹到边界，
    // 而不是任由它变成一个无意义的数字
    if (result > 100)
    {
        result = 100;
    }
    else if (result < 0)
    {
        result = 0;
    }

    s_param.humi_high = (uint8_t)result;
}

void Param_AdjustTempLow(int8_t delta)
{
    s_param.temp_low = (int8_t)(s_param.temp_low + delta);
}

void Param_AdjustHumiLow(int8_t delta)
{
    int16_t result = (int16_t)s_param.humi_low + delta;

    if (result > 100)
    {
        result = 100;
    }
    else if (result < 0)
    {
        result = 0;
    }

    s_param.humi_low = (uint8_t)result;
}