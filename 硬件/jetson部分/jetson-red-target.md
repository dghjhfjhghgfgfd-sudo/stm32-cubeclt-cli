# Jetson TX2 红色目标识别

当前稳定版文件：

- Jetson: `/home/nvidia/jetson_gesture/red`
- Jetson 源码: `/home/nvidia/jetson_gesture/red.cpp`
- GitHub 备份源码: `scripts/jetson/red.cpp`

运行命令：

```bash
cd ~/jetson_gesture
DISPLAY=:0 XAUTHORITY=/home/nvidia/.Xauthority ./red /dev/video1
```

编译命令：

```bash
cd ~/jetson_gesture
g++ -std=c++11 -O2 -Wall -o red red.cpp -lX11
```

当前效果：

- 使用 USB 摄像头 `/dev/video1`
- 实时显示摄像头窗口，窗口标题为 `jetson手势识别测试`
- 识别红色目标，画框并输出中心坐标 `x,y`、面积 `area`
- 实测帧率约 29 FPS
- 已加入简单稳定逻辑：最小面积过滤和短暂丢失保持

当前结论：

红色目标识别比裸手肤色识别稳定，适合作为后续串口控制 STM32/执行机构的视觉输入。
