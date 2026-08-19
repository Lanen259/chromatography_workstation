// io/src/ExporterCsv.cpp —— CSV 导出器实现
//
// 自研 CSV 格式（与 ImporterCsv 严格互逆，格式锁定进 t_io.cpp 字节级测试）：
//   首行表头 retentionTimeMs,intensity；其后每行一个采样点「整数毫秒,6 位定点强度」。
//   行尾 LF（不带 QIODevice::Text 打开，跨平台字节一致）；显式 UTF-8（Qt5 QTextStream
//   默认 codec=codecForLocale()，中文 Windows=GBK，需覆盖为 UTF-8 才满足字节承诺）。
#include "ExporterCsv.h"

#include <QtCore/qfile.h>
#include <QtCore/qtextcodec.h>
#include <QtCore/qtextstream.h>

namespace cdsw {

QStringList ExporterCsv::supportedExtensions() const
{
    return { QStringLiteral(".csv") };
}

bool ExporterCsv::export_(const QString& filePath, const Chromatogram& chrom)
{
    QFile file(filePath);
    // 不带 QIODevice::Text：\n 原样写（LF 行尾），保证导出字节可跨平台复现
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QTextStream out(&file);
    out.setCodec(QTextCodec::codecForName("UTF-8"));   // Qt5 默认 locale codec，显式 UTF-8 保字节一致

    out << "retentionTimeMs,intensity\n";
    const QVector<Signal>& points = chrom.signalPoints();
    for (const Signal& s : points) {
        out << QString::number(s.retentionTimeMs)
            << QLatin1Char(',')
            << QString::number(s.intensity, 'f', 6)
            << QLatin1Char('\n');
    }

    out.flush();
    return file.error() == QFileDevice::NoError;
}

} // namespace cdsw
