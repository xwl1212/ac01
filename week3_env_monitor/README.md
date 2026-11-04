# 环境监测终端（STM32F103C8T6）

基于 STM32F103C8T6 的环境光强监测终端。ADC 采集光敏电压，经滑动平均滤波后换算为光照百分比，实时显示在 0.96 寸 OLED 上；支持串口命令查询数据、远程切换显示页，也可用板载/外接按键本地翻页。

## 硬件

| 元件 | 型号 | 说明 |
|---|---|---|
| 主控 | STM32F103C8T6 | Cortex-M3，72MHz，20KB RAM，64KB Flash |
| 显示屏 | SSD1306 0.96" OLED | 128×64，I2C 接口，7 位地址 0x3C |
| 光敏 | LM393 光敏模块 | 4 针（VDD / GND / DO / AO），使用 AO 模拟输出 |
| 调试 | USB-TTL | 115200 8-N-1，用于 printf 输出与命令输入 |

## 引脚连接

| 功能 | 引脚 | 外设 |
|---|---|---|
| 光敏 AO | PA0 | ADC1_IN0 |
| OLED SCL | PB6 | I2C1_SCL |
| OLED SDA | PB7 | I2C1_SDA |
| 串口 TX | PA9 | USART1_TX |
| 串口 RX | PA10 | USART1_RX |
| 按键 | PB0 | GPIO 上拉输入，按下接地 |

## 功能

- **数据采集**：ADC1 单次转换，12 位分辨率，采样时间 55.5 周期
- **数字滤波**：8 点环形窗口滑动平均，抑制光照抖动
- **本地显示**：128×64 OLED，8×16 点阵字库，两页界面切换
- **串口命令**：中断接收 + 环形缓冲，支持查询与远程翻页
- **按键交互**：20ms 非阻塞消抖，按下翻页，与串口命令共用同一入口

## 显示界面

**第 1 页 — 光强监视**

```
ENV MONITOR
LIGHT      87%
1.37V   BRIGHT
[==================    ]
```

**第 2 页 — 原始数据**

```
ENV DATA
RAW 1716
AVG 1720
V  1.37
```

## 串口命令

波特率 115200，发送时需带换行符（`\r\n`）。

| 命令 | 作用 | 返回 |
|---|---|---|
| `GET` | 查询当前数据 | `>> light=87% raw=1716 avg=1720 v=1.37 page=1` |
| `PAGE 1` | 切到光强监视页 | `>> page 1: light monitor` |
| `PAGE 2` | 切到原始数据页 | `>> page 2: raw data` |
| `HELP` | 打印命令列表 | 命令说明 |

未知命令返回 `>> unknown: xxx (type HELP)`。

## 软件结构

采用前后台架构：中断只做最轻的数据搬运，业务逻辑全部在主循环中完成。

```
Core/
├── Inc/
│   ├── oled.h          SSD1306 对外接口
│   ├── oledfont.h      8×16 点阵字库（95 个可见字符）
│   ├── app_adc.h       ADC 采样与滤波接口、标定常量
│   ├── app_cmd.h       命令解析与页号管理
│   ├── bsp_uart.h      串口环形缓冲接口
│   └── bsp_key.h       按键扫描接口与引脚宏
└── Src/
    ├── main.c          初始化、主循环调度
    ├── oled.c          SSD1306 驱动、字符/字符串/数值显示
    ├── app_adc.c       ADC 读取、滑动平均、百分比换算
    ├── app_cmd.c       关键字匹配、命令执行、翻页
    ├── bsp_uart.c      printf 重定向、中断接收、环形缓冲
    ├── bsp_key.c       非阻塞消抖、下降沿检测
    ├── adc.c / i2c.c / usart.c / gpio.c / tim.c    CubeMX 生成的外设初始化
    └── stm32f1xx_it.c  中断服务函数
```

## 关键实现

**ADC 滑动平均（增量式）**

窗口固定 8 点，每次只减最老样本、加最新样本：

```c
sum -= buf[idx];
buf[idx] = val;
sum += val;
idx = (idx + 1) % 8;
avg = sum / 8;
```

复杂度 O(1)，比每次重算 8 个数求和少 7 次加法。

**串口中断 + 环形缓冲**

115200 波特率下一个字符仅占 87μs，主循环无法用轮询及时读取。中断服务函数只把字节写入 64 字节环形缓冲并置位标志，主循环从缓冲中取字节、拼串、比对关键字。输入突发不会丢数据，命令处理耗时不占用中断。

**按键非阻塞消抖**

电平变化后需要稳定 20ms 才承认，且只响应“松开 → 按下”方向：

```c
if (now != s_stable) {
    if (HAL_GetTick() - s_tick >= 20) { ... }
}
```

全程不使用 `HAL_Delay`，不影响主循环刷新节奏。

**ADC 标定**

不同光敏模块的分压方向与范围不一致，通过两个常量标定：

```c
#define APP_ADC_DARK    3100    /* 遮光时的 ADC 值 → 0%   */
#define APP_ADC_BRIGHT  1500    /* 强光时的 ADC 值 → 100% */
```

本模块光照越强输出电压越低，故换算为 `(DARK - raw) * 100 / (DARK - BRIGHT)`。

## 构建

1. STM32CubeMX 打开 `OLED1.ioc`（外设：ADC1_IN0、I2C1、USART1、GPIO），生成代码
2. Keil MDK 打开 `MDK-ARM/OLED1.uvprojx`，编译下载
3. 串口工具 115200 连接，上电后输出 `I2C scan start / found 0x3C / scan done / OLED init done`

外设参数：I2C1 标准模式 100kHz；USART1 115200 8-N-1 且开启全局中断；ADC 非连续转换、软件触发。
