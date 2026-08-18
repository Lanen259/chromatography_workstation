// core_processing/src/PeakDetectorFirstDerivative.cpp —— 一阶导数峰检测实现
#include "PeakDetectorFirstDerivative.h"

#include "RawPeak.h"
#include "WorkingSignal.h"

#include <algorithm>
#include <cmath>

namespace cdsw {

namespace {

// OpenChrom BasePeakDetector 常量（MODULE_04 §2.2.4，源码确认）
constexpr double kNormalizationBase = 100000.0; // NORMALIZATION_BASE：峰值缩放到 10 万
constexpr int kConsecutiveScanSteps = 3;        // CONSECUTIVE_SCAN_STEPS：连续斜率判定个数

} // namespace

PeakDetectorFirstDerivative::PeakDetectorFirstDerivative() = default;

void PeakDetectorFirstDerivative::configure(const QVariantMap& p)
{
    m_thresholdName =
        p.value(QStringLiteral("threshold"), QStringLiteral("MEDIUM")).toString();
    m_windowSize = p.value(QStringLiteral("windowSize"), 5).toInt();
    // 与 OpenChrom IntSettings 校验一致（0–45 奇数含 0，MODULE_04 §2.2.5）：越界/负数钳位，
    // 偶数强制奇数，防止 detect() 里 diff=windowSize/2 产生越界写
    m_windowSize = qBound(0, m_windowSize, 45);
    if (m_windowSize > 0 && (m_windowSize % 2) == 0)
        --m_windowSize;
}

QString PeakDetectorFirstDerivative::id() const
{
    return QStringLiteral("first_derivative_peak_detector");
}

double PeakDetectorFirstDerivative::thresholdValue(const QString& thresholdName)
{
    const QString name = thresholdName.trimmed().toUpper();
    if (name == QStringLiteral("OFF"))
        return 0.0005;
    if (name == QStringLiteral("LOW"))
        return 0.005;
    if (name == QStringLiteral("HIGH"))
        return 0.5;
    if (name == QStringLiteral("MEDIUM"))
        return 0.05;
    bool ok = false;
    const double v = name.toDouble(&ok); // 数值字符串（如 "0.1"）
    return (ok && v > 0.0) ? v : 0.05;   // 非法 → MEDIUM
}

QList<IRawPeak*> PeakDetectorFirstDerivative::detect(const Chromatogram& chrom) const
{
    QList<IRawPeak*> peaks;
    const QVector<Signal>& sig = workingSignal(chrom);
    const int n = sig.size();
    if (n < kConsecutiveScanSteps + 1) // 至少 4 点才有 3 个斜率
        return peaks;

    // ① 归一化（TotalScanSignalsModifier.normalize(signals, NORMALIZATION_BASE)）：
    //    峰值缩放到 10 万，阈值与绝对丰度无关
    double maxIntensity = 0.0;
    for (const Signal& s : sig)
        maxIntensity = std::max(maxIntensity, s.intensity);
    QVector<double> y(n);
    if (maxIntensity > 0.0) {
        const double factor = kNormalizationBase / maxIntensity;
        for (int i = 0; i < n; ++i)
            y[i] = sig.at(i).intensity * factor;
    }

    // ② 相邻斜率 slope[i] = (y[i+1]-y[i])/(x[i+1]-x[i])，共 n-1 个
    const int S = n - 1;
    QVector<double> slopes(S);
    for (int i = 0; i < S; ++i) {
        const qint64 dx = sig.at(i + 1).retentionTimeMs - sig.at(i).retentionTimeMs;
        slopes[i] = (dx != 0) ? (y[i + 1] - y[i]) / static_cast<double>(dx) : 0.0;
    }

    // ③ 可选居中滑动平均（windowSize=0 关闭；首尾 windowSize/2 个保持原值）
    if (m_windowSize != 0 && slopes.size() >= m_windowSize) {
        const int diff = m_windowSize / 2;
        QVector<double> smoothed = slopes;
        for (int i = diff; i < slopes.size() - diff; ++i) {
            double sum = 0.0;
            for (int k = 0; k < m_windowSize; ++k)
                sum += slopes.at(i - diff + k);
            smoothed[i] = sum / m_windowSize;
        }
        slopes = smoothed;
    }

    // ④ getRawPeaks（BasePeakDetector）：阈值 → 峰区间判定
    const double threshold = thresholdValue(m_thresholdName);
    if (S < kConsecutiveScanSteps + 1)
        return peaks;
    // Java `limit = size - CONSECUTIVE_SCAN_STEPS`（1 基）→ 0 基末次 = size-4
    const int lastScan = S - kConsecutiveScanSteps - 1;
    for (int i = 0; i <= lastScan; ++i) {
        // detectPeakStart：峰起 = 连续 3 斜率同时 > 阈值 且 严格递增（3 阶拐点上升支）
        int startIdx = -1;
        for (int j = i; j <= lastScan; ++j) {
            const double v0 = slopes.at(j);
            const double v1 = slopes.at(j + 1);
            const double v2 = slopes.at(j + 2);
            if (v0 > threshold && v1 > threshold && v2 > threshold && v0 < v1 && v1 < v2) {
                startIdx = j;
                break;
            }
        }
        if (startIdx < 0) // 之后不可能再有峰
            break;
        // detectPeakMaximum：峰顶 = 斜率首次 < 0（一阶导数过零 = 真实峰顶）
        int apexIdx = startIdx;
        for (int j = startIdx; j <= lastScan; ++j) {
            if (slopes.at(j) < 0.0) {
                apexIdx = j;
                break;
            }
        }
        // detectPeakStop：峰止 = 峰顶后斜率首次回正（进入下一峰上升沿即截断）；默认 lastScan
        int stopIdx = lastScan;
        for (int j = apexIdx; j <= lastScan; ++j) {
            if (slopes.at(j) > 0.0) {
                stopIdx = j;
                break;
            }
        }
        // isValidRawPeak：宽度 ≥ 3 扫描（IPeakModel.MINIMUM_SCANS）
        if (stopIdx - startIdx + 1 >= kConsecutiveScanSteps) {
            peaks.append(new RawPeak(sig.at(startIdx).retentionTimeMs,
                                     sig.at(apexIdx).retentionTimeMs,
                                     sig.at(stopIdx).retentionTimeMs));
        }
        i = stopIdx; // 逐峰推进：下一峰从本峰 stop 续扫
    }
    return peaks;
}

} // namespace cdsw
