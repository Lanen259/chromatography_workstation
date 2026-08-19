// ui/src/InfoView.cpp —— 信息视图实现（色谱元数据键值表，只读）
#include <ui/InfoView.h>

#include <core_model/Chromatogram.h>

#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qtablewidget.h>

#include "ui_InfoView.h"

namespace cdsw {

namespace {
constexpr int kRowCount = 9;          // 0..7 元数据 + 8 峰数
constexpr int kPeakCountRow = 8;
} // namespace

InfoView::InfoView(QWidget* parent) : QWidget(parent), ui(new Ui::InfoViewUi)
{
    ui->setupUi(this);
    ui->table->setColumnCount(2);
    ui->table->setHorizontalHeaderLabels({ tr("键"), tr("值") });
    ui->table->horizontalHeader()->setStretchLastSection(true);
    ui->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->table->verticalHeader()->setVisible(false);
    ui->table->setRowCount(kRowCount);
}

InfoView::~InfoView()
{
    delete ui;
}

void InfoView::setRow(int row, const QString& key, const QString& value)
{
    ui->table->setItem(row, 0, new QTableWidgetItem(key));
    ui->table->setItem(row, 1, new QTableWidgetItem(value));
}

void InfoView::setChromatogram(const Chromatogram* chrom)
{
    if (!chrom) {
        for (int i = 0; i < kRowCount; ++i)
            setRow(i, QString(), QString());
        return;
    }
    const QVector<Signal>& pts = chrom->signalPoints();
    const qint64 first = pts.isEmpty() ? 0 : pts.first().retentionTimeMs;
    const qint64 last = pts.isEmpty() ? 0 : pts.last().retentionTimeMs;
    setRow(0, tr("名称"), chrom->name());
    setRow(1, tr("采样点数"), QString::number(pts.size()));
    setRow(2, tr("RT 范围"), QStringLiteral("%1 – %2 min")
                                .arg(first / 60000.0, 0, 'f', 2)
                                .arg(last / 60000.0, 0, 'f', 2));
    setRow(3, tr("扫描延迟"), QString::number(chrom->scanDelayMs()));
    setRow(4, tr("扫描间隔"), QString::number(chrom->scanIntervalMs()));
    setRow(5, tr("转换器"), chrom->converterId());
    setRow(6, tr("已处理"), chrom->processedPoints().isEmpty() ? tr("否") : tr("是"));
    setRow(7, tr("脏标记"), chrom->isDirty() ? tr("是") : tr("否"));
    setRow(kPeakCountRow, tr("峰数"), QString());
}

void InfoView::setPeaks(int count)
{
    setRow(kPeakCountRow, tr("峰数"), QString::number(count));
}

} // namespace cdsw
