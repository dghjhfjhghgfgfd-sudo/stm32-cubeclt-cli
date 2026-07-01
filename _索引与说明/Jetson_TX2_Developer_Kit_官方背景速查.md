# Jetson TX2 Developer Kit 官方背景速查

用途：作为 2026 浙江省赛视觉端备查资料，配合 `02_TX2实战填洞_摄像头OpenCV与串口.md` 使用。本文整理 Jetson TX2 Developer Kit 的产品定位、硬件规格、J21 串口、电源指示、软件版本边界和官方文档入口。

## 一、产品定位

NVIDIA Jetson TX2 Developer Kit 发布于 2017 年 3 月，是 NVIDIA 推出的边缘 AI 开发平台。核心定位是在设备本地运行深度学习推理，不依赖云端 GPU，面向机器人、无人机、智能安防、工业视觉等嵌入式 AI 场景。

## 二、核心硬件规格

| 组件 | 规格 |
|---|---|
| SoC | NVIDIA Tegra X2，代号 Parker |
| CPU | 6 核：2x NVIDIA Denver2 64 位 + 4x ARM Cortex-A57 |
| GPU | 256 核 NVIDIA Pascal 架构，1.3 GHz |
| AI 算力 | 1.33 TFLOPS，FP16 |
| 内存 | 8 GB LPDDR4，128-bit，带宽 59.7 GB/s |
| 存储 | 32 GB eMMC 5.1 + Micro SD + SATA |
| 视频编解码 | 4K60 H.265/H.264 硬件编解码 |
| 功耗 | 7.5W 到 15W，可配置 |
| 模块尺寸 | 50 mm x 87 mm |

## 三、P2597 载板接口

- 1x HDMI 2.0，4K60
- 1x USB 3.0 + 1x USB 2.0 Micro AB，OTG/Recovery
- 1x 千兆网口
- 1x Micro SD + 1x SATA
- 2x MIPI CSI-2 摄像头接口
- 1x 40-pin GPIO J21 扩展头，兼容树莓派引脚布局
- 1x M.2 Key E，WiFi/BT
- 1x PCIe x4 mini-PCIe
- 19V DC 电源输入

## 四、J21 40-pin 串口相关引脚

你当前用的是 J21 排针。方向速记：

```text
        靠近散热片/模块，Pin 1 在右列顶部

   左列 GND 侧       右列 J21 侧
   Pin 2  5V         Pin 1  3.3V  <- 白色三角
   Pin 4  5V         Pin 3  SDA
   Pin 6  GND        Pin 5  SCL
   Pin 8  TXD        Pin 7  GPIO
   Pin 10 RXD        Pin 9  GND

        远离散热片
```

串口调试最常用三根线：

- TX2 Pin 8 TXD -> USB-TTL / MCU RX
- TX2 Pin 10 RXD <- USB-TTL / MCU TX
- TX2 Pin 9 或 Pin 6 GND -> 对端 GND

注意：UART 是 3.3V TTL。TX2、MCU、USB-TTL 必须共地。不要把 5V TTL 直接硬接到 TX2 UART。

## 五、软件生态和版本边界

- 系统：Ubuntu 18.04 LTS，ARM64
- 内核：Linux for Tegra，L4T Kernel 4.9
- 最终官方 JetPack 4 版本：JetPack 4.6.6 / Jetson Linux L4T 32.7.6。NVIDIA 官方说明 R32.7.6 with JetPack 4.6.6 marks the final release for Jetson Linux R32 and JetPack 4。

JetPack 常见组件：

| 组件 | 用途 |
|---|---|
| CUDA 10.2 | GPU 并行计算 |
| cuDNN 8.2 | 深度学习加速 |
| TensorRT 8.0 | 推理优化 |
| OpenCV 4.1.1，CUDA | 计算机视觉 |
| GStreamer | 硬件加速多媒体处理 |

比赛工程结论：

- 不要尝试 JetPack 5.x/6.x，TX2 不支持。
- 已跑通的路线是 Python 3.6 + `opencv-python==4.6.0` + USB UVC 摄像头 + `/dev/ttyTHS2` 串口。
- 不要在赛前最后阶段大改系统镜像、apt 源、JetPack 或系统 OpenCV。

## 六、LED 指示灯

| LED | 颜色 | 含义 |
|---|---|---|
| CR1 | 绿 | SoC enabled |
| CR2 | 绿 | carrier board is powered |
| CR3 | 绿 | J18 M.2 Key E connector pin 6 is active |
| CR4 | 绿 | J18 M.2 Key E connector pin 16 is active |
| CR5 | 红 | main power supply is connected and active |
| CR6 | 红 | 12V supply for PCIe/SATA is active |

CR2、CR5、CR6 都亮，只能说明载板已上电、主电源接入且 PCIe/SATA 12V rail 激活；不能直接推出“5V/3.3V rails 全部正常”。它也不能排除 DC 接头松动、瞬时跌落、线材接触不良导致的运行中复位。比赛前仍要做连续数小时摄像头 + OpenCV + 串口运行的 soak 测试。

## 七、生命周期

| 时间 | 事件 |
|---|---|
| 2017-03 | 首次发布 |
| 2018-2022 | JetPack 4.x 持续更新 |
| 2022 | 停产通知，EOL |
| 2023 | 最终采购截止 |

TX2 Developer Kit 已在 NVIDIA 官方生命周期页列为 End of Life。最终 Jetson Linux R32 / JetPack 4 版本是 JetPack 4.6.6 / L4T 32.7.6；JetPack 5.x/6.x/7.x 的官方归档条目不包含 Jetson TX2 系列。

## 八、比赛当天使用边界

- TX2 只做视觉识别、坐标解算、上位机显示或日志。
- 电机、舵机、定时器捕获、ADC、实时闭环都放在 MSPM0/STM32。
- 串口协议优先使用可调试 ASCII 帧；稳定后再换二进制帧。
- 如果题目指定 TI MSPM0 或主要器件，TX2 必须作为可拔掉的增强项，不要让系统基本功能依赖 TX2。

## 九、官方文档入口

| 文档 | 链接 |
|---|---|
| Developer Kit User Guide，DA_09452-005 | https://developer.nvidia.com/downloads/jetson-tx2-developer-kit-user-guide |
| Module Data Sheet | https://developer.nvidia.com/embedded/dlc/jetson-tx2-series-modules-data-sheet |
| OEM Design Guide | https://developer.nvidia.com/embedded/dlc/jetson-tx2-oem-product-design-guide |
| Jetson Linux Dev Guide L4T 32.7.5 | https://docs.nvidia.com/jetson/archives/l4t-archived/l4t-3275/index.html |
| JetPack 文档 | https://docs.nvidia.com/jetson/jetpack/index.html |
| SDK Manager | https://developer.nvidia.com/nvidia-sdk-manager |
| Jetson TX2 论坛 | https://forums.developer.nvidia.com/c/embedded-systems/jetson-tx2/ |
| J21 引脚图，JetsonHacks 非官方速查 | https://jetsonhacks.com/nvidia-jetson-tx2-j21-header-pinout/ |
