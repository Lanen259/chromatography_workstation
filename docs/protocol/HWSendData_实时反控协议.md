# HWSendData 实时反控协议（工作站接收侧规格）

> 本文件是色谱工作站（CDS，上位机）与仪器控制程序（CtrlPanel.exe，下位控制面板）之间**实时反控通讯协议**的规格总文档。
> **发送侧语义**来自厂商《实时反控接口编程说明》（HWSendData.dll，2024-01 增补版，命令集 `data3` 0–17）；
> **接收侧**由本工作站定制（§4），语义与发送侧完全同构。
> 状态：**规格冻结**；实现归入里程碑 **M3（acq 模块）首块**，见 `PROJECT.md`。

---

## 1. 定位与角色

```
┌──────────────────────────┐        HWSendData.dll 进程间通讯        ┌──────────────────────────┐
│ 仪器控制程序 CtrlPanel.exe │ ───────────▶  SendDataToHW(d1,d2,d3)  ▶ │  色谱工作站（本工程）      │
│ （发送方，如 gas_chromatograph│         （每次调用 = 一条命令三元组）     │  （接收方，解析+显示+存储） │
│   工程的 HwSendDataClient）  │                                    │  本项目 QT/CDS            │
└──────────────────────────┘                                    └──────────────────────────┘
```

- **发送方 = 仪器控制程序**：连接仪器后持续把采样数据与启停/归零/自动进样参数发给工作站主程序。
- **接收方 = 本工作站主程序**：解析命令三元组，做实时采集、显示、存储、报告。
- **协议 = 命令三元组 `(data1, data2, data3)` 的语义**。`data3` 定命令类型，`data1/data2` 带数据。
- 厂商 DLL 的 IPC 实现对我们不透明，**接收侧的传输/接入方式由我方定制**（§4）。

