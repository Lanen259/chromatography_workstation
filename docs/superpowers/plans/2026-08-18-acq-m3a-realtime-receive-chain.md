# acq M3a 实时反控协议接收链 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `project/chromatography_workstation/acq/` 内实现 M3 首块——HWSendData 实时反控协议接收链（HwRealtimeReceiver 解码 + RingBuffer + AcquisitionController），协议复刻示例端到端跑通，ctest 全绿。

**Architecture:** 三层 in-process 接收链：`HwRealtimeReceiver`（逐条 `receive(d1,d2,d3)` 解码为通道事件，签名冻结于契约 §4.3b）→ 每通道 `RingBuffer`（无锁 SPSC，覆盖最旧保新）→ `AcquisitionController`（`std::thread` 后台轮询 `IDevice::read` → 批量 `sigDataAcquired`）。测试用 `MockDevice`/`RingBackedDevice` 注入，无真硬件。

**Tech Stack:** Qt 5.14.2 / C++17 / QTest / CMake（`cdsw_add_module`）。纯 QtCore，禁 QtWidgets。

## Global Constraints

- 只许改 `project/chromatography_workstation/acq/` 下的 `include/ src/ tests/ CMakeLists.txt`；不碰其他模块、不改契约 §4.3/§4.3b 冻结签名。
- 纯 QtCore：头文件只 include `<QtCore/...>`，禁止 `<QtWidgets>` / `<QtGui>`。
- 命名：类 `PascalCase`，成员 `m_` 前缀，枚举 `enum class`，禁止魔法数字（常量命名+注释）。
- RingBuffer 满策略 = **覆盖最旧保新**；保留时间计数 **启动命令时清零**（2026-08-18 用户拍板）。
- 头文件 `#pragma once`；include 顺序：本头 → 工程头 → Qt 头 → 标准库头。
- 验收：`cmake --build build --target acq` 通过；`ctest --test-dir build --output-on-failure -R acq` 全绿。

---

### Task 1: hal.h 接口头 + RingBuffer 实现 + RingBuffer 测试

**Files:**
- Create: `project/chromatography_workstation/acq/include/acq/hal.h`
- Create: `project/chromatography_workstation/acq/src/hal.cpp`
- Create: `project/chromatography_workstation/acq/tests/t_acq.cpp`（首块：RingBuffer 测试类 + 自定义 main）

**Interfaces:**
- Produces: `cdsw::RingBuffer`（`explicit RingBuffer(int)`、`bool write(const double*, int)`、`int read(double*, int)`、`int available() const`、`int capacity() const`）；`cdsw::DeviceConfig`；`cdsw::IDevice` 纯虚接口；`cdsw::AcquisitionController` 声明（实现 Task 3）。

- [ ] **Step 1: 写 hal.h 接口头**

```cpp
// include/acq/hal.h —— 硬件抽象（契约 §4.3，签名冻结）
#pragma once
#include <QtCore/qglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qvector.h>
#include <atomic>
#include <memory>
#include <thread>

namespace cdsw {

struct DeviceConfig {
    int channelCount = 1;
    double sampleRateHz = 0.0;    // 采样率
    int adcRange = 0;             // AD 量程
};

class IDevice {                   // 硬件接口（= "仪器驱动扩展点"）
public:
    virtual ~IDevice() = default;
    virtual bool open(const DeviceConfig&) = 0;
    virtual void close() = 0;
    virtual bool startAcquisition() = 0;
    virtual bool stopAcquisition() = 0;
    virtual qint64 read(double* buffer, qint64 maxSamples) = 0;  // 返回实际读到的样本数；<0 设备错误
    virtual QString deviceInfo() const = 0;
};

// 无锁 SPSC 环形缓冲：生产者 write、消费者 read；满时覆盖最旧保新。
// 槽位用 std::atomic<double>（x86-64 对齐 8 字节无锁），并发读写同一槽无撕裂读。
class RingBuffer {
public:
    explicit RingBuffer(int capacity);
    bool write(const double* data, int count);   // 覆盖策略：全部接受，恒返回 true
    int read(double* out, int maxCount);         // 非阻塞
    int available() const;
    int capacity() const;
private:
    int m_capacity;
    std::unique_ptr<std::atomic<double>[]> m_data;
    std::atomic<int> m_head{0};   // 生产者：下一写入索引（单调递增）
    std::atomic<int> m_tail{0};   // 消费者排空 / 生产者满时覆盖：最旧未读索引（单调递增）
};

// 采集控制器：持有 IDevice，后台线程轮询 read → 环形缓冲 → 批量信号。
class AcquisitionController : public QObject {
    Q_OBJECT
public:
    AcquisitionController(IDevice* device, QObject* parent = nullptr);
    ~AcquisitionController() override;
    void start();
    void stop();
    qint64 samplesRead() const;
signals:
    void sigDataAcquired(const QVector<double>& samples);
    void sigDeviceError(const QString& message);
    void sigStarted();
    void sigStopped();
private:
    void workerLoop();
    IDevice* m_device;
    RingBuffer m_ring;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<qint64> m_samplesRead{0};
    std::thread m_worker;
};

} // namespace cdsw

Q_DECLARE_METATYPE(QVector<double>)
```

- [ ] **Step 2: 写 hal.cpp（RingBuffer 实现 + AcquisitionController 桩）**

