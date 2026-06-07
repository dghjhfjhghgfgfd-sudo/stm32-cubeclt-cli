# 2026 浙江省赛 GitHub 解法雷达

> 目标：不是收藏链接，而是把开源项目压缩成比赛当天可调用的方案库。题目一出，先按题型命中路线，再决定主控、模块、代码骨架和第一天可交付版本。

## 总体判断

浙江专科/高职高专赛道优先选择“模块化、可调试、可第一天跑通”的路线。当前 GitHub 本地库里最有价值的不是完整照搬某个作品，而是抽取这些稳定部件：

- 小车：MSPM0G3507 + 编码器 + IMU + 灰度/ToF + 状态机 + PID。
- 测量：ADC/DMA 采样 + FFT/Goertzel + RMS/功率/频率/相位计算 + OLED/串口显示。
- 视觉：OpenMV/MaixCam/低成本 Linux 板运行 OpenCV，主控只收坐标和状态。
- 声源/超声：发声端固定频率，接收端比较器/ADC 捕获到达时间，TDOA 或超声测距。
- 执行控制：云台/舵机/二维平台用 PID 或分段动作表，先闭环可用，再追求速度和精度。

## 2024-2026 新近 GitHub 优先补充

> 这一段优先级高于老项目。老项目只用于补底层算法，新项目更贴近当前 MSPM0G3507、SysConfig、Keil/CCS、OpenMV/MaixCam 的实际环境。

| 优先级 | 仓库 | 方向 | 为什么有用 | 比赛当天怎么用 |
|---|---|---|---|---|
| S | `MadGodBob/2025-Electronic-Design-Competition-E` | 2025 E 简易自行瞄准装置 | MSPM0G3507 + OPENMV4 H7 PLUS + 闭环步进电机 + JY901S + 12 路灰度；README 明确给出主体思路和供电/串口坑；项目星标较高，实战信息密度高 | 如果赛题出现“瞄准、云台、视觉目标、车载打点”，优先采用它的系统分工：M0 管巡线/电机/二维云台，OpenMV 只发矩阵坐标 |
| S | `abcuer/2025-NUEDC-E-Ti_CAR` | 2025 E 自瞄云台小车 | 与上一个同题但工程结构更接近 2024 H 小车模板，含 `App/Bsp/Control/Hardware/User/keil/src/ti`，适合快速拆 C 工程 | 适合做“底盘巡线 + 云台角度接收 + 任务状态机”的快速版 |
| S | `IllusionMZX/NEU-EEContest2025-SignalDev` | 2025 MSPM0G3507 简易信号分析仪 | 双 ADC + DMA、比较器、VREF、OPA、OLED、蓝牙；频率测量分低/中/高三段：计数、捕获、FFT；更像今年能直接复用的测量模板 | 如果赛题是“信号分析、频率/幅值/波形识别、音频识别”，优先用它做测量主线 |
| A | `danshoujieyi/TI-MSPM0G3507` | MSPM0G3507 工程模板 | 2025 后的 TI-MSPM0G3507 SDK 工程模板，含裸机、FreeRTOS、RT-Thread；适合解决工程搭建问题 | 比赛前备用；比赛当天不建议上 RTOS，除非题目任务并发特别明显 |
| A | `Kuriharamio/Electric-Race-Control` | 控制题备赛代码 | MSPM0G3507 控制题模块化 C 工程，适合抽 PID、状态机、接口组织 | 如果控制题不是小车而是平台/摆杆/云台，可参考其模块化结构 |
| A | `xieyangyingshutong/25xiaoche` | 2025 小车 | 使用 MSPM0G3507 + Keil5 + SysConfig，实现 2025 小车大部分功能 | 作为小车 B 方案，和 `abcuer/JamieK32` 互相对照 |
| A | `superbusycool/Ti_MSPM0G3507_Car` | MSPM0G3507 小车/扩展板 | 有 LED 屏、MPU6050、中断读编码器、PCB 扩展板信息；作者说明巡线效果一般，但驱动有参考价值 | 抽 MPU6050、编码器中断、扩展板接口，不直接拿巡线算法 |
| A | `woai66/SeekFree_MSPM0G3507_Opensource_Library` | MSPM0G3507 开源库 | 偏通用库，适合缺某个驱动时查接口写法 | 作为驱动备件库 |
| B | `D-Zed-529/mspm0g3507-car-control` | 2024 小车，2026 上传 | 湖北省一等奖小车代码，但公开时间晚、星标少，需要先验结构 | 有时间再看，可能有可取参数和结构 |
| B | `pjuehui/2025nuedc` | STM32F4 + MSPM0 备赛/PCB | 2026 上传的个人硬盘整理项目，可能有 PCB 和备赛代码 | 作为备件仓，不作为主线 |

