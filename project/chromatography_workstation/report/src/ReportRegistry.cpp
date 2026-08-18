// report/src/ReportRegistry.cpp —— 报告注册表单例 + 内置格式注册（开闭原则）
#include <report/reporters.h>

#include <memory>

#include "ReporterCsv.h"

namespace cdsw {

namespace {

// 内置报告格式注册（模块初始化 = ReportRegistry::instance() 首次调用时执行一次）
bool registerBuiltIns(ReportRegistry& r)
{
    r.registerReporter([]() -> IReporter* { return new ReporterCsv; });
    return true;
}

} // namespace

ReportRegistry& ReportRegistry::instance()
{
    static ReportRegistry registry;
    static const bool registered = registerBuiltIns(registry);
    (void)registered;
    return registry;
}

void ReportRegistry::registerReporter(IReporter* (*factory)())
{
    if (!factory)
        return;
    // 注册需要键：实例化探针取 formatName（构造无副作用）
    std::unique_ptr<IReporter> probe(factory());
    if (probe)
        m_reporterFactories.insert(probe->formatName(), factory);
}

QStringList ReportRegistry::availableFormats() const
{
    QStringList formats = m_reporterFactories.keys();
    formats.sort();
    return formats;
}

IReporter* ReportRegistry::reporterFor(const QString& format) const
{
    const auto it = m_reporterFactories.constFind(format);
    return (it != m_reporterFactories.constEnd()) ? (it.value())() : nullptr;
}

} // namespace cdsw
