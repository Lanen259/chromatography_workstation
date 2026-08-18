// acq/tests/t_acq.cpp —— acq 模块 QTest（RingBuffer / HwRealtimeReceiver / 控制器+MockDevice / §5 复刻示例）
// 设计决策见 docs/superpowers/specs/2026-08-18-acq-realtime-receive-design.md（用户拍板：覆盖最旧保新、启动重置 RT 计数）。
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "acq/hal.h"
#include "acq/hw_realtime.h"
#include "MockDevice.h"
#include "RingBackedDevice.h"

#include <algorithm>
#include <atomic>
#include <thread>

using cdsw::RingBuffer;
using cdsw::AcquisitionController;
using cdsw::HwChannel;
using cdsw::AutosamplerParams;
using cdsw::HwRealtimeReceiver;

// ---------- 测试辅助 ----------

static int chIndex(HwChannel ch) { return static_cast<int>(ch); }

static QVector<double> makeRamp(int n)
{
    QVector<double> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i)
        v.append(double(i));
    return v;
}

// §5 复刻示例：启动 A → 1000 点(A + B 占位) → 停止 A → 10 尾点 A（值 999，助工作站跳出等待）。
static void feedReplicationExample(HwRealtimeReceiver& rx)
{
    rx.receive(1, 0, 1);                                  // 启动 A 通道
    for (long d = 0; d < 1000; ++d) {
        rx.receive(d, 0, 3);                              // A 通道值 0..999
        rx.receive(0, 0, 4);                              // 补发 B 通道占位 0
    }
    rx.receive(1, 0, 2);                                  // 停止 A 通道
    for (int i = 0; i < 10; ++i)
        rx.receive(999, 0, 3);                            // stop 后补发尾点 A（值 999）
}

// ---------- RingBuffer（覆盖最旧保新） ----------
class RingBufferTest : public QObject
{
    Q_OBJECT
private slots:
    void basicReadWrite();
    void wrapAround();
    void emptyRead();
    void overflowOverwritesOldest();
    void capacityClampAndAccessors();
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
    // 覆盖最旧保新：满时写新样本 → 最旧被覆盖，available 恒 == capacity，最新样本保留
    RingBuffer rb(4);
    const double in[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };  // 超容量
    QVERIFY(rb.write(in, 6));                             // 覆盖策略：不拒收
    QCOMPARE(rb.available(), 4);                          // 只保留最新 4 个
    double out[8] = {};
    QCOMPARE(rb.read(out, 8), 4);
    QVERIFY(out[0] == 3.0 && out[1] == 4.0 && out[2] == 5.0 && out[3] == 6.0);
}

void RingBufferTest::capacityClampAndAccessors()
{
    RingBuffer rb(0);                     // 非法容量钳到 1
    QCOMPARE(rb.capacity(), 1);
    const double one[1] = { 7.0 };
    QVERIFY(rb.write(one, 1));
    QCOMPARE(rb.available(), 1);
    RingBuffer rb2(8);
    QCOMPARE(rb2.capacity(), 8);
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

// ---------- HwRealtimeReceiver 解码（data3 0–17） ----------
class HwRealtimeReceiverTest : public QObject
{
    Q_OBJECT
private slots:
    void defaultSamplePeriod();
    void syncABSample();
    void singleChannelSamples();
    void augmentedChannelsCDEF();
    void startStopImmediateZero();
    void augmentedChannelStartStop();
    void autosamplerParams();
    void autosamplerZeroDenominator();
    void autosamplerParamsIsValid();
    void retentionTimeIncrements();
    void startCommandResetsSampleCount();
    void unknownData3Ignored();
};

void HwRealtimeReceiverTest::defaultSamplePeriod()
{
    HwRealtimeReceiver rx;
    QCOMPARE(rx.samplePeriodMs(), 50);     // 默认 50（= HWFrequence.txt）
    rx.setSamplePeriodMs(20);
    QCOMPARE(rx.samplePeriodMs(), 20);
}

void HwRealtimeReceiverTest::syncABSample()
{
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigChannelSample);
    rx.receive(10, 20, 0);                 // data3=0 双通道同步
    QCOMPARE(spy.count(), 2);
    QCOMPARE(chIndex(spy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::A));
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(0));
    QCOMPARE(spy.at(0).at(2).toLongLong(), qint64(10));
    QCOMPARE(chIndex(spy.at(1).at(0).value<HwChannel>()), chIndex(HwChannel::B));
    QCOMPARE(spy.at(1).at(1).toLongLong(), qint64(0));
    QCOMPARE(spy.at(1).at(2).toLongLong(), qint64(20));
    rx.receive(30, 40, 0);                 // 第二点：A/B 保留时间各 +50
    QCOMPARE(spy.count(), 4);
    QCOMPARE(spy.at(2).at(1).toLongLong(), qint64(50));
    QCOMPARE(spy.at(2).at(2).toLongLong(), qint64(30));
    QCOMPARE(spy.at(3).at(1).toLongLong(), qint64(50));
    QCOMPARE(spy.at(3).at(2).toLongLong(), qint64(40));
}