## 新项目蒸馏结论

### 自瞄/云台/视觉车

2025 E 题相关仓库比老的 2019/2023 云台项目更值得优先参考。推荐路线：

1. 主控：MSPM0G3507。
2. 视觉：OpenMV 或 MaixCam 独立处理，只输出目标坐标/偏差/是否识别到目标。
3. 云台：优先闭环步进电机，其次 360 度舵机；普通 270 度舵机不要作为主方案。
4. 姿态：JY901S 单次回传，避免连续串口中断冲突。
5. 串口分配：视觉一个 UART，JY901S 一个 UART，两个闭环步进最好各占一个发送通道；不要强行一根 TX 控两个电机。
6. 供电：云台/瞄准系统单独电池，和 M0 控制系统共地。

可直接套用的比赛策略：

- M0 只做实时控制：巡线、电机、编码器、灰度、云台动作。
- 视觉板只做识别：找黑色色块、白色色块、圆心/矩阵坐标，然后串口发给 M0。
- 云台控制用“前馈角度 + PID 微调 + 卡尔曼/滑动滤波”，不要只靠视觉闭环硬追。

### MSPM0G3507 信号分析/测量

`IllusionMZX/NEU-EEContest2025-SignalDev` 是目前新近资源里测量方向最值得吸收的一个。它的价值不是“题目相同”，而是工程骨架正好覆盖常见仪器题：

- 输入信号 60mV-1000mV，经反相放大和 1.65V 直流偏置进入 ADC。
- 内部 OPA、VREF、COMP 都用起来，适合 TI 杯强调 MSPM0 片上模拟外设的风格。
- 双 ADC + DMA：一路主信号 1024 点，一路音频 4096 点。
- 频率测量三段式：
  - 1Hz-1kHz：门控计数。
  - 1kHz-10kHz：定时器捕获测周期。
  - 10kHz-100kHz：FFT 找峰值。
- OLED 显示频率、峰峰值、波形类型、音频识别结果。

如果今年出现信号/仪器题，第一天应该先做：

1. 模拟输入保护和偏置。
2. ADC DMA 固定采样。
3. 峰峰值、RMS、频率显示。
4. 串口/蓝牙输出调试数据。

第二天再做：

1. FFT 频谱。
2. 波形分类。
3. 校准曲线。
4. 误差表和报告数据。

## 旧项目降级规则

下面这些仍然有用，但不作为第一优先：

- `framist_NUEDC2022-E`：声源定位算法和报告可用，工程偏老，硬件调理风险高。
- `VxTeemo_NUEDC2019-D`、`hadesczq_Signal-distortion-measuring-device`：只看 FFT、菜单、ADC 思路。
- `Charmve_BallPlate`、`jark006_Ball_And_Plate_Balance_control_system`：只看 PID/控制理论，不做比赛主方案。
- `dlphay_m430_dcdc_2015` 等老电源项目：只做拓扑和保护思路参考。

## S 级：比赛当天优先打开

