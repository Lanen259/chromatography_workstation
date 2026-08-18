// core_model/tests/t_core_model.cpp —— 领域模型 QTest（契约 §4.1 测试点全覆盖）
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <core_model/Signal.h>
#include <core_model/Peak.h>
#include <core_model/Chromatogram.h>
#include <core_model/Selection.h>

using cdsw::Signal;
using cdsw::Peak;
using cdsw::Chromatogram;
using cdsw::Selection;

class CoreModelTest : public QObject
{
    Q_OBJECT

private slots:
    // —— §4.1-1 Signal / Peak 默认值与字段读写 ——
    void signalDefaultsAndWrite();
    void peakDefaultsAndWrite();
    // —— §4.1-2 元数据读写 ——
    void chromatogramMetadata();
    // —— §4.1-2 派生量：空信号边界 ——
    void emptySignalDerivedQuantities();
    // —— §4.1-2 派生量：有数据 ——
    void derivedQuantitiesWithData();
    // —— §4.1-2 processedPoints 与 signalPoints 分离 ——
    void processedSignalSeparation();
    // —— §4.1-3 扫描号反查 floor 语义 ——
    void scanNumberFloor();
    // —— §4.1-2 isDirty 翻转 ——
    void isDirtyFlip();
    // —— §4.1-4 Selection 信号与取值 ——
    void selectionSignals();

private:
    static QVector<Signal> makeSignal(
        std::initializer_list<qint64> rtMs, std::initializer_list<double> intensities);
};

QVector<Signal> CoreModelTest::makeSignal(
    std::initializer_list<qint64> rtMs, std::initializer_list<double> intensities)
{
    QVector<Signal> points;
    auto rtIt = rtMs.begin();
    auto intIt = intensities.begin();
    for (; rtIt != rtMs.end() && intIt != intensities.end(); ++rtIt, ++intIt)
        points.append(Signal{ *rtIt, *intIt });
    return points;
}

void CoreModelTest::signalDefaultsAndWrite()
{
    // 默认值：retentionTimeMs=0，intensity=0.0
    const Signal s;
    QCOMPARE(s.retentionTimeMs, qint64(0));
    QCOMPARE(s.intensity, 0.0);

    // 赋值后读回
    Signal w;
    w.retentionTimeMs = qint64(1234);
    w.intensity = 87.5;
    QCOMPARE(w.retentionTimeMs, qint64(1234));
    QCOMPARE(w.intensity, 87.5);
}

void CoreModelTest::peakDefaultsAndWrite()
{
    // 默认值：三 RT 全 0、两强度全 0.0、未删除、profile 空
    const Peak p;
    QCOMPARE(p.startRTMs, qint64(0));
    QCOMPARE(p.apexRTMs, qint64(0));
    QCOMPARE(p.stopRTMs, qint64(0));
    QCOMPARE(p.peakHeight, 0.0);
    QCOMPARE(p.peakArea, 0.0);
    QVERIFY(!p.markedAsDeleted);
    QVERIFY(p.profile.isEmpty());

    // 赋值后读回
    Peak w;
    w.startRTMs = qint64(100);
    w.apexRTMs = qint64(150);
    w.stopRTMs = qint64(200);
    w.peakHeight = 12.25;
    w.peakArea = 34.5;
    w.markedAsDeleted = true;
    w.profile = makeSignal({ 100, 150 }, { 1.0, 2.0 });
    QCOMPARE(w.startRTMs, qint64(100));
    QCOMPARE(w.apexRTMs, qint64(150));
    QCOMPARE(w.stopRTMs, qint64(200));
    QCOMPARE(w.peakHeight, 12.25);
    QCOMPARE(w.peakArea, 34.5);
    QVERIFY(w.markedAsDeleted);
    QCOMPARE(w.profile.size(), 2);
    QCOMPARE(w.profile.at(0).retentionTimeMs, qint64(100));
    QCOMPARE(w.profile.at(1).intensity, 2.0);
}

void CoreModelTest::chromatogramMetadata()
{
    Chromatogram c;
    // 默认：空名、空 converterId、未 finalized、网格 0
    QCOMPARE(c.name(), QString());
    QCOMPARE(c.converterId(), QString());
    QVERIFY(!c.isFinalized());
    QCOMPARE(c.scanDelayMs(), qint64(0));
    QCOMPARE(c.scanIntervalMs(), qint64(0));

    // 读写往返
    c.setName(QStringLiteral("Sample A"));
    c.setConverterId(QStringLiteral("xyz-format"));
    c.setFinalized(true);
    c.setScanDelayMs(qint64(1500));
    c.setScanIntervalMs(qint64(250));
    QCOMPARE(c.name(), QStringLiteral("Sample A"));
    QCOMPARE(c.converterId(), QStringLiteral("xyz-format"));
    QVERIFY(c.isFinalized());
    QCOMPARE(c.scanDelayMs(), qint64(1500));
    QCOMPARE(c.scanIntervalMs(), qint64(250));
}

