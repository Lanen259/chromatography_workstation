// acq/src/hw_realtime.cpp —— 实时反控接收器实现（契约 §4.3b）：命令三元组 → 事件解码
// 命令表语义：docs/protocol/HWSendData_实时反控协议.md §2（data3 0–17 发送侧冻结）。
#include <acq/hw_realtime.h>

#include <QtCore/qmetatype.h>

namespace cdsw {

namespace {
int channelIndex(HwChannel ch)
{
    switch (ch) {
    case HwChannel::A: return 0;
    case HwChannel::B: return 1;
    case HwChannel::C: return 2;
    case HwChannel::D: return 3;
    case HwChannel::E: return 4;
    case HwChannel::F: return 5;
    }
    return 0;
}
} // namespace

// 校验：sampleType∈[0,4] 且三值 >0（默认 1.0 恒有效；分母 0 → 值 0 → 无效）
bool AutosamplerParams::isValid() const
{
    return sampleType >= 0 && sampleType <= 4
        && sampleAmount > 0.0 && dilutionFactor > 0.0 && injectionVolumeRatio > 0.0;
}

HwRealtimeReceiver::HwRealtimeReceiver(QObject* parent)
    : QObject(parent)
{
    // 跨线程 queued 投递 + QSignalSpy 参数解析：按 moc 记录的信号参数名注册
    qRegisterMetaType<HwChannel>("HwChannel");
    qRegisterMetaType<AutosamplerParams>("AutosamplerParams");
}

void HwRealtimeReceiver::receive(long data1, long data2, long data3)
{
    switch (data3) {
    case 0:                              // A、B 双通道同步采样
        emitChannelSample(HwChannel::A, data1);
        emitChannelSample(HwChannel::B, data2);
        break;
    case 1:                              // 启动 A/B 图谱采集：先清零被启动通道计数（一次采集从 0 计时）
        if (data1 == 1) { m_sampleCount[0] = 0; emit sigAcquisitionStarted(HwChannel::A); }
        if (data2 == 1) { m_sampleCount[1] = 0; emit sigAcquisitionStarted(HwChannel::B); }
        break;
    case 2:                              // 停止 A/B 图谱采集
        if (data1 == 1) emit sigAcquisitionStopped(HwChannel::A);
        if (data2 == 1) emit sigAcquisitionStopped(HwChannel::B);
        break;
    case 3:                              // 单独发送 A 通道采样数据
        emitChannelSample(HwChannel::A, data1);
        break;
    case 4:                              // 单独发送 B 通道采样数据
        emitChannelSample(HwChannel::B, data2);
        break;
    case 5:                              // A/B 立即归零
        if (data1 == 1) emit sigImmediateZero(HwChannel::A);
        if (data2 == 1) emit sigImmediateZero(HwChannel::B);
        break;
    case 6:                              // 样品类型
        m_autosampler.sampleType = static_cast<int>(data1);
        emit sigAutosamplerParamsChanged(m_autosampler);
        break;
    case 7:                              // 样品量（分子/分母）；分母 0 → 置 0 不计算
        m_autosampler.sampleAmount = data2 != 0 ? static_cast<double>(data1) / data2 : 0.0;
        emit sigAutosamplerParamsChanged(m_autosampler);
        break;
    case 8:                              // 稀释倍数（分子/分母）；分母 0 → 置 0 不计算
        m_autosampler.dilutionFactor = data2 != 0 ? static_cast<double>(data1) / data2 : 0.0;
        emit sigAutosamplerParamsChanged(m_autosampler);
        break;
    case 9:                              // 进样量比值（分子/分母）；分母 0 → 置 0 不计算
        m_autosampler.injectionVolumeRatio = data2 != 0 ? static_cast<double>(data1) / data2 : 0.0;
        emit sigAutosamplerParamsChanged(m_autosampler);
        break;
    case 10:                             // 单独发送 C 通道采样数据
        emitChannelSample(HwChannel::C, data1);
        break;
    case 11:                             // 控制 C 通道启停：启动先清零计数
        if (data1 == 1) { m_sampleCount[2] = 0; emit sigAcquisitionStarted(HwChannel::C); }
        if (data2 == 1) emit sigAcquisitionStopped(HwChannel::C);
        break;
    case 12:                             // 单独发送 D 通道采样数据
        emitChannelSample(HwChannel::D, data1);
        break;
    case 13:                             // 控制 D 通道启停：启动先清零计数
        if (data1 == 1) { m_sampleCount[3] = 0; emit sigAcquisitionStarted(HwChannel::D); }
        if (data2 == 1) emit sigAcquisitionStopped(HwChannel::D);
        break;
    case 14:                             // 单独发送 E 通道采样数据
        emitChannelSample(HwChannel::E, data1);
        break;
    case 15:                             // 控制 E 通道启停：启动先清零计数
        if (data1 == 1) { m_sampleCount[4] = 0; emit sigAcquisitionStarted(HwChannel::E); }
        if (data2 == 1) emit sigAcquisitionStopped(HwChannel::E);
        break;
    case 16:                             // 单独发送 F 通道采样数据
        emitChannelSample(HwChannel::F, data1);
        break;
    case 17:                             // 控制 F 通道启停：启动先清零计数
        if (data1 == 1) { m_sampleCount[5] = 0; emit sigAcquisitionStarted(HwChannel::F); }
        if (data2 == 1) emit sigAcquisitionStopped(HwChannel::F);
        break;
    default:
        break;                           // 未定义命令：忽略（命令表 data3 0–17）
    }
}

void HwRealtimeReceiver::setSamplePeriodMs(int periodMs)
{
    m_samplePeriodMs = qMax(1, periodMs);   // 周期 ≥1ms，避免保留时间退化
}

int HwRealtimeReceiver::samplePeriodMs() const { return m_samplePeriodMs; }

AutosamplerParams HwRealtimeReceiver::autosamplerParams() const { return m_autosampler; }

void HwRealtimeReceiver::emitChannelSample(HwChannel ch, qint64 rawValue)
{
    const int idx = channelIndex(ch);
    const qint64 retentionMs = m_sampleCount[idx] * m_samplePeriodMs;   // 第 n 点（n 从 0）→ n×周期
    ++m_sampleCount[idx];
    emit sigChannelSample(ch, retentionMs, rawValue);
}

} // namespace cdsw
