// core_processing/src/IConfigurable.h —— 算法参数注入接口（模块内私有辅助）
//
// 不在契约 §4.2 冻结接口内：冻结接口的 apply/detect/quantitate 无参数形参，
// 算法参数（Method.step.parameters）由管线在执行前经本接口注入。属实现层辅助，
// 不对外暴露（其他模块只准用 include/core_processing/ 下的头）。
#pragma once
#include <QtCore/qvariant.h>
namespace cdsw {

class IConfigurable {
public:
    virtual ~IConfigurable() = default;
    virtual void configure(const QVariantMap& parameters) = 0;
};

} // namespace cdsw