```cpp
// src/hal.cpp —— 环形缓冲 + 采集控制器（契约 §4.3）
#include "acq/hal.h"

#include <algorithm>
#include <chrono>

namespace cdsw {
namespace {
constexpr int kReadChunkSize = 256;   // 单次 read 拉取上限
constexpr int kEmitBatchSize = 1024;  // 单次 sigDataAcquired 批大小
constexpr int kRingCapacity = 4096;   // 控制器内部环形容量（实时路径缓冲，覆盖保新）
} // namespace

// ---------- RingBuffer ----------

RingBuffer::RingBuffer(int capacity)
    : m_capacity(capacity > 0 ? capacity : 1)
    , m_data(new std::atomic<double>[static_cast<size_t>(m_capacity)]())
{}

bool RingBuffer::write(const double* data, int count)
{
    if (!data || count <= 0)
        return true;
    int head = m_head.load(std::memory_order_relaxed);
    int tail = m_tail.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (head - tail >= m_capacity) {
            // 满：重载消费者最新位置，确认真满后才覆盖最旧（tail 前移一格腾位，保新）
            tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= m_capacity) {
                tail = head - m_capacity + 1;
                m_tail.store(tail, std::memory_order_relaxed);
            }
        }
        m_data[head % m_capacity].store(data[i], std::memory_order_relaxed);
        ++head;
    }
    m_head.store(head, std::memory_order_release);
    return true;  // 覆盖策略：不拒收
}

int RingBuffer::read(double* out, int maxCount)
{
    if (!out || maxCount <= 0)
        return 0;
    const int head = m_head.load(std::memory_order_acquire);
    const int tail = m_tail.load(std::memory_order_relaxed);
    const int avail = head - tail;
    if (avail <= 0)
        return 0;
    const int n = std::min(avail, maxCount);
    for (int i = 0; i < n; ++i)
        out[i] = m_data[(tail + i) % m_capacity].load(std::memory_order_relaxed);
    m_tail.store(tail + n, std::memory_order_release);
    return n;
}

int RingBuffer::available() const
{
    return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_acquire);
}

int RingBuffer::capacity() const { return m_capacity; }

// ---------- AcquisitionController ----------

AcquisitionController::AcquisitionController(IDevice* device, QObject* parent)
    : QObject(parent)
    , m_device(device)
    , m_ring(kRingCapacity)
{
    qRegisterMetaType<QVector<double>>("QVector<double>");  // 跨线程 queued 信号
}

AcquisitionController::~AcquisitionController() { stop(); }

void AcquisitionController::start()
{
    if (m_running.load(std::memory_order_acquire))
        return;
    if (!m_device) {
        emit sigDeviceError(QStringLiteral("采集设备未注入"));
        return;
    }
    m_stopRequested.store(false, std::memory_order_relaxed);
    m_running.store(true, std::memory_order_release);
    m_worker = std::thread(&AcquisitionController::workerLoop, this);
    emit sigStarted();
}

void AcquisitionController::stop()
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    if (m_worker.joinable())
        m_worker.join();
    const bool wasRunning = m_running.exchange(false, std::memory_order_acq_rel);
    if (wasRunning)
        emit sigStopped();
}

qint64 AcquisitionController::samplesRead() const
{
    return m_samplesRead.load(std::memory_order_acquire);
}

void AcquisitionController::workerLoop()
{
    while (!m_stopRequested.load(std::memory_order_relaxed)) {
        QVector<double> chunk(kReadChunkSize);
        const qint64 n = m_device->read(chunk.data(), kReadChunkSize);
        if (n < 0) {
            emit sigDeviceError(QStringLiteral("设备读取失败"));
            break;
        }
        if (n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 空闲退避，防空转
            continue;
        }
        m_ring.write(chunk.constData(), static_cast<int>(n));
        QVector<double> batch(kEmitBatchSize);
        const int got = m_ring.read(batch.data(), kEmitBatchSize);
        if (got > 0) {
            batch.resize(got);
            m_samplesRead.fetch_add(got, std::memory_order_relaxed);
            emit sigDataAcquired(batch);
        }
    }
}

} // namespace cdsw
```

- [ ] **Step 3: 写 t_acq.cpp（首块：RingBufferTest + 自定义 main）**

```cpp
// acq/tests/t_acq.cpp —— acq 模块 QTest（RingBuffer / HwRealtimeReceiver / 控制器+MockDevice / §5 复刻示例）
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "acq/hal.h"
#include "MockDevice.h"
#include "RingBackedDevice.h"

using cdsw::RingBuffer;
using cdsw::AcquisitionController;

// ---------- RingBuffer ----------
class RingBufferTest : public QObject
{
    Q_OBJECT
private slots:
    void basicReadWrite();
    void wrapAround();
    void emptyRead();
    void overflowOverwritesOldest();
    void spscStress();
};

void RingBufferTest::basicReadWrite()
{
    RingBuffer rb(4);
    QCOMPARE(rb.capacity(), 4);
    QCOMPARE(rb.available(), 0);

    const double in[] = { 1.0, 2.0, 3.0 };
    QVERIFY(rb.write(in, 3));
    QCOMPARE(rb.available(), 3);

    double out[4] = {};
    QCOMPARE(rb.read(out, 4), 3);
    QCOMPARE(out[0], 1.0);
    QCOMPARE(out[1], 2.0);
    QCOMPARE(out[2], 3.0);
    QCOMPARE(rb.available(), 0);
}

void RingBufferTest::wrapAround()
{
    RingBuffer rb(4);
    const double in1[] = { 1.0, 2.0, 3.0, 4.0 };  // 填满
    QVERIFY(rb.write(in1, 4));
    QCOMPARE(rb.available(), 4);
    double out[2] = {};
    QCOMPARE(rb.read(out, 2), 2);                 // 读走 1,2
    const double in2[] = { 5.0, 6.0 };            // 写回 → 环绕
    QVERIFY(rb.write(in2, 2));
    double out2[4] = {};
    QCOMPARE(rb.read(out2, 4), 4);
    QVERIFY(out2[0] == 3.0 && out2[1] == 4.0 && out2[2] == 5.0 && out2[3] == 6.0);
}

void RingBufferTest::emptyRead()
{
    RingBuffer rb(4);
    double out[4] = {};
    QCOMPARE(rb.read(out, 4), 0);
}

void RingBufferTest::overflowOverwritesOldest()
{
    RingBuffer rb(4);
    const double in[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };  // 超容量
    QVERIFY(rb.write(in, 6));
    QCOMPARE(rb.available(), 4);                            // 只保留最新 4 个
    double out[8] = {};
    QCOMPARE(rb.read(out, 8), 4);
    QVERIFY(out[0] == 3.0 && out[1] == 4.0 && out[2] == 5.0 && out[3] == 6.0);
}

void RingBufferTest::spscStress()
{
    // SPSC 压力：容量 > 样本数 → 无覆盖，校验和精确（不丢不重不撕裂）
    const int N = 10000;
    RingBuffer rb(1 << 16);
    std::atomic<bool> producerDone{ false };
    qint64 sum = 0;
    int readCount = 0;

    std::thread consumer([&] {
        double buf[256];
        while (true) {
            const int n = rb.read(buf, 256);
            if (n > 0) {
                for (int i = 0; i < n; ++i)
                    sum += static_cast<qint64>(buf[i]);
                readCount += n;
            } else if (producerDone.load(std::memory_order_acquire)) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
    });

    double in[64];
    for (int base = 0; base < N; base += 64) {
        const int m = std::min(64, N - base);
        for (int i = 0; i < m; ++i)
            in[i] = static_cast<double>(base + i);
        rb.write(in, m);
    }
    producerDone.store(true, std::memory_order_release);
    consumer.join();

    QCOMPARE(readCount, N);
    QCOMPARE(sum, static_cast<qint64>(N) * (N - 1) / 2);
}

// ---------- main：串行跑所有测试类 ----------
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    RingBufferTest t1;
    int rc = QTest::qExec(&t1, argc, argv);
    return rc;
}

#include "t_acq.moc"
```

