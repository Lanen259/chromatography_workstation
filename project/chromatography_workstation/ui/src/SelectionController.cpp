// ui/src/SelectionController.cpp —— Selection ↔ 管线 桥接实现
#include <ui/SelectionController.h>

namespace cdsw {

SelectionController::SelectionController(Selection* selection, ProcessingPipeline* pipeline, QObject* parent)
    : QObject(parent), m_selection(selection), m_pipeline(pipeline)
{
    // 契约 §4.6「唯一握手点」：选区变化 → 重跑管线 → 峰表/结果刷新
    if (m_selection) {
        connect(m_selection, &Selection::sigSelectionChanged,
                this, [this] { onChromatogramChanged(); });
    }
}

void SelectionController::setChromatogram(Chromatogram* chrom) { m_chrom = chrom; }

void SelectionController::setMethod(const Method* method) { m_method = method; }

void SelectionController::onChromatogramChanged()
{
    if (!m_chrom || !m_method)
        return;
    // 契约 §4.6：数据更新 → setDirty → 重新 execute(Method, chrom)；结果经 sigPeaksUpdated 广播
    m_chrom->setDirty(true);
    m_pipeline->execute(*m_method, *m_chrom);
    emit sigPeaksUpdated(m_pipeline->peaks());
}

} // namespace cdsw