void HwRealtimeReceiverTest::singleChannelSamples()
{
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigChannelSample);
    rx.receive(100, 0, 3);                 // data3=3 A 单发
    rx.receive(0, 200, 4);                 // data3=4 B 单发（占位）
    QCOMPARE(spy.count(), 2);
    QCOMPARE(chIndex(spy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::A));
    QCOMPARE(spy.at(0).at(2).toLongLong(), qint64(100));
    QCOMPARE(chIndex(spy.at(1).at(0).value<HwChannel>()), chIndex(HwChannel::B));
    QCOMPARE(spy.at(1).at(2).toLongLong(), qint64(200));
    rx.receive(300, 0, 3);                 // A 第二点：rt 50
    QCOMPARE(spy.at(2).at(1).toLongLong(), qint64(50));
}

void HwRealtimeReceiverTest::augmentedChannelsCDEF()
{
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigChannelSample);
    rx.receive(7, 0, 10);                  // C
    rx.receive(8, 0, 12);                  // D
    rx.receive(9, 0, 14);                  // E
    rx.receive(10, 0, 16);                 // F
    QCOMPARE(spy.count(), 4);
    QCOMPARE(chIndex(spy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::C));
    QCOMPARE(spy.at(0).at(2).toLongLong(), qint64(7));
    QCOMPARE(chIndex(spy.at(1).at(0).value<HwChannel>()), chIndex(HwChannel::D));
    QCOMPARE(spy.at(1).at(2).toLongLong(), qint64(8));
    QCOMPARE(chIndex(spy.at(2).at(0).value<HwChannel>()), chIndex(HwChannel::E));
    QCOMPARE(spy.at(2).at(2).toLongLong(), qint64(9));
    QCOMPARE(chIndex(spy.at(3).at(0).value<HwChannel>()), chIndex(HwChannel::F));
    QCOMPARE(spy.at(3).at(2).toLongLong(), qint64(10));
}

void HwRealtimeReceiverTest::startStopImmediateZero()
{
    HwRealtimeReceiver rx;
    QSignalSpy startSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStarted);
    QSignalSpy stopSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStopped);
    QSignalSpy zeroSpy(&rx, &HwRealtimeReceiver::sigImmediateZero);

    rx.receive(1, 1, 1);                   // 启动 A + B
    QCOMPARE(startSpy.count(), 2);
    QCOMPARE(chIndex(startSpy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::A));
    QCOMPARE(chIndex(startSpy.at(1).at(0).value<HwChannel>()), chIndex(HwChannel::B));
    rx.receive(1, 0, 1);                   // 只启动 A
    QCOMPARE(startSpy.count(), 3);
    rx.receive(0, 0, 1);                   // 不动
    QCOMPARE(startSpy.count(), 3);

    rx.receive(1, 0, 2);                   // 只停止 A
    QCOMPARE(stopSpy.count(), 1);
    QCOMPARE(chIndex(stopSpy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::A));
    rx.receive(0, 1, 2);                   // 只停止 B
    QCOMPARE(stopSpy.count(), 2);
    QCOMPARE(chIndex(stopSpy.at(1).at(0).value<HwChannel>()), chIndex(HwChannel::B));

    rx.receive(1, 1, 5);                   // A/B 立即归零
    QCOMPARE(zeroSpy.count(), 2);
    QCOMPARE(chIndex(zeroSpy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::A));
    QCOMPARE(chIndex(zeroSpy.at(1).at(0).value<HwChannel>()), chIndex(HwChannel::B));
}

