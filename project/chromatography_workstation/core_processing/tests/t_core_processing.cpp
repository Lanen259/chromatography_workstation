// tests/t_core_processing.cpp —— core_processing 金标准信号测试（契约 §6）
//
// 覆盖：FilterSavitzkyGolay / BaselineLinear / PeakDetectorFirstDerivative /
//       IntegratorTrapezoid / QuantifierCalibration / Registry / ProcessingPipeline。
// 金标准来源：逆向文档 MODULE_04（峰检测/积分/基线）/ MODULE_10（S-G）确认的常量与算法。
// 每个算法一组测试槽；测试直接构造 src/ 算法实现类（模块内私有，CMake 已加 include 路径）。

#include <QtTest/QtTest>
#include <core_processing/interfaces.h>

#include "BaselineLinear.h"
#include "FilterSavitzkyGolay.h"
#include "IntegratorTrapezoid.h"
#include "PeakDetectorFirstDerivative.h"
#include "QuantifierCalibration.h"

#include <QtCore/qalgorithms.h>
#include <QtCore/qpair.h>

#include <cmath>

using namespace cdsw;

namespace {

// 高斯信号（测试信号发生器）
double gauss(double t, double mu, double sigma)
{
    const double z = (t - mu) / sigma;
    return std::exp(-0.5 * z * z);
}

// QVariantMap 便捷构造：mapWith({{key, value}, ...})
QVariantMap mapWith(const QList<QPair<QString, QVariant>>& items)
{
    QVariantMap m;
    for (const auto& kv : items)
        m.insert(kv.first, kv.second);
    return m;
}

// 两个可分辨高斯峰：100·gauss(600,120) + 80·gauss(1200,120)，RT 0..2000ms 步 10ms，201 点。
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

// 高峰 + 宽浅凸包：100·gauss(600,120) + 0.5·gauss(30000,7000)，RT 0..52000ms 步 10ms，5201 点。
// 归一化（×1000）后凸包最大斜率 = 500/(7000·√e) ≈ 0.0433：LOW(0.005) 检出、MEDIUM(0.05)/HIGH(0.5) 不检出。
Chromatogram makePeakPlusBumpChrom()
{
    Chromatogram chrom;
    QVector<Signal> pts;
    for (int t = 0; t <= 52000; t += 10) {
        Signal s;
        s.retentionTimeMs = t;
        s.intensity = 100.0 * gauss(t, 600.0, 120.0) + 0.5 * gauss(t, 30000.0, 7000.0);
        pts.append(s);
    }
    chrom.setSignalPoints(pts);
    return chrom;
}

} // namespace

class TestCoreProcessing : public QObject {
    Q_OBJECT
private slots:
    // —— FilterSavitzkyGolay ——
    void sgPreservesQuadratic();
    void sgSmoothsSpike();
    void sgClampsParameters();
    // —— BaselineLinear ——
    void baselineLinearChord();
    // —— PeakDetectorFirstDerivative ——
    void peakDetectorTwoResolvedPeaks();
    void peakDetectorThresholdFiltering();
    void peakDetectorDefaultWindowSize();
    void peakDetectorFlatLineNoPeaks();
    void peakDetectorThresholdMapping();
    // —— IntegratorTrapezoid ——
    void integratorTriangleGoldenArea();
    void integratorSlopedBaseline();
    void integratorAreaConstraint();
    void integratorFallbackToChromatogram();
    // —— QuantifierCalibration ——
    void quantifierLeastSquares();
    void quantifierSinglePointAndEmpty();
    // —— Registry / ProcessingPipeline ——
    void registryBuiltIns();
    void pipelineFullMethod();
    void pipelineResetProcessed();
};

// ---------- FilterSavitzkyGolay ----------

