// report/src/ReporterCsv.cpp —— CSV 报告器实现（分区式：表头节 + 峰表 + 定量表）
#include "ReporterCsv.h"

#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>

#include <algorithm>

namespace cdsw {

namespace {

// RFC4180 引号：字段含逗号/双引号/换行时加双引号包裹，内部双引号翻倍
QString csvEscape(const QString& field)
{
    const bool needQuote = field.contains(QLatin1Char(','))
        || field.contains(QLatin1Char('"'))
        || field.contains(QLatin1Char('\n'));
    if (!needQuote)
        return field;
    QString escaped = field;
    escaped.replace(QLatin1String("\""), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// 保留时间毫秒 → 分钟定点 6 位（MODULE_06 MINUTE_CORRELATION_FACTOR = 60000.0）
QString rtMinutes(qint64 ms)
{
    return QString::number(ms / 60000.0, 'f', 6);
}

QString fixed6(double v)
{
    return QString::number(v, 'f', 6);
}

void writeCsvRow(QTextStream& out, const QList<QString>& cells)
{
    for (int i = 0; i < cells.size(); ++i) {
        if (i > 0)
            out << QLatin1Char(',');
        out << csvEscape(cells.at(i));
    }
    out << QLatin1Char('\n');
}

} // namespace

QString ReporterCsv::formatName() const { return QStringLiteral("CSV"); }

QString ReporterCsv::fileSuffix() const { return QStringLiteral(".csv"); }

bool ReporterCsv::generate(const ReportData& data, const QString& filePath)
{
    QFile file(filePath);
    // 不带 QIODevice::Text：\n 原样写（LF 行尾），保证跨平台字节一致
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QTextStream out(&file);   // QTextStream 默认 UTF-8

    // —— 表头节 ——
    writeCsvRow(out, { QStringLiteral("Sample Name"), data.sampleName });
    writeCsvRow(out, { QStringLiteral("Method Name"), data.methodName });
    writeCsvRow(out, { QStringLiteral("Acquired At"), data.acquiredAt.toString(Qt::ISODate) });
    writeCsvRow(out, { QStringLiteral("Number of Peaks"), QString::number(data.peaks.size()) });
    writeCsvRow(out, { QStringLiteral("Number of Quantitation Entries"),
                       QString::number(data.quantEntries.size()) });
    out << QLatin1Char('\n');

    // —— 峰表（按 apexRTMs 升序排序后编号 1..N；MODULE_06 PeakRetentionTimeComparator ASC）——
    writeCsvRow(out, { QStringLiteral("Peak Number"), QStringLiteral("Apex RT (min)"),
                       QStringLiteral("Start RT (min)"), QStringLiteral("Stop RT (min)"),
                       QStringLiteral("Height"), QStringLiteral("Area") });
    QList<Peak> sorted = data.peaks;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Peak& a, const Peak& b) { return a.apexRTMs < b.apexRTMs; });
    for (int i = 0; i < sorted.size(); ++i) {
        const Peak& p = sorted.at(i);
        writeCsvRow(out, { QString::number(i + 1), rtMinutes(p.apexRTMs), rtMinutes(p.startRTMs),
                           rtMinutes(p.stopRTMs), fixed6(p.peakHeight), fixed6(p.peakArea) });
    }
    out << QLatin1Char('\n');

    // —— 定量表（保持 ReportData 给定顺序）——
    writeCsvRow(out, { QStringLiteral("Component"), QStringLiteral("Apex RT (min)"),
                       QStringLiteral("Area"), QStringLiteral("Concentration"),
                       QStringLiteral("Unit") });
    for (const QuantEntry& e : data.quantEntries) {
        writeCsvRow(out, { e.componentName, rtMinutes(e.apexRTMs), fixed6(e.area),
                           fixed6(e.concentration), e.unit });
    }

    out.flush();
    return file.error() == QFileDevice::NoError;
}

} // namespace cdsw
