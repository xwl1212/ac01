#ifndef __APP_POWER_H
#define __APP_POWER_H

#include "main.h"

/*
 * 低功耗管理：空闲 1 分钟后进 STOP 模式，按键（PB0 / EXTI0）唤醒。
 *
 * 为什么是 STOP 而不是 Standby：
 *   Standby 功耗最低（约 2µA），但它只能被 WKUP 引脚（PA0）唤醒。
 *   本工程的 PA0 已经给 ADC1_IN0 采集光敏电压了，占用同一个脚，
 *   所以 Standby 用不了。STOP 能被任意 EXTI 线唤醒，功耗约 20µA，
 *   相比正常工作时的 50mA 已经足够省电，因此选 STOP + PB0/EXTI0。
 */

#define POWER_IDLE_TIMEOUT_MS   60000U   /* 60s 没有操作就休眠 */

void    Power_Init(void);            /* 初始化：开 PWR 时钟、记下活跃时刻 */
void    Power_TouchActivity(void);   /* 有操作就调一下，重置空闲计时 */
uint8_t Power_Task(void);            /* 主循环里调用；睡了一觉醒来时返回 1 */

#endif /* __APP_POWER_H */
