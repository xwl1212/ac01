#ifndef __APP_CMD_H
#define __APP_CMD_H

#include "main.h"

/*
 * 串口命令解析。
 * 分工：中断只把字节塞进环形缓冲（bsp_uart.c），这里在主循环里拼字符串、比对关键字、执行动作。
 * 命令以回车或换行结束，大小写不敏感。
 *
 *   GET       查当前光照数据
 *   PAGE 1    切到主界面（光强 + 进度条）
 *   PAGE 2    切到数据界面（原始值 / 平均值 / 电压）
 *   HELP      打印命令列表
 */

void Cmd_Init(void);
void Cmd_Process(void);     /* 主循环里轮询调用，不会阻塞 */
void Cmd_NextPage(void);    /* 页号 +1，到第 2 页后回到第 1 页（按键用） */

extern uint8_t g_page;      /* 当前显示页，main.c 的绘制逻辑读它 */

#endif /* __APP_CMD_H */