void TestCoreProcessing::sgPreservesQuadratic()
{
    // S-G（阶数 2）精确重现二次多项式（首尾边界核也来自同一最小二乘拟合）
    Chromatogram chrom;
    QVector<Signal> pts;
    for (int t = -10; t <= 10; ++t) {
        Signal s;
        s.retentionTimeMs = t;
        s.intensity = static_cast<double>(t * t);
        pts.append(s);
    }
    chrom.setSignalPoints(pts);

    FilterSavitzkyGolay f;
    f.configure(mapWith({{QStringLiteral("order"), 2}, {QStringLiteral("width"), 5}}));
    f.apply(chrom);

    const QVector<Signal>& out = chrom.processedPoints();
    QCOMPARE(out.size(), pts.size());
    for (int i = 0; i < out.size(); ++i) {
        QVERIFY2(std::fabs(out.at(i).intensity - pts.at(i).intensity) < 1e-6,
                 qPrintable(QStringLiteral("quadratic drift at %1: %2 vs %3")
                                .arg(i).arg(out.at(i).intensity).arg(pts.at(i).intensity)));
    }
}

void TestCoreProcessing::sgSmoothsSpike()
{
    // 中心单点尖峰 100：width5/order2 标准核 [-3/35, 12/35, 17/35, 12/35, -3/35]
    Chromatogram chrom;
    QVector<Signal> pts;
    for (int t = -10; t <= 10; ++t) {
        Signal s;
        s.retentionTimeMs = t;
        s.intensity = (t == 0) ? 100.0 : 0.0;
        pts.append(s);
    }
    chrom.setSignalPoints(pts);

    FilterSavitzkyGolay f;
    f.configure(mapWith({{QStringLiteral("order"), 2}, {QStringLiteral("width"), 5}}));
    f.apply(chrom);

    const QVector<Signal>& out = chrom.processedPoints();
    const int center = 10; // t=0 → 下标 10
    QVERIFY(std::fabs(out.at(center).intensity - (17.0 / 35.0) * 100.0) < 1e-6);
    QVERIFY(std::fabs(out.at(center - 1).intensity - (12.0 / 35.0) * 100.0) < 1e-6);
    QVERIFY(std::fabs(out.at(center - 2).intensity - (-3.0 / 35.0) * 100.0) < 1e-6);
    QVERIFY(std::fabs(out.at(center - 3).intensity) < 1e-9);
    // 面积守恒（核系数和为 1）
    double sum = 0.0;
    for (const Signal& s : out)
        sum += s.intensity;
    QVERIFY(std::fabs(sum - 100.0) < 1e-6);
}

void TestCoreProcessing::sgClampsParameters()
{
    // 构造纠正（MODULE_10 §2.2）：width 强制奇数且 ≥5、order 钳位 [0,5] 且 < width
    FilterSavitzkyGolay f;
    f.configure(mapWith({{QStringLiteral("order"), 10}, {QStringLiteral("width"), 6}}));
    QCOMPARE(f.width(), 5);
    QCOMPARE(f.order(), 4);
}

// ---------- BaselineLinear ----------

void TestCoreProcessing::baselineLinearChord()
{
    // 线性基线 = 起点到终点的两点式直线，按输入 RT 网格采样
    Chromatogram chrom;
    const double ys[] = {0.0, 5.0, 3.0, 1.0, 6.0};
    QVector<Signal> pts;
    for (int i = 0; i < 5; ++i) {
        Signal s;
        s.retentionTimeMs = i;
        s.intensity = ys[i];
        pts.append(s);
    }
    chrom.setSignalPoints(pts);

    BaselineLinear bl;
    const QVector<Signal> base = bl.detect(chrom);
    QCOMPARE(base.size(), 5);
    const double expected[] = {0.0, 1.5, 3.0, 4.5, 6.0}; // 弦 (0,0)-(4,6)：y=1.5t
    for (int i = 0; i < 5; ++i) {
        QVERIFY(std::fabs(base.at(i).intensity - expected[i]) < 1e-9);
        QCOMPARE(base.at(i).retentionTimeMs, static_cast<qint64>(i));
    }

    // 纯线性漂移：基线 == 信号
    Chromatogram drift;
    QVector<Signal> dpts;
    for (int t = 0; t <= 4; ++t) {
        Signal s;
        s.retentionTimeMs = t;
        s.intensity = 25.0 * t;
        dpts.append(s);
    }
    drift.setSignalPoints(dpts);
    const QVector<Signal> dbase = bl.detect(drift);
    QCOMPARE(dbase.size(), 5);
    for (int i = 0; i < 5; ++i)
        QVERIFY(std::fabs(dbase.at(i).intensity - dpts.at(i).intensity) < 1e-9);
}