> Step 3 只含 RingBufferTest + main（后续任务往 main 里加测试类、往文件里加类）。MockDevice.h / RingBackedDevice.h 当前被 include 但还未创建 → Step 4 先创建空桩让编译通过。

- [ ] **Step 4: 创建 MockDevice.h / RingBackedDevice.h 桩（空实现，后续任务填充）**

```cpp
// tests/MockDevice.h —— 仿真设备：按注入序列吐采样；可切换失败模式（read 返回 -1）
#pragma once
#include "acq/hal.h"
#include <QtCore/qvector.h>

class MockDevice : public cdsw::IDevice
{
public:
    bool open(const cdsw::DeviceConfig& cfg) override { m_config = cfg; return true; }
    void close() override {}
    bool startAcquisition() override { return true; }
    bool stopAcquisition() override { return true; }
    qint64 read(double* buffer, qint64 maxSamples) override
    {
        if (m_failRead)
            return -1;
        qint64 n = 0;
        while (n < maxSamples && m_cursor < m_sequence.size()) {
            buffer[n] = m_sequence.at(static_cast<int>(m_cursor));
            ++m_cursor;
            ++n;
        }
        return n;
    }
    QString deviceInfo() const override { return QStringLiteral("MockDevice"); }

    void setSequence(const QVector<double>& seq) { m_sequence = seq; m_cursor = 0; }
    void setFailRead(bool fail) { m_failRead = fail; }

private:
    cdsw::DeviceConfig m_config;
    QVector<double> m_sequence;
    qint64 m_cursor = 0;
    bool m_failRead = false;
};
```

```cpp
// tests/RingBackedDevice.h —— 桥接设备：从 RingBuffer 读样本，作为"解码样本 → 控制器"的 in-process 桥
#pragma once
#include "acq/hal.h"

class RingBackedDevice : public cdsw::IDevice
{
public:
    explicit RingBackedDevice(int capacity = 4096) : m_buffer(capacity) {}
    cdsw::RingBuffer m_buffer;   // 测试向 m_buffer.write() 喂解码样本
    bool open(const cdsw::DeviceConfig&) override { return true; }
    void close() override {}
    bool startAcquisition() override { return true; }
    bool stopAcquisition() override { return true; }
    qint64 read(double* out, qint64 max) override
    { return m_buffer.read(out, static_cast<int>(max)); }
    QString deviceInfo() const override { return QStringLiteral("RingBackedDevice"); }
};
```

- [ ] **Step 5: 配置 + 编译 + 跑测试**

```bash
cmake -S project/chromatography_workstation -B build -DCDSW_BUILD_TESTS=ON
cmake --build build --target acq_tests
ctest --test-dir build --output-on-failure -R acq
```

Expected: RingBuffer 5 个测试全过。

- [ ] **Step 6: Commit**

```bash
git add project/chromatography_workstation/acq/include/acq/hal.h project/chromatography_workstation/acq/src/hal.cpp project/chromatography_workstation/acq/tests/t_acq.cpp project/chromatography_workstation/acq/tests/MockDevice.h project/chromatography_workstation/acq/tests/RingBackedDevice.h
git commit -m "feat(acq): RingBuffer 无锁 SPSC 环形缓冲（覆盖最旧保新）"
```

---

### Task 2: hw_realtime.h + HwRealtimeReceiver 实现 + 解码测试

**Files:**
- Create: `project/chromatography_workstation/acq/include/acq/hw_realtime.h`
- Create: `project/chromatography_workstation/acq/src/hw_realtime.cpp`
- Modify: `project/chromatography_workstation/acq/tests/t_acq.cpp`（加 HwRealtimeReceiverTest 类 + main 里加跑）

**Interfaces:**
- Consumes: 无
- Produces: `cdsw::HwChannel`（`enum class {A,B,C,D,E,F}`）、`cdsw::AutosamplerParams`（`sampleType/sampleAmount/dilutionFactor/injectionVolumeRatio/isValid()`）、`cdsw::HwRealtimeReceiver`（`receive(long,long,long)`、`setSamplePeriodMs(int)`、`samplePeriodMs()`、`autosamplerParams()` + 5 个信号）。语义照契约 §4.3b + 协议规格 §2。

- [ ] **Step 1: 写 hw_realtime.h**

