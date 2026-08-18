// core_processing/src/RawPeak.h —— 一阶导数峰检测的原始峰载体（IRawPeak 实现）
//
// 契约 §4.2 约定「实现者返回堆对象，由调用方释放」：峰检测返回 QList<IRawPeak*>，
// 由管线/调用方 qDeleteAll 释放。仅存三要素 RT，峰高/剖面在管线转 Peak 时从色谱工作信号补齐。
#pragma once
#include <core_processing/interfaces.h>
namespace cdsw {

class RawPeak final : public IRawPeak {
public:
    RawPeak(qint64 startRTMs, qint64 apexRTMs, qint64 stopRTMs)
        : m_start(startRTMs), m_apex(apexRTMs), m_stop(stopRTMs)
    {
    }
    qint64 startRTMs() const override { return m_start; }
    qint64 apexRTMs() const override { return m_apex; }
    qint64 stopRTMs() const override { return m_stop; }

private:
    qint64 m_start = 0;
    qint64 m_apex = 0;
    qint64 m_stop = 0;
};

} // namespace cdsw
