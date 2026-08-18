// io/tests/t_io.cpp —— 转换器 QTest（契约 §4.4 + 回环/格式字节/注册表/失败路径）
#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include <memory>

#include <io/converters.h>

using cdsw::ImportResult;
using cdsw::IChromatogramImporter;
using cdsw::IChromatogramExporter;
using cdsw::ConverterRegistry;
using cdsw::Chromatogram;
using cdsw::Signal;

class IoTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void emptyFileImport();
    void badLine();
    void headerVariants();
    void exportFormatBytes();
    void registry();
    void missingFileImport();

private:
    static Chromatogram makeFixedChromatogram();
    static QByteArray readFile(const QString& path);
    static bool writeFile(const QString& path, const QByteArray& content);
};

// —— 固定色谱：RT 网格 1000..2000 步长 250（5 点），强度取二进制精确表示值（6 位定点往返无损）——
Chromatogram IoTest::makeFixedChromatogram()
{
    Chromatogram c;
    c.setName(QStringLiteral("Std-1000ppm"));
    c.setConverterId(QStringLiteral("io.csv"));
    c.setScanDelayMs(1000);
    c.setScanIntervalMs(250);
    const QVector<Signal> points{
        Signal{ 1000, 12.5 },
        Signal{ 1250, 75.25 },
        Signal{ 1500, 180.0 },
        Signal{ 1750, 42.125 },
        Signal{ 2000, 3.0 },
    };
    c.setSignalPoints(points);
    return c;
}

QByteArray IoTest::readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

bool IoTest::writeFile(const QString& path, const QByteArray& content)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(content) == content.size();
}

// —— 核心回环：导出 → 导入 → 逐点一致 + 元数据一致 ——
void IoTest::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("roundtrip.csv"));

    const Chromatogram original = makeFixedChromatogram();

    std::unique_ptr<IChromatogramExporter> exporter(
        ConverterRegistry::instance().exporterFor(path));
    QVERIFY(exporter);
    QVERIFY(exporter->export_(path, original));

    std::unique_ptr<IChromatogramImporter> importer(
        ConverterRegistry::instance().importerFor(path));
    QVERIFY(importer);

    Chromatogram loaded;
    const ImportResult result = importer->import(path, loaded);
    QVERIFY2(result.ok, qPrintable(result.errorMessage));

    QCOMPARE(loaded.signalPoints().size(), original.signalPoints().size());
    for (int i = 0; i < original.signalPoints().size(); ++i) {
        QCOMPARE(loaded.signalPoints().at(i).retentionTimeMs,
                 original.signalPoints().at(i).retentionTimeMs);
        QCOMPARE(loaded.signalPoints().at(i).intensity,
                 original.signalPoints().at(i).intensity);
    }

    QCOMPARE(loaded.name(), QStringLiteral("roundtrip"));
    QCOMPARE(loaded.converterId(), QStringLiteral("io.csv"));
    QCOMPARE(loaded.scanDelayMs(), qint64(1000));
    QCOMPARE(loaded.scanIntervalMs(), qint64(250));
}

// —— 空文件 → ok=false ——
void IoTest::emptyFileImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("empty.csv"));
    QVERIFY(writeFile(path, QByteArray()));

    std::unique_ptr<IChromatogramImporter> importer(
        ConverterRegistry::instance().importerFor(path));
    QVERIFY(importer);

    Chromatogram chrom;
    const ImportResult result = importer->import(path, chrom);
    QCOMPARE(result.ok, false);
    QVERIFY(!result.errorMessage.isEmpty());
}

// —— 坏行/缺列/非数字 → ok=false 且 errorMessage 非空 ——
void IoTest::badLine()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 缺列：某行只有 1 列
    {
        const QString path = dir.filePath(QStringLiteral("missing_col.csv"));
        QVERIFY(writeFile(path, QByteArray("1000,1.0\n2000\n")));
        std::unique_ptr<IChromatogramImporter> importer(
            ConverterRegistry::instance().importerFor(path));
        QVERIFY(importer);
        Chromatogram chrom;
        const ImportResult result = importer->import(path, chrom);
        QCOMPARE(result.ok, false);
        QVERIFY(!result.errorMessage.isEmpty());
    }
    // 强度非数字
    {
        const QString path = dir.filePath(QStringLiteral("bad_intensity.csv"));
        QVERIFY(writeFile(path, QByteArray("1000,abc\n")));
        std::unique_ptr<IChromatogramImporter> importer(
            ConverterRegistry::instance().importerFor(path));
        QVERIFY(importer);
        Chromatogram chrom;
        const ImportResult result = importer->import(path, chrom);
        QCOMPARE(result.ok, false);
        QVERIFY(!result.errorMessage.isEmpty());
    }
    // 保留时间非整数
    {
        const QString path = dir.filePath(QStringLiteral("bad_rt.csv"));
        QVERIFY(writeFile(path, QByteArray("abc,1.0\n")));
        std::unique_ptr<IChromatogramImporter> importer(
            ConverterRegistry::instance().importerFor(path));
        QVERIFY(importer);
        Chromatogram chrom;
        const ImportResult result = importer->import(path, chrom);
        QCOMPARE(result.ok, false);
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

// —— 表头变体：有表头 / 无表头 / # 注释行 / retention 关键词表头 都能解析 ——
void IoTest::headerVariants()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    struct Case {
        QString name;
        QString content;
    };
    const QVector<Case> cases = {
        { QStringLiteral("with_header"),
          QStringLiteral("# exported by cdsw io\nretentionTimeMs,intensity\n1000,12.5\n1250,75.25\n") },
        { QStringLiteral("retention_keyword"),
          QStringLiteral("retention,intensity\n1000,12.5\n1250,75.25\n") },
        { QStringLiteral("no_header"),
          QStringLiteral("1000,12.5\n1250,75.25\n") },
        { QStringLiteral("crlf_and_trailing_blank"),
          QStringLiteral("retentionTimeMs,intensity\r\n1000,12.5\r\n1250,75.25\r\n") },
    };

    for (const Case& c : cases) {
        const QString path = dir.filePath(c.name + QStringLiteral(".csv"));
        QVERIFY2(writeFile(path, c.content.toUtf8()), qPrintable(c.name));

        std::unique_ptr<IChromatogramImporter> importer(
            ConverterRegistry::instance().importerFor(path));
        QVERIFY(importer);
        Chromatogram chrom;
        const ImportResult result = importer->import(path, chrom);
        QVERIFY2(result.ok, qPrintable(c.name + QStringLiteral(": ") + result.errorMessage));
        QCOMPARE(chrom.signalPoints().size(), 2);
        QCOMPARE(chrom.signalPoints().at(0).retentionTimeMs, qint64(1000));
        QCOMPARE(chrom.signalPoints().at(0).intensity, 12.5);
        QCOMPARE(chrom.signalPoints().at(1).retentionTimeMs, qint64(1250));
        QCOMPARE(chrom.signalPoints().at(1).intensity, 75.25);
    }
}

