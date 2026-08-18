// core_model/include/core_model/Method.h —— 处理方法（OpenChrom "processing method" 的 Qt 版）
#pragma once
#include <QtCore/qstring.h>
#include <QtCore/qvector.h>
#include <QtCore/qvariant.h>
namespace cdsw {

// 处理方法 = 有序步骤列表
struct ProcessingStep {
    QString id;                     // 对应注册表里的算法 id，如 "sg_smooth" / "first_derivative_peak_detector"
    QVariantMap parameters;         // 算法参数
};
struct Method { QVector<ProcessingStep> steps; };

} // namespace cdsw
