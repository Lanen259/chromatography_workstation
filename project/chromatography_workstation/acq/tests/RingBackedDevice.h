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
