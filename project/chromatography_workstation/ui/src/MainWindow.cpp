// ui/src/MainWindow.cpp —— 主窗口实现（.ui 装配 + 可停靠工作区 + 菜单/管线握手 + 方法 JSON 存读）
#include <ui/MainWindow.h>

#include <io/converters.h>
#include <ui/ChromatogramView.h>
#include <ui/PeakTableView.h>
#include <ui/MethodEditorView.h>
#include <ui/InfoView.h>
#include <ui/LogView.h>
#include <ui/Theme.h>

#include <QtCore/qdatetime.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qsettings.h>
#include <QtWidgets/qaction.h>
#include <QtWidgets/qdockwidget.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbar.h>

#include <memory>

#include <cmath>

#include "ui_MainWindow.h"

namespace cdsw {

namespace {
QSettings appSettings()
{
    return QSettings(QStringLiteral("cdsw"), QStringLiteral("chromatography_workstation"));
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowUi)
    , m_pipeline(Registry::instance())
    , m_controller(&m_selection, &m_pipeline, this)
{
    applyTheme();   // 现代暗色主题（资源 :/theme.qss）
    ui->setupUi(this);
    // 演示数据动作（C++ 建，置 File 顶部 + 工具栏首格）
    QAction* actionLoadDemo = new QAction(tr("加载演示数据"), this);
    connect(actionLoadDemo, &QAction::triggered, this, &MainWindow::loadDemoData);
    ui->menuFile->addAction(actionLoadDemo);
    ui->menuFile->addSeparator();
    ui->menuFile->addAction(ui->actionImportCsv);
    ui->menuFile->addAction(ui->actionRunMethod);
    ui->menuFile->addSeparator();
    ui->menuFile->addAction(ui->actionOpenMethod);
    ui->menuFile->addAction(ui->actionSaveMethod);
    ui->menuFile->addSeparator();
    ui->menuFile->addAction(ui->actionExportCsv);
    ui->menuFile->addSeparator();
    ui->menuFile->addAction(ui->actionQuit);
    ui->menuHelp->addAction(ui->actionAbout);
    ui->toolbar->addAction(ui->actionRunMethod);
    ui->toolbar->addAction(ui->actionImportCsv);
    ui->toolbar->addAction(ui->actionExportCsv);
    ui->toolbar->addAction(actionLoadDemo);

    connect(ui->actionImportCsv, &QAction::triggered, this, &MainWindow::onImportCsv);
    connect(ui->actionRunMethod, &QAction::triggered, this, &MainWindow::onRunMethod);
    connect(ui->actionOpenMethod, &QAction::triggered, this, &MainWindow::onOpenMethod);
    connect(ui->actionSaveMethod, &QAction::triggered, this, &MainWindow::onSaveMethod);
    connect(ui->actionExportCsv, &QAction::triggered, this, &MainWindow::onExportCsv);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionAbout, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("关于色谱工作站"),
                           tr("<b>色谱工作站 CDS</b><br/>Qt/C++ 桌面色谱数据系统，逆向 OpenChrom 架构。"
                              "<br/>模块：core_model / core_processing / acq / io / report / ui。"));
    });

    createDocks();

    m_statusInfo = new QLabel(this);
    m_statusInfo->setText(tr("就绪"));
    statusBar()->addPermanentWidget(m_statusInfo);

    connect(this, &MainWindow::sigLogMessage, m_logView, &LogView::appendMessage);
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

void MainWindow::createDocks()
{
    m_peakView = new PeakTableView(this);
    m_peakDock = new QDockWidget(tr("峰表"), this);
    m_peakDock->setObjectName(QStringLiteral("peakDock"));
    m_peakDock->setWidget(m_peakView);
    addDockWidget(Qt::BottomDockWidgetArea, m_peakDock);
    ui->menuView->addAction(m_peakDock->toggleViewAction());

    m_methodEditor = new MethodEditorView(this);
    m_methodDock = new QDockWidget(tr("方法编辑器"), this);
    m_methodDock->setObjectName(QStringLiteral("methodDock"));
    m_methodDock->setWidget(m_methodEditor);
    addDockWidget(Qt::RightDockWidgetArea, m_methodDock);
    ui->menuView->addAction(m_methodDock->toggleViewAction());

    m_infoView = new InfoView(this);
    m_infoDock = new QDockWidget(tr("信息"), this);
    m_infoDock->setObjectName(QStringLiteral("infoDock"));
    m_infoDock->setWidget(m_infoView);
    addDockWidget(Qt::RightDockWidgetArea, m_infoDock);
    ui->menuView->addAction(m_infoDock->toggleViewAction());

    m_logView = new LogView(this);
    m_logDock = new QDockWidget(tr("日志"), this);
    m_logDock->setObjectName(QStringLiteral("logDock"));
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    ui->menuView->addAction(m_logDock->toggleViewAction());
}

