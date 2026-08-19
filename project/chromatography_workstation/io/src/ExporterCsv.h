// io/src/ExporterCsv.h —— CSV 导出器（IChromatogramExporter 实现，模块内私有）
#pragma once
#include <io/converters.h>

namespace cdsw {

class ExporterCsv : public IChromatogramExporter {
public:
    QStringList supportedExtensions() const override;
    bool export_(const QString& filePath, const Chromatogram& chrom) override;
};

} // namespace cdsw
