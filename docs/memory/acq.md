# acq 模块记忆

## 2026-08-18 acq 线程 — M3a 实时反控协议接收链完成（待合并）
- **做了什么**：实现 M3 首块「实时反控协议接收链」，ctest 全绿（RingBuffer 6 + 解码 12 + 控制器 2 + 复刻端到端 1，共 21 用例）。文件：`acq/include/acq/hal.h`（DeviceConfig/IDevice/RingBuffer/AcquisitionController）、`acq/include/acq/hw_realtime.h`（HwChannel/AutosamplerParams/HwRealtimeReceiver）、`acq/src/hal.cpp`、`acq/src/hw_realtime.cpp`、`acq/tests/t_acq.cpp` + MockDevice.h/RingBackedDevice.h。
- **为什么这么设计**：用户 2026-08-18 拍板两项非冻结点——① RingBuffer **覆盖最旧保新**（缓冲槽用 `std::atomic<double>` 消除并发读写同一槽的撕裂读，x86-64 对齐 8 字节无锁；`write` 恒 true 不拒收）；② **启动命令时重置该通道采样计数**（data3=1/11/13/15/17，清零先于 `sigAcquisitionStarted`），一次采集一张谱图从 0 计时。自动进样 data3=7/8/9 分母=0 时字段置 0 不计算、`isValid()` 校验 sampleType∈[0,4] 且三值>0。控制器用 `std::thread` 后台轮询 `IDevice::read`（非阻塞，<0 → sigDeviceError 并停），`sigStarted/Stopped` 同步发出保证测试确定性。
- **下一步**：等主控审查后把 dev-acq 合回 main；M3b 做 IPC 端点（QLocalServer/共享内存适配真实 CtrlPanel）；M3 后期把 `HWFrequence.txt` 读入覆盖默认周期。
- **卡点/依赖**：① 并行会话在同一 worktree 写过竞争实现（be8b2fe 已提交 RingBuffer+HAL 部分，作者 Lanen259）——本线程采用其设计决策补齐 hw_realtime + 完整测试，避免回退并行版；若再开线程务必先 `git log` + `git status` 核对。② QSignalSpy 捕获 `HwChannel`/`AutosamplerParams` 参数必须 `qRegisterMetaType<HwChannel>("HwChannel")` 按 moc 记录的参数名注册（默认注册名带命名空间不匹配）。③ Git Bash 管道捕获该 exe 的 stdout 为空（Qt 程序控制台输出问题），查测试结果用 `ctest --output-on-failure` 或 `-o 文件`。