// ---------- PeakDetectorFirstDerivative ----------

void TestCoreProcessing::peakDetectorTwoResolvedPeaks()
{
    // 金标准：两个可分辨高斯峰 → 检出 2 峰，峰顶 ≈ 600/1200ms（±1 扫描）
    Chromatogram chrom = makeTwoPeakChrom();
    PeakDetectorFirstDerivative d;
    d.configure(mapWith({{QStringLiteral("threshold"), QStringLiteral("MEDIUM")},
                         {QStringLiteral("windowSize"), 0}}));
    const QList<IRawPeak*> peaks = d.detect(chrom);
    QCOMPARE(peaks.size(), 2);
    QVERIFY(qAbs(peaks.at(0)->apexRTMs() - 600) <= 10);
    QVERIFY(qAbs(peaks.at(1)->apexRTMs() - 1200) <= 10);
    QVERIFY(peaks.at(0)->startRTMs() < peaks.at(0)->apexRTMs());
    QVERIFY(peaks.at(0)->apexRTMs() < peaks.at(0)->stopRTMs());
    QVERIFY(peaks.at(1)->startRTMs() < peaks.at(1)->apexRTMs());
    QVERIFY(peaks.at(1)->apexRTMs() < peaks.at(1)->stopRTMs());
    qDeleteAll(peaks);
}

void TestCoreProcessing::peakDetectorThresholdFiltering()
{
    // 阈值过滤：归一化后凸包最大斜率 ≈0.0433 → LOW(0.005) 检出、MEDIUM(0.05)/HIGH(0.5) 不检出
    Chromatogram chrom = makePeakPlusBumpChrom();
    PeakDetectorFirstDerivative d;

    d.configure(mapWith({{QStringLiteral("threshold"), QStringLiteral("LOW")},
                         {QStringLiteral("windowSize"), 0}}));
    QList<IRawPeak*> low = d.detect(chrom);
    QCOMPARE(low.size(), 2);
    QVERIFY(qAbs(low.at(0)->apexRTMs() - 600) <= 10);
    QVERIFY(qAbs(low.at(1)->apexRTMs() - 30000) <= 50);
    qDeleteAll(low);

    d.configure(mapWith({{QStringLiteral("threshold"), QStringLiteral("MEDIUM")},
                         {QStringLiteral("windowSize"), 0}}));
    QList<IRawPeak*> medium = d.detect(chrom);
    QCOMPARE(medium.size(), 1);
    QVERIFY(qAbs(medium.at(0)->apexRTMs() - 600) <= 10);
    qDeleteAll(medium);

    d.configure(mapWith({{QStringLiteral("threshold"), QStringLiteral("HIGH")},
                         {QStringLiteral("windowSize"), 0}}));
    QList<IRawPeak*> high = d.detect(chrom);
    QCOMPARE(high.size(), 1);
    QVERIFY(qAbs(high.at(0)->apexRTMs() - 600) <= 10);
    qDeleteAll(high);
}

void TestCoreProcessing::peakDetectorDefaultWindowSize()
{
    // 默认配置（MEDIUM / windowSize=5 居中滑动平均）仍能检出两峰，峰顶偏差放宽到 ±2 扫描
    Chromatogram chrom = makeTwoPeakChrom();
    PeakDetectorFirstDerivative d;
    const QList<IRawPeak*> peaks = d.detect(chrom);
    QCOMPARE(peaks.size(), 2);
    QVERIFY(qAbs(peaks.at(0)->apexRTMs() - 600) <= 20);
    QVERIFY(qAbs(peaks.at(1)->apexRTMs() - 1200) <= 20);
    qDeleteAll(peaks);
}

