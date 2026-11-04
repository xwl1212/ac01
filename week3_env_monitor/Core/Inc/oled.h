#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/*
 * SSD1306 0.96 寸 OLED（128x64，4 针 IIC）驱动。
 *
 * 坐标系约定：
 *   x     : 0 ~ 127，从左往右，单位是像素列
 *   page  : 0 ~ 7，从上往下，单位是"页"——1 页 = 8 行像素
 *   一个 8x16 字符占 8 列 x 2 页，所以字符的 page 只能取 0/2/4/6
 */

#define OLED_ADDR   0x3C    /* 7 位地址；HAL 接口要的是 8 位，调用时左移一位 */

void OLED_Init(void);
void OLED_Clear(void);
void OLED_DisplayOn(void);                        /* 点亮显示（0xAF） */
void OLED_DisplayOff(void);                       /* 关闭显示（0xAE），进入低功耗前调用 */
void OLED_ShowChar(uint8_t x, uint8_t page, char ch);
void OLED_ShowString(uint8_t x, uint8_t page, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t page, uint32_t num, uint8_t len);
void OLED_ShowBar(uint8_t page, uint8_t width);   /* 整页画一条 8 像素高的进度条 */

#endif /* __OLED_H */
