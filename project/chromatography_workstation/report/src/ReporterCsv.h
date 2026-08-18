// report/src/ReporterCsv.h —— CSV 报告器（IReporter 实现，模块内私有）
#pragma once
#include <report/reporters.h>

namespace cdsw {

class ReporterCsv : public IReporter {
public:
    QString formatName() const override;
    QString fileSuffix() const override;
    bool generate(const ReportData& data, const QString& filePath) override;
};

} // namespace cdsw
