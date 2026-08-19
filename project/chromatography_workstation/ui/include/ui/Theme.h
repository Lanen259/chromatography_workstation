// ui/include/ui/Theme.h —— 应用主题（QSS 资源）+ 图表色板
#pragma once
#include <QtGui/qcolor.h>
namespace cdsw {

// 图表色板：与 theme.qss 同源的设计变量，供自绘控件（色谱图等）取色，避免硬编码
struct ThemeColors {
    QColor window;      // 应用背景
    QColor surface;     // 面板/表格背景
    QColor border;      // 分隔/描边
    QColor text;        // 主文字
    QColor textDim;     // 次要文字/刻度
    QColor accent;      // 强调色（按钮/聚焦/选区）
    QColor curve;       // 色谱曲线
    QColor curveFill;   // 曲线下沿渐变
    QColor peak;        // 峰标注
    QColor grid;        // 网格线
    QColor crosshair;   // 十字线
    QColor overview;    // 概览条视窗

    static ThemeColors dark();   // 当前唯一主题（与 theme.qss 一致）
};

// 应用主题：从资源 :/theme.qss 加载并设置全局样式表
void applyTheme();

} // namespace cdsw