void TestCoreProcessing::peakDetectorFlatLineNoPeaks()
{
    // 平线：任何档位都不产生伪峰
    Chromatogram chrom;
    QVector<Signal> pts;
    for (int t = 0; t <= 1000; t += 10) {
        Signal s;
        s.retentionTimeMs = t;
        s.intensity = 5.0;
        pts.append(s);
    }
    chrom.setSignalPoints(pts);

    PeakDetectorFirstDerivative d;
    d.configure(mapWith({{QStringLiteral("threshold"), QStringLiteral("LOW")},
                         {QStringLiteral("windowSize"), 0}}));
    const QList<IRawPeak*> peaks = d.detect(chrom);
    QCOMPARE(peaks.size(), 0);
}

void TestCoreProcessing::peakDetectorThresholdMapping()
{
    // BasePeakDetector 硬编码阈值映射（MODULE_04 §2.2.4）
    QCOMPARE(PeakDetectorFirstDerivative::thresholdValue(QStringLiteral("OFF")), 0.0005);
    QCOMPARE(PeakDetectorFirstDerivative::thresholdValue(QStringLiteral("LOW")), 0.005);
    QCOMPARE(PeakDetectorFirstDerivative::thresholdValue(QStringLiteral("MEDIUM")), 0.05);
    QCOMPARE(PeakDetectorFirstDerivative::thresholdValue(QStringLiteral("HIGH")), 0.5);
    QCOMPARE(PeakDetectorFirstDerivative::thresholdValue(QStringLiteral("0.1")), 0.1);
    QCOMPARE(PeakDetectorFirstDerivative::thresholdValue(QStringLiteral("garbage")), 0.05);
}

// ---------- IntegratorTrapezoid ----------

// 三角信号：RT 0..200ms 步 20ms，峰值 50（½·200·50 = 5000；÷100 校正因子 → 50.0）
QVector<Signal> makeTriangleProfile(double height, double baseOffset = 0.0)
{
    QVector<Signal> pts;
    for (int t = 0; t <= 200; t += 20) {
        Signal s;
        s.retentionTimeMs = t;
        const double h = (t <= 100) ? height * t / 100.0 : height * (200 - t) / 100.0;
        s.intensity = h + baseOffset;
        pts.append(s);
    }
    return pts;
}

void TestCoreProcessing::integratorTriangleGoldenArea()
{
    // VV 背景 = 峰两端信号点连直线（此处基线 0）→ 纯信号即三角 → 面积 50.0
    Chromatogram chrom;
    const QVector<Signal> pts = makeTriangleProfile(50.0);
    chrom.setSignalPoints(pts);

    Peak p;
    p.startRTMs = 0;
    p.apexRTMs = 100;
    p.stopRTMs = 200;
    p.profile = pts;

    IntegratorTrapezoid itg;
    itg.configure(mapWith({{QStringLiteral("useAreaConstraint"), true}}));
    QList<Peak> peaks;
    peaks.append(p);
    itg.integrate(chrom, peaks);

    QVERIFY(std::fabs(peaks.at(0).peakArea - 50.0) < 1e-6);
}

void TestCoreProcessing::integratorSlopedBaseline()
{
    // 整体抬高 10：背景线 (0,10)-(200,10)=10，纯信号仍为三角 → 面积仍 50.0
    Chromatogram chrom;
    const QVector<Signal> pts = makeTriangleProfile(50.0, 10.0);
    chrom.setSignalPoints(pts);

    Peak p;
    p.startRTMs = 0;
    p.apexRTMs = 100;
    p.stopRTMs = 200;
    p.profile = pts;

    IntegratorTrapezoid itg;
    itg.configure(mapWith({{QStringLiteral("useAreaConstraint"), true}}));
    QList<Peak> peaks;
    peaks.append(p);
    itg.integrate(chrom, peaks);

    QVERIFY(std::fabs(peaks.at(0).peakArea - 50.0) < 1e-6);
}

