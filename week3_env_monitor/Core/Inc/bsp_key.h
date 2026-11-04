#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "main.h"

/*
 * 按键扫描：非阻塞消抖。
 *
 * 接线方式：按键一端接引脚，另一端接 GND；引脚在 CubeMX 里配成 Input + Pull-up。
 * 所以"没按下"读到 1，"按下"读到 0。
 *
 * 换引脚只改下面两个宏，再回 CubeMX 把新引脚配成上拉输入即可。
 */
#define KEY_PORT        GPIOB
#define KEY_PIN         GPIO_PIN_0

uint8_t BspKey_Scan(void);      /* 检测到一次有效按下返回 1，否则返回 0 */

#endif /* __BSP_KEY_H */
