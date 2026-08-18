// core_processing/src/IntegratorTrapezoid.cpp —— 梯形积分实现
#include "IntegratorTrapezoid.h"

#include "WorkingSignal.h"

#include <algorithm>

namespace cdsw {

namespace {
// OpenChrom trapezoid 校正因子（MODULE_04 §4.3 PK-BA：ChemStation 因子，RT ms→百 ms 归一化）
constexpr double kCorrectionFactorTrapezoid = 100.0;

double linearInterp(double x0, double y0, double x1, double y1, double x)
{
    return (x1 != x0) ? y0 + (y1 - y0) * (x - x0) / (x1 - x0) : y0;
}
} // namespace

IntegratorTrapezoid::IntegratorTrapezoid() = default;

void IntegratorTrapezoid::configure(const QVariantMap& p)
{
    m_useAreaConstraint = p.value(QStringLiteral("useAreaConstraint"), true).toBool();
}

QString IntegratorTrapezoid::id() const { return QStringLiteral("trapezoid_integrator"); }

void IntegratorTrapezoid::integrate(Chromatogram& chrom, QList<Peak>& peaks) const
{
    const QVector<Signal>& sig = workingSignal(chrom);
    for (Peak& peak : peaks) {
        // 收集峰内点：优先用峰自带 profile，否则从色谱工作信号按 [start,stop] 提取
        QVector<Signal> pts = peak.profile;
        if (pts.isEmpty()) {
            const int startIdx = chrom.scanNumberAtRetentionTime(peak.startRTMs) - 1;
            const int stopIdx = chrom.scanNumberAtRetentionTime(peak.stopRTMs) - 1;
            if (startIdx >= 0 && stopIdx >= startIdx && stopIdx < sig.size())
                pts = sig.mid(startIdx, stopIdx - startIdx + 1);
        }
        peak.peakArea = 0.0;
        if (pts.size() < 2)
            continue;

        // VV 背景：峰两端信号点连直线
        const Signal& p0 = pts.constFirst();
        const Signal& p1 = pts.constLast();
        const double x0 = static_cast<double>(p0.retentionTimeMs);
        const double x1 = static_cast<double>(p1.retentionTimeMs);

        double area = 0.0;
        for (int i = 0; i + 1 < pts.size(); ++i) {
            const Signal& a = pts.at(i);
            const Signal& b = pts.at(i + 1);
            const double dt = static_cast<double>(b.retentionTimeMs - a.retentionTimeMs);
            if (dt <= 0.0)
                continue;
            const double bgA =
                linearInterp(x0, p0.intensity, x1, p1.intensity, static_cast<double>(a.retentionTimeMs));
            const double bgB =
                linearInterp(x0, p0.intensity, x1, p1.intensity, static_cast<double>(b.retentionTimeMs));
            const double pureA = std::max(0.0, a.intensity - bgA); // 建峰时已扣背景（纯信号 ≥0）
            const double pureB = std::max(0.0, b.intensity - bgB);
            area += (pureA + pureB) * 0.5 * dt;
        }
        area /= kCorrectionFactorTrapezoid;
        if (m_useAreaConstraint ? (area < 1.0) : (area < 0.0))
            area = 0.0;
        peak.peakArea = area;
    }
}

} // namespace cdsw
