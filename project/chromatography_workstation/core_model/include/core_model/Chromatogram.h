// core_model/include/core_model/Chromatogram.h —— 色谱图（纯数据容器，不含分析逻辑）
#pragma once
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qvector.h>
#include <core_model/Signal.h>
namespace cdsw {

class Chromatogram {
public:
    // —— 标识与元数据 ——
    QString name() const; void setName(const QString&);               // 文件名/样品名
    QString converterId() const; void setConverterId(const QString&); // 可回写格式 id（空串 = 需"另存为"）
    bool isFinalized() const; void setFinalized(bool);                // finalized 色谱禁止覆盖保存
    // —— RT 网格（导入时由 Reader 计算，模型只存毫秒）——
    qint64 scanDelayMs() const; void setScanDelayMs(qint64);
    qint64 scanIntervalMs() const; void setScanIntervalMs(qint64);
    // —— 原始信号（永不改）——
    void setSignalPoints(const QVector<Signal>&);
    const QVector<Signal>& signalPoints() const;
    int scanCount() const;
    // —— 处理后信号（滤波/基线后的副本；改参数→重跑管线→覆盖本副本）——
    void setProcessedPoints(const QVector<Signal>&);
    const QVector<Signal>& processedPoints() const;   // 未处理时为空
    // —— 派生量 ——
    qint64 startTimeMs() const; qint64 stopTimeMs() const;
    double minIntensity() const; double maxIntensity() const;
    // —— 扫描号反查（M2 峰检测核心，floor 语义）——
    int scanNumberAtRetentionTime(qint64 retentionTimeMs) const; // 1-based；无匹配返回 0
    // —— 脏标记（处理管线改完模型统一置 true，供 UI 决定是否重跑）——
    bool isDirty() const; void setDirty(bool);
private:
    QString m_name;
    QString m_converterId;
    bool m_finalized = false;
    qint64 m_scanDelayMs = 0;
    qint64 m_scanIntervalMs = 0;
    QVector<Signal> m_signal;        // 原始
    QVector<Signal> m_processed;     // 处理后副本
    bool m_dirty = true;
};

} // namespace cdsw