void TestCoreProcessing::integratorAreaConstraint()
{
    // 极小峰：½·200·0.1 = 10 → ÷100 = 0.1；面积约束 true → 置 0；false → 保留 0.1
    Chromatogram chrom;
    const QVector<Signal> pts = makeTriangleProfile(0.1);
    chrom.setSignalPoints(pts);

    Peak p;
    p.startRTMs = 0;
    p.apexRTMs = 100;
    p.stopRTMs = 200;
    p.profile = pts;

    IntegratorTrapezoid itg;
    QList<Peak> constrained;
    constrained.append(p);
    itg.configure(mapWith({{QStringLiteral("useAreaConstraint"), true}}));
    itg.integrate(chrom, constrained);
    QVERIFY(std::fabs(constrained.at(0).peakArea - 0.0) < 1e-12);

    QList<Peak> unconstrained;
    unconstrained.append(p);
    itg.configure(mapWith({{QStringLiteral("useAreaConstraint"), false}}));
    itg.integrate(chrom, unconstrained);
    QVERIFY(std::fabs(unconstrained.at(0).peakArea - 0.1) < 1e-9);
}

void TestCoreProcessing::integratorFallbackToChromatogram()
{
    // 峰无 profile → 积分器从色谱工作信号按 [start,stop] 提取
    Chromatogram chrom;
    const QVector<Signal> pts = makeTriangleProfile(50.0);
    chrom.setSignalPoints(pts);

    Peak p;
    p.startRTMs = 0;
    p.apexRTMs = 100;
    p.stopRTMs = 200;

    IntegratorTrapezoid itg;
    itg.configure(mapWith({{QStringLiteral("useAreaConstraint"), true}}));
    QList<Peak> peaks;
    peaks.append(p);
    itg.integrate(chrom, peaks);

    QVERIFY(std::fabs(peaks.at(0).peakArea - 50.0) < 1e-6);
}

// ---------- QuantifierCalibration ----------

void TestCoreProcessing::quantifierLeastSquares()
{
    // 校准点 (1,100)(2,200)(5,500) → 最小二乘 area = 100·conc + 0
    CalibrationTable calib;
    calib.componentName = QStringLiteral("caffeine");
    calib.points = {{1.0, 100.0}, {2.0, 200.0}, {5.0, 500.0}};

    QList<Peak> peaks;
    Peak a;
    a.apexRTMs = 600;
    a.peakArea = 250.0;
    peaks.append(a);
    Peak b;
    b.apexRTMs = 1200;
    b.peakArea = 1000.0;
    peaks.append(b);
    Peak c;
    c.apexRTMs = 1800;
    c.peakArea = 0.0;
    peaks.append(c);

    QuantifierCalibration q;
    q.configure(mapWith({{QStringLiteral("unit"), QStringLiteral("mg/L")}}));
    const QList<QuantEntry> entries = q.quantitate(peaks, calib);

    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).componentName, QStringLiteral("caffeine"));
    QCOMPARE(entries.at(0).unit, QStringLiteral("mg/L"));
    QCOMPARE(entries.at(0).apexRTMs, static_cast<qint64>(600));
    QCOMPARE(entries.at(0).area, 250.0);
    QVERIFY(std::fabs(entries.at(0).concentration - 2.5) < 1e-9);
    QVERIFY(std::fabs(entries.at(1).concentration - 10.0) < 1e-9);
    QVERIFY(std::fabs(entries.at(2).concentration) < 1e-12);
}

void TestCoreProcessing::quantifierSinglePointAndEmpty()
{
    // 单点校准 (3,300) → 过原点斜率 100
    CalibrationTable single;
    single.componentName = QStringLiteral("x");
    single.points = {{3.0, 300.0}};

    QList<Peak> peaks;
    Peak p;
    p.apexRTMs = 600;
    p.peakArea = 600.0;
    peaks.append(p);

    QuantifierCalibration q;
    QList<QuantEntry> entries = q.quantitate(peaks, single);
    QCOMPARE(entries.size(), 1);
    QVERIFY(std::fabs(entries.at(0).concentration - 6.0) < 1e-9);

    // 空校准表 → 无可拟合曲线 → 浓度 0
    CalibrationTable empty;
    empty.componentName = QStringLiteral("y");
    entries = q.quantitate(peaks, empty);
    QCOMPARE(entries.size(), 1);
    QVERIFY(std::fabs(entries.at(0).concentration) < 1e-12);
}

