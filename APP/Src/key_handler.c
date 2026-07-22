/**
 * @file    key_handler.c
 * @brief
 *
 * @details
 *
 * @version 1.0.0
 * @date    2026/7/22
 */
#include "key_handler.h"
#include "key.h"

static ParamField_t s_param_field = PARAM_FIELD_NONE;

void Key_Update(void)
{
    KeyEvent_t key_event = Key_Scan();

    if (key_event == KEY_EVENT_NONE)
        return;

    switch (key_event)
    {
        case KEY_EVENT_MENU:
            s_param_field++;
            if (s_param_field > PARAM_FIELD_HUMI_HIGH)
                s_param_field = PARAM_FIELD_TEMP_LOW;
            break;

        case KEY_EVENT_ENTER:
            s_param_field = PARAM_FIELD_NONE;
            break;

        case KEY_EVENT_PLUS:
            switch (s_param_field)
            {
                case PARAM_FIELD_TEMP_LOW:  Param_AdjustTempLow(3); break;;
                case PARAM_FIELD_TEMP_HIGH: Param_AdjustTempHigh(3); break;
                case PARAM_FIELD_HUMI_LOW:  Param_AdjustHumiLow(3);  break;
                case PARAM_FIELD_HUMI_HIGH: Param_AdjustHumiHigh(3); break;
                default: break;     // PARAM_FIELD_NONE:未选中任何字段,不响应加减
            }
            break;

        case KEY_EVENT_MINUS:
            switch (s_param_field)
            {
                case PARAM_FIELD_TEMP_LOW:  Param_AdjustTempLow(-3);  break;
                case PARAM_FIELD_TEMP_HIGH: Param_AdjustTempHigh(-3); break;
                case PARAM_FIELD_HUMI_LOW:  Param_AdjustHumiLow(-3);  break;
                case PARAM_FIELD_HUMI_HIGH: Param_AdjustHumiHigh(-3); break;
                default: break;     // PARAM_FIELD_NONE:未选中任何字段,不响应加减
            }
            break;

        default:
            break;
    }
}

ParamField_t KeyHandler_GetSelectedField(void)
{
    return s_param_field;
}
