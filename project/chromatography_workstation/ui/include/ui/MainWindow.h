// ui/include/ui/MainWindow.h —— 主窗口（契约 §4.6：菜单/工具栏/分栏，持有 Selection 与管线）
#pragma once
#include <core_model/Chromatogram.h>
#include <core_model/Method.h>
#include <core_model/Selection.h>
#include <core_processing/interfaces.h>
#include <report/reporters.h>
#include <ui/SelectionController.h>
#include <QtWidgets/qmainwindow.h>

namespace Ui { class MainWindowUi; }

namespace cdsw {

class ChromatogramView;
class PeakTableView;
class MethodEditorView;

// 装配：File(导入/运行方法/导出报告/退出) + Help(关于) + 工具栏 + 左曲线/右分栏（峰表+方法编辑）。
// 持有 Chromatogram/Method（指针，不拥有）、Selection、ProcessingPipeline、SelectionController。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void setChromatogram(Chromatogram* chrom);
    void setMethod(Method* method);
    void setPeaks(const QList<Peak>& peaks);
    void runMethod();                          // 跑管线并刷新视图
    bool importCsv(const QString& filePath);   // 经 io ConverterRegistry 导入（Phase B）
    bool exportCsv(const QString& filePath);   // 生成 CSV 报告到路径（report 模块）
    Chromatogram* chromatogram() const;
    ChromatogramView* chromatogramView() const;
    PeakTableView* peakTableView() const;
    MethodEditorView* methodEditorView() const;
private slots:
    void onImportCsv();
    void onRunMethod();
    void onExportCsv();
    void onPeaksUpdated(const QList<Peak>& peaks);
private:
    void buildReportData(ReportData& out) const;
    Ui::MainWindowUi* ui;
    Chromatogram m_chromData;      // 导入数据的所有者（setChromatogram 仍接受外部指针）
    Chromatogram* m_chrom = nullptr;
    Method* m_method = nullptr;
    Selection m_selection;
    ProcessingPipeline m_pipeline;
    SelectionController m_controller;
};

} // namespace cdsw
