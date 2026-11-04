#ifndef __APP_ADC_H
#define __APP_ADC_H

#include "main.h"

/*
 * ADC 采样 + 滑动平均。
 * 硬件层只负责"读一次电压"，应用层负责"把抖动的数据变平滑"。
 * 两者分开，换通道/换传感器时只改 CubeMX，不动这里的算法。
 */

#define APP_ADC_WIN     8       /* 滑动窗口长度，8 次平均 */
#define APP_ADC_VREF    3300    /* 参考电压 3.3V，单位 mV */
#define APP_ADC_FULL    4095    /* 12 位 ADC 满量程 */

/*
 * 光照百分比的两个端点，按实测值填，不要用 0 和 4095。
 * 这块 LM393 模块是反相的：越亮电压越低，所以"暗"对应大数值。
 * 实测：遮光 2.54~2.76V（约 3150 以上，已经夹到 0%），日常光照 1.37V（约 1700）。
 * 满格点定在 1500（约 1.2V），这样日常光照能到 87%，手电直照才会顶到 100%。
 */
#define APP_ADC_DARK    3100    /* 遮光时的原始值（电压高） */
#define APP_ADC_BRIGHT  1500    /* 强光时的原始值（电压低） */

void     App_AdcInit(void);              /* 上电校准，只调用一次 */
uint16_t App_AdcReadRaw(void);          /* 读一次原始值 0~4095 */
uint16_t App_AdcSmooth(uint16_t raw);   /* 把原始值塞进窗口，返回平均值 */
uint8_t  App_AdcPercent(uint16_t avg);  /* 按实测区间换算成 0~100 的光强百分比 */

#endif /* __APP_ADC_H */
