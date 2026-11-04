#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "main.h"
#include "stdio.h"

/*
 * 串口 1 的板级封装：printf 输出 + 中断方式接收。
 * 接收用环形缓冲，中断只负责"把字节存进去"，主循环慢慢取，
 * 这样高速输入时不会丢字符，也不占用中断时间。
 */

#define RX_BUF_SIZE     64      /* 环形缓冲深度，2 的幂便于观察下标 */

void    BspUart_Init(void);                 /* 启动第一次中断接收，上电调用一次 */
uint8_t BspUart_GetByte(uint8_t *byte);     /* 有数据返回 1，空返回 0 */

#endif /* __BSP_UART_H */
