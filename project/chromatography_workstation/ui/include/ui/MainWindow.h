// ui/include/ui/MainWindow.h —— 主窗口（契约 §4.6：菜单/工具栏/可停靠工作区，持有 Selection 与管线）
#pragma once
#include <core_model/Chromatogram.h>
#include <core_model/Method.h>
#include <core_model/Selection.h>
#include <core_processing/interfaces.h>
#include <report/reporters.h>
#include <ui/SelectionController.h>
#include <QtWidgets/qdockwidget.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qmainwindow.h>

namespace Ui { class MainWindowUi; }

namespace cdsw {

class ChromatogramView;
class PeakTableView;
class MethodEditorView;
class InfoView;
class LogView;

// 装配：File(导入/打开·保存方法/运行/导出报告/退出) + 视图(停靠面板开关) + 帮助(关于) + 工具栏。
// 中央 = 色谱图；峰表/方法编辑为 QDockWidget（Info/Log 见后续）。持有 Chromatogram/Method
// （指针，不拥有）、Selection、ProcessingPipeline、SelectionController。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void setChromatogram(Chromatogram* chrom);
    void setMethod(Method* method);
    void setPeaks(const QList<Peak>& peaks);
    void runMethod();                          // 跑管线并刷新视图
    bool importCsv(const QString& filePath);   // 经 io ConverterRegistry 导入
    bool exportCsv(const QString& filePath);   // 生成 CSV 报告到路径（report 模块）
    bool openMethod(const QString& filePath);  // JSON 方法文件载入（P4）
    bool saveMethod(const QString& filePath) const;
    Chromatogram* chromatogram() const;
    ChromatogramView* chromatogramView() const;
    PeakTableView* peakTableView() const;
    MethodEditorView* methodEditorView() const;
    InfoView* infoView() const;
    LogView* logView() const;
    void restoreWorkspace();                   // QSettings：窗口几何 + 停靠布局
    void saveWorkspace() const;
signals:
    void sigLogMessage(const QString& message);   // 事件日志（P5 接入 LogView）
private slots:
    void onImportCsv();
    void onRunMethod();
    void onOpenMethod();
    void onSaveMethod();
    void onExportCsv();
    void onPeaksUpdated(const QList<Peak>& peaks);
private:
    void createDocks();
    void buildReportData(ReportData& out) const;
    Ui::MainWindowUi* ui;
    Chromatogram m_chromData;      // 导入数据的所有者（setChromatogram 仍接受外部指针）
    Chromatogram* m_chrom = nullptr;
    Method m_methodData;           // 方法文件载入的持有者（openMethod 用）
    Method* m_method = nullptr;
    QString m_methodName;          // 方法显示名（core_model Method 无名称字段）
    Selection m_selection;
    ProcessingPipeline m_pipeline;
    SelectionController m_controller;
    PeakTableView* m_peakView = nullptr;
    MethodEditorView* m_methodEditor = nullptr;
    InfoView* m_infoView = nullptr;
    LogView* m_logView = nullptr;
    QDockWidget* m_peakDock = nullptr;
    QDockWidget* m_methodDock = nullptr;
    QDockWidget* m_infoDock = nullptr;
    QDockWidget* m_logDock = nullptr;
    QLabel* m_statusInfo = nullptr;    // 状态栏永久信息（峰数/RT 范围）
};

} // namespace cdsw