```cpp
// include/acq/hw_realtime.h —— 实时反控接收器（契约 §4.3b，签名冻结）
#pragma once
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <array>

namespace cdsw {

// 协议通道：A/B 原始双通道 + C/D/E/F 增补（data3 3/4/10/12/14/16）
enum class HwChannel { A, B, C, D, E, F };

// 自动进样扩展参数（data3 6~9；浮点用分子/分母表示）
struct AutosamplerParams {
    int sampleType = 0;                 // 0标准 / 1单点 / 2多点 / 3普通归一 / 4校正归一
    double sampleAmount = 1.0;          // data3=7
    double dilutionFactor = 1.0;        // data3=8
    double injectionVolumeRatio = 1.0;  // data3=9
    bool isValid() const;               // sampleType∈[0,4] 且三值>0（分母=0 时置 0 → 无效）
};

// 实时反控接收器：逐条喂入命令三元组 → 解码为事件。
class HwRealtimeReceiver : public QObject {
    Q_OBJECT
public:
    explicit HwRealtimeReceiver(QObject* parent = nullptr);
    void receive(long data1, long data2, long data3);  // 协议唯一入口
    void setSamplePeriodMs(int periodMs);              // 默认 50（= HWFrequence.txt）
    int samplePeriodMs() const;
    AutosamplerParams autosamplerParams() const;
signals:
    void sigChannelSample(HwChannel ch, qint64 retentionMs, qint64 rawValue);
    void sigAcquisitionStarted(HwChannel ch);
    void sigAcquisitionStopped(HwChannel ch);
    void sigImmediateZero(HwChannel ch);
    void sigAutosamplerParamsChanged(const AutosamplerParams& params);
private:
    void emitSample(HwChannel ch, long raw);
    void startChannel(HwChannel ch, bool start);
    int m_samplePeriodMs = 50;
    std::array<qint64, 6> m_pointCount{};
    AutosamplerParams m_params;
};

} // namespace cdsw

Q_DECLARE_METATYPE(cdsw::HwChannel)
```

- [ ] **Step 2: 写 hw_realtime.cpp（命令表解码）**

```cpp
// src/hw_realtime.cpp —— 实时反控协议解码（命令表 data3 0–17，规格 docs/protocol/HWSendData_实时反控协议.md §2）
#include "acq/hw_realtime.h"

namespace cdsw {
namespace {

int channelIndex(HwChannel ch) { return static_cast<int>(ch); }

HwChannel channelForSampleCommand(long data3)   // 10/12/14/16 → C/D/E/F 采样
{
    switch (data3) {
    case 10: return HwChannel::C;
    case 12: return HwChannel::D;
    case 14: return HwChannel::E;
    default: return HwChannel::F;
    }
}

HwChannel channelForControlCommand(long data3)  // 11/13/15/17 → C/D/E/F 启停
{
    switch (data3) {
    case 11: return HwChannel::C;
    case 13: return HwChannel::D;
    case 15: return HwChannel::E;
    default: return HwChannel::F;
    }
}

double divideFraction(long numerator, long denominator)
{
    if (denominator == 0)
        return 0.0;   // 分母 0 → 置 0，isValid() 返回 false
    return static_cast<double>(numerator) / static_cast<double>(denominator);
}

} // namespace

bool AutosamplerParams::isValid() const
{
    return sampleType >= 0 && sampleType <= 4
        && sampleAmount > 0.0
        && dilutionFactor > 0.0
        && injectionVolumeRatio > 0.0;
}

HwRealtimeReceiver::HwRealtimeReceiver(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<HwChannel>("cdsw::HwChannel");  // 跨线程 queued 信号
}

int HwRealtimeReceiver::samplePeriodMs() const { return m_samplePeriodMs; }

void HwRealtimeReceiver::setSamplePeriodMs(int periodMs)
{
    if (periodMs > 0)
        m_samplePeriodMs = periodMs;   // 非正周期忽略，防 RT 退化
}

AutosamplerParams HwRealtimeReceiver::autosamplerParams() const { return m_params; }

void HwRealtimeReceiver::receive(long data1, long data2, long data3)
{
    switch (data3) {
    case 0:   // A/B 双通道同步采样
        emitSample(HwChannel::A, data1);
        emitSample(HwChannel::B, data2);
        break;
    case 3:
        emitSample(HwChannel::A, data1);
        break;
    case 4:
        emitSample(HwChannel::B, data2);
        break;
    case 10: case 12: case 14: case 16:
        emitSample(channelForSampleCommand(data3), data1);
        break;
    case 1:   // 启动 A/B
        startChannel(HwChannel::A, data1 != 0);
        startChannel(HwChannel::B, data2 != 0);
        break;
    case 2:   // 停止 A/B
        if (data1 != 0) emit sigAcquisitionStopped(HwChannel::A);
        if (data2 != 0) emit sigAcquisitionStopped(HwChannel::B);
        break;
    case 5:   // 立即归零 A/B
        if (data1 != 0) emit sigImmediateZero(HwChannel::A);
        if (data2 != 0) emit sigImmediateZero(HwChannel::B);
        break;
    case 11: case 13: case 15: case 17: {  // C/D/E/F 启停（data1=启动 data2=停止）
        const HwChannel ch = channelForControlCommand(data3);
        if (data1 != 0) startChannel(ch, true);
        if (data2 != 0) emit sigAcquisitionStopped(ch);
        break;
    }
    case 6:   // 样品类型
        m_params.sampleType = static_cast<int>(data1);
        emit sigAutosamplerParamsChanged(m_params);
        break;
    case 7:   // 样品量
        m_params.sampleAmount = divideFraction(data1, data2);
        emit sigAutosamplerParamsChanged(m_params);
        break;
    case 8:   // 稀释倍数
        m_params.dilutionFactor = divideFraction(data1, data2);
        emit sigAutosamplerParamsChanged(m_params);
        break;
    case 9:   // 进样量比值
        m_params.injectionVolumeRatio = divideFraction(data1, data2);
        emit sigAutosamplerParamsChanged(m_params);
        break;
    default:
        break;   // 未知 data3：协议只定义 0–17，忽略
    }
}

void HwRealtimeReceiver::emitSample(HwChannel ch, long raw)
{
    const int i = channelIndex(ch);
    const qint64 rt = m_pointCount[static_cast<size_t>(i)] * m_samplePeriodMs;
    ++m_pointCount[static_cast<size_t>(i)];
    emit sigChannelSample(ch, rt, static_cast<qint64>(raw));
}

void HwRealtimeReceiver::startChannel(HwChannel ch, bool start)
{
    if (!start)
        return;
    m_pointCount[static_cast<size_t>(channelIndex(ch))] = 0;   // 启动命令 → 计数清零（RT 从 0 计时）
    emit sigAcquisitionStarted(ch);
}

} // namespace cdsw
```

