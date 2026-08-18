// core_processing/src/IntegratorTrapezoid.h —— 梯形积分（IIntegrator 实现）
//
// 对应 ChemClipse `integrator.supplier.trapezoid`（逆向 MODULE_04 §4.3）。
// VV 语义：每峰背景线 = 峰两端信号点的直线；逐段梯形积分纯信号 max(0, y−背景)。
// 结果 ÷ CORRECTION_FACTOR_TRAPEZOID=100（ChemStation 因子，RT ms→百 ms 归一化）。
// 与参考实现的刻意偏差（离散模型下的正确类比）：OpenChrom 逐段 [rts[i], rts[i+1]−1]
// （减 1ms 防整毫秒界重叠），因其峰模型把丰度定义在每 ms 连续映射上；本模块剖面是离散
// qint64 RT 采样点，相邻段梯形只共享端点、无重叠歧义，故直接积满间隔 [rts[i], rts[i+1]]。
// 参数（configure / Method.step.parameters）：
//   "useAreaConstraint"  面积约束，默认 true（area<1 置 0；false 时 area<0 置 0）
#pragma once
#include <core_processing/interfaces.h>

#include "IConfigurable.h"
namespace cdsw {

class IntegratorTrapezoid final : public IIntegrator, public IConfigurable {
public:
    IntegratorTrapezoid();
    void configure(const QVariantMap& parameters) override;
    QString id() const override;
    void integrate(Chromatogram& chrom, QList<Peak>& peaks) const override;

private:
    bool m_useAreaConstraint = true;
};

} // namespace cdsw
