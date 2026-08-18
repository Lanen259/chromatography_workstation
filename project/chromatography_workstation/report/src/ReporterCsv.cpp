// report/src/ReporterCsv.cpp —— CSV 报告器（Task 2 实现真实写入，本任务先桩）
#include "ReporterCsv.h"

namespace cdsw {

QString ReporterCsv::formatName() const { return QStringLiteral("CSV"); }

QString ReporterCsv::fileSuffix() const { return QStringLiteral(".csv"); }

bool ReporterCsv::generate(const ReportData&, const QString&)
{
    return false;   // 桩：Task 2 实现
}

} // namespace cdsw