// ---------- Registry / ProcessingPipeline ----------

void TestCoreProcessing::registryBuiltIns()
{
    Registry& r = Registry::instance();
    QCOMPARE(r.availableFilterIds(), QStringList{QStringLiteral("sg_smooth")});
    QCOMPARE(r.availableBaselineIds(), QStringList{QStringLiteral("baseline_linear")});
    QCOMPARE(r.availablePeakDetectorIds(),
             QStringList{QStringLiteral("first_derivative_peak_detector")});
    QCOMPARE(r.availableIntegratorIds(), QStringList{QStringLiteral("trapezoid_integrator")});
    QCOMPARE(r.availableQuantifierIds(), QStringList{QStringLiteral("calibration_curve")});

    std::unique_ptr<IFilter> f(r.createFilter(QStringLiteral("sg_smooth")));
    QVERIFY(f);
    QCOMPARE(f->id(), QStringLiteral("sg_smooth"));
    QVERIFY(!f->displayName().isEmpty());
    std::unique_ptr<IPeakDetector> d(r.createPeakDetector(QStringLiteral("first_derivative_peak_detector")));
    QVERIFY(d);
    std::unique_ptr<IIntegrator> itg(r.createIntegrator(QStringLiteral("trapezoid_integrator")));
    QVERIFY(itg);
    std::unique_ptr<IBaselineDetector> bl(r.createBaseline(QStringLiteral("baseline_linear")));
    QVERIFY(bl);
    std::unique_ptr<IQuantifier> q(r.createQuantifier(QStringLiteral("calibration_curve")));
    QVERIFY(q);

    QVERIFY(r.createFilter(QStringLiteral("no_such")) == nullptr);
    QVERIFY(&Registry::instance() == &r); // 单例
}

