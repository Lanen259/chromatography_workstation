// ui/src/PeakTableModel.cpp —— 峰表模型实现（RT 分钟定点，与报告峰表同序同列）
#include <ui/PeakTableModel.h>

#include <algorithm>

namespace cdsw {

namespace {

constexpr int kFixedPrecision = 6;        // 定点精度（与 report 模块一致，跨视图一致）
constexpr double kMsPerMinute = 60000.0;  // 毫秒→分钟

QString rtMinutes(qint64 ms)
{
    return QString::number(ms / kMsPerMinute, 'f', kFixedPrecision);
}

QString fixedPrecision(double v)
{
    return QString::number(v, 'f', kFixedPrecision);
}

} // namespace

PeakTableModel::PeakTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void PeakTableModel::setPeaks(const QList<Peak>& peaks)
{
    beginResetModel();
    m_peaks = peaks;
    // 按 apexRTMs 升序（与 M5 report 峰表同序，Peak # 与报告对齐）
    std::stable_sort(m_peaks.begin(), m_peaks.end(),
                     [](const Peak& a, const Peak& b) { return a.apexRTMs < b.apexRTMs; });
    endResetModel();
}

const QList<Peak>& PeakTableModel::peaks() const { return m_peaks; }

int PeakTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_peaks.size();
}

int PeakTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 6;
}

QVariant PeakTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();
    if (index.row() < 0 || index.row() >= m_peaks.size())
        return QVariant();
    const Peak& p = m_peaks.at(index.row());
    switch (index.column()) {
    case 0: return QString::number(index.row() + 1);
    case 1: return rtMinutes(p.apexRTMs);
    case 2: return rtMinutes(p.startRTMs);
    case 3: return rtMinutes(p.stopRTMs);
    case 4: return fixedPrecision(p.peakHeight);
    case 5: return fixedPrecision(p.peakArea);
    default: return QVariant();
    }
}

QVariant PeakTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();
    if (orientation == Qt::Vertical)
        return section + 1;
    static const QStringList kHeaders = {
        QStringLiteral("Peak #"), QStringLiteral("Apex RT (min)"), QStringLiteral("Start RT (min)"),
        QStringLiteral("Stop RT (min)"), QStringLiteral("Height"), QStringLiteral("Area") };
    return (section >= 0 && section < kHeaders.size()) ? kHeaders.at(section) : QVariant();
}

} // namespace cdsw
