# 比赛当天实现手册 · TX2视觉端 + 单片机执行端

> 适用：2026 TI杯第十四届浙江省大学生电子设计竞赛
> 固定架构：**Jetson TX2（Linux/OpenCV，识别）→ 串口发坐标/状态 → MSPM0G3507 或 STM32（执行电机/舵机/声光）**
> 编写时间：2026-06-22　数据源：本地获奖报告 + 本地开源工程 + 联网复盘（每条尽量标出处）

---

## 0. 三类题型通用约定（先读这一节，省一半时间）

### 0.1 角色分工（铁律，三类题都一样）

- **TX2 只做识别**：跑 OpenCV，算出"目标在哪/是什么/偏差多少"，通过串口发出去。**不要让 TX2 直接控电机**——延迟和实时性都不如单片机。
- **单片机只做执行**：收串口 → 解析 → PID/状态机 → 驱动电机/舵机/声光。**不要在单片机上做图像**。
- 出处：`_索引与说明\2026浙江省赛_GitHub解法雷达.md` 反复强调"M0 只做实时控制，视觉板只做识别"（行 47-49、142-149）；`abcuer/2025-NUEDC-E-Ti_CAR` README 同样建议"M0 管巡线/电机/二维云台，OpenMV 只发坐标"。

### 0.2 TX2 串口物理层（本地 repo 没有，联网补全）

