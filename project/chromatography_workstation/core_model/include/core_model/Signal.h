// core_model/include/core_model/Signal.h —— 采样点（OpenChrom IScan 的扁平版）
#pragma once
#include <QtCore/qglobal.h>
namespace cdsw {

// 一个采样点：保留时间(ms) + 强度。CSD 单通道起步；扫描号 = 容器索引 + 1（1-based，不存字段）。
struct Signal {
    qint64 retentionTimeMs = 0;   // 保留时间，毫秒制（OpenChrom 约定）
    double intensity = 0.0;       // 信号强度（可 0–100 相对或原始 AD 值）
};

} // namespace cdsw
