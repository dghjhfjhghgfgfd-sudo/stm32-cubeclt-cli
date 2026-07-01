# 02 · TX2 实战填洞 · 摄像头 → OpenCV → 串口(已真板跑通)

> 2026-06-23,在真 TX2 上验证通过。视觉端这条链已无未知数。配合 `00_作战总纲`、`01_演习` 视觉节使用。

## 结论:走 Python 路线,已端到端跑通

- **摄像头**:USB UVC 摄像头(实测 `DH USB2.0`),即插即用,`cv2.VideoCapture` 直接打开。**不用板载 ov5693 CSI**(需 GStreamer、无 python 绑定)。
- **OpenCV**:**Python 3.6 + pip 装 `opencv-python 4.6.0`**,可用。之前以为要退 C++ 是多虑——**Python 走得通**。
- **采集**:640×480,稳态约 **29.8 FPS**(识别足够)。
- **识别**:已计算亮区质心 `(x,y)`。
- **串口**:`/dev/ttyTHS2`,115200 8N1。
- **数据帧**:`0xAA  LEN  ASCII("x,y")  0x55`。
- **自启**:**systemd** 服务已启用,异常退出自动重启(比赛现场可靠)。
- **没有动 apt / JetPack / 系统 OpenCV**,环境未被破坏。

## 复现要点(比赛当天)

1. 插 USB 摄像头 → 确认 `/dev/video*` 出现。
2. `pip install opencv-python==4.6.0`(Py3.6),`import cv2` 通过。
3. `VideoCapture(0)` 取帧 → OpenCV 算目标质心 (x,y)。
4. pyserial 开 `/dev/ttyTHS2` @115200,按帧 `AA LEN "x,y" 55` 发出。
5. 用 systemd 设成开机自启 + 异常重启。

## 已知坑

- **电脑端 COM 口不能被两个软件同时占用**:SSCOM 占了 COM 口,别的就读不到(板端仍在正常写)。换工具或关掉占用者即可,不是板子问题。
- **黑屏 ≠ 崩溃**:串口出现 `nvidia@tegra-ubuntu:~$` 就是系统正常,黑屏只是 HDMI 握手。
- **⚠️ 电源/复位风险未清**:`pmic: reset reason 0x80` + 多次 Power on reset,怀疑电源/DC接头/排针/复位线。功能虽通,但**比赛中随机复位会断视觉流**——必须做"连续数小时不复位"的电源 soak 验证(用原装供电、插紧接头)。这是视觉端唯一剩下的未知数。
