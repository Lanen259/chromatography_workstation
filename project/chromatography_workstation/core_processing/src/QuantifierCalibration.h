// core_processing/src/QuantifierCalibration.h —— 校准曲线定量（IQuantifier 实现）
//
// 对应 OpenChrom peakQuantifierSupplier + 校准曲线（MODULE_05）。
// 对每个峰用校准表线性拟合 area = a·conc + b（最小二乘），反解 concentration = (area−b)/a。
// 简化：单表/单组分——一张校准表代表一个组分，本次调用的全部峰都套用该表（每峰一个 QuantEntry，
// componentName 均取表名）；RT 匹配多组分表留待后续里程碑按需扩展。
// 参数（configure / Method.step.parameters）：
//   "unit"  定量结果单位（默认空串；校准表本身不带单位，契约 M1 冻结口径）
// 校准表本体由管线从步骤参数 "componentName" + "points"[{concentration,area}] 构建后传入 quantitate()。
#pragma once
#include <core_processing/interfaces.h>

#include <QtCore/qstring.h>

#include "IConfigurable.h"
namespace cdsw {

class QuantifierCalibration final : public IQuantifier, public IConfigurable {
public:
    void configure(const QVariantMap& parameters) override;
    QString id() const override;
    QList<QuantEntry> quantitate(const QList<Peak>& peaks,
                                 const CalibrationTable& calib) const override;

private:
    QString m_unit;
};

} // namespace cdsw
