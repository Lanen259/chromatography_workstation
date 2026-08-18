// ui/include/ui/PeakTableModel.h —— 峰表模型（适配 QList<Peak>，契约 §4.6）
#pragma once
#include <core_model/Peak.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qstringlist.h>
namespace cdsw {

// 列语义与 M5 report 峰表一致：Peak # / Apex RT (min) / Start RT (min) / Stop RT (min) / Height / Area
class PeakTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit PeakTableModel(QObject* parent = nullptr);
    void setPeaks(const QList<Peak>& peaks);
    const QList<Peak>& peaks() const;
    // —— QAbstractTableModel ——
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
private:
    QList<Peak> m_peaks;
};

} // namespace cdsw
