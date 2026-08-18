// core_processing/src/FilterSavitzkyGolay.h —— Savitzky-Golay 平滑（IFilter 实现）
//
// 对应 ChemClipse `xxd.filter.supplier.savitzkygolay`（逆向 MODULE_10 §2，Qt 可整段照搬）。
// 参数（configure / Method.step.parameters）：
//   "order"  多项式阶数，默认 2，钳位 [0,5] 且 < width
//   "width"  滤波宽度，默认 5，钳位 [5,51] 且强制奇数
// 算法：法方程最小二乘运行时求卷积权重（非查表），三区处理（中间对称核 + 首尾边界核）。
#pragma once
#include <core_processing/interfaces.h>

#include <QtCore/qvector.h>

#include "IConfigurable.h"
namespace cdsw {

class FilterSavitzkyGolay final : public IFilter, public IConfigurable {
public:
    FilterSavitzkyGolay();
    void configure(const QVariantMap& parameters) override;
    QString id() const override;
    QString displayName() const override;
    void apply(Chromatogram& chrom) override;

    // 配置查询（测试验证构造纠正用）
    int order() const { return m_order; }
    int width() const { return m_width; }

private:
    // 权重行 W[width][width]：W[r][k] = phi(t_r)·(X^T X)^-1·X^T；求解失败返回空
    QVector<QVector<double>> buildWeightRows() const;
    static double dot(const QVector<double>& weights, const QVector<Signal>& sig, int offset);

    int m_order = 2;
    int m_width = 5;
};

} // namespace cdsw
