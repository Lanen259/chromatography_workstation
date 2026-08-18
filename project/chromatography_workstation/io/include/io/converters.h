// io/include/io/converters.h —— 导入导出转换器接口（契约 §4.4，冻结）
//
// 本文件逐字实现契约 §4.4 的公开类型：
//   ImportResult / IChromatogramImporter / IChromatogramExporter / ConverterRegistry
//
// 相对契约文本的「编译必要增补」（任务明示许可，不改冻结公开签名）：
//   1. 补齐 QtCore include（QString/QStringList/QHash）；
//   2. ConverterRegistry 补私有工厂表存储 + 私有默认构造（单例必需）。
#pragma once
#include <core_model/Chromatogram.h>
#include <QtCore/qhash.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
namespace cdsw {

struct ImportResult {
    bool ok = false;
    QString errorMessage;
};

class IChromatogramImporter {     // = chromatogramSupplier importConverter
public:
    virtual ~IChromatogramImporter() = default;
    virtual QStringList supportedExtensions() const = 0; // {".csv", ".cdf"}
    virtual QString formatName() const = 0;
    virtual ImportResult import(const QString& filePath, Chromatogram& out) = 0;
};

class IChromatogramExporter {     // = chromatogramSupplier exportConverter
public:
    virtual ~IChromatogramExporter() = default;
    virtual QStringList supportedExtensions() const = 0;
    virtual bool export_(const QString& filePath, const Chromatogram& chrom) = 0;
};

// 扩展名 → 转换器 的注册表（文件打开对话框按扩展名枚举，= OpenChrom UI 枚举 suppliers）
class ConverterRegistry {
public:
    static ConverterRegistry& instance();
    void registerImporter(IChromatogramImporter* (*factory)());
    void registerExporter(IChromatogramExporter* (*factory)());
    IChromatogramImporter* importerFor(const QString& filePath) const;
    IChromatogramExporter* exporterFor(const QString& filePath) const;
    QStringList allImportExtensions() const;
private:
    ConverterRegistry() = default;
    QHash<QString, IChromatogramImporter* (*)()> m_importerFactories;  // key: 小写扩展名含点 ".csv"
    QHash<QString, IChromatogramExporter* (*)()> m_exporterFactories;
};

} // namespace cdsw
