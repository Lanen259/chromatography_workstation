// ui/tests/t_ui.cpp —— 界面层 offscreen 冒烟（契约 §4.6）
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

#include <core_model/Chromatogram.h>
#include <core_model/Method.h>
#include <core_model/Selection.h>
#include <core_processing/interfaces.h>
#include <ui/SelectionController.h>
#include <ui/PeakTableModel.h>
#include <ui/PeakTableView.h>
#include <ui/ChromatogramView.h>
#include <ui/MethodEditorView.h>
#include <ui/MainWindow.h>

#include <QComboBox>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>

using cdsw::Signal;
using cdsw::Chromatogram;
using cdsw::Method;
using cdsw::ProcessingStep;
using cdsw::Selection;
using cdsw::Registry;
using cdsw::ProcessingPipeline;
using cdsw::Peak;
using cdsw::SelectionController;
using cdsw::PeakTableModel;
using cdsw::PeakTableView;
using cdsw::ChromatogramView;
using cdsw::MethodEditorView;
using cdsw::MainWindow;

namespace {

double gauss(double x, double mu, double sigma)
{
    const double d = (x - mu) / sigma;
    return std::exp(-0.5 * d * d);
}

// 两个可分辨高斯峰（照 core_processing 金标准信号）
Chromatogram makeTwoPeakChrom()
{
    Chromatogram chrom;
    QVector<Signal> pts;
    for (int t = 0; t <= 2000; t += 10) {
        Signal s;
        s.retentionTimeMs = t;
        s.intensity = 100.0 * gauss(t, 600.0, 120.0) + 80.0 * gauss(t, 1200.0, 120.0);
        pts.append(s);
    }
    chrom.setSignalPoints(pts);
    return chrom;
}

Method makePeakDetectMethod()
{
    Method m;
    m.steps.append(ProcessingStep{
        QStringLiteral("first_derivative_peak_detector"),
        QVariantMap{ { QStringLiteral("threshold"), QStringLiteral("MEDIUM") },
                     { QStringLiteral("windowSize"), 0 } } });
    return m;
}

} // namespace

class UiTest : public QObject
{
    Q_OBJECT
private slots:
    void selectionControllerRunsPipeline();
    void peakTableModelShowsPeaks();
    void peakTableModelEmpty();
    void chromatogramViewRendersSelectsAndZooms();
    void methodEditorEditsSteps();
    void mainWindowAssemblesAndRuns();
    void mainWindowImportsCsv();
};

void UiTest::selectionControllerRunsPipeline()
{
    // QSignalSpy 按 moc 记录的参数名 "QList<Peak>" 解析类型；默认注册名带命名空间不匹配，需显式注册
    qRegisterMetaType<QList<cdsw::Peak>>("QList<Peak>");

    Registry& reg = Registry::instance();
    ProcessingPipeline pipeline(reg);
    Selection selection;

    SelectionController controller(&selection, &pipeline);
    Chromatogram chrom = makeTwoPeakChrom();
    Method method = makePeakDetectMethod();
    controller.setChromatogram(&chrom);
    controller.setMethod(&method);

    QSignalSpy spy(&controller, &SelectionController::sigPeaksUpdated);
    controller.onChromatogramChanged();
    QCOMPARE(spy.count(), 1);
    // 直查管线结果，区分「检测器没跑出峰」vs「信号捕获元类型问题」
    QCOMPARE(pipeline.peaks().size(), 2);
    const QList<Peak> peaks = spy.at(0).at(0).value<QList<Peak>>();
    QCOMPARE(peaks.size(), 2);
    QVERIFY(chrom.isDirty());   // 契约 §4.1：管线跑过 → 脏标记置 true
}

void UiTest::peakTableModelShowsPeaks()
{
    PeakTableModel model;
    // 乱序传入 → 按 apexRTMs 升序排列后编号（与 M5 报告峰表同序）
    const QList<Peak> peaks = {
        Peak{ 186000, 204000, 222000, 31000.0, 77500.0 },  // apex 3.4 min
        Peak{ 60000,  75000,  90000,  52000.0, 104000.0 }, // apex 1.25 min
    };
    model.setPeaks(peaks);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), 6);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("1"));
    QCOMPARE(model.data(model.index(0, 1), Qt::DisplayRole).toString(), QStringLiteral("1.250000"));
    QCOMPARE(model.data(model.index(1, 1), Qt::DisplayRole).toString(), QStringLiteral("3.400000"));
    QCOMPARE(model.data(model.index(1, 4), Qt::DisplayRole).toString(), QStringLiteral("31000.000000"));
    QCOMPARE(model.data(model.index(1, 5), Qt::DisplayRole).toString(), QStringLiteral("77500.000000"));
    QCOMPARE(model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
             QStringLiteral("Apex RT (min)"));

    // PeakTableView 转发 setPeaks → model 数据可见
    PeakTableView view;
    view.setPeaks(peaks);
    QCOMPARE(view.model()->rowCount(), 2);
}

void UiTest::peakTableModelEmpty()
{
    PeakTableModel model;
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.peaks().isEmpty());
}

