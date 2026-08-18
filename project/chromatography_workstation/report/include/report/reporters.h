// report/include/report/reporters.h —— 报告接口（契约 §4.5，冻结）
//
// 本文件逐字实现契约 §4.5 的三个公开类型：
//   ReportData / IReporter / ReportRegistry
//
// 相对契约文本的「编译必要增补」（任务明示许可，不改冻结公开签名）：
//   1. 补齐 QtCore include（QDateTime/QList/QString/QStringList/QHash）；
//   2. ReportRegistry 补私有工厂表存储 + 私有默认构造（单例必需）。
#pragma once
#include <core_model/Chromatogram.h>
#include <core_processing/interfaces.h>   // 复用 QuantEntry/Peak
#include <QtCore/qdatetime.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
namespace cdsw {

struct ReportData {                // = ReportColumns 的 Qt 版（见逆向 MODULE_06）
    QString sampleName;
    QString methodName;
    QDateTime acquiredAt;
    QList<Peak> peaks;
    QList<QuantEntry> quantEntries;
};

class IReporter {                 // = chromatogramReportSupplier
public:
    virtual ~IReporter() = default;
    virtual QString formatName() const = 0;        // "CSV" / "PDF"
    virtual QString fileSuffix() const = 0;        // ".csv"
    virtual bool generate(const ReportData& data, const QString& filePath) = 0;
};

class ReportRegistry {
public:
    static ReportRegistry& instance();
    void registerReporter(IReporter* (*factory)());
    QStringList availableFormats() const;
    IReporter* reporterFor(const QString& format) const;
private:
    ReportRegistry() = default;
    QHash<QString, IReporter* (*)()> m_reporterFactories;
};

} // namespace cdsw
