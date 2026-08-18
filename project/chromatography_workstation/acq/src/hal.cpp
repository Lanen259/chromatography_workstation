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