- TX2 对外串口设备节点是 **`/dev/ttyTHS2`**（J17 排针），用 Python `pyserial` 直接读写即可。出处：[NVIDIA Developer Forums - TX2 UART ttyTHS2](https://forums.developer.nvidia.com/t/tx2-uart-enabling-dev-ttyths2/50173)、[JetsonHacks - Jetson Nano/TX2 UART](https://jetsonhacks.com/2019/10/10/jetson-nano-uart/)。
- **三个上电必做的坑**（联网+报告共识）：
  1. **共地**：TX2 的 GND 必须和单片机 GND 接到一起，否则串口乱码或不通。
  2. **电平**：TX2 的 UART 是 **3.3V TTL**，单片机也用 3.3V，可直连；若执行端是 5V 板（部分 STM32 外设板）需加电平转换，**不要硬接**。
  3. **TX2 默认会在 ttyTHS2 上跑串口控制台**，要先 `systemctl stop nvgetty && systemctl disable nvgetty` 释放串口，否则你的数据被系统抢走。出处：[NVIDIA Forums - /dev/ttyTHS2 missing/被占用](https://forums.developer.nvidia.com/t/dev-ttyths2-missing/48953)。
- 波特率：**统一 115200**（与本地 repo 一致，见 0.3）。小数据量可上更高，但 115200 最稳、最不容易现场翻车。

### 0.3 统一串口协议（强烈建议三类题共用一套）

直接采用本地 `JamieK32_ti-contest-2025` 的成熟帧格式（已在国一工程里跑通），三类题只改"数据段"含义：

```
帧头  长度   数据段(len字节)        帧尾
0xAA  LEN    DATA[0..LEN-1]        0x55
```

- 出处：`GitHub资料库\JamieK32_ti-contest-2025\...\2025K-Code\maixcam_project\uart.py`（`_HEADER=0xAA`, `_TAIL=0x55`, 第二字节为长度）。
- 单片机端用**状态机逐字节解析**（state 0 等帧头 → 1 读长度 → 2 收数据 → 3 校验帧尾），uart.py 里已给出完整状态机，可直接照搬到 C。
- **A/B 协议路线**（冲突方案并列）：
  - **A：ASCII 文本帧**（调试友好）。数据段写成可读字符串，如 `"X:320,Y:240\n"` 或 `JamieK32` 的 `"C:0x03"`、`"T:0x.."`、`"N:.."` 前缀。优点：串口助手肉眼可读、好调试；缺点：解析稍慢、字节数多。**第一天强烈推荐用这个**，出错能一眼看出来。
  - **B：二进制定长帧**（高频/抗噪）。如 `0xAA len x_hi x_lo y_hi y_lo flag checksum 0x55`。优点：快、可加校验和；缺点：调试要靠逻辑分析仪。**第二天追性能再换**。
- 出处佐证：`abcuer_2024-NUEDC-H-TI_CAR\Bsp\uart.c` 用 UART 中断 + 逐字节 parse（`IMU_ParseData`）；JY901S 等模块也是定长二进制帧，证明二进制路线在 MCU 端成熟。

### 0.4 三类题共同的第一天纪律

1. 先让"TX2 能发、单片机能收并点亮一个 LED/打印一行"——**通信链路先通，再谈算法**。
2. 任何模块先做**最小闭环**再加功能（解法雷达行 190-195 反复强调，不要一上来复刻大工程）。
3. 关键 repo / 库 / 镜像**比赛前离线备好**，现场别指望联网下载（解法雷达行 195）。

---

# 一、小车控制（自动行驶 / 避障 / 视觉巡线小车）

适配赛题：2024 H 自动行驶小车、2025 K 自动避障小车、2022 C 小车跟随。
主参考：`abcuer_2024-NUEDC-H-TI_CAR`（快速版）、`JamieK32_ti-contest-2025`（完整版）、`AMingKL_2022-ElectronicDesignContest`（OpenMV+STM32 跟随）。

## A. 系统框图

```
            ┌─────────────────────────┐
            │   Jetson TX2 (Linux)     │
            │  USB摄像头→OpenCV识别     │
            │  巡线偏差 / 障碍 / 目标   │
            └───────────┬─────────────┘
                        │ UART  ttyTHS2  TX/RX/GND  115200,8N1
                        │ 帧: 0xAA LEN DATA 0x55
            ┌───────────▼─────────────┐
            │ MSPM0G3507 / STM32 执行端 │
            │  解析→状态机→速度PID+角度PID│
            └──┬────┬────┬────┬────┬────┘
   PWM/方向 │    │AB相 │I2C  │GPIO │ ←灰度阵列(8路,数字)
        ┌───▼─┐ ┌▼────┐┌▼────┐┌▼────────┐
        │TB6612│ │编码器││IMU  ││超声/VL53 │
        │电机驱动│ │×2  ││JY901S││L1X ToF  │
        └──────┘ └─────┘└──────┘└──────────┘
```

- **每根线讲清楚**：
  - TX2 `ttyTHS2-TX` → MCU `UART_RX`；TX2 `ttyTHS2-RX` ← MCU `UART_TX`；**GND 必须共地**。
  - MCU → TB6612：每电机 2 路方向 GPIO + 1 路 PWM（PWMA/AIN1/AIN2，PWMB/BIN1/BIN2）。
  - 编码器 A/B → MCU 两路定时器编码器模式或外部中断（abcuer 用中断读编码器测速）。
  - IMU JY901S → **独占一个 UART**（abcuer 在 UART 中断里 `IMU_ParseData`，见 `Bsp\uart.c`）。
  - 灰度 8 路 → 8 个数字 GPIO（abcuer `Hardware\gray.c`）。
  - 超声/VL53L1X ToF → I2C 或触发/回响 GPIO（JamieK32 `drivers\sensors\vl53l1x`）。

> **串口冲突警告**：视觉(1) + JY901S(1) 至少占 2 个 UART，规划好别撞。出处：解法雷达行 41-42"视觉一个 UART，JY901S 一个 UART，不要强行一根 TX 控两个"。

## B. 硬件清单（执行端）

| 模块 | 推荐型号 | 必买/可选 | 出处 |
|---|---|---|---|
| 主控 | MSPM0G3507 小板（首选）/ STM32F407(备选) | 必买 | 解法雷达行 162-166 |
| 电机驱动 | TB6612FNG（首选）/ DRV8833 | 必买 | 资源库行 159 |
| 编码器电机+底盘 | 带 AB 相编码器减速电机 ×2/4 | 必买 | 资源库行 158 |
| IMU | JY901S（首选，串口直出姿态）/ MPU6050 | 必买 | 资源库行 160；abcuer `jy901s.c` |
| 灰度循迹 | 8 路灰度阵列（数字量） | 必买（巡线题） | abcuer `gray.c` |
| 测距 | VL53L1X ToF（首选，避障稳）/ HC-SR04 超声 | 必买（避障题） | JamieK32 vl53l1x；解法雷达行 156 |
| 显示 | 0.96" OLED | 可选(强烈建议) | 资源库行 162 |
| 人机 | 按键 ×3 + 蜂鸣器 + LED | 可选(选模式用) | 资源库行 162 |
| 视觉端 | USB 摄像头（接 TX2，480P 够用） | 必买 | DBinK README "480P 就够" |

- **A/B 主控路线**：A=MSPM0G3507（今年 TI 杯主推，SDK/SysConfig 新，本地 repo 多）；B=STM32F407（老项目多、生态熟，旧代码可秒验）。出处：解法雷达行 162-166、200。

## C. 串口协议（小车）

- 波特率 115200，8N1，帧 `0xAA LEN DATA 0x55`。
- **TX2→MCU 发什么**：巡线偏差 + 状态标志。
- ASCII 例（第一天推荐）：
  ```
  TX2 发:  AA 09 "X:+035,F1" 55      ← X 偏差 +35 像素, F1=检测到目标/线
  含义:   center_x - 图像中心 = +35（车应右偏修正）, flag=1
  无目标:  AA 06 "X:0,F0" 55         ← F0 = 丢线/无目标, MCU 进搜索状态
  ```
- 二进制例（第二天）：`AA 04 28 00 01 CK 55` → x偏差=0x0028=40，flag=01，CK 校验和。
- 出处：帧格式来自 JamieK32 `uart.py`；偏差→PID 思路来自 AMingKL `CONTROL` 与 abcuer `track.c`。

## D. TX2 端代码骨架要点

识别什么 → 发什么：摄像头取帧 → HSV/灰度阈值 → 找线/目标轮廓 → 算中心 `cx` → 偏差 = `cx - W/2` → 串口发。

```python
# 关键函数（不贴整篇）
ser = serial.Serial('/dev/ttyTHS2', 115200, timeout=0.05)   # 0.2 节物理层
def find_line_offset(frame):       # OpenCV: cvtColor→inRange→findContours→boundingRect→中心
    # 参考 DBinK point_detector.py 的 find_max_contours: 取最大轮廓中心 (center_x,center_y)
    return cx                       # 没找到返回 None
def send_frame(payload: bytes):     # 封 0xAA LEN ... 0x55，照搬 uart.py _send()
    ser.write(bytes([0xAA, len(payload)]) + payload + bytes([0x55]))
# 主循环: while True: 取帧→find_line_offset→拼 "X:%+03d,F%d"→send_frame
```

- 关键点：① 处理帧率与发送解耦，**丢线时也要发 F0**，别让 MCU 卡在旧值；② 曝光/白平衡锁定，防光照漂移（解法雷达视觉坑）。
- 出处：轮廓取中心的写法直接对应 `DBinK_2023-NUEDC-E-OpenCV\point_detector.py`（`find_max_contours` 返回 `center_x/center_y`）；发送封帧对应 `JamieK32 uart.py _send()`。

## E. 单片机端代码骨架要点

收什么 → 怎么执行：UART 中断逐字节进状态机 → 收齐一帧 → 取偏差/flag → 喂给转向 PID → 叠加到左右轮速度 → 速度环 PID 输出 PWM。

- 关键模块（对应 abcuer 文件）：
  - `Bsp\uart.c`：UART 中断 + parse（套 0xAA/0x55 状态机）。
  - `Control\pid.c`：速度 PID（编码器测速闭环）+ 角度/偏差 PID。
  - `Control\track.c`：`Action_Track_To_White(speed, kp)` 循迹原子动作。
  - `Control\dist.c` / `Action_Turn_And_Distance(angle, dist)`：转向+定距。
  - `App\task.c`：**任务状态机**（workstep++ 推进，每段 A→B→C→D 配速度/kp），见 task.c 行 117-157。
  - `Hardware\motor.c / encoder.c / gray.c / jy901s.c`：底层驱动。
- 执行逻辑：`flag==0`（丢线）→ 进搜索/减速；`flag==1` → 偏差 PID 修正航向。避障题用 VL53L1X 阈值触发"停/绕"状态。
- 出处：以上文件名与函数名均来自本地 `abcuer_2024-NUEDC-H-TI_CAR`（解法雷达行 91 列出关键文件）。

## F. 第一天最小可交付（一句话）

**小车能直行、转弯、定距停车，编码器速度闭环稳定，并能用按键手动切换任务模式——现场放到地上跑一段直线停准即算通过。**（出处：资源库行 166-172"第一天必须完成"清单。）

## G. 已知坑（来自报告/repo 提炼）

1. **机械误差、轮胎打滑、电源压降**——不要只靠延时控制，必须编码器闭环。出处：资源库行 176-180。
2. **地面/光照变化导致循迹失效**——灰度阈值现场重标定。出处：资源库行 180。
3. **传感器安装不稳**——固定牢，否则数据跳。出处：资源库行 179。
4. **superbusycool 作者自述"巡线效果一般"**——别直接抄别人巡线参数，参数必须现场重调。出处：解法雷达行 27。
5. **VL53L1X 标定/串扰**——多个 ToF 互相干扰、近距异常，需做 XTalk 标定。出处：[ST 社区 VL53L1X detection](https://community.st.com/t5/imaging-sensors/vl53l1x-and-vl53l4cd-detection/td-p/763705)。

---

# 二、视觉识别 / 自瞄（运动目标追踪 / 自行瞄准 / 目标测量）

适配赛题：2023 E 运动目标控制与自动追踪、2025 E 简易自行瞄准、2025 C 单目视觉目标测量。
主参考：`DBinK_2023-NUEDC-E-OpenCV`（Linux+OpenCV，最贴 TX2）、`STM32xxx_Electromagnetic-gun`（OpenMV+云台）、联网 2023E 复盘、`MadGodBob/2025-...-E`（解法雷达 S 级）。

## A. 系统框图

```
   ┌──────────────────────────────┐
   │  Jetson TX2 (Linux/OpenCV)    │
   │  USB摄像头(固定)→识别目标/激光点 │
   │  输出像素误差 (ex, ey) + 状态  │
   └──────────────┬───────────────┘
                  │ UART ttyTHS2  115200  0xAA LEN ex ey flag 0x55
   ┌──────────────▼───────────────┐
   │   MSPM0G3507 / STM32 执行端    │
   │   ex→水平轴PID  ey→俯仰轴PID    │
   └───┬──────────────┬───────────┘
   PWM │              │ PWM/脉冲方向
 ┌─────▼────┐   ┌─────▼─────────┐
 │水平舵机/  │   │俯仰舵机/步进    │  → 末端: 激光笔/电磁铁/夹爪
 │闭环步进   │   │闭环步进        │
 └──────────┘   └───────────────┘
```

- 每根线：摄像头固定（不随云台动，减少标定复杂度——2023E 国一常见做法）；TX2 串口同 0.2 节；MCU 两路 PWM/脉冲分别驱动二维云台两轴；末端执行器一路 GPIO/PWM。
- 出处：联网 2023E 复盘"摄像头固定，二维云台搭载激光笔运动"（[CSDN OpenMV 方案](https://blog.csdn.net/qq_63922192/article/details/132054666)）。

## B. 硬件清单（执行端）

| 模块 | 推荐型号 | 必买/可选 | 出处 |
|---|---|---|---|
| 主控 | MSPM0G3507 / STM32F407 | 必买 | 解法雷达行 21-22 |
| 云台两轴 | **闭环步进电机×2（首选）** / 360°舵机 | 必买 | 解法雷达行 39-40、48 |
| 末端执行 | 激光笔（自瞄）/ 电磁铁 / 舵机夹爪 | 必买(按题) | 资源库行 197 |
| 姿态 | JY901S（单次回传） | 可选 | 解法雷达行 41 |
| 视觉端 | TX2 + USB 摄像头（480P+） | 必买 | DBinK README |
| 显示/人机 | OLED + 按键 | 可选 | 资源库行 162 |
| 供电 | 云台单独电池，与 MCU **共地** | 必买 | 解法雷达行 43 |

- **A/B 云台路线（关键冲突点）**：
  - **A：闭环步进电机**——精度高、能停准。普通 270° PWM 舵机在 1m 距离要 0.05° 分辨率，**做不到毫米级**，国一普遍弃用。出处：联网 2023E 复盘"舵机精度超出 PWM 范围"（[CSDN caoying12138](https://blog.csdn.net/caoying12138/article/details/132064041)）；解法雷达行 39"普通 270° 舵机不要作为主方案"。
  - **B：高精度数字舵机**——成本低、接线简单，适合精度要求不高的目标跟踪/居中题。

## C. 串口协议（视觉自瞄）

- 帧 `0xAA LEN DATA 0x55`，115200。**TX2 发像素误差**（目标相对图像中心），MCU 转成两轴角度增量。
- ASCII 例（第一天）：
  ```
  AA 0C "EX:+12,EY:-08,L1" 55   ← 水平误差+12px, 俯仰误差-8px, L1=锁定目标
  AA 09 "EX:0,EY:0,L0" 55       ← L0=未识别, MCU 停止/进搜索
  ```
- 二进制例（第二天，高帧率追踪）：`AA 05 0C 00 F8 FF 01 55`（ex=12, ey=-8, lock=1）。
- 出处：误差定义遵循联网 2023E 复盘"误差输入 PID"；帧格式同 JamieK32。

## D. TX2 端代码骨架要点

识别什么 → 发什么：取帧 → HSV 阈值找红/绿激光点或色块/矩形 → 取中心 → 误差 `ex=cx-W/2, ey=cy-H/2` → 发。

```python
# 关键函数（DBinK 风格，封装好可直接调）
red_pt, green_pt = detector.find_point(hsv)   # 返回 [x,y,w,h,center_x,center_y]
ex, ey = red_pt[4]-W//2, red_pt[5]-H//2
lock = 1 if red_pt[4] else 0                   # 没找到返回全 0
send_frame(b"EX:%+d,EY:%+d,L%d" % (ex, ey, lock))
```

- 关键点：① **HSV 阈值现场重标**（最大坑，见 G）；② 红光要用双区间 mask 合并（DBinK `find_red_point` 用 mask1|mask2，因红色在 HSV 两端）；③ 矩形/四边形目标用 `quad_detector.py` 取四顶点+中心，支持透视形变。
- 出处：`DBinK point_detector.py`（`find_point` 返回 center、红色双 mask）、`quad_detector.py`（四边形顶点）。

## E. 单片机端代码骨架要点

收什么 → 怎么执行：UART 状态机收 `ex,ey,lock` → 两路独立 PID（ex→水平轴、ey→俯仰轴）→ 输出步进脉冲/舵机 PWM → lock=1 时末端激光常亮/触发。

- 关键模块：
  - UART parse（套 0.3 状态机）。
  - 双轴 PID（参考 `STM32xxx_Electromagnetic-gun\USER\main.c` 的 outputdata/控制；`Charmve_BallPlate`/`jark006` 仅借 PID 理论）。
  - **前馈 + PID 微调 + 滑动/卡尔曼滤波**，不要纯视觉硬追（解法雷达行 49）。
  - 步进驱动：脉冲+方向；舵机：定时器 PWM。
- 执行：`lock==0` → 停轴/搜索扫描；`lock==1` → 误差趋零后稳定，末端动作。
- 出处：解法雷达行 48-49、104（Electromagnetic-gun 关键文件）。

## F. 第一天最小可交付（一句话）

**摄像头识别目标并输出坐标到主控，二维云台能据此移动并把激光点/指向大致对准目标——能跟着手持目标缓慢移动即算通过。**（出处：资源库行 223-227。）

## G. 已知坑

1. **精度是核心**：1m 距离毫米级需 ~0.05° 分辨率，超普通 PWM 舵机能力——上闭环步进。出处：联网 2023E 复盘（[CSDN caoying12138](https://blog.csdn.net/caoying12138/article/details/132064041)）。
2. **光照变化和反光**——HSV 阈值漂移、棋子/靶纸反光是头号杀手。出处：资源库行 236-239、行 219。
3. **机械回程误差 / 云台抖动**——加滤波+死区，前馈减少超调。出处：资源库行 240、解法雷达行 49。
4. **坐标标定不准**——摄像头固定可少标一层；像素→角度映射要现场标。出处：资源库行 239。
5. **机械结构耗时过大**——第一天别死磕机械，先把识别+单轴闭环跑通。出处：资源库行 237。
6. **串口中断冲突**：JY901S 用单次回传，避免与视觉连续帧抢中断。出处：解法雷达行 41-42。

---

# 三、声光 / 传感（声源定位 / 超声信标 / 非接触 / 光电）

适配赛题：2025 J 超声信标定位、2023 F 声传播智能定位、2025 I 非接触控制盘、2021 K 照度可调台灯、2022 声源定位跟踪。
主参考：`framist_NUEDC2022-E`（声源定位/TDOA）、`JamieK32`（ToF/超声驱动）、联网 TDOA 复盘。

> **架构说明（重要）**：声光/传感题的**敏感元件天然在执行端（MCU）侧**（麦克风、超声、光电用 MCU 的 ADC/比较器/定时器捕获实时性最好）。因此本类题 TX2 的角色与前两类不同，给出 A/B：
> - **A 路线（推荐）**：MCU 自己采集+解算+执行，TX2 仅作**上位机**（串口收 MCU 上报的距离/角度/坐标，做显示、记录、参数下发、可选摄像头辅助校验）。即数据方向以 **MCU→TX2 上报**为主。
> - **B 路线**：若题目含**视觉辅助定位**（如非接触控制盘用摄像头识别手势/光斑），则回到标准方向 **TX2 识别→发坐标→MCU 执行**。

## A. 系统框图（A 路线为主）

```
   ┌──────────────────────────────┐
   │  Jetson TX2 (上位机/可选视觉)   │
   │  显示距离/角度/坐标, 下发参数    │
   │  (B路线:摄像头识别手势/光斑→发坐标)│
   └──────────────┬───────────────┘
        UART ttyTHS2 115200  双向: MCU上报↑ / 参数下发↓
   ┌──────────────▼───────────────┐
   │   MSPM0G3507 / STM32 执行/采集  │
   │  定时器输入捕获→TDOA/测距解算   │
   └─┬──────┬──────┬──────┬─────────┘
 比较│整形  │      │ADC   │PWM
 ┌──▼──┐┌──▼───┐┌──▼───┐┌──▼──────┐
 │麦克风││超声收发││光电/  ││LED/蜂鸣器│
 │阵列×3││40kHz ││光敏   ││/舵机/风扇│
 └─────┘└──────┘└───────┘└──────────┘
```

- 每根线：麦克风→放大→**比较器整形**→MCU 定时器输入捕获通道（测到达时间差）；超声 40kHz 发射 PWM + 接收包络/比较器触发；光电→ADC；输出→PWM 驱动 LED/蜂鸣器/舵机。TX2 串口同 0.2 节，**双向**。
- 出处：资源库行 254-268（传感→计时→解算→输出结构）；联网"TDOA + 麦克风阵列 + STM32 定时器捕获"（[CSDN STM32 声源定位](https://blog.csdn.net/weixin_39895684/article/details/117183826)）。

## B. 硬件清单（执行端）

| 模块 | 推荐型号 | 必买/可选 | 出处 |
|---|---|---|---|
| 主控 | MSPM0G3507（片上 OPA/COMP/VREF 多）/ STM32F4 | 必买 | 解法雷达行 23、资源库行 463 |
| 声源采集 | 驻极体/MEMS 麦克风 ×3~4 + 运放 + 比较器 | 必买(声源题) | framist `HARDWARE\ADC`；联网 TDOA |
| 超声 | 40kHz 收发对管（信标/测距题） | 必买(超声题) | 资源库行 263、275 |
| 测距备选 | VL53L1X ToF / HC-SR04 | 可选 | JamieK32 vl53l1x |
| 光电 | 光敏电阻/光电二极管 + 基准 | 必买(光照题) | 资源库行 268、276 |
| 输出 | LED 模块 + 蜂鸣器 + 舵机/风扇 | 必买 | 资源库行 259 |
| 显示 | OLED（显示距离/角度/状态） | 必买 | 资源库行 282 |
| 信号调理 | 运放模块、比较器模块、基准源 | 必买 | 解法雷达行 173 |

- **A/B 测距路线**：A=现成超声/ToF 模块（测"距离"，稳、快出结果）；B=麦克风阵列 TDOA（做"定位/角度"，难度高，仅题目明确要求定位时上）。出处：解法雷达行 151-153"测距优先现成超声/ToF，定位才考虑 TDOA"。

## C. 串口协议（声光/传感，双向）

- 帧 `0xAA LEN DATA 0x55`，115200。
- **MCU→TX2 上报**（A 路线主方向）：
  ```
  AA 0E "D:1234,A:+45,S1" 55   ← 距离1234mm, 角度+45°, S1=信标锁定
  ```
- **TX2→MCU 下发参数 / B 路线发坐标**：
  ```
  AA 0A "TH:600,M:2" 55        ← 阈值600, 模式2
  AA 09 "X:160,Y:90" 55        ← B路线: 视觉光斑坐标→MCU执行
  ```
- 出处：帧格式同 JamieK32；上报字段结构参考 framist 报告的距离/角度输出。

## D. TX2 端代码骨架要点

- **A 路线**：TX2 几乎不做信号处理，`ser.readline()` 收 MCU 上报 → 解析 `D/A/S` → 显示/存 CSV/可选画轨迹。可做"现场可视化看板"帮调试。
- **B 路线（视觉辅助）**：摄像头识别手势/光斑/标志点 → 取中心坐标 → 发 MCU（复用第二类的 `find_point` 骨架，DBinK `point_detector.py`）。
- 关键点：声/超声的实时解算**不要放 TX2**（Linux 非实时，定时器捕获精度不够）。出处：解法雷达行 151-158（实时性放 MCU）。

## E. 单片机端代码骨架要点

收什么 → 怎么执行（核心在 MCU）：

- 采集：超声发射用定时器产生 40kHz PWM；接收经比较器整形进**定时器输入捕获**记时间戳。
- 解算：单通道→距离（声速×时间/2）；多麦克风→**TDOA**（各通道到达时间差→角度/坐标）。
- 闭环输出：距离/角度→阈值判定或 PID→驱动 LED/蜂鸣器/舵机；**控制要加死区**防抖。
- 关键模块（对应 framist 文件）：`HARDWARE\TIM`（捕获计时）、`HARDWARE\ADC`/`DMA`（包络采样）、`HARDWARE\DAC`（发射波形）、`USER\main.c`（解算主循环）。
- 出处：`framist_NUEDC2022-E`（解法雷达行 97 关键文件）；联网"4 麦克风 TDOA 在 STM32 实时运行"（[CSDN](https://blog.csdn.net/weixin_30248619/article/details/154760745)）。

## F. 第一天最小可交付（一句话）

**单通道传感器（一路超声/一路麦克风/一路光电）可靠读数，OLED 显示距离/区域/状态，并能据此驱动一个输出动作（LED 亮/蜂鸣/舵机动）——拿尺子比对读数大致正确即算通过。**（出处：资源库行 280-285。）

## G. 已知坑

1. **超声多径与环境噪声**——现场人多、墙面反射严重，比赛馆比实验室差。出处：资源库行 294-298。
2. **接收阈值不稳 / 比较器触发抖**——加滞回比较、自动增益要可控。出处：资源库行 296、342-346。
3. **传感器安装角度不一致**——阵列几何决定 TDOA 精度，机械要对齐。出处：资源库行 297。
4. **闭环无死区导致抖动**——输出加死区/限幅。出处：资源库行 298。
5. **STM32 上 TDOA 实时性紧张**——少有队伍做到 4 麦克风实时，优先简化为测距/单角度。出处：[知乎声源定位综述](https://zhuanlan.zhihu.com/p/1913275649000449868)、解法雷达行 157。
6. **麦克风前端噪声大、AGC 难控**——模拟调理比代码更关键，提前调好。出处：资源库行 342-346、解法雷达行 97。

---

## 附录 · 比赛当天选题与器件优先级（速查）

- 选题优先级（多题并行时）：测量/信号 ＞ 小车/执行 ＞ 非接触/超声 ＞ 视觉+简单执行 ＞ 音频 ＞ 磁悬浮 ＞ 无人机 ＞ 高功率电源。出处：资源库行 446-457。
- 通用主控板只画"通用载板"（UARTx3、I2Cx2、SPIx1、ADC4-8、PWM/舵机、编码器AB、OLED、按键、蜂鸣器、测试点），**不要画专用题板**。出处：资源库行 459-483、解法雷达行 177-187。
- 三类题串口全部统一 115200 + `0xAA/0x55` 帧，第一天用 ASCII 调通、第二天按需换二进制。

### 本地引用文件清单（可直接打开）
- `_索引与说明\2026浙江省赛_方案分组与可复用资源库.md`
- `_索引与说明\2026浙江省赛_GitHub解法雷达.md`
- `全国大学生电子设计竞赛\获奖作品与论文\2023_E题国一_设计报告_运动目标控制与自动追踪系统.pdf`
- `全国大学生电子设计竞赛\获奖作品与论文\2024_H题国一_设计报告_自动行驶小车.pdf`
- `全国大学生电子设计竞赛\获奖作品与论文\2025_K题国一_设计报告_自动避障小车.pdf`
- `全国大学生电子设计竞赛\获奖作品与论文\2022_C题国一_设计报告_小车跟随行驶系统.pdf`
- `GitHub资料库\JamieK32_ti-contest-2025`（串口协议 uart.py / maix_cam.c）
- `GitHub资料库\abcuer_2024-NUEDC-H-TI_CAR`（小车 PID/状态机/驱动）
- `GitHub资料库\DBinK_2023-NUEDC-E-OpenCV`（Linux+OpenCV 识别，最贴 TX2）
- `GitHub资料库\framist_NUEDC2022-E`（声源定位/TDOA）
- `GitHub资料库\AMingKL_2022-ElectronicDesignContest`、`STM32xxx_Electromagnetic-gun`（云台/跟随）

### 联网来源
- [NVIDIA Forums - TX2 UART ttyTHS2](https://forums.developer.nvidia.com/t/tx2-uart-enabling-dev-ttyths2/50173)
- [NVIDIA Forums - ttyTHS2 占用问题](https://forums.developer.nvidia.com/t/dev-ttyths2-missing/48953)
- [JetsonHacks - Jetson UART](https://jetsonhacks.com/2019/10/10/jetson-nano-uart/)
- [CSDN - 2023E OpenMV 方案](https://blog.csdn.net/qq_63922192/article/details/132054666)
- [CSDN - 2023E 精度/舵机分析](https://blog.csdn.net/caoying12138/article/details/132064041)
- [CSDN - MaixCAM 与 STM32 串口通信](https://blog.csdn.net/bkrclp/article/details/142638971)
- [CSDN - STM32 声源定位/TDOA](https://blog.csdn.net/weixin_39895684/article/details/117183826)
- [知乎 - 声源定位技术综述](https://zhuanlan.zhihu.com/p/1913275649000449868)
- [ST 社区 - VL53L1X 标定](https://community.st.com/t5/imaging-sensors/vl53l1x-and-vl53l4cd-detection/td-p/763705)