- [ ] **Step 3: 往 t_acq.cpp 追加 HwRealtimeReceiverTest（解码全覆盖）**

在 `RingBufferTest` 类后追加：

```cpp
// ---------- HwRealtimeReceiver 解码 ----------
struct SampleEvent { HwChannel ch; qint64 rt; qint64 raw; };

class HwRealtimeReceiverTest : public QObject
{
    Q_OBJECT
private slots:
    void defaults();
    void syncSampleData3Zero();
    void singleChannelAB();
    void extChannelsCDEF();
    void startStopAB();
    void immediateZero();
    void extControlStartStop();
    void autosamplerParams();
    void invalidFractionMarksInvalid();
    void unknownCommandIgnored();
    void samplePeriodOverride();
    void startResetsRetentionCounter();
};

void HwRealtimeReceiverTest::defaults()
{
    HwRealtimeReceiver rx;
    QCOMPARE(rx.samplePeriodMs(), 50);
    const AutosamplerParams p = rx.autosamplerParams();
    QCOMPARE(p.sampleType, 0);
    QCOMPARE(p.sampleAmount, 1.0);
    QCOMPARE(p.dilutionFactor, 1.0);
    QCOMPARE(p.injectionVolumeRatio, 1.0);
    QVERIFY(p.isValid());
}

void HwRealtimeReceiverTest::syncSampleData3Zero()
{
    HwRealtimeReceiver rx;
    QVector<SampleEvent> samples;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) { samples.append({ ch, rt, raw }); });
    rx.receive(100, 200, 0);   // A=100 B=200 同步
    QCOMPARE(samples.size(), 2);
    QCOMPARE(samples.at(0).ch, HwChannel::A);
    QCOMPARE(samples.at(0).rt, qint64(0));
    QCOMPARE(samples.at(0).raw, qint64(100));
    QCOMPARE(samples.at(1).ch, HwChannel::B);
    QCOMPARE(samples.at(1).rt, qint64(0));
    QCOMPARE(samples.at(1).raw, qint64(200));
}

void HwRealtimeReceiverTest::singleChannelAB()
{
    HwRealtimeReceiver rx;
    QVector<SampleEvent> samples;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) { samples.append({ ch, rt, raw }); });
    rx.receive(10, 0, 3);   // A 单独
    rx.receive(0, 20, 4);   // B 单独（占位补发）
    QCOMPARE(samples.size(), 2);
    QCOMPARE(samples.at(0).ch, HwChannel::A);
    QCOMPARE(samples.at(0).raw, qint64(10));
    QCOMPARE(samples.at(1).ch, HwChannel::B);
    QCOMPARE(samples.at(1).raw, qint64(20));
}

void HwRealtimeReceiverTest::extChannelsCDEF()
{
    HwRealtimeReceiver rx;
    QVector<SampleEvent> samples;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) { samples.append({ ch, rt, raw }); });
    rx.receive(1, 0, 10);
    rx.receive(2, 0, 12);
    rx.receive(3, 0, 14);
    rx.receive(4, 0, 16);
    QCOMPARE(samples.size(), 4);
    QCOMPARE(samples.at(0).ch, HwChannel::C); QCOMPARE(samples.at(0).raw, qint64(1));
    QCOMPARE(samples.at(1).ch, HwChannel::D); QCOMPARE(samples.at(1).raw, qint64(2));
    QCOMPARE(samples.at(2).ch, HwChannel::E); QCOMPARE(samples.at(2).raw, qint64(3));
    QCOMPARE(samples.at(3).ch, HwChannel::F); QCOMPARE(samples.at(3).raw, qint64(4));
}

void HwRealtimeReceiverTest::startStopAB()
{
    HwRealtimeReceiver rx;
    QVector<HwChannel> started, stopped;
    connect(&rx, &HwRealtimeReceiver::sigAcquisitionStarted, &rx,
            [&](HwChannel ch) { started.append(ch); });
    connect(&rx, &HwRealtimeReceiver::sigAcquisitionStopped, &rx,
            [&](HwChannel ch) { stopped.append(ch); });
    rx.receive(1, 0, 1);      // 只启动 A
    rx.receive(0, 1, 1);      // 只启动 B
    rx.receive(1, 1, 2);      // 停止 A+B
    QCOMPARE(started.size(), 2);
    QCOMPARE(started.at(0), HwChannel::A);
    QCOMPARE(started.at(1), HwChannel::B);
    QCOMPARE(stopped.size(), 2);
    QCOMPARE(stopped.at(0), HwChannel::A);
    QCOMPARE(stopped.at(1), HwChannel::B);
}

void HwRealtimeReceiverTest::immediateZero()
{
    HwRealtimeReceiver rx;
    QVector<HwChannel> zeroed;
    connect(&rx, &HwRealtimeReceiver::sigImmediateZero, &rx,
            [&](HwChannel ch) { zeroed.append(ch); });
    rx.receive(1, 1, 5);
    rx.receive(0, 0, 5);      // 全 0 → 不动
    QCOMPARE(zeroed.size(), 2);
    QCOMPARE(zeroed.at(0), HwChannel::A);
    QCOMPARE(zeroed.at(1), HwChannel::B);
}

void HwRealtimeReceiverTest::extControlStartStop()
{
    HwRealtimeReceiver rx;
    QVector<HwChannel> started, stopped;
    connect(&rx, &HwRealtimeReceiver::sigAcquisitionStarted, &rx,
            [&](HwChannel ch) { started.append(ch); });
    connect(&rx, &HwRealtimeReceiver::sigAcquisitionStopped, &rx,
            [&](HwChannel ch) { stopped.append(ch); });
    rx.receive(1, 0, 11);   // 启动 C
    rx.receive(1, 0, 13);   // 启动 D
    rx.receive(1, 0, 15);   // 启动 E
    rx.receive(1, 0, 17);   // 启动 F
    rx.receive(0, 1, 11);   // 停止 C
    rx.receive(0, 1, 17);   // 停止 F
    QCOMPARE(started.size(), 4);
    QCOMPARE(started.at(0), HwChannel::C);
    QCOMPARE(started.at(3), HwChannel::F);
    QCOMPARE(stopped.size(), 2);
    QCOMPARE(stopped.at(0), HwChannel::C);
    QCOMPARE(stopped.at(1), HwChannel::F);
}

void HwRealtimeReceiverTest::autosamplerParams()
{
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigAutosamplerParamsChanged);
    rx.receive(3, 0, 6);          // 样品类型=3
    rx.receive(1234, 1000, 7);    // 样品量=1.234
    rx.receive(1, 2, 8);          // 稀释=0.5
    rx.receive(3, 2, 9);          // 进样比值=1.5
    QCOMPARE(spy.count(), 4);
    const AutosamplerParams p = rx.autosamplerParams();
    QCOMPARE(p.sampleType, 3);
    QCOMPARE(p.sampleAmount, 1.234);
    QCOMPARE(p.dilutionFactor, 0.5);
    QCOMPARE(p.injectionVolumeRatio, 1.5);
    QVERIFY(p.isValid());
}

void HwRealtimeReceiverTest::invalidFractionMarksInvalid()
{
    HwRealtimeReceiver rx;
    rx.receive(1, 0, 7);          // 分母=0 → 置 0
    QVERIFY(!rx.autosamplerParams().isValid());
    QCOMPARE(rx.autosamplerParams().sampleAmount, 0.0);
    rx.receive(0, 1000, 6);       // 非法样品类型 -... 先恢复
    rx.receive(1, 1, 6);          // sampleType=1 恢复
    QVERIFY(rx.autosamplerParams().isValid());
}

void HwRealtimeReceiverTest::unknownCommandIgnored()
{
    HwRealtimeReceiver rx;
    QVector<SampleEvent> samples;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) { samples.append({ ch, rt, raw }); });
    rx.receive(1, 2, 99);   // 未知 data3
    rx.receive(1, 2, -5);   // 负 data3
    QCOMPARE(samples.size(), 0);
}

void HwRealtimeReceiverTest::samplePeriodOverride()
{
    HwRealtimeReceiver rx;
    rx.setSamplePeriodMs(25);
    QCOMPARE(rx.samplePeriodMs(), 25);
    QVector<SampleEvent> samples;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) { samples.append({ ch, rt, raw }); });
    rx.receive(1, 0, 3);
    rx.receive(2, 0, 3);
    rx.receive(3, 0, 3);
    QCOMPARE(samples.at(0).rt, qint64(0));
    QCOMPARE(samples.at(1).rt, qint64(25));
    QCOMPARE(samples.at(2).rt, qint64(50));
}

void HwRealtimeReceiverTest::startResetsRetentionCounter()
{
    HwRealtimeReceiver rx;
    QVector<SampleEvent> a;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) { if (ch == HwChannel::A) a.append({ ch, rt, raw }); });
    rx.receive(1, 0, 1);     // 启动 A → 计数清零
    rx.receive(10, 0, 3);
    rx.receive(20, 0, 3);
    rx.receive(30, 0, 3);
    rx.receive(1, 0, 1);     // 再启动 A → 计数清零
    rx.receive(40, 0, 3);
    rx.receive(50, 0, 3);
    QCOMPARE(a.size(), 5);
    QCOMPARE(a.at(0).rt, qint64(0));   QCOMPARE(a.at(0).raw, qint64(10));
    QCOMPARE(a.at(1).rt, qint64(50));  QCOMPARE(a.at(1).raw, qint64(20));
    QCOMPARE(a.at(2).rt, qint64(100)); QCOMPARE(a.at(2).raw, qint64(30));
    QCOMPARE(a.at(3).rt, qint64(0));   QCOMPARE(a.at(3).raw, qint64(40));
    QCOMPARE(a.at(4).rt, qint64(50));  QCOMPARE(a.at(4).raw, qint64(50));
}
```

