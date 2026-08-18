// core_model/include/core_model/Selection.h —— 选择（OpenChrom IChromatogramSelection → UI 刷新广播载体）
#pragma once
#include <QtCore/qobject.h>
#include <core_model/Peak.h>
namespace cdsw {

class Selection : public QObject {
    Q_OBJECT
public:
    void setRange(qint64 startMs, qint64 stopMs);
    qint64 startMs() const; qint64 stopMs() const;
    void setPeak(const Peak&); const Peak& peak() const;
signals:
    void sigSelectionChanged();     // = fireUpdateChange；UI/其他模块订阅
private:
    qint64 m_startMs = 0; qint64 m_stopMs = 0;
    Peak m_peak;
};

} // namespace cdsw