| 题型 | 仓库 | 当天用途 | 关键文件/模块 | 风险 |
|---|---|---|---|---|
| 自动小车/避障/巡线 | `abcuer_2024-NUEDC-H-TI_CAR` | 轻量小车模板，适合快速移植到 MSPM0G3507 | `Control\pid.c`, `Control\track.c`, `Control\angle.c`, `Control\dist.c`, `Hardware\motor.c`, `Hardware\encoder.c`, `Hardware\gray.c`, `Hardware\jy901s.c`, `App\task.c`, `Bsp\uart.c` | 针对 2024 H 赛道，参数和动作表必须现场重调 |
| 自动避障小车/视觉小车 | `JamieK32_ti-contest-2025` | 完整高质量小车工程，适合拆驱动和架构 | `2025K-Code\mspm0g3507\custom_src\application\control`, `drivers\sensors\encoder`, `drivers\sensors\gray_detect`, `drivers\sensors\vl53l1x`, `drivers\communication\maix_cam.c`, `maixcam_project\main.py`, `maixcam_project\uart.py` | 工程较大，比赛当天不要整体硬搬，优先摘驱动/协议/状态机 |
| 单相功率/测量仪器 | `Sh4peshifting_2024-nuedc-mspm0` | MSPM0 ADC/DMA/FFT/显示模板，适合测电压电流、RMS、功率因数、谐波 | `mspm0-proj\adc_data_proc\fft.c`, `adc_data_conv.c`, `screen\disp.c`, `timer\timer.c`, `empty.c` | 模拟前端比代码更关键，必须提前准备电压/电流隔离和量程保护 |
| 调制识别/参数估计 | `outline1988_h743iit6_modulation_recognition` | 高端信号处理参考，适合 AM/FM/频谱/参数估计题 | `CoreCpp\Src\adc_capture.cpp`, `sigvector.cpp`, `sigvector_am.cpp`, `sigvector_fm.cpp`, `Drivers2\Src\fft.c`, `AD9834.c`, `AD9854.c`, `AD9910.c` | H743 工程复杂，专科赛不建议作为唯一主线；适合借算法 |
| 信号分离/频谱分析 | `wu854065020_signal_separator` | STM32F4 ADC + FFT + DDS + UI 参考 | `User\Src\sample.c`, `myfft.c`, `calculate.c`, `ad9833.c`, `Core\Src\adc.c`, `dma.c` | 硬件平台不是 MSPM0，移植要重写底层采样 |
| 视觉坐标识别/激光点 | `DBinK_2023-NUEDC-E-OpenCV` | 四边形、中心点、红绿光点识别，适合视觉瞄准/棋盘/定位题 | `quad_detector.py`, `point_detector.py`, `stream.py` | 依赖 Linux/Python 摄像头环境；比赛现场要保证供电、开机、自启动 |
| 声源定位/声控跟踪 | `framist_NUEDC2022-E` | 声源定位、TDOA、信号调理、云台跟踪参考 | `HARDWARE\ADC`, `HARDWARE\DMA`, `HARDWARE\TIM`, `HARDWARE\DAC`, `USER\main.c`, `报告.docx` | 模拟调理和环境噪声风险高，建议作为备选而非唯一押注 |

## A 级：命中特定题型再打开

| 题型 | 仓库 | 可取内容 | 关键文件/模块 |
|---|---|---|---|
| 自动泊车/平衡/简易车 | `AMingKL_2022-ElectronicDesignContest` | STM32F1 小车 PID、电机、编码器、MPU6050、OpenMV 全流程 | `STM32代码\HARDWARE\PID`, `MOTOR`, `ENCODER`, `MPU6050`, `CONTROL`, `OpenMV代码\全程.py` |
| 自动瞄准/二维云台 | `STM32xxx_Electromagnetic-gun` | OpenMV + STM32 + 舵机/云台控制思路 | `OpenMV.py`, `STM32最终版\SYSTEM\adc`, `dac`, `i2c`, `outputdata`, `USER\main.c` |
| 电路参数测量 | `VxTeemo_NUEDC2019-D` | 简易电路特性测试仪，ADC、TFT、USB/存储显示参考 | `main.c`, `_03_Drive\Drive_ADC.c`, `_07_TFT_LCD` |
| 信号失真测量 | `hadesczq_Signal-distortion-measuring-device` | FFT、菜单、OLED/显示、MSP430 采样测量思路 | `Src\Fourier.c`, `Src\MenuMeasure.C`, `User\main.c`, `MSP-EXP430F5529_HAL\ADC.c` |
| 题库/历史题快速定位 | `CCBP_NUEDC_Topic`, `zuoliangyu_NUEDC_TOPIC`, `John-Jameson_NUEDC_Topic` | 查相似题、评分点、历史要求 | 先用文件名搜索年份和题号 |
| 备赛路线/工具箱 | `CNYuYang_nuedc-prepare-roadmap`, `CNLHC_NUEDC-Toolbox` | 控制/信号/电源方向知识补洞，比赛前训练 | 作为学习资料，不作为比赛工程 |

## B 级：高风险或只作参考

| 题型 | 仓库 | 用法 | 不建议原因 |
|---|---|---|---|
| 电源/功率变换 | `yagami-light7_2025_TI_electronic_source_A`, `dlphay_m430_dcdc_2015`, `MaxDYi_2021NationalUndergraduateElectronicsDesignContest-A` | 看采样、保护、PWM/SPWM、功率拓扑思路 | 高功率调试风险高，专科赛道不作为主押方向 |
| 无人机 | `Hyf338_Hunting-UAV` | 只看任务拆解和视觉/通信思路 | 机械、飞控、安全风险高，现场不可控 |
| 磁悬浮/板球 | `Charmve_BallPlate`, `jark006_Ball_And_Plate_Balance_control_system` | 看闭环控制和 PID/滤波 | 机械结构和调参时间长，适合作为备选练习 |

## 题目一出时的调用规则

### 1. 小车类

先问四个点：是否巡线、是否避障、是否视觉、是否要求路径规划。

