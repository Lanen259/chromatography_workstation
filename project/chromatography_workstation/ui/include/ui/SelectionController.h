// ui/include/ui/SelectionController.h —— Selection ↔ 管线 桥接（契约 §4.6 冻结）
#pragma once
#include <core_model/Selection.h>
#include <core_processing/interfaces.h>
#include <QtCore/qobject.h>
#include <QtCore/qmetatype.h>
namespace cdsw {

// Selection 桥接（= OpenChrom fireUpdateChange 的 Qt 版，UI 与引擎唯一握手点）：
//   曲线选区 → Selection.sigSelectionChanged → 触发管线重跑 → 峰表/结果刷新
class SelectionController : public QObject {
    Q_OBJECT
public:
    SelectionController(Selection* selection, ProcessingPipeline* pipeline, QObject* parent = nullptr);
    void onChromatogramChanged();   // 数据更新 → setDirty → 重新 execute(Method, chrom)
    // —— 增补（open-closed，不改冻结签名）：控制器需知道跑哪份色谱/方法 ——
    void setChromatogram(Chromatogram* chrom);
    void setMethod(const Method* method);
signals:
    void sigPeaksUpdated(const QList<Peak>&);
private:
    Selection* m_selection;
    ProcessingPipeline* m_pipeline;
    Chromatogram* m_chrom = nullptr;
    const Method* m_method = nullptr;
};

} // namespace cdsw

// QSignalSpy 捕获 QList<Peak> 参数所需（moc 信号参数的类型注册）
Q_DECLARE_METATYPE(cdsw::Peak)
Q_DECLARE_METATYPE(QList<cdsw::Peak>)