void TestCoreProcessing::pipelineFullMethod()
{
    // 完整方法链：SG 平滑 → 线性基线 → 一阶导数峰检测 → 梯形积分 → 校准定量
    Chromatogram chrom = makeTwoPeakChrom();
    ProcessingPipeline pipeline(Registry::instance());
    QSignalSpy stepSpy(&pipeline, &ProcessingPipeline::sigStepFinished);
    QSignalSpy doneSpy(&pipeline, &ProcessingPipeline::sigFinished);

    Method method;
    method.steps.append({QStringLiteral("sg_smooth"),
                         mapWith({{QStringLiteral("order"), 2}, {QStringLiteral("width"), 5}})});
    method.steps.append({QStringLiteral("baseline_linear"), QVariantMap()});
    method.steps.append({QStringLiteral("first_derivative_peak_detector"),
                         mapWith({{QStringLiteral("threshold"), QStringLiteral("MEDIUM")},
                                  {QStringLiteral("windowSize"), 0}})});
    method.steps.append({QStringLiteral("trapezoid_integrator"),
                         mapWith({{QStringLiteral("useAreaConstraint"), true}})});
    QVariantMap quantParams;
    quantParams.insert(QStringLiteral("componentName"), QStringLiteral("caffeine"));
    quantParams.insert(QStringLiteral("unit"), QStringLiteral("mg/L"));
    QVariantList points;
    QVariantMap p1;
    p1.insert(QStringLiteral("concentration"), 1.0);
    p1.insert(QStringLiteral("area"), 100.0);
    points.append(p1);
    QVariantMap p2;
    p2.insert(QStringLiteral("concentration"), 2.0);
    p2.insert(QStringLiteral("area"), 200.0);
    points.append(p2);
    QVariantMap p3;
    p3.insert(QStringLiteral("concentration"), 5.0);
    p3.insert(QStringLiteral("area"), 500.0);
    points.append(p3);
    quantParams.insert(QStringLiteral("points"), points);
    method.steps.append({QStringLiteral("calibration_curve"), quantParams});

    pipeline.execute(method, chrom);

    // 滤波生效：处理后副本长度保持、内容确实被写
    QCOMPARE(chrom.processedPoints().size(), chrom.signalPoints().size());
    bool changed = false;
    for (int i = 0; i < chrom.processedPoints().size(); ++i) {
        if (!qFuzzyCompare(chrom.processedPoints().at(i).intensity,
                           chrom.signalPoints().at(i).intensity)) {
            changed = true;
            break;
        }
    }
    QVERIFY(changed);

    // 峰检测 + 积分
    QCOMPARE(pipeline.peaks().size(), 2);
    QVERIFY(qAbs(pipeline.peaks().at(0).apexRTMs - 600) <= 20);
    QVERIFY(qAbs(pipeline.peaks().at(1).apexRTMs - 1200) <= 20);
    QVERIFY(pipeline.peaks().at(0).peakHeight > 99.0 && pipeline.peaks().at(0).peakHeight <= 100.0);
    QVERIFY(pipeline.peaks().at(1).peakHeight > 79.0 && pipeline.peaks().at(1).peakHeight <= 80.0);
    QVERIFY(pipeline.peaks().at(0).peakArea > pipeline.peaks().at(1).peakArea);
    QVERIFY(pipeline.peaks().at(0).peakArea > 0.0);

    // 基线步
    QCOMPARE(pipeline.baseline().size(), chrom.signalPoints().size());

    // 定量
    QCOMPARE(pipeline.quantEntries().size(), 2);
    QCOMPARE(pipeline.quantEntries().at(0).componentName, QStringLiteral("caffeine"));
    QCOMPARE(pipeline.quantEntries().at(0).unit, QStringLiteral("mg/L"));
    QVERIFY(pipeline.quantEntries().at(0).concentration > 0.0);
    QVERIFY(pipeline.quantEntries().at(0).concentration > pipeline.quantEntries().at(1).concentration);

    // 信号：每步一次 sigStepFinished(index, id)，最后 sigFinished 一次
    QCOMPARE(stepSpy.count(), 5);
    const QStringList expectedIds{
        QStringLiteral("sg_smooth"),
        QStringLiteral("baseline_linear"),
        QStringLiteral("first_derivative_peak_detector"),
        QStringLiteral("trapezoid_integrator"),
        QStringLiteral("calibration_curve"),
    };
    for (int i = 0; i < 5; ++i) {
        const QList<QVariant>& args = stepSpy.at(i);
        QCOMPARE(args.at(0).toInt(), i);
        QCOMPARE(args.at(1).toString(), expectedIds.at(i));
    }
    QCOMPARE(doneSpy.count(), 1);
    QVERIFY(chrom.isDirty());
}

void TestCoreProcessing::pipelineResetProcessed()
{
    // 重跑覆盖语义：执行开头重置 processed 为空；无滤波步骤时检测读原始信号
    Chromatogram chrom = makeTwoPeakChrom();
    QVector<Signal> stale;
    for (const Signal& s : chrom.signalPoints()) {
        Signal t = s;
        t.intensity = 999.0;
        stale.append(t);
    }
    chrom.setProcessedPoints(stale);

    Method method;
    method.steps.append({QStringLiteral("first_derivative_peak_detector"),
                         mapWith({{QStringLiteral("threshold"), QStringLiteral("MEDIUM")},
                                  {QStringLiteral("windowSize"), 0}})});

    ProcessingPipeline pipeline(Registry::instance());
    pipeline.execute(method, chrom);

    QVERIFY(chrom.processedPoints().isEmpty());
    QCOMPARE(pipeline.peaks().size(), 2);
}

QTEST_APPLESS_MAIN(TestCoreProcessing)

#include "t_core_processing.moc"