void CoreModelTest::emptySignalDerivedQuantities()
{
    // 空信号：派生量全 0 / 0.0
    const Chromatogram c;
    QCOMPARE(c.startTimeMs(), qint64(0));
    QCOMPARE(c.stopTimeMs(), qint64(0));
    QCOMPARE(c.minIntensity(), 0.0);
    QCOMPARE(c.maxIntensity(), 0.0);
    QCOMPARE(c.scanCount(), 0);
}

void CoreModelTest::derivedQuantitiesWithData()
{
    Chromatogram c;
    c.setSignalPoints(makeSignal({ 100, 200, 300 }, { 5.0, 3.0, 8.0 }));
    // start/stop = 首/末点 retentionTimeMs；min/max = 实际极值
    QCOMPARE(c.startTimeMs(), qint64(100));
    QCOMPARE(c.stopTimeMs(), qint64(300));
    QCOMPARE(c.minIntensity(), 3.0);
    QCOMPARE(c.maxIntensity(), 8.0);
    QCOMPARE(c.scanCount(), 3);
}

void CoreModelTest::processedSignalSeparation()
{
    const QVector<Signal> raw  = makeSignal({ 100, 200 }, { 1.0, 2.0 });
    const QVector<Signal> proc = makeSignal({ 100, 200 }, { 9.0, 8.0 });

    // 未 setProcessedPoints 前 processedPoints 为空（即便 signalPoints 有数据）
    Chromatogram c;
    c.setSignalPoints(raw);
    QVERIFY(c.processedPoints().isEmpty());
    QCOMPARE(c.signalPoints().size(), 2);

    // setProcessedPoints 后各自持有
    c.setProcessedPoints(proc);
    QCOMPARE(c.processedPoints().size(), 2);
    QCOMPARE(c.processedPoints().at(0).intensity, 9.0);
    QCOMPARE(c.signalPoints().at(0).intensity, 1.0);

    // 再 setSignalPoints：只换原始，不覆盖已设 processed
    c.setSignalPoints(makeSignal({ 300, 400 }, { 4.0, 6.0 }));
    QCOMPARE(c.signalPoints().at(0).retentionTimeMs, qint64(300));
    QCOMPARE(c.signalPoints().size(), 2);
    QCOMPARE(c.processedPoints().size(), 2);
    QCOMPARE(c.processedPoints().at(0).intensity, 9.0);
    QCOMPARE(c.processedPoints().at(1).intensity, 8.0);

    // 再 setProcessedPoints：只换副本，signalPoints 原封不动
    c.setProcessedPoints(makeSignal({ 100 }, { 0.5 }));
    QCOMPARE(c.processedPoints().size(), 1);
    QCOMPARE(c.signalPoints().at(0).retentionTimeMs, qint64(300));
    QCOMPARE(c.signalPoints().size(), 2);
}

void CoreModelTest::scanNumberFloor()
{
    // 空信号 → 0
    Chromatogram empty;
    QCOMPARE(empty.scanNumberAtRetentionTime(qint64(100)), 0);

    // 升序 [100, 200, 300]
    Chromatogram c;
    c.setSignalPoints(makeSignal({ 100, 200, 300 }, { 1.0, 1.0, 1.0 }));

    // rt 早于首点 → 0
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(50)), 0);
    // 精确命中 → 对应 1-based 扫描号
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(100)), 1);
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(200)), 2);
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(300)), 3);
    // 两点之间 → floor 到前一个
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(150)), 1);
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(250)), 2);
    // rt 晚于末点 → 末点扫描号
    QCOMPARE(c.scanNumberAtRetentionTime(qint64(999)), 3);
}

void CoreModelTest::isDirtyFlip()
{
    // 默认 true
    Chromatogram c;
    QVERIFY(c.isDirty());
    c.setDirty(false);
    QVERIFY(!c.isDirty());
    c.setDirty(true);
    QVERIFY(c.isDirty());
}

void CoreModelTest::selectionSignals()
{
    Selection sel;
    QSignalSpy spy(&sel, &Selection::sigSelectionChanged);
    QCOMPARE(spy.count(), 0);

    // 连续两次 setRange：各触发一次 sigSelectionChanged，取值正确
    sel.setRange(qint64(100), qint64(500));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(sel.startMs(), qint64(100));
    QCOMPARE(sel.stopMs(), qint64(500));

    sel.setRange(qint64(200), qint64(600));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(sel.startMs(), qint64(200));
    QCOMPARE(sel.stopMs(), qint64(600));

    // setPeak 不触发信号
    Peak p;
    p.apexRTMs = qint64(12345);
    p.peakArea = 42.0;
    sel.setPeak(p);
    QCOMPARE(spy.count(), 2);
    // peak() 读回所设 Peak
    QCOMPARE(sel.peak().apexRTMs, qint64(12345));
    QCOMPARE(sel.peak().peakArea, 42.0);
}

QTEST_APPLESS_MAIN(CoreModelTest)
#include "t_core_model.moc"
