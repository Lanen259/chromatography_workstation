// core_processing/src/ProcessingPipeline.cpp —— 管线执行器实现
//
// 契约 §4.2：按 Method.steps 顺序执行，每步经注册表按 id 取算法实例、配置参数、执行，emit sigStepFinished。
// 未知步骤 id → 跳过继续（OpenChrom 引擎语义，MODULE_03 §7.5：找不到 supplier 警告并继续）。
#include <core_processing/interfaces.h>

#include <QtCore/qalgorithms.h>
#include <QtCore/qvariant.h>

#include "IConfigurable.h"
#include "WorkingSignal.h"

namespace cdsw {

namespace {

// 从定量步骤参数构建校准表：parameters["componentName"] + parameters["points"] = [{concentration,area}, ...]
CalibrationTable calibrationFromParameters(const QVariantMap& params)
{
    CalibrationTable table;
    table.componentName = params.value(QStringLiteral("componentName")).toString();
    const QVariant points = params.value(QStringLiteral("points"));
    if (points.canConvert<QVariantList>()) {
        const QVariantList list = points.toList();
        for (const QVariant& v : list) {
            if (!v.canConvert<QVariantMap>())
                continue;
            const QVariantMap m = v.toMap();
            CalibrationPoint pt;
            pt.concentration = m.value(QStringLiteral("concentration")).toDouble();
            pt.area = m.value(QStringLiteral("area")).toDouble();
            table.points.append(pt);
        }
    }
    return table;
}

} // namespace

ProcessingPipeline::ProcessingPipeline(const Registry& registry)
    : QObject(nullptr), m_registry(registry)
{
}

const QList<Peak>& ProcessingPipeline::peaks() const { return m_peaks; }

const QVector<Signal>& ProcessingPipeline::baseline() const { return m_baseline; }

const QList<QuantEntry>& ProcessingPipeline::quantEntries() const { return m_quantEntries; }

QList<Peak> ProcessingPipeline::buildPeaks(const Chromatogram& chrom,
                                           const QList<IRawPeak*>& raws) const
{
    const QVector<Signal>& sig = workingSignal(chrom);
    QList<Peak> out;
    out.reserve(raws.size());
    for (IRawPeak* raw : raws) {
        Peak p;
        p.startRTMs = raw->startRTMs();
        p.apexRTMs = raw->apexRTMs();
        p.stopRTMs = raw->stopRTMs();
        // 剖面：峰区间内的工作信号点
        const int startIdx = chrom.scanNumberAtRetentionTime(p.startRTMs) - 1;
        const int stopIdx = chrom.scanNumberAtRetentionTime(p.stopRTMs) - 1;
        if (startIdx >= 0 && stopIdx >= startIdx && stopIdx < sig.size())
            p.profile = sig.mid(startIdx, stopIdx - startIdx + 1);
        // 峰高：峰顶处工作信号强度（未扣背景；相对比较/排序足够，积分面积由积分器按 VV 背景另算）
        const int apexIdx = chrom.scanNumberAtRetentionTime(p.apexRTMs) - 1;
        if (apexIdx >= 0 && apexIdx < sig.size())
            p.peakHeight = sig.at(apexIdx).intensity;
        out.append(p);
    }
    return out;
}

void ProcessingPipeline::execute(const Method& method, Chromatogram& chrom)
{
    m_peaks.clear();
    m_baseline.clear();
    m_quantEntries.clear();
    // 重跑管线 → 覆盖处理后副本（契约 §4.1「改参数→重跑管线→覆盖本副本」）：
    // 清空 processed，首个滤波器从原始开始
    const bool hadProcessed = !chrom.processedPoints().isEmpty();
    chrom.setProcessedPoints(QVector<Signal>());

    bool anyRan = false;
    for (int i = 0; i < method.steps.size(); ++i) {
        const ProcessingStep& step = method.steps.at(i);
        const QString& id = step.id;
        bool handled = false;

        if (IFilter* f = m_registry.createFilter(id)) {
            if (IConfigurable* cfg = dynamic_cast<IConfigurable*>(f))
                cfg->configure(step.parameters);
            f->apply(chrom);
            delete f;
            handled = true;
        } else if (IBaselineDetector* b = m_registry.createBaseline(id)) {
            if (IConfigurable* cfg = dynamic_cast<IConfigurable*>(b))
                cfg->configure(step.parameters);
            m_baseline = b->detect(chrom);
            delete b;
            handled = true;
        } else if (IPeakDetector* d = m_registry.createPeakDetector(id)) {
            if (IConfigurable* cfg = dynamic_cast<IConfigurable*>(d))
                cfg->configure(step.parameters);
            const QList<IRawPeak*> raws = d->detect(chrom);
            m_peaks = buildPeaks(chrom, raws);
            qDeleteAll(raws);
            handled = true;
        } else if (IIntegrator* itg = m_registry.createIntegrator(id)) {
            if (IConfigurable* cfg = dynamic_cast<IConfigurable*>(itg))
                cfg->configure(step.parameters);
            itg->integrate(chrom, m_peaks);
            delete itg;
            handled = true;
        } else if (IQuantifier* q = m_registry.createQuantifier(id)) {
            if (IConfigurable* cfg = dynamic_cast<IConfigurable*>(q))
                cfg->configure(step.parameters);
            m_quantEntries = q->quantitate(m_peaks, calibrationFromParameters(step.parameters));
            delete q;
            handled = true;
        }

        // 每步都上报（含未知步：跳过但步进仍推进，供 UI 进度/刷新）
        emit sigStepFinished(i, id);
        if (handled)
            anyRan = true;
    }
    // 脏标记：有步骤实际执行、或重置清掉了既有 processed 副本，模型都算被改过（契约 §4.1 统一置 true）
    if (anyRan || hadProcessed)
        chrom.setDirty(true);
    emit sigFinished();
}

} // namespace cdsw
