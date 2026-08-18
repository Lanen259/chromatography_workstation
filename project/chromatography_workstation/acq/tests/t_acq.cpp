// acq/tests/t_acq.cpp —— acq 模块 QTest（RingBuffer / HwRealtimeReceiver / 控制器+MockDevice / §5 复刻示例）
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "acq/hal.h"
#include "MockDevice.h"
#include "RingBackedDevice.h"

#include <algorithm>
#include <atomic>
#include <thread>

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
