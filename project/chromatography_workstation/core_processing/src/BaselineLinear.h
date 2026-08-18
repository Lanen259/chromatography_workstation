// core_processing/src/BaselineLinear.h —— 线性基线（IBaselineDetector 实现）
//
// 对应一阶导数峰检测配套的基线处理。线性基线 = 起点到终点的两点式直线，按输入信号 RT 网格采样输出。
// 参数：无。
#pragma once
#include <core_processing/interfaces.h>

#include "IConfigurable.h"
namespace cdsw {

class BaselineLinear final : public IBaselineDetector, public IConfigurable {
public:
    void configure(const QVariantMap& parameters) override;
    QString id() const override;
    QVector<Signal> detect(const Chromatogram& chrom) const override;
};

} // namespace cdsw
