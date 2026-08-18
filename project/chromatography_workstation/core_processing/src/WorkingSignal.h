// core_processing/src/WorkingSignal.h —— 处理管线的「当前工作信号」约定（模块内私有辅助）
//
// 契约 §4.1：原始信号（signalPoints）永不改；处理后信号（processedPoints）为滤波/基线后副本，
// 「未处理时为空」「改参数→重跑管线→覆盖本副本」。
// 由此推出统一约定：所有算法消费 workingSignal() = processedPoints（非空）否则 signalPoints（原始）；
// 滤波器把结果写回 processedPoints()；管线每轮 execute() 开头重置 processed 为空 →
// 首个滤波器从原始开始、多滤波器链式串联、重跑覆盖。
#pragma once
#include <core_model/Chromatogram.h>
#include <core_model/Signal.h>
namespace cdsw {

inline const QVector<Signal>& workingSignal(const Chromatogram& chrom)
{
    const QVector<Signal>& processed = chrom.processedPoints();
    return processed.isEmpty() ? chrom.signalPoints() : processed;
}

} // namespace cdsw
