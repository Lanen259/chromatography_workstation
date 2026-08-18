// io/src/ImporterCsv.h —— CSV 导入器（IChromatogramImporter 实现，模块内私有）
#pragma once
#include <io/converters.h>

namespace cdsw {

class ImporterCsv : public IChromatogramImporter {
public:
    QStringList supportedExtensions() const override;
    QString formatName() const override;
    ImportResult import(const QString& filePath, Chromatogram& out) override;
};

} // namespace cdsw
