// core_processing/src/BaselineLinear.cpp —— 线性基线实现
#include "BaselineLinear.h"

#include "WorkingSignal.h"

namespace cdsw {

void BaselineLinear::configure(const QVariantMap&) {}

QString BaselineLinear::id() const { return QStringLiteral("baseline_linear"); }

QVector<Signal> BaselineLinear::detect(const Chromatogram& chrom) const
{
    const QVector<Signal>& sig = workingSignal(chrom);
    QVector<Signal> baseline;
    if (sig.size() < 2)
        return baseline;

    const Signal& first = sig.constFirst();
    const Signal& last = sig.constLast();
    const double x0 = static_cast<double>(first.retentionTimeMs);
    const double x1 = static_cast<double>(last.retentionTimeMs);
    const double y0 = first.intensity;
    const double y1 = last.intensity;

    baseline.reserve(sig.size());
    for (const Signal& s : sig) {
        Signal b;
        b.retentionTimeMs = s.retentionTimeMs;
        b.intensity = (x1 != x0) ? y0 + (y1 - y0) * (s.retentionTimeMs - x0) / (x1 - x0) : y0;
        baseline.append(b);
    }
    return baseline;
}

} // namespace cdsw
