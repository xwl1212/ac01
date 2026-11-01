# STM32 嵌入式项目集

基于 STM32F103C8T6 的嵌入式项目仓库，包含两个独立可演示的工程，覆盖 GPIO 与外部中断、串口通信、定时器与 PWM 输出。

## 项目索引

| 项目 | 功能概述 | 关键技术 | 演示 |
|---|---|---|---|
| [week1_gpio_uart](./week1_gpio_uart/README.md) | 按键中断 + 串口回显 | EXTI 外部中断、USART 接收中断、非阻塞软件定时 | [Release v1.0-week1](https://github.com/xwl1212/ac01/releases/tag/v1.0-week1) |
| [week2_pwm_uart](./week2_pwm_uart/README.md) | PWM 调光 + 串口命令控制 | TIM2 硬件 PWM、环形缓冲、命令行解析、模式状态机 | [demo.mp4](./week2_pwm_uart/demo.mp4) |

## week1_gpio_uart

- 芯片 STM32F103C8T6 @ 72MHz
- 按键外部中断（EXTI0）与串口接收中断双入口
- 中断服务只置标志位，业务逻辑全部在主循环执行
- 指示灯延时采用 `HAL_GetTick()` 时间戳比较，不使用阻塞延时
- USART1 115200 8N1 字符回显

## week2_pwm_uart

- TIM2_CH2 输出 1kHz PWM，1000 级亮度分辨率，驱动共阳极 LED
- 串口命令实时切换工作模式：`SET n`（固定亮度 0~100）、`BREATH`（呼吸）、`STOP`（全灭）
- 环形缓冲解耦串口中断与主循环，突发数据不丢字节
- 按行累积解析命令，前缀匹配 + 数值范围校验，非法命令回 `ERR unknown`
- PWM 全灭使用 `CCR = ARR + 1`，消除 0.1% 占空比残留

## 开发环境

| 项 | 版本 |
|---|---|
| STM32CubeMX | 6.14.1（F1 固件包 V1.8.6） |
| Keil MDK | 5.36（ARMCC V5.06 update 7） |
| 开发板 | 野火小智 STM32F103C8T6 |
| 下载器 | CMSIS-DAP |
| 串口工具 | 115200 / 8N1 |

## 仓库结构

```
ac01/
├── week1_gpio_uart/     按键 + 串口中断
│   ├── Core/
│   ├── week_1_1.ioc
│   └── README.md
├── week2_pwm_uart/      PWM 调光 + 命令解析
│   ├── Core/
│   ├── week_1_1.ioc
│   ├── demo.mp4
│   └── README.md
└── README.md
```

每个工程目录只保留 `Core/` 与 CubeMX 工程文件，HAL 驱动与编译中间文件不纳入版本管理，克隆后可直接用 CubeMX 重新生成或导入现有 IDE 工程编译。
