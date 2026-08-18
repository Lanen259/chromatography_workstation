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
    void utf8EncodingForCjk();
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
    // 注意：Peak 聚合初始化按字段序 {startRTMs, apexRTMs, stopRTMs, peakHeight, peakArea, ...}
    d.peaks = {
        Peak{ 186000, 204000, 222000, 31000.0, 77500.0 },   // apex 3.4 min
        Peak{ 60000,  75000,  90000,  52000.0, 104000.0 },  // apex 1.25 min
    };
    // QuantEntry 聚合初始化按字段序 {apexRTMs, componentName, area, concentration, unit}
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

void ReportTest::goldenReportMatchesReference()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("report.csv"));

    std::unique_ptr<IReporter> reporter(ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    QVERIFY(reporter);
    QVERIFY(reporter->generate(makeGoldenData(), outPath));

    const QString goldenPath = QStringLiteral(REPORT_GOLDEN_DIR "/golden_report.csv");
    QVERIFY2(QFileInfo::exists(goldenPath), qPrintable(goldenPath));
    QCOMPARE(readFile(outPath), readFile(goldenPath));
}

void ReportTest::emptyDataBoundary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("empty.csv"));

    std::unique_ptr<IReporter> reporter(ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    QVERIFY(reporter);
    QVERIFY(reporter->generate(ReportData(), outPath));

    const QString expected = QStringLiteral(
        "Sample Name,\n"
        "Method Name,\n"
        "Acquired At,\n"
        "Number of Peaks,0\n"
        "Number of Quantitation Entries,0\n"
        "\n"
        "Peak Number,Apex RT (min),Start RT (min),Stop RT (min),Height,Area\n"
        "\n"
        "Component,Apex RT (min),Area,Concentration,Unit\n");
    QCOMPARE(QString::fromUtf8(readFile(outPath)), expected);
}

void ReportTest::peaksWithoutQuant()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("peaks_only.csv"));

    ReportData d;
    d.sampleName = QStringLiteral("Sample With Peaks");
    d.acquiredAt = QDateTime(QDate(2026, 8, 18), QTime(9, 0, 0));
    d.peaks = { Peak{ 60000, 75000, 90000, 52000.0, 104000.0 } };

    std::unique_ptr<IReporter> reporter(ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    QVERIFY(reporter);
    QVERIFY(reporter->generate(d, outPath));

    const QString expected = QStringLiteral(
        "Sample Name,Sample With Peaks\n"
        "Method Name,\n"
        "Acquired At,2026-08-18T09:00:00\n"
        "Number of Peaks,1\n"
        "Number of Quantitation Entries,0\n"
        "\n"
        "Peak Number,Apex RT (min),Start RT (min),Stop RT (min),Height,Area\n"
        "1,1.250000,1.000000,1.500000,52000.000000,104000.000000\n"
        "\n"
        "Component,Apex RT (min),Area,Concentration,Unit\n");
    QCOMPARE(QString::fromUtf8(readFile(outPath)), expected);
}

void ReportTest::rfc4180Quoting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("quoting.csv"));

    ReportData d;
    d.sampleName = QStringLiteral("A,B");                 // 含逗号 → 加引号
    d.methodName = QStringLiteral("Say \"hi\"");          // 含双引号 → 内部翻倍
    d.peaks = { Peak{ 60000, 60000, 60000, 1.0, 2.0 } };
    d.quantEntries = { QuantEntry{ 60000, QStringLiteral("line1\nline2"), 2.0, 0.0, QString() } }; // 含换行 → 加引号

    std::unique_ptr<IReporter> reporter(ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    QVERIFY(reporter);
    QVERIFY(reporter->generate(d, outPath));

    const QString expected = QStringLiteral(
        "Sample Name,\"A,B\"\n"
        "Method Name,\"Say \"\"hi\"\"\"\n"
        "Acquired At,\n"
        "Number of Peaks,1\n"
        "Number of Quantitation Entries,1\n"
        "\n"
        "Peak Number,Apex RT (min),Start RT (min),Stop RT (min),Height,Area\n"
        "1,1.000000,1.000000,1.000000,1.000000,2.000000\n"
        "\n"
        "Component,Apex RT (min),Area,Concentration,Unit\n"
        "\"line1\nline2\",1.000000,2.000000,0.000000,\n");
    QCOMPARE(QString::fromUtf8(readFile(outPath)), expected);
}

void ReportTest::utf8EncodingForCjk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outPath = dir.filePath(QStringLiteral("utf8.csv"));

    // 中文 Windows 的 QTextStream 默认 codec 是 GBK：中文字段必须显式 UTF-8，否则字节不一致
    ReportData d;
    d.sampleName = QStringLiteral("苯标样");
    d.methodName = QStringLiteral("外标法");
    d.peaks = { Peak{ 60000, 75000, 90000, 52000.0, 104000.0 } };

    std::unique_ptr<IReporter> reporter(ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    QVERIFY(reporter);
    QVERIFY(reporter->generate(d, outPath));

    const QString expected = QStringLiteral(
        "Sample Name,苯标样\n"
        "Method Name,外标法\n"
        "Acquired At,\n"
        "Number of Peaks,1\n"
        "Number of Quantitation Entries,0\n"
        "\n"
        "Peak Number,Apex RT (min),Start RT (min),Stop RT (min),Height,Area\n"
        "1,1.250000,1.000000,1.500000,52000.000000,104000.000000\n"
        "\n"
        "Component,Apex RT (min),Area,Concentration,Unit\n");
    // 读原始字节 == 预期 UTF-8 字节（金样字节一致性对中文同样成立）
    QCOMPARE(readFile(outPath), expected.toUtf8());
}

void ReportTest::generateFailsOnBadPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // 父目录不存在 → QFile::open(WriteOnly) 失败 → generate 返回 false
    const QString badPath = dir.filePath(QStringLiteral("missing/report.csv"));

    std::unique_ptr<IReporter> reporter(ReportRegistry::instance().reporterFor(QStringLiteral("CSV")));
    QVERIFY(reporter);
    QCOMPARE(reporter->generate(ReportData(), badPath), false);
}

QTEST_APPLESS_MAIN(ReportTest)
#include "t_report.moc"
