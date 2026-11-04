# 环境监测终端（STM32F103C8T6）

基于 STM32F103C8T6 的环境光强监测终端，整条链路自下而上由采集、显示、交互三部分组成。

**采集**：ADC1 对光敏模块 AO 输出的模拟电压进行采样（PA0，12 位分辨率，采样时间 55.5 周期），得到 0~4095 的原始码值；由于光照和传感器输出都存在抖动，原始值先进入 8 点环形窗口做滑动平均，再按实测标定的上下限换算成 0~100% 的光照百分比与对应电压。

**显示**：OLED 使用 SSD1306 控制器（128×64），通过 I2C1 总线进行显示控制，SCL 接 PB6、SDA 接 PB7，从机地址 0x3C。每次传输的首字节为控制字，`0x00` 表示后续是命令、`0x40` 表示后续是显存数据；显存按 8 页 × 128 列组织，一个 8×16 字符占用上下两页共 16 字节，因此字符、字符串、数值、进度条都走同一套“选页 → 选列 → 写数据”的流程。屏幕内容由主循环按时间片刷新，页号可通过串口命令或按键改变。

**交互**：按键采用上拉输入检测，PB0 内部上拉、按键一端接地，松开时为高电平、按下时被拉成低电平；程序检测该低电平，并要求电平稳定 20ms 才承认状态变化，以此实现软件消抖，每识别到一次“松开 → 按下”的下降沿就翻一页。PB0 同时配置成 EXTI0 下降沿中断，空闲时作为 STOP 模式的唤醒源，同一个按键既负责翻页又负责唤醒。串口侧，USART1 以 115200 8N1 工作，接收中断只把字节写入 64 字节环形缓冲，主循环取出后按行累积、比对关键字，执行 `GET` / `PAGE n` / `HELP` 等命令。

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
| 按键 | PB0 | EXTI0 下降沿中断 + 上拉输入，按下接地 |

## 功能

- **数据采集**：ADC1 单次转换，12 位分辨率，采样时间 55.5 周期
- **数字滤波**：8 点环形窗口滑动平均，抑制光照抖动
- **本地显示**：128×64 OLED，8×16 点阵字库，两页界面切换
- **串口命令**：中断接收 + 环形缓冲，支持查询与远程翻页
- **按键交互**：20ms 非阻塞消抖，按下翻页，与串口命令共用同一入口
- **低功耗**：空闲 60s 自动关屏进 STOP（约 20µA），按键下降沿经 EXTI0 唤醒

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
│   ├── bsp_key.h       按键扫描接口与引脚宏
│   └── app_power.h     低功耗休眠 / 唤醒接口
└── Src/
    ├── main.c          初始化、主循环调度
    ├── oled.c          SSD1306 驱动、字符/字符串/数值显示
    ├── app_adc.c       ADC 读取、滑动平均、百分比换算
    ├── app_cmd.c       关键字匹配、命令执行、翻页
    ├── app_power.c     STOP 进出、唤醒后外设重初始化、EXTI 回调
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

## 低功耗：为什么用 PB0 而不是 PA0

**PA0 的方式（Standby + WKUP）走不通**

STM32F103 的 Standby 是功耗最低的模式（约 2µA），但它只能被专用的 WKUP 引脚唤醒，而 F103C8 的 WKUP 是**固定焊死在 PA0** 上的。本工程 PA0 已经分配给 ADC1_IN0，用来采光敏模块的 AO 模拟电压——同一个引脚不能既当模拟输入又当唤醒源，所以 Standby 方案在这块板上直接不可行，除非额外牺牲一个模拟通道。

**为什么选 PB0 + STOP 模式**

STOP 模式可以被任意一根 EXTI 线唤醒，不需要专用 WKUP 引脚。PB0 本来就是翻页按键，把它从“普通上拉输入”改成“EXTI0 下降沿触发”，于是同一个按键同时承担两件事：清醒时按一下翻页，休眠时按一下唤醒。硬件不需要加任何东西。

功耗对比：正常工作约 50mA，STOP 约 20µA，相差约 2500 倍。对这块板子来说已经够用，没必要为了 2µA 的 Standby 去改动 ADC 接线。

**PB0 的配置方式**

1. CubeMX 里把 PB0 设为 `GPIO_Input` + `Pull-up`，再在 GPIO 的 EXTI 页签勾选 `EXTI0`，模式选 `External Interrupt Mode with Falling edge trigger detection`
2. 生成的代码就是 `GPIO_MODE_IT_FALLING` + `GPIO_PULLUP`，并在 NVIC 中使能 `EXTI0_IRQn`
3. 硬件：按键一端接 PB0、一端接 GND。内部上拉保证松开读到高电平，按下被拉成低电平，产生一个下降沿
4. 中断服务函数 `EXTI0_IRQHandler` 调 `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0)`，最终进入 `HAL_GPIO_EXTI_Callback`，回调里只更新空闲计时，不做耗时操作

**休眠与唤醒流程**

```
空闲计时 ≥ 60s
  → OLED 发 0xAE 关显示
  → HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, WFI)
  → 按键按下，PB0 下降沿 → EXTI0 中断唤醒 CPU
  → STOP 关掉了 HSE + PLL，时钟回落 HSI 8MHz
  → SystemClock_Config() 重新配 72MHz
  → I2C / ADC / USART 依次 DeInit + 重新 Init，串口接收中断重新挂起
  → OLED_Init() 重新初始化并清屏
  → 回到主循环，重新采集、显示
```

STOP 模式下 EXTI 线仍然有效，所以下降沿能直接把 CPU 叫醒；这是它比 Standby 更适合本工程的关键点。

几个容易踩的坑，代码里都做了处理：

- **按键还按着时不能睡**：没有新的下降沿，会一直睡下去。进 STOP 前先读一次引脚，低电平就推迟休眠
- **旧的 EXTI 标志要清掉**：否则刚进 STOP 就会被残留事件立刻唤醒，等于没睡
- **唤醒的那次按键不算翻页**：唤醒后先调 `BspKey_Sync()` 把当前电平当成基准，避免手还没松就又翻一页
- **EXTI 没使能就不能睡**：进 STOP 前检查 `EXTI->IMR`，否则只能按复位键恢复
- **调试器会断开**：进 STOP 后 SWD 掉线，重新下载要在 Keil 的 `Debug → Settings` 里把 Connect 改成 `under Reset`，或者按住复位键再点下载。代码里已把 `DBGMCU_CR` 的 `DBG_STOP` 位置 1（宏 `POWER_KEEP_SWD_IN_STOP`），STOP 时调试时钟保持运行，SWD 不会断；正式测功耗前把这个宏改成 0

## 构建

1. STM32CubeMX 打开 `OLED1.ioc`（外设：ADC1_IN0、I2C1、USART1、GPIO），生成代码
2. Keil MDK 打开 `MDK-ARM/OLED1.uvprojx`，编译下载
3. 串口工具 115200 连接，上电后输出 `I2C scan start / found 0x3C / scan done / OLED init done`

外设参数：I2C1 标准模式 100kHz；USART1 115200 8-N-1 且开启全局中断；ADC 非连续转换、软件触发。
