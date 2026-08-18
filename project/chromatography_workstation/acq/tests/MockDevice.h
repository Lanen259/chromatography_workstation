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
