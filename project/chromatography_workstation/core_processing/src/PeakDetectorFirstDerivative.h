// core_processing/src/PeakDetectorFirstDerivative.h —— 一阶导数峰检测（IPeakDetector 实现）
//
// 本工程核心算法，逆向 OpenChrom `PeakDetectorCSD.java`（MODULE_04 §2.2，全链可移植）。
// 参数（configure / Method.step.parameters）：
//   "threshold"  档位 "OFF"/"LOW"/"MEDIUM"/"HIGH"（或数值字符串），默认 "MEDIUM"
//                OFF=0.0005 / LOW=0.005 / MEDIUM=0.05 / HIGH=0.5（BasePeakDetector 硬编码）
//   "windowSize" 斜率居中滑动平均窗（0=关闭），默认 5（0–45 奇数含 0）
// 流程：归一化(NORMALIZATION_BASE=100000) → 相邻斜率 → 可选滑动平均 →
//       峰起 = 连续 3 斜率 > 阈值且严格递增 → 峰顶 = 斜率首次 < 0（一阶导数过零）→
//       峰止 = 峰顶后斜率首次回正（进入下一峰上升沿即截断）→ 宽度 ≥ 3 才建峰。
#pragma once
#include <core_processing/interfaces.h>

#include <QtCore/qstring.h>

#include "IConfigurable.h"
namespace cdsw {

class PeakDetectorFirstDerivative final : public IPeakDetector, public IConfigurable {
public:
    PeakDetectorFirstDerivative();
    void configure(const QVariantMap& parameters) override;
    QString id() const override;
    QList<IRawPeak*> detect(const Chromatogram& chrom) const override;

    // 阈值映射（BasePeakDetector 硬编码，测试验证用）
    static double thresholdValue(const QString& thresholdName);

private:
    QString m_thresholdName = QStringLiteral("MEDIUM");
    int m_windowSize = 5;
};

} // namespace cdsw
