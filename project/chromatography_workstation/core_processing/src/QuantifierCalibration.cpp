// core_processing/src/QuantifierCalibration.cpp —— 校准曲线定量实现
#include "QuantifierCalibration.h"

namespace cdsw {

void QuantifierCalibration::configure(const QVariantMap& p)
{
    m_unit = p.value(QStringLiteral("unit")).toString();
}

QString QuantifierCalibration::id() const { return QStringLiteral("calibration_curve"); }

QList<QuantEntry> QuantifierCalibration::quantitate(const QList<Peak>& peaks,
                                                    const CalibrationTable& calib) const
{
    // 线性拟合 area = a·conc + b（最小二乘）
    double a = 0.0;
    double b = 0.0;
    const int np = calib.points.size();
    if (np >= 2) {
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        for (const CalibrationPoint& pt : calib.points) {
            sx += pt.concentration;
            sy += pt.area;
            sxx += pt.concentration * pt.concentration;
            sxy += pt.concentration * pt.area;
        }
        const double denom = np * sxx - sx * sx;
        if (denom != 0.0) {
            a = (np * sxy - sx * sy) / denom;
            b = (sy - a * sx) / np;
        }
    } else if (np == 1) {
        // 单点校准：过原点直线
        const CalibrationPoint& pt = calib.points.constFirst();
        if (pt.concentration != 0.0)
            a = pt.area / pt.concentration;
        b = 0.0;
    }

    QList<QuantEntry> out;
    out.reserve(peaks.size());
    for (const Peak& peak : peaks) {
        QuantEntry e;
        e.apexRTMs = peak.apexRTMs;
        e.componentName = calib.componentName;
        e.area = peak.peakArea;
        e.concentration = (a != 0.0) ? (peak.peakArea - b) / a : 0.0;
        e.unit = m_unit;
        out.append(e);
    }
    return out;
}

} // namespace cdsw