void HwRealtimeReceiverTest::augmentedChannelStartStop()
{
    HwRealtimeReceiver rx;
    QSignalSpy startSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStarted);
    QSignalSpy stopSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStopped);

    rx.receive(1, 0, 11);                  // 启动 C（data1=1）
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(chIndex(startSpy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::C));
    rx.receive(0, 1, 11);                  // 停止 C（data2=1）
    QCOMPARE(stopSpy.count(), 1);
    QCOMPARE(chIndex(stopSpy.at(0).at(0).value<HwChannel>()), chIndex(HwChannel::C));
    rx.receive(1, 1, 11);                  // 同时启停 C
    QCOMPARE(startSpy.count(), 2);
    QCOMPARE(stopSpy.count(), 2);

    rx.receive(1, 0, 13);                  // D 启动
    QCOMPARE(chIndex(startSpy.at(2).at(0).value<HwChannel>()), chIndex(HwChannel::D));
    rx.receive(1, 0, 15);                  // E 启动
    QCOMPARE(chIndex(startSpy.at(3).at(0).value<HwChannel>()), chIndex(HwChannel::E));
    rx.receive(1, 0, 17);                  // F 启动
    QCOMPARE(chIndex(startSpy.at(4).at(0).value<HwChannel>()), chIndex(HwChannel::F));
}

void HwRealtimeReceiverTest::autosamplerParams()
{
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigAutosamplerParamsChanged);
    rx.receive(1, 0, 6);                   // 样品类型 = 1
    QCOMPARE(rx.autosamplerParams().sampleType, 1);
    QCOMPARE(spy.count(), 1);
    rx.receive(1234, 1000, 7);             // 样品量 = 1234/1000 = 1.234
    QCOMPARE(rx.autosamplerParams().sampleAmount, 1.234);
    QCOMPARE(spy.count(), 2);
    rx.receive(250, 100, 8);               // 稀释倍数 = 2.5
    QCOMPARE(rx.autosamplerParams().dilutionFactor, 2.5);
    rx.receive(3, 2, 9);                   // 进样量比值 = 1.5
    QCOMPARE(rx.autosamplerParams().injectionVolumeRatio, 1.5);
    QCOMPARE(spy.count(), 4);
}

void HwRealtimeReceiverTest::autosamplerZeroDenominator()
{
    // 分母=0：字段置 0 不计算，仍发变更信号；isValid 转无效
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigAutosamplerParamsChanged);
    rx.receive(1, 0, 6);
    rx.receive(5, 0, 7);                   // 样品量分母=0 → sampleAmount=0
    QCOMPARE(rx.autosamplerParams().sampleAmount, 0.0);
    QVERIFY(!rx.autosamplerParams().isValid());
    rx.receive(0, 0, 8);                   // 稀释倍数分母=0 → dilutionFactor=0
    QCOMPARE(rx.autosamplerParams().dilutionFactor, 0.0);
    rx.receive(0, 0, 9);                   // 进样量比值分母=0 → injectionVolumeRatio=0
    QCOMPARE(rx.autosamplerParams().injectionVolumeRatio, 0.0);
    QCOMPARE(spy.count(), 4);              // data3=6/7/8/9 各发一次
}

void HwRealtimeReceiverTest::autosamplerParamsIsValid()
{
    AutosamplerParams p;
    QVERIFY(p.isValid());                  // 默认 sampleType=0、三值 1.0 → 有效
    p.sampleType = 5;                      // 越界
    QVERIFY(!p.isValid());
    p.sampleType = -1;
    QVERIFY(!p.isValid());
    p.sampleType = 4;
    p.sampleAmount = 0.0;                  // 分母 0 置 0 → 无效
    QVERIFY(!p.isValid());
    p.sampleAmount = 1.0;
    p.dilutionFactor = 0.0;
    QVERIFY(!p.isValid());
    p.dilutionFactor = 1.0;
    p.injectionVolumeRatio = 0.0;
    QVERIFY(!p.isValid());
}

void HwRealtimeReceiverTest::retentionTimeIncrements()
{
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigChannelSample);
    rx.receive(1, 0, 3);                   // A rt=0
    rx.receive(2, 0, 3);                   // A rt=50
    rx.receive(3, 0, 3);                   // A rt=100
    rx.receive(4, 0, 4);                   // B rt=0（独立计数）
    rx.receive(5, 0, 4);                   // B rt=50
    QCOMPARE(spy.count(), 5);
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(0));
    QCOMPARE(spy.at(1).at(1).toLongLong(), qint64(50));
    QCOMPARE(spy.at(2).at(1).toLongLong(), qint64(100));
    QCOMPARE(spy.at(3).at(1).toLongLong(), qint64(0));
    QCOMPARE(spy.at(4).at(1).toLongLong(), qint64(50));
}