在 main 里加跑 `HwRealtimeReceiverTest`：

```cpp
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    RingBufferTest t1;
    HwRealtimeReceiverTest t2;
    int rc = QTest::qExec(&t1, argc, argv);
    rc |= QTest::qExec(&t2, argc, argv);
    return rc;
}
```

`t_acq.cpp` 顶部改用 `using cdsw::HwChannel; using cdsw::AutosamplerParams; using cdsw::HwRealtimeReceiver;`。

- [ ] **Step 4: 编译 + 跑测试**

```bash
cmake --build build --target acq_tests
ctest --test-dir build --output-on-failure -R acq
```

Expected: RingBuffer 5 + 解码 12 全过。

- [ ] **Step 5: Commit**

```bash
git add project/chromatography_workstation/acq/include/acq/hw_realtime.h project/chromatography_workstation/acq/src/hw_realtime.cpp project/chromatography_workstation/acq/tests/t_acq.cpp
git commit -m "feat(acq): HwRealtimeReceiver 命令三元组解码（data3 0–17）"
```

---

### Task 3: AcquisitionController 实现 + MockDevice + 控制器测试

**Files:**
- Modify: `project/chromatography_workstation/acq/src/hal.cpp`（已有实现，无需改；仅确认）
- Modify: `project/chromatography_workstation/acq/tests/t_acq.cpp`（加 ControllerMockTest + main）

**Interfaces:**
- Consumes: `cdsw::IDevice`（Task 1）、`MockDevice`（Task 1 桩）
- Produces: `AcquisitionController` 完整行为：后台线程拉取、`sigDataAcquired` 批次、`samplesRead()`、`sigStarted/sigStopped/sigDeviceError`、启停幂等。