void MainWindow::setChromatogram(Chromatogram* chrom)
{
    m_chrom = chrom;
    m_controller.setChromatogram(chrom);
    ui->chromatogramView->setChromatogram(chrom);
    m_infoView->setChromatogram(chrom);
    setPeaks(QList<Peak>());   // 新数据清旧峰/选区/状态（跨数据不错标）
    statusBar()->showMessage(
        tr("色谱已加载：%1").arg(chrom ? chrom->name() : QString()), 3000);
}

void MainWindow::setMethod(Method* method)
{
    m_method = method;
    m_controller.setMethod(method);
    m_methodEditor->setMethod(method);
}

void MainWindow::setPeaks(const QList<Peak>& peaks)
{
    m_peakView->setPeaks(peaks);
    ui->chromatogramView->setPeaks(peaks);
    m_infoView->setPeaks(peaks.size());
    if (m_chrom) {
        m_statusInfo->setText(tr("%1 个峰 · RT %2–%3 min")
                                  .arg(peaks.size())
                                  .arg(m_chrom->startTimeMs() / 60000.0, 0, 'f', 2)
                                  .arg(m_chrom->stopTimeMs() / 60000.0, 0, 'f', 2));
    } else {
        m_statusInfo->setText(tr("%1 个峰").arg(peaks.size()));
    }
}

void MainWindow::runMethod()
{
    if (!m_chrom || !m_method)
        return;
    m_controller.onChromatogramChanged();
    emit sigLogMessage(tr("管线已执行：%1 步，检出 %2 个峰")
                           .arg(m_method->steps.size())
                           .arg(m_pipeline.peaks().size()));
}

void MainWindow::onPeaksUpdated(const QList<Peak>& peaks)
{
    setPeaks(peaks);
}

void MainWindow::onRunMethod()
{
    if (!m_chrom || !m_method) {
        statusBar()->showMessage(tr("先导入数据并设置方法"), 5000);
        return;
    }
    runMethod();
    statusBar()->showMessage(tr("管线已执行"), 3000);
}

bool MainWindow::importCsv(const QString& filePath)
{
    std::unique_ptr<IChromatogramImporter> importer(
        ConverterRegistry::instance().importerFor(filePath));
    if (!importer)
        return false;
    m_chromData = Chromatogram();
    const ImportResult result = importer->import(filePath, m_chromData);
    if (!result.ok) {
        statusBar()->showMessage(tr("导入失败：%1").arg(result.errorMessage), 5000);
        emit sigLogMessage(tr("导入失败：%1").arg(result.errorMessage));
        return false;
    }
    setChromatogram(&m_chromData);
    emit sigLogMessage(tr("已导入 %1（%2 个采样点）")
                           .arg(m_chromData.name()).arg(m_chromData.signalPoints().size()));
    return true;
}

void MainWindow::onImportCsv()
{
    const QString lastDir = appSettings().value(QStringLiteral("lastImportDir")).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("导入 CSV 数据"), lastDir, tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    appSettings().setValue(QStringLiteral("lastImportDir"), QFileInfo(path).absolutePath());
    importCsv(path);
}

bool MainWindow::openMethod(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    m_methodName = root.value(QStringLiteral("name")).toString();
    m_methodData = Method();
    const QJsonArray steps = root.value(QStringLiteral("steps")).toArray();
    for (const QJsonValue& sv : steps) {
        const QJsonObject so = sv.toObject();
        ProcessingStep step;
        step.id = so.value(QStringLiteral("id")).toString();
        step.parameters = so.value(QStringLiteral("parameters")).toObject().toVariantMap();
        m_methodData.steps.append(step);
    }
    setMethod(&m_methodData);   // 同时接 controller 与编辑器
    statusBar()->showMessage(tr("已加载方法：%1").arg(m_methodName), 3000);
    emit sigLogMessage(tr("已加载方法 %1（%2 步）").arg(m_methodName).arg(m_methodData.steps.size()));
    return true;
}

