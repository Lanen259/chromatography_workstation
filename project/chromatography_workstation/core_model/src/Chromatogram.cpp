// core_model/src/Chromatogram.cpp —— 色谱图纯数据容器实现（契约 §4.1）
#include <core_model/Chromatogram.h>

#include <algorithm>

namespace cdsw {

QString Chromatogram::name() const { return m_name; }

void Chromatogram::setName(const QString& name) { m_name = name; }

QString Chromatogram::converterId() const { return m_converterId; }

void Chromatogram::setConverterId(const QString& converterId) { m_converterId = converterId; }

bool Chromatogram::isFinalized() const { return m_finalized; }

void Chromatogram::setFinalized(bool finalized) { m_finalized = finalized; }

qint64 Chromatogram::scanDelayMs() const { return m_scanDelayMs; }

void Chromatogram::setScanDelayMs(qint64 ms) { m_scanDelayMs = ms; }

qint64 Chromatogram::scanIntervalMs() const { return m_scanIntervalMs; }

void Chromatogram::setScanIntervalMs(qint64 ms) { m_scanIntervalMs = ms; }

void Chromatogram::setSignalPoints(const QVector<Signal>& points)
{
    // 直接拷贝：假定 Reader 已按 retentionTimeMs 升序交付，此处不排序
    m_signal = points;
}

const QVector<Signal>& Chromatogram::signalPoints() const { return m_signal; }

int Chromatogram::scanCount() const { return m_signal.size(); }

void Chromatogram::setProcessedPoints(const QVector<Signal>& points) { m_processed = points; }

// 独立存储：与 m_signal 互不覆盖，未处理时为空
const QVector<Signal>& Chromatogram::processedPoints() const { return m_processed; }

qint64 Chromatogram::startTimeMs() const
{
    if (m_signal.isEmpty())
        return 0;
    return m_signal.first().retentionTimeMs;
}

qint64 Chromatogram::stopTimeMs() const
{
    if (m_signal.isEmpty())
        return 0;
    return m_signal.last().retentionTimeMs;
}

double Chromatogram::minIntensity() const
{
    if (m_signal.isEmpty())
        return 0.0;
    double min = m_signal.first().intensity;
    for (const Signal& s : m_signal)
        if (s.intensity < min)
            min = s.intensity;
    return min;
}

double Chromatogram::maxIntensity() const
{
    if (m_signal.isEmpty())
        return 0.0;
    double max = m_signal.first().intensity;
    for (const Signal& s : m_signal)
        if (s.intensity > max)
            max = s.intensity;
    return max;
}

int Chromatogram::scanNumberAtRetentionTime(qint64 retentionTimeMs) const
{
    // floor 语义：返回「最后一个 retentionTimeMs <= rt 的点」之后的位置。
    // 升序序列上 lower_bound 找到首个 > rt 的点，其下标 = 满足条件的点数 = 1-based 扫描号。
    const auto it = std::lower_bound(
        m_signal.cbegin(), m_signal.cend(), retentionTimeMs,
        [](const Signal& s, qint64 v) { return s.retentionTimeMs <= v; });
    return static_cast<int>(it - m_signal.cbegin());
}

bool Chromatogram::isDirty() const { return m_dirty; }

void Chromatogram::setDirty(bool dirty) { m_dirty = dirty; }

} // namespace cdsw