- [ ] **Step 1: 确认 hal.cpp 已含 AcquisitionController 完整实现**（Task 1 Step 2 已写全，无需改动）

- [ ] **Step 2: 往 t_acq.cpp 追加 ControllerMockTest**

```cpp
// ---------- 控制器 + MockDevice ----------
class ControllerMockTest : public QObject
{
    Q_OBJECT
private slots:
    void acquiresSequenceInOrder();
    void startStopSignals();
    void deviceErrorEmitsError();
    void idempotentStartStop();
};

void ControllerMockTest::acquiresSequenceInOrder()
{
    MockDevice dev;
    QVector<double> seq;
    for (int i = 0; i < 100; ++i) seq.append(double(i));
    dev.setSequence(seq);
    QVERIFY(dev.open(cdsw::DeviceConfig{ 1, 1000.0, 0 }));

    AcquisitionController ctrl(&dev);
    QVector<double> collected;
    connect(&ctrl, &AcquisitionController::sigDataAcquired, &ctrl,
            [&](const QVector<double>& v) { collected.append(v); });

    ctrl.start();
    QTRY_COMPARE(collected.size(), 100);   // 等待 queued 信号送达
    ctrl.stop();

    QCOMPARE(ctrl.samplesRead(), qint64(100));
    QCOMPARE(collected.size(), 100);
    for (int i = 0; i < 100; ++i)
        QCOMPARE(collected.at(i), seq.at(i));
}

void ControllerMockTest::startStopSignals()
{
    MockDevice dev;
    QVector<double> seq(50, 1.0);
    dev.setSequence(seq);
    QVERIFY(dev.open(cdsw::DeviceConfig{ 1, 1000.0, 0 }));

    AcquisitionController ctrl(&dev);
    QSignalSpy started(&ctrl, &AcquisitionController::sigStarted);
    QSignalSpy stopped(&ctrl, &AcquisitionController::sigStopped);

    ctrl.start();
    QCOMPARE(started.count(), 1);              // sigStarted 同步发出
    QTRY_COMPARE(ctrl.samplesRead(), qint64(50));
    ctrl.stop();
    QCOMPARE(stopped.count(), 1);              // join 后同步发出
}

void ControllerMockTest::deviceErrorEmitsError()
{
    MockDevice dev;
    dev.setFailRead(true);
    QVERIFY(dev.open(cdsw::DeviceConfig{ 1, 1000.0, 0 }));

    AcquisitionController ctrl(&dev);
    QSignalSpy errSpy(&ctrl, &AcquisitionController::sigDeviceError);

    ctrl.start();
    QTRY_COMPARE(errSpy.count(), 1);
    QSignalSpy stopped(&ctrl, &AcquisitionController::sigStopped);
    ctrl.stop();
    QCOMPARE(stopped.count(), 1);
    QVERIFY(!errSpy.at(0).at(0).toString().isEmpty());
}

void ControllerMockTest::idempotentStartStop()
{
    MockDevice dev;
    QVector<double> seq(10, 1.0);
    dev.setSequence(seq);
    QVERIFY(dev.open(cdsw::DeviceConfig{ 1, 1000.0, 0 }));

    AcquisitionController ctrl(&dev);
    QSignalSpy started(&ctrl, &AcquisitionController::sigStarted);
    QSignalSpy stopped(&ctrl, &AcquisitionController::sigStopped);

    ctrl.start();
    ctrl.start();                              // 重复 start 忽略
    QTRY_COMPARE(ctrl.samplesRead(), qint64(10));
    ctrl.stop();
    ctrl.stop();                               // 重复 stop 忽略
    QCOMPARE(started.count(), 1);
    QCOMPARE(stopped.count(), 1);
}
```

main 加 `ControllerMockTest t3;` 与 `rc |= QTest::qExec(&t3, argc, argv);`。

- [ ] **Step 3: 编译 + 跑测试**

```bash
cmake --build build --target acq_tests
ctest --test-dir build --output-on-failure -R acq
```

Expected: 原 17 + 控制器 4 全过。

- [ ] **Step 4: Commit**

```bash
git add project/chromatography_workstation/acq/tests/t_acq.cpp
git commit -m "test(acq): 采集控制器 + MockDevice 测试（启停/顺序/错误/幂等）"
```

---

### Task 4: §5 复刻示例端到端 + 接收链集成 + 全量验证 + 提交文档

**Files:**
- Modify: `project/chromatography_workstation/acq/tests/t_acq.cpp`（加 ReplicationEndToEndTest + main）

**Interfaces:**
- Consumes: `HwRealtimeReceiver`（Task 2）、`RingBackedDevice`（Task 1）、`AcquisitionController`（Task 3）
- Produces: 规格 §8 要求的 §5 复刻示例端到端用例全绿。

- [ ] **Step 1: 追加 ReplicationEndToEndTest（§5 复刻示例 + 接收链集成）**

