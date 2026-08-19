// io/src/ConverterRegistry.cpp —— 转换器注册表单例 + 内置 CSV 注册（开闭原则）
#include <io/converters.h>

#include <memory>

#include "ExporterCsv.h"
#include "ImporterCsv.h"

namespace cdsw {

namespace {

// 内置转换器注册（模块初始化 = ConverterRegistry::instance() 首次调用时执行一次）
bool registerBuiltIns(ConverterRegistry& r)
{
    r.registerImporter([]() -> IChromatogramImporter* { return new ImporterCsv; });
    r.registerExporter([]() -> IChromatogramExporter* { return new ExporterCsv; });
    return true;
}

// 从 filePath 提取小写扩展名（含点），纯字符串不碰文件系统；无扩展名返回空串
QString extensionOf(const QString& filePath)
{
    const int dot = filePath.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return QString();
    return filePath.mid(dot).toLower();
}

} // namespace

ConverterRegistry& ConverterRegistry::instance()
{
    static ConverterRegistry registry;
    static const bool registered = registerBuiltIns(registry);
    (void)registered;
    return registry;
}

void ConverterRegistry::registerImporter(IChromatogramImporter* (*factory)())
{
    if (!factory)
        return;
    // 注册需要键：实例化探针取 supportedExtensions（构造无副作用），按每个扩展名登记
    std::unique_ptr<IChromatogramImporter> probe(factory());
    if (!probe)
        return;
    const QStringList extensions = probe->supportedExtensions();
    for (const QString& ext : extensions)
        m_importerFactories.insert(ext.toLower(), factory);
}

void ConverterRegistry::registerExporter(IChromatogramExporter* (*factory)())
{
    if (!factory)
        return;
    std::unique_ptr<IChromatogramExporter> probe(factory());
    if (!probe)
        return;
    const QStringList extensions = probe->supportedExtensions();
    for (const QString& ext : extensions)
        m_exporterFactories.insert(ext.toLower(), factory);
}

IChromatogramImporter* ConverterRegistry::importerFor(const QString& filePath) const
{
    const QString ext = extensionOf(filePath);
    const auto it = m_importerFactories.constFind(ext);
    return (it != m_importerFactories.constEnd()) ? (it.value())() : nullptr;
}

IChromatogramExporter* ConverterRegistry::exporterFor(const QString& filePath) const
{
    const QString ext = extensionOf(filePath);
    const auto it = m_exporterFactories.constFind(ext);
    return (it != m_exporterFactories.constEnd()) ? (it.value())() : nullptr;
}

QStringList ConverterRegistry::allImportExtensions() const
{
    QStringList extensions = m_importerFactories.keys();
    extensions.sort();
    return extensions;
}

} // namespace cdsw
