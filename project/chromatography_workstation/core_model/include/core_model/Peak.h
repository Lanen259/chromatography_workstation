// core_model/include/core_model/Peak.h —— 峰（OpenChrom IPeakModel 语义）
#pragma once
#include <QtCore/qglobal.h>
#include <QtCore/qvector.h>
#include <core_model/Signal.h>
namespace cdsw {

// 峰三要素 + 强度。组分名不在峰上（由 QuantEntry 携带，apexRTMs 关联）。
struct Peak {
    qint64 startRTMs = 0; qint64 apexRTMs = 0; qint64 stopRTMs = 0;
    double peakHeight = 0.0;
    double peakArea = 0.0;          // 由积分器填充
    bool markedAsDeleted = false;   // 删除标记（OpenChrom 不用 DELETED 枚举，用此 flag）
    QVector<Signal> profile;        // 峰内原始点（可空）
};

} // namespace cdsw