void HwRealtimeReceiverTest::startCommandResetsSampleCount()
{
    // 启动命令把被启动通道计数清零：start 后首点 RT=0
    HwRealtimeReceiver rx;
    QSignalSpy spy(&rx, &HwRealtimeReceiver::sigChannelSample);
    rx.receive(1, 0, 3);                   // A rt=0
    rx.receive(2, 0, 3);                   // A rt=50
    rx.receive(1, 0, 1);                   // 启动 A → 计数清零
    rx.receive(3, 0, 3);                   // A rt=0（重置后重新计时）
    rx.receive(4, 0, 3);                   // A rt=50
    QCOMPARE(spy.count(), 4);
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(0));
    QCOMPARE(spy.at(1).at(1).toLongLong(), qint64(50));
    QCOMPARE(spy.at(2).at(1).toLongLong(), qint64(0));
    QCOMPARE(spy.at(3).at(1).toLongLong(), qint64(50));
}

void HwRealtimeReceiverTest::unknownData3Ignored()
{
    HwRealtimeReceiver rx;
    QSignalSpy sampleSpy(&rx, &HwRealtimeReceiver::sigChannelSample);
    QSignalSpy startSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStarted);
    QSignalSpy stopSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStopped);
    QSignalSpy asSpy(&rx, &HwRealtimeReceiver::sigAutosamplerParamsChanged);
    rx.receive(0, 0, 18);                  // 未定义命令
    rx.receive(0, 0, 99);
    QCOMPARE(sampleSpy.count(), 0);
    QCOMPARE(startSpy.count(), 0);
    QCOMPARE(stopSpy.count(), 0);
    QCOMPARE(asSpy.count(), 0);
}

// ---------- 采集控制器 + MockDevice ----------
class ControllerMockTest : public QObject
{
    Q_OBJECT
private slots:
    void mockDeviceDeliversInOrder();
    void deviceErrorStopsAcquisition();
};

void ControllerMockTest::mockDeviceDeliversInOrder()
{
    MockDevice dev;
    dev.setSequence(makeRamp(100));
    AcquisitionController ctrl(&dev);
    QSignalSpy startedSpy(&ctrl, &AcquisitionController::sigStarted);
    QSignalSpy stoppedSpy(&ctrl, &AcquisitionController::sigStopped);
    QSignalSpy acquiredSpy(&ctrl, &AcquisitionController::sigDataAcquired);
    // 排队连接（主线程事件循环内累加）：与跨线程 queued 信号一致，不并发读 spy
    std::atomic<int> received{ 0 };
    QObject::connect(&ctrl, &AcquisitionController::sigDataAcquired, &ctrl,
                     [&received](const QVector<double>& s) { received += s.size(); });

    ctrl.start();
    QCOMPARE(startedSpy.count(), 1);       // sigStarted 同步发出
    QTRY_VERIFY_WITH_TIMEOUT(received.load() == 100, 5000);
    ctrl.stop();
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(ctrl.samplesRead(), qint64(100));

    double expected = 0.0;
    int total = 0;
    for (const auto& args : acquiredSpy) {   // stop() join 后读，无并发
        const QVector<double> batch = args.at(0).value<QVector<double>>();
        for (double v : batch) {
            QCOMPARE(v, expected);         // 顺序一致 0..99
            ++expected;
        }
        total += batch.size();
    }
    QCOMPARE(total, 100);
}

void ControllerMockTest::deviceErrorStopsAcquisition()
{
    MockDevice dev;
    dev.setFailRead(true);                 // read 返回 -1（设备错误哨兵）
    AcquisitionController ctrl(&dev);
    QSignalSpy errorSpy(&ctrl, &AcquisitionController::sigDeviceError);
    QSignalSpy stoppedSpy(&ctrl, &AcquisitionController::sigStopped);
    ctrl.start();
    QVERIFY(errorSpy.wait(2000));          // read<0 → 发错误并退出采集循环
    QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
    QCOMPARE(stoppedSpy.count(), 0);       // 尚未调用 stop()
    ctrl.stop();
    QCOMPARE(stoppedSpy.count(), 1);
}

