#include "app_adc.h"
#include "adc.h"

/* hadc1 由 CubeMX 生成在 adc.c 里，adc.h 中有 extern 声明 */

/*
 * 滑动窗口：保存最近 8 次原始值，返回它们的平均值。
 * 用 s_sum 累加而不是每次把 8 个数重新加一遍，是为了少做无用功。
 * 环形索引 s_idx 走到头就回 0，所以永远只覆盖最老的那个数。
 */
static uint16_t s_win[APP_ADC_WIN];
static uint32_t s_sum;
static uint8_t  s_idx;
static uint8_t  s_cnt;

void App_AdcInit(void)
{
  /* F1 系列的 ADC 必须先校准，否则零点会偏几十个码值 */
  HAL_ADCEx_Calibration_Start(&hadc1);
}

uint16_t App_AdcReadRaw(void)
{
  uint16_t value;

  HAL_ADC_Start(&hadc1);                                  /* 启动一次转换 */
  if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)    /* 等转换完成，10ms 超时 */
  {
    HAL_ADC_Stop(&hadc1);
    return 0;
  }
  value = (uint16_t)HAL_ADC_GetValue(&hadc1);             /* 取 0~4095 的结果 */
  HAL_ADC_Stop(&hadc1);
  return value;
}

uint16_t App_AdcSmooth(uint16_t raw)
{
  if (s_cnt < APP_ADC_WIN)
  {
    /* 窗口还没填满，直接往里放，平均值按已有样本算 */
    s_win[s_idx] = raw;
    s_sum += raw;
    s_cnt++;
  }
  else
  {
    /* 窗口满了，用新的替换最老的：先减老值，再加新值 */
    s_sum -= s_win[s_idx];
    s_win[s_idx] = raw;
    s_sum += raw;
  }

  s_idx++;
  if (s_idx >= APP_ADC_WIN)
  {
    s_idx = 0;
  }

  return (uint16_t)(s_sum / s_cnt);
}

/*
 * 把原始值换算成 0~100 的百分比。
 * 注意方向：这块模块越亮电压越低，所以是 DARK - raw 而不是 raw - BRIGHT。
 * 两端做了夹取，保证遮光再严也不会出现负数、强光再猛也不会超过 100。
 */
uint8_t App_AdcPercent(uint16_t avg)
{
  if (avg >= APP_ADC_DARK)
  {
    return 0;
  }
  if (avg <= APP_ADC_BRIGHT)
  {
    return 100;
  }

  return (uint8_t)(((uint32_t)(APP_ADC_DARK - avg) * 100) /
                   (APP_ADC_DARK - APP_ADC_BRIGHT));
}
