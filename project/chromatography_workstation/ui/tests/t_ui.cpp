// ui/tests/t_ui.cpp —— 界面层 offscreen 冒烟（契约 §4.6）
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

#include <core_model/Chromatogram.h>
#include <core_model/Method.h>
#include <core_model/Selection.h>
#include <core_processing/interfaces.h>
#include <ui/SelectionController.h>

using cdsw::Signal;
using cdsw::Chromatogram;
using cdsw::Method;
using cdsw::ProcessingStep;
using cdsw::Selection;
using cdsw::Registry;
using cdsw::ProcessingPipeline;
using cdsw::Peak;
using cdsw::SelectionController;

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

QTEST_MAIN(UiTest)
#include "t_ui.moc"