// ---------- §5 协议复刻示例端到端 ----------
class ReplicationEndToEndTest : public QObject
{
    Q_OBJECT
private slots:
    void replicationExample();
};

void ReplicationEndToEndTest::replicationExample()
{
    HwRealtimeReceiver rx;
    RingBackedDevice dev(4096);
    // 解码采样事件 → RingBackedDevice 内 RingBuffer（覆盖最旧保新）；链路只关注 A 通道
    QObject::connect(&rx, &HwRealtimeReceiver::sigChannelSample, &rx,
                     [&dev](HwChannel ch, qint64, qint64 raw) {
                         if (ch == HwChannel::A) {
                             const double d = static_cast<double>(raw);
                             dev.m_buffer.write(&d, 1);
                         }
                     });

    QSignalSpy startedSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStarted);
    QSignalSpy stoppedSpy(&rx, &HwRealtimeReceiver::sigAcquisitionStopped);
    QSignalSpy sampleSpy(&rx, &HwRealtimeReceiver::sigChannelSample);

    AcquisitionController ctrl(&dev);
    QSignalSpy ctrlStartedSpy(&ctrl, &AcquisitionController::sigStarted);
    QSignalSpy ctrlStoppedSpy(&ctrl, &AcquisitionController::sigStopped);
    QSignalSpy acquiredSpy(&ctrl, &AcquisitionController::sigDataAcquired);
    std::atomic<int> received{ 0 };
    QObject::connect(&ctrl, &AcquisitionController::sigDataAcquired, &ctrl,
                     [&received](const QVector<double>& s) { received += s.size(); });

    ctrl.start();
    QCOMPARE(ctrlStartedSpy.count(), 1);
    feedReplicationExample(rx);            // 启动 → 1000 点 → 停止 → 10 尾点

    // 控制器把缓冲全部排空并发信号（事件循环驱动 queued 信号）
    QTRY_VERIFY_WITH_TIMEOUT(received.load() == 1010, 5000);
    ctrl.stop();
    QCOMPARE(ctrlStoppedSpy.count(), 1);
    QCOMPARE(ctrl.samplesRead(), qint64(1010));

    // —— 解码断言（规格 §8：启动/停止各 1、采样点数与保留时间）——
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(stoppedSpy.count(), 1);

    qint64 lastARt = -50, lastBRt = -50;
    int aCount = 0, bCount = 0;
    int aIndex = 0;
    for (const auto& args : sampleSpy) {
        const HwChannel ch = args.at(0).value<HwChannel>();
        const qint64 rt = args.at(1).toLongLong();
        if (ch == HwChannel::A) {
            QCOMPARE(rt, lastARt + 50);    // 保留时间按 50ms/点递增
            lastARt = rt;
            const qint64 expectVal = (aIndex < 1000) ? qint64(aIndex) : qint64(999);
            QCOMPARE(args.at(2).toLongLong(), expectVal);  // A 值 0..999 + 999×10
            ++aIndex;
            ++aCount;
        } else {
            QCOMPARE(rt, lastBRt + 50);
            lastBRt = rt;
            QCOMPARE(args.at(2).toLongLong(), qint64(0));  // B 占位恒 0
            ++bCount;
        }
    }
    QCOMPARE(aCount, 1010);                // 1000 点 + 10 尾点
    QCOMPARE(bCount, 1000);                // 主循环 B 占位（尾点不再补发 B）
    QCOMPARE(aIndex, 1010);

    // —— 链路断言：经 RingBuffer→控制器送出的 A 值 0..999 + 999×10 顺序一致 ——
    int n = 0;
    int total = 0;
    for (const auto& args : acquiredSpy) {
        const QVector<double> batch = args.at(0).value<QVector<double>>();
        for (double v : batch) {
            const double expectVal = (n < 1000) ? double(n) : 999.0;
            QCOMPARE(v, expectVal);
            ++n;
        }
        total += batch.size();
    }
    QCOMPARE(total, 1010);
}

// 自定义 main：QCoreApplication 供跨线程 queued 信号投递；四组测试依序执行。
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;
    {
        RingBufferTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        HwRealtimeReceiverTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        ControllerMockTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        ReplicationEndToEndTest t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "t_acq.moc"
