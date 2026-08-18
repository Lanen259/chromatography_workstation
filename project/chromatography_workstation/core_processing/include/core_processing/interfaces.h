// core_processing/include/core_processing/interfaces.h —— 处理引擎接口（契约 §4.2，冻结）
//
// 本文件逐字实现契约 §4.2 的全部接口：
//   IFilter / IBaselineDetector / IRawPeak / IPeakDetector / IIntegrator / IQuantifier /
//   QuantEntry / CalibrationPoint / CalibrationTable / Registry / ProcessingPipeline
//
// 相对契约文本的「编译必要增补」（任务明示许可，不改冻结公开签名）：
//   1. ProcessingPipeline 补 `: public QObject` + Q_OBJECT —— signals 编译必需；
//   2. 补齐 QtCore include（QString/QStringList/QVector/QHash/QObject）；
//   3. Registry 增补「其余访问器」（契约原文 `// ... 其余访问器` 授权）；
//   4. ProcessingPipeline 增补结果只读访问器 + 私有结果存储（M6 峰表/报告取数据用，缺之管线结果不可见）。
#pragma once
#include <core_model/Signal.h>
#include <core_model/Chromatogram.h>
#include <core_model/Peak.h>
#include <core_model/Method.h>
#include <QtCore/qglobal.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvector.h>
namespace cdsw {

// —— 算法接口（= OpenChrom 各 supplier 扩展点）——
class IFilter {                 // 滤波/平滑（对应 filter.supplier.savitzkygolay 等）
public:
    virtual ~IFilter() = default;
    virtual QString id() const = 0;          // 唯一 id，注册表键
    virtual QString displayName() const = 0;
    virtual void apply(Chromatogram& chrom) = 0;   // 原地修改信号
};

class IBaselineDetector {       // 基线检测（对应 xxd.baseline.detector）
public:
    virtual ~IBaselineDetector() = default;
    virtual QString id() const = 0;
    virtual QVector<Signal> detect(const Chromatogram& chrom) const = 0; // 返回基线点
};

class IRawPeak {
public:
    virtual ~IRawPeak() = default;
    virtual qint64 startRTMs() const = 0;
    virtual qint64 apexRTMs() const = 0;
    virtual qint64 stopRTMs() const = 0;
};

// 数据载体前向声明：契约把 IQuantifier 的签名放在结构体定义之前（冻结原文），
// 类声明引用未定义类型需要前向声明（编译必要增补，与 QObject 增补同类）。
struct QuantEntry;
struct CalibrationTable;

class IPeakDetector {           // 峰检测（对应 firstderivative / amdis）
public:
    virtual ~IPeakDetector() = default;
    virtual QString id() const = 0;
    virtual QList<IRawPeak*> detect(const Chromatogram& chrom) const = 0;
    // 实现者返回堆对象，由调用方释放；或用 shared_ptr 约定（见下方别名）
};

class IIntegrator {             // 峰积分（对应 integrator.supplier.trapezoid）
public:
    virtual ~IIntegrator() = default;
    virtual QString id() const = 0;
    virtual void integrate(Chromatogram& chrom, QList<Peak>& peaks) const = 0;
};

class IQuantifier {             // 定量（对应 peakQuantifierSupplier + 校准曲线）
public:
    virtual ~IQuantifier() = default;
    virtual QString id() const = 0;
    // 输入峰面积表 + 校准表，输出定量结果行
    virtual QList<QuantEntry> quantitate(const QList<Peak>& peaks,
                                         const CalibrationTable& calib) const = 0;
};

// —— 数据载体（供 Quantifier 使用）——
struct QuantEntry {
    qint64 apexRTMs = 0;
    QString componentName;      // 组分名（对照表匹配）
    double area = 0.0;
    double concentration = 0.0; // 计算浓度
    QString unit;
};
struct CalibrationPoint { double concentration; double area; };
struct CalibrationTable { QString componentName; QVector<CalibrationPoint> points; };

// —— 注册表（= 扩展点注册机制。算法实现类在 src 里，模块初始化时注册）——
class Registry {
public:
    static Registry& instance();
    void registerFilter(IFilter* (*factory)());
    void registerPeakDetector(IPeakDetector* (*factory)());
    void registerIntegrator(IIntegrator* (*factory)());
    void registerBaseline(IBaselineDetector* (*factory)());
    void registerQuantifier(IQuantifier* (*factory)());
    IFilter* createFilter(const QString& id) const;
    IPeakDetector* createPeakDetector(const QString& id) const;
    IIntegrator* createIntegrator(const QString& id) const;
    IBaselineDetector* createBaseline(const QString& id) const;
    IQuantifier* createQuantifier(const QString& id) const;
    QStringList availableFilterIds() const;
    QStringList availablePeakDetectorIds() const;
    QStringList availableBaselineIds() const;
    QStringList availableIntegratorIds() const;
    QStringList availableQuantifierIds() const;
private:
    Registry() = default;
    QHash<QString, IFilter* (*)()> m_filterFactories;
    QHash<QString, IBaselineDetector* (*)()> m_baselineFactories;
    QHash<QString, IPeakDetector* (*)()> m_peakDetectorFactories;
    QHash<QString, IIntegrator* (*)()> m_integratorFactories;
    QHash<QString, IQuantifier* (*)()> m_quantifierFactories;
};

// —— 管线执行器（= processing method 引擎：按 Method.steps 顺序执行）——
class ProcessingPipeline : public QObject {
    Q_OBJECT
public:
    explicit ProcessingPipeline(const Registry& registry);
    void execute(const Method& method, Chromatogram& chrom);
    // 结果只读访问（M2 增补：契约未冻结；M6 峰表/报告取数据用）
    const QList<Peak>& peaks() const;
    const QVector<Signal>& baseline() const;
    const QList<QuantEntry>& quantEntries() const;
    // 每步执行后 emit，供 UI 刷新/进度
signals:
    void sigStepFinished(int stepIndex, const QString& id);
    void sigFinished();
private:
    QList<Peak> buildPeaks(const Chromatogram& chrom, const QList<IRawPeak*>& raws) const;
    QList<Peak> m_peaks;
    QVector<Signal> m_baseline;
    QList<QuantEntry> m_quantEntries;
    const Registry& m_registry;
};

} // namespace cdsw
