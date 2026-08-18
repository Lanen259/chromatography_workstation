// ui/src/MainWindow.cpp —— 主窗口实现（.ui 装配 + 菜单/工具栏/管线握手）
#include <ui/MainWindow.h>

#include <ui/ChromatogramView.h>
#include <ui/PeakTableView.h>
#include <ui/MethodEditorView.h>

#include <QtCore/qdatetime.h>
#include <QtWidgets/qaction.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbar.h>

#include <memory>

#include "ui_MainWindow.h"

namespace cdsw {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowUi)
    , m_pipeline(Registry::instance())
    , m_controller(&m_selection, &m_pipeline, this)
{
    ui->setupUi(this);
    ui->menuFile->addAction(ui->actionImportCsv);
    ui->menuFile->addAction(ui->actionRunMethod);
    ui->menuFile->addSeparator();
    ui->menuFile->addAction(ui->actionExportCsv);
    ui->menuFile->addSeparator();
    ui->menuFile->addAction(ui->actionQuit);
    ui->menuHelp->addAction(ui->actionAbout);
    ui->toolbar->addAction(ui->actionRunMethod);
    ui->toolbar->addAction(ui->actionExportCsv);

    connect(ui->actionRunMethod, &QAction::triggered, this, &MainWindow::onRunMethod);
    connect(ui->actionExportCsv, &QAction::triggered, this, &MainWindow::onExportCsv);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionAbout, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("关于"), tr("Qt/C++ 色谱工作站（CDS）"));
    });
    // actionImportCsv 在 M4(io) 合入后接线（Phase B）

    connect(&m_controller, &SelectionController::sigPeaksUpdated,
            this, &MainWindow::onPeaksUpdated);

    // 曲线选区 → Selection 广播（契约 §4.6 握手点）
    connect(ui->chromatogramView, &ChromatogramView::sigSelectionRangeChanged,
            this, [this](qint64 start, qint64 stop) {
                m_selection.setRange(start, stop);
                ui->chromatogramView->setSelectionRange(start, stop);
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setChromatogram(Chromatogram* chrom)
{
    m_chrom = chrom;
    m_controller.setChromatogram(chrom);
    ui->chromatogramView->setChromatogram(chrom);
    statusBar()->showMessage(
        tr("色谱已加载：%1").arg(chrom ? chrom->name() : QString()), 3000);
}

void MainWindow::setMethod(Method* method)
{
    m_method = method;
    m_controller.setMethod(method);
    ui->methodEditorView->setMethod(method);
}

void MainWindow::setPeaks(const QList<Peak>& peaks)
{
    ui->peakTableView->setPeaks(peaks);
    ui->chromatogramView->setPeaks(peaks);
}

void MainWindow::runMethod()
{
    m_controller.onChromatogramChanged();
}

void MainWindow::onPeaksUpdated(const QList<Peak>& peaks)
{
    setPeaks(peaks);
}

void MainWindow::onRunMethod()
{
    runMethod();
    statusBar()->showMessage(tr("管线已执行"), 3000);
}

bool MainWindow::exportCsv(const QString& filePath)
{
    std::unique_ptr<IReporter> reporter(
        ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    if (!reporter)
        return false;
    ReportData data;
    buildReportData(data);
    return reporter->generate(data, filePath);
}

void MainWindow::buildReportData(ReportData& out) const
{
    out.sampleName = m_chrom ? m_chrom->name() : QString();
    out.methodName = QString();   // Method 无名称字段，留空
    out.acquiredAt = QDateTime::currentDateTime();
    out.peaks = m_pipeline.peaks();
    out.quantEntries = m_pipeline.quantEntries();
}

void MainWindow::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出 CSV 报告"), QString(), tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    if (exportCsv(path))
        statusBar()->showMessage(tr("报告已导出：%1").arg(path), 3000);
    else
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件：%1").arg(path));
}

ChromatogramView* MainWindow::chromatogramView() const { return ui->chromatogramView; }
PeakTableView* MainWindow::peakTableView() const { return ui->peakTableView; }
MethodEditorView* MainWindow::methodEditorView() const { return ui->methodEditorView; }

} // namespace cdsw
