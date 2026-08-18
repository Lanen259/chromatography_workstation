// acq/include/acq/hw_realtime.h —— 实时反控接收器（契约 §4.3b，签名冻结）
// 接收侧自定义：逐条喂入命令三元组 (data1,data2,data3) → 解码为事件。传输无关。
#pragma once
#include <QtCore/qglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

namespace cdsw {

// 协议通道：A/B 原始双通道 + C/D/E/F 增补（data3 3/4/10/12/14/16）
enum class HwChannel { A, B, C, D, E, F };

// 自动进样扩展参数（data3 6~9；浮点用分子/分母表示）
struct AutosamplerParams {
    int sampleType = 0;              // 0标准 / 1单点 / 2多点 / 3普通归一 / 4校正归一
    double sampleAmount = 1.0;       // data3=7 分子/分母
    double dilutionFactor = 1.0;     // data3=8 分子/分母
    double injectionVolumeRatio = 1.0; // data3=9 分子/分母
    bool isValid() const;            // sampleType∈[0,4] 且三值 >0（默认 1.0 恒有效；分母 0 → 值 0 → 无效）
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
    void sigChannelSample(HwChannel ch, qint64 retentionMs, qint64 rawValue); // raw=最小单位整数
    void sigAcquisitionStarted(HwChannel ch);
    void sigAcquisitionStopped(HwChannel ch);
    void sigImmediateZero(HwChannel ch);
    void sigAutosamplerParamsChanged(const AutosamplerParams& params);
private:
    void emitChannelSample(HwChannel ch, qint64 rawValue);   // 计数 → 保留时间 → 发采样信号
    int m_samplePeriodMs = 50;
    qint64 m_sampleCount[6] = { 0, 0, 0, 0, 0, 0 };   // 每通道采样计数 → 保留时间 = 计数 × 周期
    AutosamplerParams m_autosampler;
};

} // namespace cdsw

Q_DECLARE_METATYPE(cdsw::HwChannel)
Q_DECLARE_METATYPE(cdsw::AutosamplerParams)