- 纯巡线/定距/定向：先用 `abcuer_2024-NUEDC-H-TI_CAR`。
- 避障/动态障碍/视觉识别：先用 `JamieK32_ti-contest-2025`。
- 简单车加 OpenMV：补看 `AMingKL_2022-ElectronicDesignContest`。

第一天目标：车能直行、转向、停在指定距离、读传感器、串口打印状态；晚上再做任务状态机和补偿表。

### 2. 测量仪器类

先把指标拆成：输入范围、频率范围、精度、显示项目、是否隔离、是否要求谐波/相位/功率。

- 电压/电流/功率：先用 `Sh4peshifting_2024-nuedc-mspm0`。
- 频谱/信号分离：先用 `wu854065020_signal_separator`。
- 调制识别/复杂参数估计：借 `outline1988_h743iit6_modulation_recognition` 的算法，不建议整体移植。
- 失真度/THD：看 `hadesczq_Signal-distortion-measuring-device` 的 FFT 和菜单思路。

第一天目标：ADC 稳定采样、量程保护、显示 RMS/频率/峰峰值；第二天再加 FFT、相位、功率因数、校准。

### 3. 视觉/瞄准类

先判断是否必须识别颜色、形状、坐标、激光点，是否允许独立视觉模块。

- 白板/四边形/光点：先用 `DBinK_2023-NUEDC-E-OpenCV`。
- OpenMV + 舵机/云台：看 `STM32xxx_Electromagnetic-gun` 和 `AMingKL`。
- MaixCam + MCU 协议：看 `JamieK32_ti-contest-2025`。

第一天目标：摄像头输出坐标，MCU 串口收到并能控制舵机移动；不要一开始追求完美识别。

### 4. 声源/超声/非接触定位

先分清是“测距”还是“定位”。测距优先用现成超声/ToF；定位才考虑 TDOA。

- 声源定位/跟踪：看 `framist_NUEDC2022-E`。
- 近距离避障/距离测量：用 `JamieK32_ti-contest-2025` 的 VL53L1X/ToF 驱动路线更稳。

第一天目标：先稳定测一个距离/一个角度，别一开始上完整阵列算法。

## 推荐提前准备的板子和模块

### 主控

- 首选：MSPM0G3507 小板或自画最简主控板。
- 备选：STM32F407/STM32F103 成熟板，用于旧项目快速验证。
- 视觉协处理：OpenMV、MaixCam、K210、核桃派/香橙派等 Linux 小板任选一种稳定熟悉的。

### 通用模块

- 显示：0.96 OLED、串口屏至少一种。
- 调试：USB-TTL、蓝牙串口、逻辑分析仪、杜邦线、排针排母。
- 小车：TB6612、编码器电机、8 路灰度、JY901S/MPU6050/LSM6DSV16X、VL53L1X/超声。
- 测量：运放模块、比较器模块、基准源、分压/电流互感/霍尔电流、保护电阻、TVS、可调信号源。
- 执行：舵机、二维云台、激光笔、蜂鸣器、LED、继电器/MOS 模块。

### 最简自画主控板接口

只画“通用载板”，不要画专用题板：

- MCU 最小系统、SWD、复位、BOOT。
- 3.3V/5V 电源输入输出，电源指示，保险/防反接。
- UART x3、I2C x2、SPI x1。
- ADC 输入 4-8 路，预留 GND 伴随针和限流/保护位。
- PWM/舵机/电机控制排针。
- 编码器 A/B 两组。
- OLED、按键、蜂鸣器、LED。
- 所有关键 IO 丝印清楚，预留测试点。

## 比赛当天禁止踩的坑

- 不要一上来复刻大工程。先跑最小闭环，再加功能。
- 不要把 GitHub 项目当最终答案。只能拿架构、驱动、算法和调试方式。
- 不要选高功率电源或无人机作为默认主线，除非题目和器件非常明确。
- 不要在未知模拟前端上裸接 MCU ADC，先限流、分压、钳位、共地检查。
- 不要依赖现场联网下载库。关键仓库已经在本地，比赛前要把需要的 zip/工程和模块包离线备好。

## 当前结论

最值得押的可执行路线是：

1. `MSPM0G3507 通用主控板 + 现成模块`。
2. `小车类` 用 `abcuer` 快速版和 `JamieK32 2025K` 完整版双保险。
3. `测量类` 用 `Sh4peshifting MSPM0` 做第一主线，`wu854/outline/hadesczq` 做算法补充。
4. `视觉类` 用 `DBinK OpenCV` 或 `MaixCam/OpenMV` 独立处理，MCU 只管执行。
5. `声源/超声类` 只在题目明确时启动，优先用现成 ToF/超声简化。
