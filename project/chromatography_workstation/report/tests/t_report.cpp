// report/tests/t_report.cpp —— 报告器 QTest（契约 §4.5 + 金样逐字节比对）
#include <QtTest/QtTest>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <memory>

#include <report/reporters.h>

using cdsw::ReportData;
using cdsw::IReporter;
using cdsw::ReportRegistry;
using cdsw::Peak;
using cdsw::QuantEntry;

class ReportTest : public QObject
{
    Q_OBJECT

private slots:
    void registryDispatch();
    void goldenReportMatchesReference();
    void emptyDataBoundary();
    void peaksWithoutQuant();
    void rfc4180Quoting();
    void generateFailsOnBadPath();

private:
    static ReportData makeGoldenData();
    static QByteArray readFile(const QString& path);
};

// —— 金样固定数据（数值取干净分钟值，便于人工复核）——
ReportData ReportTest::makeGoldenData()
{
    ReportData d;
    d.sampleName = QStringLiteral("Benzene Calibration Std");
    d.methodName = QStringLiteral("External Standard");
    d.acquiredAt = QDateTime(QDate(2026, 8, 18), QTime(10, 30, 0)); // Qt::LocalTime 默认 → ISODate 无时区后缀
    // 峰故意乱序传入：输出必须按 apexRTMs 升序重排后编号
    d.peaks = {
        Peak{ 186000, 204000, 222000, 31000.0, 77500.0 },   // apex 3.4 min
        Peak{ 60000,  75000,  90000,  52000.0, 104000.0 },  // apex 1.25 min
    };
    d.quantEntries = {
        QuantEntry{ 75000, QStringLiteral("Benzene"), 104000.0, 12.5, QStringLiteral("mg/L") },
        QuantEntry{ 204000, QStringLiteral("Toluene"), 77500.0, 25.0, QStringLiteral("mg/L") },
    };
    return d;
}

QByteArray ReportTest::readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

void ReportTest::registryDispatch()
{
    auto& registry = ReportRegistry::instance();

    // 内置 CSV 已注册
    QVERIFY(registry.availableFormats().contains(QStringLiteral("CSV")));

    std::unique_ptr<IReporter> csv(registry.reporterFor(QStringLiteral("CSV")));
    QVERIFY(csv);
    QCOMPARE(csv->formatName(), QStringLiteral("CSV"));
    QCOMPARE(csv->fileSuffix(), QStringLiteral(".csv"));

    // 未注册格式 → nullptr
    QCOMPARE(registry.reporterFor(QStringLiteral("PDF")), nullptr);

    // 开放注册：新格式 = 注册一行（开闭原则）
    struct FakeReporter : IReporter {
        QString formatName() const override { return QStringLiteral("FAKE"); }
        QString fileSuffix() const override { return QStringLiteral(".fake"); }
        bool generate(const ReportData&, const QString&) override { return true; }
    };
    registry.registerReporter([]() -> IReporter* { return new FakeReporter; });
    QVERIFY(registry.availableFormats().contains(QStringLiteral("FAKE")));
    std::unique_ptr<IReporter> fake(registry.reporterFor(QStringLiteral("FAKE")));
    QVERIFY(fake);
}

// —— 以下用例在 Task 2 实现 generate 后补全，本任务先以 stub 让编译通过 ——
void ReportTest::goldenReportMatchesReference() { QSKIP("Task 2 实现"); }
void ReportTest::emptyDataBoundary()            { QSKIP("Task 2 实现"); }
void ReportTest::peaksWithoutQuant()            { QSKIP("Task 2 实现"); }
void ReportTest::rfc4180Quoting()               { QSKIP("Task 2 实现"); }
void ReportTest::generateFailsOnBadPath()       { QSKIP("Task 2 实现"); }

QTEST_APPLESS_MAIN(ReportTest)
#include "t_report.moc"