bool MainWindow::saveMethod(const QString& filePath) const
{
    QJsonObject root;
    root.insert(QStringLiteral("name"), m_methodName);
    QJsonArray steps;
    if (m_method) {
        for (const ProcessingStep& step : m_method->steps) {
            QJsonObject so;
            so.insert(QStringLiteral("id"), step.id);
            so.insert(QStringLiteral("parameters"), QJsonObject::fromVariantMap(step.parameters));
            steps.append(so);
        }
    }
    root.insert(QStringLiteral("steps"), steps);
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void MainWindow::onOpenMethod()
{
    const QString lastDir = appSettings().value(QStringLiteral("lastMethodDir")).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("打开方法"), lastDir, tr("方法文件 (*.json)"));
    if (path.isEmpty())
        return;
    appSettings().setValue(QStringLiteral("lastMethodDir"), QFileInfo(path).absolutePath());
    if (!openMethod(path))
        QMessageBox::warning(this, tr("打开失败"), tr("无法解析方法文件：%1").arg(path));
}

void MainWindow::onSaveMethod()
{
    if (!m_method) {
        statusBar()->showMessage(tr("没有可保存的方法"), 3000);
        return;
    }
    const QString lastDir = appSettings().value(QStringLiteral("lastMethodDir")).toString();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存方法"), lastDir, tr("方法文件 (*.json)"));
    if (path.isEmpty())
        return;
    appSettings().setValue(QStringLiteral("lastMethodDir"), QFileInfo(path).absolutePath());
    if (saveMethod(path))
        statusBar()->showMessage(tr("方法已保存：%1").arg(path), 3000);
    else
        QMessageBox::warning(this, tr("保存失败"), tr("无法写入文件：%1").arg(path));
}

bool MainWindow::exportCsv(const QString& filePath)
{
    std::unique_ptr<IReporter> reporter(
        ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    if (!reporter)
        return false;
    ReportData data;
    buildReportData(data);
    const bool ok = reporter->generate(data, filePath);
    if (ok)
        emit sigLogMessage(tr("报告已导出：%1").arg(filePath));
    return ok;
}

void MainWindow::buildReportData(ReportData& out) const
{
    out.sampleName = m_chrom ? m_chrom->name() : QString();
    out.methodName = m_methodName;
    out.acquiredAt = QDateTime::currentDateTime();
    out.peaks = m_pipeline.peaks();
    out.quantEntries = m_pipeline.quantEntries();
}

void MainWindow::onExportCsv()
{
    const QString lastDir = appSettings().value(QStringLiteral("lastExportDir")).toString();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出 CSV 报告"), lastDir, tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    appSettings().setValue(QStringLiteral("lastExportDir"), QFileInfo(path).absolutePath());
    if (exportCsv(path))
        statusBar()->showMessage(tr("报告已导出：%1").arg(path), 3000);
    else
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件：%1").arg(path));
}

void MainWindow::loadDemoData()
{
    m_chromData = Chromatogram();
    QVector<Signal> pts;
    pts.reserve(201);
    for (int t = 0; t <= 2000; t += 10) {
        const double d1 = (t - 600.0) / 120.0;
        const double d2 = (t - 1200.0) / 120.0;
        pts.append(Signal{ qint64(t),
                           100.0 * std::exp(-0.5 * d1 * d1) + 80.0 * std::exp(-0.5 * d2 * d2) });
    }
    m_chromData.setSignalPoints(pts);
    m_chromData.setName(tr("演示数据"));
    m_chromData.setConverterId(QStringLiteral("demo"));
    m_chromData.setScanDelayMs(0);
    m_chromData.setScanIntervalMs(10);
    setChromatogram(&m_chromData);

    m_methodData = Method();
    m_methodData.steps.append(ProcessingStep{
        QStringLiteral("first_derivative_peak_detector"),
        QVariantMap{ { QStringLiteral("threshold"), QStringLiteral("MEDIUM") },
                     { QStringLiteral("windowSize"), 0 } } });
    m_methodName = tr("峰检测");
    setMethod(&m_methodData);
    emit sigLogMessage(tr("已加载演示数据（双峰信号，%1 点）").arg(pts.size()));
}

void MainWindow::restoreWorkspace()
{
    QSettings s(QStringLiteral("cdsw"), QStringLiteral("chromatography_workstation"));
    restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
    restoreState(s.value(QStringLiteral("dockState")).toByteArray());
}

void MainWindow::saveWorkspace() const
{
    QSettings s(QStringLiteral("cdsw"), QStringLiteral("chromatography_workstation"));
    s.setValue(QStringLiteral("geometry"), saveGeometry());
    s.setValue(QStringLiteral("dockState"), saveState());
}

Chromatogram* MainWindow::chromatogram() const { return m_chrom; }
ChromatogramView* MainWindow::chromatogramView() const { return ui->chromatogramView; }
PeakTableView* MainWindow::peakTableView() const { return m_peakView; }
MethodEditorView* MainWindow::methodEditorView() const { return m_methodEditor; }
InfoView* MainWindow::infoView() const { return m_infoView; }
LogView* MainWindow::logView() const { return m_logView; }

} // namespace cdsw
