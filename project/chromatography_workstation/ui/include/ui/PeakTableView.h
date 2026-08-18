// ui/include/ui/PeakTableView.h —— 峰表视图（契约 §4.6：QTableView + 适配 QList<Peak> 的模型）
#pragma once
#include <core_model/Peak.h>
#include <QtWidgets/qtableview.h>
namespace cdsw {

class PeakTableModel;

class PeakTableView : public QTableView {
    Q_OBJECT
public:
    explicit PeakTableView(QWidget* parent = nullptr);
    void setPeaks(const QList<Peak>& peaks);
    PeakTableModel* tableModel() const;
private:
    PeakTableModel* m_model;
};

} // namespace cdsw
