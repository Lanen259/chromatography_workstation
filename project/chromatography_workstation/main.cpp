// main.cpp —— 主程序（M7 集成：装配 ui MainWindow + 6 模块链路）
// 正常启动：显示主窗口（导入 io → 运行 core_processing 管线 → 导出 report → 曲线显示）。
// --e2e：无头自检全链路（导入→处理→报告→显示），exit 0 成功。
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <cmath>

#include <core_model/Chromatogram.h>
#include <core_model/Method.h>
#include <ui/MainWindow.h>
#include <ui/PeakTableView.h>

namespace {

// 双高斯峰样本信号（0..2000ms 步 10ms，201 点）
cdsw::Chromatogram makeSampleChromatogram()
{
    cdsw::Chromatogram chrom;
    QVector<cdsw::Signal> pts;
    for (int t = 0; t <= 2000; t += 10) {
        const double d1 = (t - 600.0) / 120.0;
        const double d2 = (t - 1200.0) / 120.0;
        cdsw::Signal s;
        s.retentionTimeMs = t;
        s.intensity = 100.0 * std::exp(-0.5 * d1 * d1) + 80.0 * std::exp(-0.5 * d2 * d2);
        pts.append(s);
    }
    chrom.setSignalPoints(pts);
    chrom.setName(QStringLiteral("Demo"));
    return chrom;
}

cdsw::Method makeDefaultMethod()
{
    cdsw::Method method;
    method.steps.append(cdsw::ProcessingStep{
        QStringLiteral("first_derivative_peak_detector"),
        QVariantMap{ { QStringLiteral("threshold"), QStringLiteral("MEDIUM") },
                     { QStringLiteral("windowSize"), 0 } } });
    return method;
}

// 写样本 CSV（UTF-8，io 格式：retentionTimeMs,intensity）
bool writeSampleCsv(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    QTextStream out(&f);
    out.setCodec("UTF-8");
    out << "retentionTimeMs,intensity\n";
    for (int t = 0; t <= 2000; t += 10) {
        const double d1 = (t - 600.0) / 120.0;
        const double d2 = (t - 1200.0) / 120.0;
        out << t << ','
            << QString::number(100.0 * std::exp(-0.5 * d1 * d1) + 80.0 * std::exp(-0.5 * d2 * d2),
                               'f', 6)
            << '\n';
    }
    f.close();
    return true;
}

// E2E 自检：导入(io) → 管线(core_processing) → 报告(report) → UI 显示（峰表 2 行）
int runE2E()
{
    cdsw::MainWindow window;
    const QString csvPath = QDir::temp().filePath(QStringLiteral("cds_e2e_input.csv"));
    if (!writeSampleCsv(csvPath))
        return 1;
    if (!window.importCsv(csvPath))
        return 2;

    cdsw::Method method = makeDefaultMethod();
    window.setMethod(&method);
    window.runMethod();
    if (window.peakTableView()->model()->rowCount() != 2)
        return 3;

    const QString reportPath = QDir::temp().filePath(QStringLiteral("cds_e2e_report.csv"));
    if (!window.exportCsv(reportPath))
        return 4;
    QFile rf(reportPath);
    if (!rf.open(QIODevice::ReadOnly))
        return 5;
    const bool hasPeakTable = rf.readAll().contains("Peak Number");
    rf.remove();
    QFile::remove(csvPath);
    if (!hasPeakTable)
        return 6;

    qInfo("E2E OK: import -> pipeline -> report -> display chain green (2 peaks)");
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--e2e")))
        return runE2E();

    cdsw::MainWindow window;
    cdsw::Method method = makeDefaultMethod();
    window.setMethod(&method);
    cdsw::Chromatogram demo = makeSampleChromatogram();
    window.setChromatogram(&demo);
    window.show();
    return app.exec();
}