// —— 导出格式字节检查：首行表头 / LF 行尾 / 强度 6 位定点 ——
void IoTest::exportFormatBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("fmt.csv"));

    std::unique_ptr<IChromatogramExporter> exporter(
        ConverterRegistry::instance().exporterFor(path));
    QVERIFY(exporter);
    QVERIFY(exporter->export_(path, makeFixedChromatogram()));

    const QByteArray bytes = readFile(path);
    const QList<QByteArray> lines = bytes.split('\n');

    // 首行表头
    QCOMPARE(lines.value(0), QByteArray("retentionTimeMs,intensity"));
    // LF 行尾：末字节是 \n，全文件无 \r
    QVERIFY(bytes.endsWith('\n'));
    QVERIFY(!bytes.contains('\r'));
    // 强度 6 位定点、RT 整数毫秒
    QCOMPARE(lines.value(1), QByteArray("1000,12.500000"));
    QCOMPARE(lines.value(2), QByteArray("1250,75.250000"));
    QCOMPARE(lines.value(5), QByteArray("2000,3.000000"));
    // 1 表头 + 5 数据 + split 尾部空串 = 7 段
    QCOMPARE(lines.size(), 7);
}

// —— 注册表：内置 CSV 注册、按扩展名分派、未注册返回 nullptr ——
void IoTest::registry()
{
    auto& registry = ConverterRegistry::instance();

    QVERIFY(registry.allImportExtensions().contains(QStringLiteral(".csv")));

    std::unique_ptr<IChromatogramImporter> importer(
        registry.importerFor(QStringLiteral("x.csv")));
    QVERIFY(importer);
    QCOMPARE(importer->formatName(), QStringLiteral("CSV"));
    QVERIFY(importer->supportedExtensions().contains(QStringLiteral(".csv")));

    // 扩展名大小写不敏感
    QVERIFY(registry.importerFor(QStringLiteral("x.CSV")));
    // 未注册扩展名 → nullptr
    QCOMPARE(registry.importerFor(QStringLiteral("x.cdf")), nullptr);

    std::unique_ptr<IChromatogramExporter> exporter(
        registry.exporterFor(QStringLiteral("x.csv")));
    QVERIFY(exporter);
    QVERIFY(exporter->supportedExtensions().contains(QStringLiteral(".csv")));
    QCOMPARE(registry.exporterFor(QStringLiteral("x.cdf")), nullptr);

    // 开放注册：新格式 = 注册一行（开闭原则）
    struct FakeImporter : IChromatogramImporter {
        QStringList supportedExtensions() const override { return { QStringLiteral(".zzz") }; }
        QString formatName() const override { return QStringLiteral("FAKE"); }
        ImportResult import(const QString&, Chromatogram&) override { return ImportResult{ true }; }
    };
    registry.registerImporter([]() -> IChromatogramImporter* { return new FakeImporter; });
    QVERIFY(registry.allImportExtensions().contains(QStringLiteral(".zzz")));
    std::unique_ptr<IChromatogramImporter> fake(
        registry.importerFor(QStringLiteral("x.zzz")));
    QVERIFY(fake);
}

// —— 失败路径：导入不存在的路径 → ok=false ——
void IoTest::missingFileImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // 父目录不存在 → QFile::open 失败
    const QString path = dir.filePath(QStringLiteral("missing/nope.csv"));

    std::unique_ptr<IChromatogramImporter> importer(
        ConverterRegistry::instance().importerFor(path));
    QVERIFY(importer);

    Chromatogram chrom;
    const ImportResult result = importer->import(path, chrom);
    QCOMPARE(result.ok, false);
    QVERIFY(!result.errorMessage.isEmpty());
}

QTEST_APPLESS_MAIN(IoTest)
#include "t_io.moc"
