// report/src/ReporterCsv.cpp —— CSV 报告器实现（分区式：表头节 + 峰表 + 定量表）
#include "ReporterCsv.h"

#include <QtCore/qfile.h>
#include <QtCore/qtextcodec.h>
#include <QtCore/qtextstream.h>

#include <algorithm>

namespace cdsw {

namespace {

constexpr int kFixedPrecision = 6;        // CSV 定点精度（金样字节规格，跨平台确定）
constexpr double kMsPerMinute = 60000.0;  // MODULE_06 MINUTE_CORRELATION_FACTOR

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

// 保留时间毫秒 → 分钟定点（kMsPerMinute）
QString rtMinutes(qint64 ms)
{
    return QString::number(ms / kMsPerMinute, 'f', kFixedPrecision);
}

QString fixedPrecision(double v)
{
    return QString::number(v, 'f', kFixedPrecision);
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

    QTextStream out(&file);
    // Qt5 QTextStream 默认 codec = codecForLocale()（中文 Windows = GBK），
    // 显式 UTF-8 才满足「UTF-8 跨平台字节一致」的金样承诺
    out.setCodec(QTextCodec::codecForName("UTF-8"));

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
                           rtMinutes(p.stopRTMs), fixedPrecision(p.peakHeight),
                           fixedPrecision(p.peakArea) });
    }
    out << QLatin1Char('\n');

    // —— 定量表（保持 ReportData 给定顺序）——
    writeCsvRow(out, { QStringLiteral("Component"), QStringLiteral("Apex RT (min)"),
                       QStringLiteral("Area"), QStringLiteral("Concentration"),
                       QStringLiteral("Unit") });
    for (const QuantEntry& e : data.quantEntries) {
        writeCsvRow(out, { e.componentName, rtMinutes(e.apexRTMs), fixedPrecision(e.area),
                           fixedPrecision(e.concentration), e.unit });
    }

    out.flush();
    return out.status() == QTextStream::Ok && file.error() == QFileDevice::NoError;
}

} // namespace cdsw
