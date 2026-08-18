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

static_assert(std::atomic<double>::is_always_lock_free,
              "RingBuffer 依赖 std::atomic<double> 无锁（x86-64 对齐 8 字节）");

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
