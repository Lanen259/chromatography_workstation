# acq M3a 实时反控协议接收链 —— 设计文档（2026-08-18）

> 里程碑 M3 首块：实时反控协议接收链。规格总文档 `docs/protocol/HWSendData_实时反控协议.md`，接口签名冻结于契约 §4.3/§4.3b。本文档记录**非冻结点**的设计决策（其余全部照冻结契约与协议规格实现）。
> 2026-08-18 更新：RingBuffer 满策略与保留时间计数重置两项决策经用户拍板变更，见 §3。

## 1. 范围

- 只交付 `project/chromatography_workstation/acq/` 下的 `include/ src/ tests/ CMakeLists.txt`。
- 传输无关：in-process + 仿真源；测试用 MockDevice / RingBackedDevice，无真硬件。
- 纯 QtCore，零 UI（acq 是 core 系模块，禁止 QtWidgets；.ui 要求只适用于 ui 模块）。

## 2. 组件与数据流

```
命令三元组 (data1,data2,data3)
   │ HwRealtimeReceiver::receive()          ← 发送方 / 仿真源
   ▼
解码 → sigChannelSample(通道,保留时间ms,raw值) / sigAcquisitionStarted|Stopped / sigImmediateZero / sigAutosamplerParamsChanged
   ▼
每通道 RingBuffer（无锁 SPSC，覆盖最旧保新）
   ▼
AcquisitionController 后台线程轮询 IDevice::read → sigDataAcquired(QVector<double>)
```

- `hal.h`：`DeviceConfig` / `IDevice` / `RingBuffer` / `AcquisitionController`。
- `hw_realtime.h`：`HwChannel` / `AutosamplerParams` / `HwRealtimeReceiver`。
- 端到端桥接：测试内 `RingBackedDevice : IDevice`（从指定通道 RingBuffer 读）把解码样本经控制器送出，**不新增契约公开头**。

## 3. 设计决策（非冻结点）

| 决策 | 定案 | 理由 |
|---|---|---|
| RingBuffer 满时策略 | **覆盖最旧保新**（2026-08-18 用户拍板："一直显示最后的曲线位置，满了就覆盖旧的保持新的"）。满时写前先把最旧槽位移除（tail 前移一格腾位），新样本总是入队。`write` 恒返回 true（覆盖策略不拒收） | 曲线始终显示最新数据。安全实现：缓冲槽用 `std::atomic<double>`（x86-64 MinGW 8 字节对齐 store/load 无锁），生产者的"覆盖写"与消费者的"读"并发到同一 slot 时由语言级原子性保证无撕裂读——读到的要么是旧值要么是新值，都是合法 double。 |
| RingBuffer 并发模型 | SPSC：head 生产者独占写、tail 由生产者（满时覆盖）与消费者（排空时）共享但只做单调前移，各自只读对方；单调索引 `int`（`%capacity` 定位槽）+ 每槽 `std::atomic<double>` relaxed + head/tail 上 release/acquire 成对 | 无锁原子，独立可测；覆盖策略借 `std::atomic<double>` 消除撕裂读（纯 `double` + 生产者写消费者指针会有数据竞争 UB） |
| 控制器线程 | 内部 `std::thread` 后台循环；`std::atomic<bool> m_running`；`read` 非阻塞，返回 `<0` 视为设备错误 → `sigDeviceError` 并退出循环 | 契约"后台线程拉取"；非阻塞 read 保证 stop() 可及时 join |
| 启停信号时机 | `sigStarted` 在 start() 同步发出、`sigStopped` 在 stop() join 后同步发出 | 测试确定性（不依赖跨线程时序） |
| 保留时间 | 每通道采样计数 × 采样周期；首点 RT=0，第 n 点 = n×周期 | 规格 §3"第 n 个点（n 从 0 起）的保留时间 = n×周期" |
| 采样计数重置 | **启动命令时重置为 0**（2026-08-18 用户拍板）：data3=1 启动 A/B、data3=11/13/15/17 启动 C/D/E/F 时，把被启动的通道计数清零（先清零再发 `sigAcquisitionStarted`）；停止/尾点不重置，续计 RT | 一次采集一张谱图，从 0 计时；§5 复刻示例只启动一次，首点 RT=0、尾点续计，与"RT 按 50ms 递增"断言兼容 |
| 自动进样 7/8/9 | 分子/分母 → double；**分母=0 时字段置 0 不计算** | 防除零；`isValid()` 校验 sampleType∈[0,4] 且三值 >0（默认 1.0 恒有效；分母 0 → 值 0 → 无效） |
| 解码 data3 控制类 | 1/2/5 按 data1=data2 驱动 A/B；11/13/15/17 按 data1=启动、data2=停止驱动 C/D/E/F | 照命令表 §2.2 |
| 元类型注册 | `Q_DECLARE_METATYPE(QVector<double>)`（hal.h）、`Q_DECLARE_METATYPE(cdsw::HwChannel)`（hw_realtime.h）+ 构造时 `qRegisterMetaType` | 跨线程 queued 信号与 QSignalSpy 捕获所需；属新增声明，不改冻结签名 |

## 4. 测试计划（QTest，一文件四组，自定义 main 建 QCoreApplication）

1. **RingBufferTest**：基本写入/读取、环绕、容量边界、**覆盖策略**（满时写 → 最旧被覆盖、available==capacity、最新样本保留）、空读返回 0、SPSC 原子性（多线程压力写读不丢不重不撕裂）。
2. **HwRealtimeReceiverTest**：data3 0/3/4/10/12/14/16 采样信号 + 保留时间按周期递增；1/2/5 启停归零按 data1/data2 分通道；11/13/15/17 C~F 启停；6~9 自动进样参数（含分母=0 置 0、isValid）；samplePeriodMs 覆盖；**启动命令重置 RT 计数**（start 后首点 RT=0）；未知 data3 忽略。
3. **ControllerMockTest**：MockDevice 喂 0..99 阶梯序列 → 控制器 → sigDataAcquired 收集值顺序一致、samplesRead 正确、sigStarted/Stopped、错误设备触发 sigDeviceError。
4. **ReplicationEndToEndTest**（§5 复刻示例）：SimulationSource 喂 启动→1000 点(A+占位B)→停止→10 尾点 → 断言 sigAcquisitionStarted/Stopped 各 1、A 采样 1010 点、RT=0..50450 按 50ms 递增、A 值 0..999+999×10、B 采样 1000 占位；经 RingBuffer→控制器 → sigDataAcquired 收集 A 值顺序一致。

## 5. 验收映射

| DoD | 达成 |
|---|---|
| D1 编译 | `cmake --build build --target acq` |
| D2 测试 | `ctest --test-dir build --output-on-failure -R acq` 全绿 |
| D3 注册 | cdsw_add_module 自动 add_test(acq_tests) |
| D4 纪律 | 只 include `include/acq/`，纯 QtCore，不碰其他模块 src |