void UiTest::chromatogramViewRendersSelectsAndZooms()
{
    ChromatogramView view;
    Chromatogram chrom = makeTwoPeakChrom();
    view.resize(400, 200);
    view.show();                     // offscreen 平台
    view.setChromatogram(&chrom);
    QCOMPARE(view.visibleStartMs(), qint64(0));
    QCOMPARE(view.visibleStopMs(), qint64(2000));
    view.repaint();                  // paintEvent 不崩

    // 左键拖拽出选区 → sigSelectionRangeChanged（映射回毫秒）
    QSignalSpy spy(&view, &ChromatogramView::sigSelectionRangeChanged);
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
    QTest::mouseMove(&view, QPoint(350, 50), 20);
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, QPoint(350, 50));
    QCOMPARE(spy.count(), 1);
    const qint64 startMs = spy.at(0).at(0).value<qint64>();
    const qint64 stopMs = spy.at(0).at(1).value<qint64>();
    QVERIFY(startMs < stopMs);
    QVERIFY(startMs > 0 && stopMs < 2000);

    // 滚轮缩放走 zoomAt：可见窗收窄
    const qint64 before = view.visibleStopMs() - view.visibleStartMs();
    view.zoomAt(qint64(1000), 2.0);
    QVERIFY(view.visibleStopMs() - view.visibleStartMs() < before);

    // setPeaks / setSelectionRange 重绘不崩
    view.setPeaks({ Peak{ 60000, 75000, 90000, 52000.0, 104000.0 } });
    view.setSelectionRange(qint64(300), qint64(800));
    view.repaint();
    QCOMPARE(view.selectionStartMs(), qint64(300));
    QCOMPARE(view.selectionStopMs(), qint64(800));
}

void UiTest::methodEditorEditsSteps()
{
    MethodEditorView view;
    view.show();
    Method method;
    method.steps.append(ProcessingStep{ QStringLiteral("sg_smooth"), QVariantMap() });
    method.steps.append(ProcessingStep{ QStringLiteral("first_derivative_peak_detector"), QVariantMap() });
    view.setMethod(&method);

    auto* list = view.findChild<QListWidget*>(QStringLiteral("listSteps"));
    auto* combo = view.findChild<QComboBox*>(QStringLiteral("comboAlgorithm"));
    auto* addBtn = view.findChild<QPushButton*>(QStringLiteral("btnAdd"));
    auto* removeBtn = view.findChild<QPushButton*>(QStringLiteral("btnRemove"));
    auto* upBtn = view.findChild<QPushButton*>(QStringLiteral("btnUp"));
    QVERIFY(list && combo && addBtn && removeBtn && upBtn);

    QSignalSpy spy(&view, &MethodEditorView::sigMethodChanged);
    QCOMPARE(list->count(), 2);

    // Add：从注册表选 sg_smooth → 追加到方法
    const int idx = combo->findText(QStringLiteral("sg_smooth"));
    QVERIFY(idx >= 0);
    combo->setCurrentIndex(idx);
    QTest::mouseClick(addBtn, Qt::LeftButton);
    QCOMPARE(method.steps.size(), 3);
    QCOMPARE(method.steps.last().id, QStringLiteral("sg_smooth"));
    QCOMPARE(list->count(), 3);
    QCOMPARE(spy.count(), 1);

    // MoveUp：最后一项上移
    list->setCurrentRow(2);
    QTest::mouseClick(upBtn, Qt::LeftButton);
    QCOMPARE(method.steps.at(1).id, QStringLiteral("sg_smooth"));
    QCOMPARE(spy.count(), 2);

    // Remove：删除当前项
    list->setCurrentRow(1);
    QTest::mouseClick(removeBtn, Qt::LeftButton);
    QCOMPARE(method.steps.size(), 2);
    QCOMPARE(spy.count(), 3);

    // 参数编辑：选中步骤 0，写参数表 → 写回 method
    list->setCurrentRow(0);
    auto* table = view.findChild<QTableWidget*>(QStringLiteral("tableParams"));
    QVERIFY(table);
    table->setRowCount(1);
    table->setItem(0, 0, new QTableWidgetItem(QStringLiteral("threshold")));
    table->setItem(0, 1, new QTableWidgetItem(QStringLiteral("MEDIUM")));
    QCOMPARE(method.steps.at(0).parameters.value(QStringLiteral("threshold")).toString(),
             QStringLiteral("MEDIUM"));
}

void UiTest::mainWindowAssemblesAndRuns()
{
    MainWindow window;
    window.show();                       // offscreen
    Chromatogram chrom = makeTwoPeakChrom();
    Method method = makePeakDetectMethod();
    window.setChromatogram(&chrom);
    window.setMethod(&method);
    QVERIFY(window.chromatogramView());
    QVERIFY(window.peakTableView());
    QVERIFY(window.methodEditorView());

    // 跑管线 → sigPeaksUpdated → 峰表有数据
    window.runMethod();
    QCOMPARE(window.peakTableView()->tableModel()->rowCount(), 2);

    // 导出 CSV 报告到临时文件 → 成功且含表头
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("report.csv"));
    QVERIFY(window.exportCsv(outPath));
    QFile f(outPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().contains("Sample Name"));
}

void UiTest::mainWindowImportsCsv()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("imported.csv"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("retentionTimeMs,intensity\n0,10.000000\n500,50.000000\n1000,20.000000\n");
    f.close();

    MainWindow window;
    QVERIFY(window.importCsv(path));
    QVERIFY(window.chromatogram());
    QCOMPARE(window.chromatogram()->signalPoints().size(), 3);
    QCOMPARE(window.chromatogram()->name(), QStringLiteral("imported"));

    // 失败路径：不存在 / 未知扩展名
    QVERIFY(!window.importCsv(dir.filePath(QStringLiteral("nope.csv"))));
    QVERIFY(!window.importCsv(QStringLiteral("x.zzz")));
}

QTEST_MAIN(UiTest)
#include "t_ui.moc"
