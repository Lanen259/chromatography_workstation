// io/src/ImporterCsv.cpp —— CSV 导入器实现
//
// 自研 CSV 格式（与 ExporterCsv 严格互逆，格式锁定进 t_io.cpp）：
//   首行表头 retentionTimeMs,intensity；其后每行一个采样点「整数毫秒,6 位定点强度」。
//   '#' 开头为注释行（跳过）；空行跳过。坏行/缺列/非数字 → ok=false 并记录行号。
//   导入成功填：setSignalPoints + RT 网格（scanDelay=首点 RT，scanInterval=(末-首)/(n-1) 推断）
//                + setName(文件名不含扩展名) + setConverterId("io.csv")。
//   编码：显式 UTF-8（Qt5 QTextStream 默认 codec=codecForLocale()，中文 Windows=GBK）；
//   兼容 UTF-8 BOM（首行剥离）。
#include "ImporterCsv.h"

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextcodec.h>
#include <QtCore/qtextstream.h>

namespace cdsw {

namespace {
constexpr int kReservePoints = 4096;   // 预留点数，避免逐点扩容
} // namespace

QStringList ImporterCsv::supportedExtensions() const
{
    return { QStringLiteral(".csv") };
}

QString ImporterCsv::formatName() const
{
    return QStringLiteral("CSV");
}

ImportResult ImporterCsv::import(const QString& filePath, Chromatogram& out)
{
    QFile file(filePath);
    // Text 模式读：兼容 CRLF 行尾；显式 UTF-8 解码（Qt5 默认 codec=locale=GBK，需覆盖）
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ImportResult{ false, QStringLiteral("无法打开文件：") + file.errorString() };
    }

    QTextStream in(&file);
    in.setCodec(QTextCodec::codecForName("UTF-8"));
    QVector<Signal> points;
    points.reserve(kReservePoints);

    int lineNo = 0;
    bool firstCandidate = true;   // 仅对首个非注释/非空行做表头探测
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++lineNo;
        if (lineNo == 1 && !line.isEmpty() && line.at(0) == QChar(0xFEFF))
            line.remove(0, 1);    // 兼容 UTF-8 BOM
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;

        // 可选表头：首行含 retentionTimeMs / retention 关键词 → 跳过（不参与数据计数）
        if (firstCandidate
            && (trimmed.contains(QStringLiteral("retentionTimeMs"), Qt::CaseInsensitive)
                || trimmed.contains(QStringLiteral("retention"), Qt::CaseInsensitive))) {
            firstCandidate = false;
            continue;
        }
        firstCandidate = false;

        const QStringList cols = trimmed.split(QLatin1Char(','));
        if (cols.size() < 2) {
            return ImportResult{ false, QStringLiteral("第 %1 行：缺列（期望两列 retentionTimeMs,intensity）")
                                        .arg(lineNo) };
        }

        bool rtOk = false;
        bool intensityOk = false;
        const qint64 rtMs = cols.at(0).trimmed().toLongLong(&rtOk);
        const double intensity = cols.at(1).trimmed().toDouble(&intensityOk);
        if (!rtOk || !intensityOk) {
            return ImportResult{ false, QStringLiteral("第 %1 行：非数字（retentionTimeMs 需整数毫秒，intensity 需浮点）")
                                        .arg(lineNo) };
        }

        points.append(Signal{ rtMs, intensity });
    }

    if (points.isEmpty()) {
        return ImportResult{ false, QStringLiteral("文件中没有有效数据点（空文件或仅注释/表头）") };
    }

    out.setSignalPoints(points);
    out.setScanDelayMs(points.first().retentionTimeMs);
    // 首末点推断等间隔（网格语义 RT = scanDelay + i*scanInterval；单点无间隔）
    out.setScanIntervalMs((points.size() >= 2)
        ? (points.last().retentionTimeMs - points.first().retentionTimeMs) / (points.size() - 1)
        : 0);
    out.setName(QFileInfo(filePath).completeBaseName());
    out.setConverterId(QStringLiteral("io.csv"));
    return ImportResult{ true };
}

} // namespace cdsw
