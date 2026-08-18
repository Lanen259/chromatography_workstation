// ui/src/PeakTableView.cpp —— 峰表视图实现
#include <ui/PeakTableView.h>

#include <ui/PeakTableModel.h>

#include <QtWidgets/qheaderview.h>

namespace cdsw {

PeakTableView::PeakTableView(QWidget* parent)
    : QTableView(parent), m_model(new PeakTableModel(this))
{
    setModel(m_model);
    horizontalHeader()->setStretchLastSection(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
}

void PeakTableView::setPeaks(const QList<Peak>& peaks)
{
    m_model->setPeaks(peaks);
}

PeakTableModel* PeakTableView::tableModel() const { return m_model; }

} // namespace cdsw