> 发送端参考实现：`E:\My_project\QT\gas_chromatograph\gas_chromatograph\HW_widget\`（`hwsenddataclient.*` 封装 DLL 调用，`samplingworker.*` 按 50ms/点节奏发数）。我方后续做 IPC 端点时以此节奏为准。

---

## 2. 命令表（data3 = 0–17，发送侧冻结语义）

### 2.1 采样数据

| data3 | 含义 | data1 | data2 | 备注 |
|---|---|---|---|---|
| 0 | A、B 双通道**同步**发送采样数据 | A 通道值 | B 通道值 | 每点一条命令 |
| 3 | 单独发送 A 通道采样数据 | A 通道值 | — | **必须紧接着**用 data3=4 补发 B 通道占位（任意数，通常 0） |
| 4 | 单独发送 B 通道采样数据 | — | B 通道值 | **必须紧接着**用 data3=3 补发 A 通道占位（任意数，通常 0） |
| 10 | 单独发送 C 通道采样数据 | C 通道值 | — | 2010-12 增补；无需补发 A/B |
| 12 | 单独发送 D 通道采样数据 | D 通道值 | — | 2024-01 增补 |
| 14 | 单独发送 E 通道采样数据 | E 通道值 | — | 2024-01 增补 |
| 16 | 单独发送 F 通道采样数据 | F 通道值 | — | 2024-01 增补 |

### 2.2 采集控制

| data3 | 含义 | data1 | data2 |
|---|---|---|---|
| 1 | 启动 A/B 通道图谱采集 | 1=启动 A，0=不动 | 1=启动 B，0=不动 |
| 2 | 停止 A/B 通道图谱采集 | 1=停止 A，0=不动 | 1=停止 B，0=不动 |
| 5 | A/B 通道立即归零 | 1=归零 A，0=不动 | 1=归零 B，0=不动 |
| 11 | 控制 C 通道启停 | 1=启动 C | 1=停止 C |
| 13 | 控制 D 通道启停 | 1=启动 D | 1=停止 D |
| 15 | 控制 E 通道启停 | 1=启动 E | 1=停止 E |
| 17 | 控制 F 通道启停 | 1=启动 F | 1=停止 F |

> 注：采样数据在仪器连上后**应持续不断地发**，启动/停止命令只是通知工作站"从缓存中开始/停止取出数据做显示与处理"。

### 2.3 自动进样扩展参数

| data3 | 含义 | data1 | data2 | 备注 |
|---|---|---|---|---|
| 6 | 样品类型 | 0=标准样品；1/2/3/4=单点/多点/普通归一/校正归一 | — | 待测样品用 1–4 |
| 7 | 样品量 | 分子 | 分母 | 浮点用 `data1/data2` 表示，如 1.234 → 1234/1000 |
| 8 | 稀释倍数 | 分子 | 分母 | 同上 |
| 9 | 进样量比值 | 分子 | 分母 | 仅对待测样品发送 = 标准样品进样量 ÷ 待测样品进样量 |

> **每针样品名不通过命令传递**，而是采集结束后工作站改写在 `HWReport_A.txt` 中指定的谱图文件名（§6）。

---

## 3. 数据语义与时间基准

- **采样值 = 最小单位整数**：例如 1.234 mV 发送 1234（uV）、1.234 pA 发送 1234（fA）。接收侧存储原始整数，显示/换算时按通道量程缩放。
- **时间基准 = 每点默认 50 ms**：工作站每收到一个数据点默认认为过了 50ms。可用工作站目录下 `HWFrequence.txt` 覆盖该默认值。
- **保留时间**：第 n 个点（n 从 0 起）的保留时间 = n × 周期。契约定**毫秒（qint64）**制。
- **通道独立性**：A/B 为原始双通道；C/D/E/F 为后续增补。单发模式下发送方负责补发另一通道占位，接收方按到达顺序逐条解析即可，A 与 B 时钟天然对齐。

---

## 4. 接收侧协议定制（本工作站，我方规格）

厂商只定义发送语义；**接收侧接入方式由本工程定制**，语义与 §2 完全同构。

### 4.1 接收接口（冻结，acq 契约 §4.3）

接收侧只认**命令三元组**，与发送侧同构；传输无关（DLL / 本地 IPC / 仿真源皆可驱动）。

```cpp
// acq/include/acq/hw_realtime.h
namespace cdsw {

enum class HwChannel { A, B, C, D, E, F };            // 协议通道

struct AutosamplerParams {                            // data3 6~9
    int sampleType = 0;             // 0标准 / 1单点 / 2多点 / 3普通归一 / 4校正归一
    double sampleAmount = 1.0;      // data3=7 分子/分母
    double dilutionFactor = 1.0;    // data3=8
    double injectionVolumeRatio = 1.0;                // data3=9
    bool isValid() const;           // 分母≠0 校验
};

// 实时反控接收器：逐条喂入命令三元组 → 解码为事件。
class HwRealtimeReceiver : public QObject {
    Q_OBJECT
public:
    explicit HwRealtimeReceiver(QObject* parent = nullptr);
    void receive(long data1, long data2, long data3); // 协议唯一入口
    void setSamplePeriodMs(int periodMs);             // 默认 50（对应 HWFrequence.txt）
    int samplePeriodMs() const;
    AutosamplerParams autosamplerParams() const;
signals:
    void sigChannelSample(HwChannel ch, qint64 retentionMs, qint64 rawValue); // raw=最小单位整数
    void sigAcquisitionStarted(HwChannel ch);
    void sigAcquisitionStopped(HwChannel ch);
    void sigImmediateZero(HwChannel ch);
    void sigAutosamplerParamsChanged(const AutosamplerParams& params);
};
} // namespace cdsw
```

### 4.2 数据流（接收链）

```
命令三元组(d1,d2,d3)
   │  HwRealtimeReceiver.receive()  ← 发送方 / 仿真源 / 未来 IPC 端点
   ▼
解码 → 通道采样事件（含保留时间ms）│ 启停 / 归零 / 自动进样参数
   ▼
每通道 RingBuffer（定容循环缓冲）
   ▼
AcquisitionController（后台轮询 IDevice::read → 缓冲 → 批量信号）
   ▼