```cpp
// ---------- §5 复刻示例端到端 ----------
class ReplicationEndToEndTest : public QObject
{
    Q_OBJECT
private slots:
    void replicaExampleDecode();
    void replicaExampleThroughChain();
};

void ReplicationEndToEndTest::replicaExampleDecode()
{
    HwRealtimeReceiver rx;
    QVector<SampleEvent> aSamples, bSamples;
    QVector<HwChannel> started, stopped;
    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 rt, qint64 raw) {
                const SampleEvent e{ ch, rt, raw };
                if (ch == HwChannel::A) aSamples.append(e);
                else if (ch == HwChannel::B) bSamples.append(e);
            });
    connect(&rx, &HwRealtimeReceiver::sigAcquisitionStarted, &rx,
            [&](HwChannel ch) { started.append(ch); });
    connect(&rx, &HwRealtimeReceiver::sigAcquisitionStopped, &rx,
            [&](HwChannel ch) { stopped.append(ch); });

    // §5 示例：启动 A → 1000 点(A+占位B) → 停止 A → 10 尾点 A
    rx.receive(1, 0, 1);
    for (long d = 0; d < 1000; ++d) {
        rx.receive(d, 0, 3);
        rx.receive(0, 0, 4);
    }
    rx.receive(1, 0, 2);
    for (int i = 0; i < 10; ++i)
        rx.receive(999, 0, 3);

    QCOMPARE(started.size(), 1);
    QCOMPARE(started.at(0), HwChannel::A);
    QCOMPARE(stopped.size(), 1);
    QCOMPARE(stopped.at(0), HwChannel::A);

    // A：1010 点，RT=0..50450 步进 50，值 0..999 + 999×10
    QCOMPARE(aSamples.size(), 1010);
    for (int n = 0; n < 1010; ++n) {
        QCOMPARE(aSamples.at(n).rt, qint64(n) * 50);
        const qint64 expectVal = (n < 1000) ? qint64(n) : qint64(999);
        QCOMPARE(aSamples.at(n).raw, expectVal);
    }
    // B：1000 占位，RT=0..49950 步进 50，值全 0
    QCOMPARE(bSamples.size(), 1000);
    for (int n = 0; n < 1000; ++n) {
        QCOMPARE(bSamples.at(n).rt, qint64(n) * 50);
        QCOMPARE(bSamples.at(n).raw, qint64(0));
    }
}

void ReplicationEndToEndTest::replicaExampleThroughChain()
{
    // 接收链：receiver 解码 → RingBuffer(RingBackedDevice) → 控制器 → sigDataAcquired
    HwRealtimeReceiver rx;
    RingBackedDevice dev;
    QVERIFY(dev.open(cdsw::DeviceConfig{ 1, 1000.0, 0 }));

    connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
            [&](HwChannel ch, qint64 /*rt*/, qint64 raw) {
                if (ch == HwChannel::A) {
                    const double v = static_cast<double>(raw);
                    dev.m_buffer.write(&v, 1);
                }
            });

    AcquisitionController ctrl(&dev);
    QVector<double> collected;
    connect(&ctrl, &AcquisitionController::sigDataAcquired, &ctrl,
            [&](const QVector<double>& v) { collected.append(v); });

    ctrl.start();

    // 与解码测试相同的 §5 示例数据
    rx.receive(1, 0, 1);
    for (long d = 0; d < 1000; ++d) {
        rx.receive(d, 0, 3);
        rx.receive(0, 0, 4);
    }
    rx.receive(1, 0, 2);
    for (int i = 0; i < 10; ++i)
        rx.receive(999, 0, 3);

    QTRY_COMPARE(collected.size(), 1010);
    ctrl.stop();

    QCOMPARE(ctrl.samplesRead(), qint64(1010));
    QCOMPARE(collected.size(), 1010);
    for (int n = 0; n < 1010; ++n) {
        const double expectVal = (n < 1000) ? double(n) : 999.0;
        QCOMPARE(collected.at(n), expectVal);
    }
}
```

main 加 `ReplicationEndToEndTest t4;` 与 `rc |= QTest::qExec(&t4, argc, argv);`。

- [ ] **Step 2: 全量构建 + ctest 全绿**

```bash
cmake -S project/chromatography_workstation -B build -DCDSW_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure -R acq
```

Expected: acq_tests 23 个测试全过（RingBuffer 5 + 解码 12 + 控制器 4 + 复刻 2）。

- [ ] **Step 3: 纪律自查（D4）**

```bash
grep -rn "QtWidgets" project/chromatography_workstation/acq/    # 应为空
grep -rn "#include <QtGui>" project/chromatography_workstation/acq/   # 应为空
grep -rn "core_model" project/chromatography_workstation/acq/   # 只允许在 CMakeLists 依赖行，源码不应 include 其他模块 src
```

- [ ] **Step 4: Commit**

```bash
git add project/chromatography_workstation/acq/tests/t_acq.cpp
git commit -m "test(acq): §5 复刻示例端到端 + 接收链集成（receiver→RingBuffer→控制器）"
```

---

### Task 5: PROJECT.md + 记忆文件 + 收尾提交

**Files:**
- Modify: `PROJECT.md`（§2 acq 状态行：待开工 → 待合并）
- Create: `docs/memory/acq.md`
- Modify: `docs/superpowers/specs/2026-08-18-acq-realtime-receive-design.md`（若实现偏离设计，同步修订）

- [ ] **Step 1: 更新 PROJECT.md §2 acq 行**

状态列 `待开工` → `待合并`；下一步列改为 `M3a 实时反控协议接收链完成（HwRealtimeReceiver 解码 + RingBuffer + 控制器），等主控审查后合回 main；M3b IPC 端点待定`。

- [ ] **Step 2: 写 docs/memory/acq.md**

四节：做了什么 / 为什么这么设计 / 下一步 / 卡点。

- [ ] **Step 3: Commit 文档**

```bash
git add PROJECT.md docs/memory/acq.md docs/superpowers/specs/2026-08-18-acq-realtime-receive-design.md
git commit -m "doc(acq): PROJECT.md 状态 + 记忆文件 + 设计文档同步"
```

---

## Self-Review 检查（写完后自查）

- 规格覆盖：§4.3 hal.h 四类（DeviceConfig/IDevice/RingBuffer/AcquisitionController）→ Task 1；§4.3b hw_realtime.h 三类型（HwChannel/AutosamplerParams/HwRealtimeReceiver）→ Task 2；命令表 data3 0–17 → Task 2 Step 2；§5 复刻示例端到端 → Task 4；MockDevice 注入 → Task 3；§6 独立测试 → 每 Task。
- 用户决策覆盖：覆盖最旧保新 → Task 1 overflowOverwritesOldest；启动重置 RT → Task 2 startResetsRetentionCounter。
- 类型一致性：`SampleEvent{ch,rt,raw}`、`MockDevice::read` 返回 `qint64`、`RingBackedDevice::m_buffer`、`AcquisitionController::samplesRead()` 全链路一致。
- 无占位符：所有代码块为完整实现。
