// core_processing/src/Registry.cpp —— 注册表单例 + 内置算法初始化注册
//
// 契约 §4.2：注册表 = 扩展点注册机制。算法实现类在 src/，模块初始化时注册（开闭原则：
// 新增算法 = 新增实现类 + 这里注册一行，不改既有类）。
#include <core_processing/interfaces.h>

#include <memory>

#include "BaselineLinear.h"
#include "FilterSavitzkyGolay.h"
#include "IntegratorTrapezoid.h"
#include "PeakDetectorFirstDerivative.h"
#include "QuantifierCalibration.h"

namespace cdsw {

namespace {

// 内置算法注册（模块初始化 = Registry::instance() 首次调用时执行一次）
bool registerBuiltIns(Registry& r)
{
    r.registerFilter([]() -> IFilter* { return new FilterSavitzkyGolay; });
    r.registerBaseline([]() -> IBaselineDetector* { return new BaselineLinear; });
    r.registerPeakDetector([]() -> IPeakDetector* { return new PeakDetectorFirstDerivative; });
    r.registerIntegrator([]() -> IIntegrator* { return new IntegratorTrapezoid; });
    r.registerQuantifier([]() -> IQuantifier* { return new QuantifierCalibration; });
    return true;
}

} // namespace

Registry& Registry::instance()
{
    static Registry registry;
    static const bool registered = registerBuiltIns(registry);
    (void)registered;
    return registry;
}

void Registry::registerFilter(IFilter* (*factory)())
{
    if (!factory)
        return;
    // 注册需要 id：实例化探针取 id（构造无副作用，只设默认参数）
    std::unique_ptr<IFilter> probe(factory());
    if (probe)
        m_filterFactories.insert(probe->id(), factory);
}

void Registry::registerBaseline(IBaselineDetector* (*factory)())
{
    if (!factory)
        return;
    std::unique_ptr<IBaselineDetector> probe(factory());
    if (probe)
        m_baselineFactories.insert(probe->id(), factory);
}

void Registry::registerPeakDetector(IPeakDetector* (*factory)())
{
    if (!factory)
        return;
    std::unique_ptr<IPeakDetector> probe(factory());
    if (probe)
        m_peakDetectorFactories.insert(probe->id(), factory);
}

void Registry::registerIntegrator(IIntegrator* (*factory)())
{
    if (!factory)
        return;
    std::unique_ptr<IIntegrator> probe(factory());
    if (probe)
        m_integratorFactories.insert(probe->id(), factory);
}

void Registry::registerQuantifier(IQuantifier* (*factory)())
{
    if (!factory)
        return;
    std::unique_ptr<IQuantifier> probe(factory());
    if (probe)
        m_quantifierFactories.insert(probe->id(), factory);
}

IFilter* Registry::createFilter(const QString& id) const
{
    const auto it = m_filterFactories.constFind(id);
    return (it != m_filterFactories.constEnd()) ? (it.value())() : nullptr;
}

IPeakDetector* Registry::createPeakDetector(const QString& id) const
{
    const auto it = m_peakDetectorFactories.constFind(id);
    return (it != m_peakDetectorFactories.constEnd()) ? (it.value())() : nullptr;
}

IIntegrator* Registry::createIntegrator(const QString& id) const
{
    const auto it = m_integratorFactories.constFind(id);
    return (it != m_integratorFactories.constEnd()) ? (it.value())() : nullptr;
}

IBaselineDetector* Registry::createBaseline(const QString& id) const
{
    const auto it = m_baselineFactories.constFind(id);
    return (it != m_baselineFactories.constEnd()) ? (it.value())() : nullptr;
}

IQuantifier* Registry::createQuantifier(const QString& id) const
{
    const auto it = m_quantifierFactories.constFind(id);
    return (it != m_quantifierFactories.constEnd()) ? (it.value())() : nullptr;
}

QStringList Registry::availableFilterIds() const
{
    QStringList ids = m_filterFactories.keys();
    ids.sort();
    return ids;
}

QStringList Registry::availablePeakDetectorIds() const
{
    QStringList ids = m_peakDetectorFactories.keys();
    ids.sort();
    return ids;
}

QStringList Registry::availableBaselineIds() const
{
    QStringList ids = m_baselineFactories.keys();
    ids.sort();
    return ids;
}

QStringList Registry::availableIntegratorIds() const
{
    QStringList ids = m_integratorFactories.keys();
    ids.sort();
    return ids;
}

QStringList Registry::availableQuantifierIds() const
{
    QStringList ids = m_quantifierFactories.keys();
    ids.sort();
    return ids;
}

} // namespace cdsw
