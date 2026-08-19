// ui/src/Theme.cpp —— 应用主题实现（QSS 资源加载 + 暗色色板）
#include <ui/Theme.h>

#include <QtCore/qfile.h>
#include <QtWidgets/qapplication.h>

extern int qInitResources_ui();   // AUTORCC 生成的资源初始化（全局符号）

namespace cdsw {

ThemeColors ThemeColors::dark()
{
    ThemeColors c;
    c.window    = QColor(0x1c, 0x21, 0x28);
    c.surface   = QColor(0x23, 0x29, 0x32);
    c.border    = QColor(0x33, 0x3c, 0x47);
    c.text      = QColor(0xd5, 0xdb, 0xe4);
    c.textDim   = QColor(0x9a, 0xa4, 0xb2);
    c.accent    = QColor(0x4a, 0x9e, 0xff);
    c.curve     = QColor(0x66, 0xd9, 0xe8);    // 青绿曲线（暗底对比）
    c.curveFill = QColor(0x66, 0xd9, 0xe8, 28);
    c.peak      = QColor(0xff, 0xb0, 0x6b);    // 琥珀峰标注
    c.grid      = QColor(0x2c, 0x34, 0x3f);
    c.crosshair = QColor(0xff, 0xf3, 0xc4);
    c.overview  = QColor(0x4a, 0x9e, 0xff, 90);
    return c;
}

void applyTheme()
{
    // 静态库链接下 qrc 对象会被丢弃，显式初始化强制链接资源（全局符号，避免 cdsw:: 解析）
    ::qInitResources_ui();
    QFile f(QStringLiteral(":/theme.qss"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
}

} // namespace cdsw