sigDataAcquired(QVector<double>)  →  M6：实时曲线显示 / 模型存储
```

### 4.3 传输方式演进

| 阶段 | 传输 | 用途 |
|---|---|---|
| 本阶段（M3 首块） | **in-process**：`HwRealtimeReceiver` 直连 + 仿真发送源（复刻 §5 示例） | 验证解码/缓冲/采集链路可行性，`ctest` 全绿 |
| M3 后期 | **IPC 端点**：QLocalServer(命名管道) / 共享内存，适配真实 CtrlPanel 跨进程接入 | 联调真实仪器控制程序 |
| 备选 | 直接加载 `HWSendData.dll` 语义对齐的适配层 | 与既有 DLL 生态互通 |

---

## 5. 复刻示例（发送方 → 接收方 全链路验证用）

厂商 C 语言例子（注意：发送应在独立采样线程中进行，非简单 while 循环）：

```
SendDataToHW(1,0,1);                       // 启动 A 通道
for data = 0..999:
    SendDataToHW(data, 0, 3);              // 发 A 通道数据
    SendDataToHW(0, 0, 4);                 // 补发 B 通道占位 0
    Sleep(50);                             // 工作站默认认为每点过 50ms
SendDataToHW(1,0,2);                       // 停止 A 通道
for i = 0..9:
    SendDataToHW(data, 0, 3);              // stop 后补发尾点，助工作站跳出等待
```

对应接收侧断言（复刻到 QTest）：启动信号 → ≥1010 个 A 通道采样点 → 停止信号；保留时间按 50ms/点递增。

---

## 6. 旁路文件契约（工作站侧职责）

| 文件 | 位置 | 用途 | 承接 |
|---|---|---|---|
| `ControlPanelInfo.txt` | 工作站目录 | 控制面板在启动采集前写入的仪器运行参数，工作站原样读入"分析报告表"的**报告头**（随谱图保存、打印时出现在报告头部） | report / io（M4/M5） |
| `HWReport_A.txt` | 工作站目录 | 采集结束后刷新，含各组份浓度/峰面积结果；其中一行"打开的谱图文件：…"给出刚生成的谱图文件名，供控制面板改名实现自动进样预命名 | report / io（M4/M5） |
| `HWFrequence.txt` | 工作站目录 | 重新定义"每点默认 50ms" | acq（M3） |
| `LZEXPAND.DLL` | 工作站目录 | 工作站启动后生成（退出时删除），存用户登录 username + 角色（Sys/Admin/Analyser/Visitor），供控制面板做用户分级管理 | ui / 权限（M6 后期） |

---

## 7. 仪器控制程序（CtrlPanel）生命周期约定

- 命名为 `CtrlPanel.exe`，放**与工作站主程序同一文件夹**；工作站工具条"控制面板"图标 / "视图"菜单可打开。
- 工作站退出时向 CtrlPanel 发送关闭消息，使其自动同步退出。
- 界面风格与主程序一致，初始位于屏幕右上角，尽量小、**总在最前面**（看起来像工作站弹出的对话框）。
- 二次点击打开时**不重复启动**，而是置前已有实例（`CreateMutex` + `FindWindow("ControlPanelClass")` + `SW_RESTORE` + `SetForegroundWindow`）。
- `CMainFrame::PreCreateWindow` 中登记类名 `"ControlPanelClass"`（必须是这个类名）。

---

## 8. 模块映射与验证标准

| 协议元素 | 落地模块 | 里程碑 |
|---|---|---|
| 命令三元组解码 / 通道时钟 / 采样周期 | **acq**（`HwRealtimeReceiver`，§4.1） | M3 首块 |
| 环形缓冲 / 采集控制器 / 设备抽象 | **acq**（§4.3 契约 hal.h） | M3 |
| IPC 端点（跨进程接入真实 CtrlPanel） | **acq** | M3 后期 |
| 实时曲线显示 / 快照 | **ui** | M6 |
| `ControlPanelInfo.txt` 报告头 / `HWReport_A.txt` 结果 | **report** + **io** | M4/M5 |
| 采样周期覆盖 `HWFrequence.txt` | **acq** | M3 |

**验证标准**：`ctest --test-dir build --output-on-failure -R acq` 全绿，含 §5 复刻示例端到端用例（启动 → 1000 点 → 停止 → 尾点 → 断言采样点数与保留时间）。
