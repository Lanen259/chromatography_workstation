// ui/include/ui/ChromatogramView.h —— 色谱曲线视图（专业版：轴/网格/标注/十字线/概览）
#pragma once
#include <core_model/Chromatogram.h>
#include <core_model/Peak.h>
#include <QtWidgets/qwidget.h>
namespace cdsw {

// 自绘专业色谱图：坐标轴 + 网格 + 抗锯齿曲线 + 峰标注 + 选区 + 十字线提示 + 概览条。
// 交互：左键拖拽=选区；中键拖拽=平移；滚轮=以光标缩放；双击=复位全谱；概览条拖动=移动视窗。
class ChromatogramView : public QWidget {
    Q_OBJECT
public:
    explicit ChromatogramView(QWidget* parent = nullptr);

    void setChromatogram(Chromatogram* chrom);
    void setPeaks(const QList<Peak>& peaks);
    void setSelectionRange(qint64 startMs, qint64 stopMs);
    void zoomAt(qint64 centerMs, double factor);
    void fitToData();                        // 可见窗复位全谱 + Y 自适应
    void setCrosshairEnabled(bool on);
    void setOverviewVisible(bool on);

    qint64 visibleStartMs() const;
    qint64 visibleStopMs() const;
    qint64 selectionStartMs() const;
    qint64 selectionStopMs() const;
    QPointF mapToData(const QPoint& pos) const;   // 像素 → (rtMs, intensity)；轴区外返回 NaN
signals:
    void sigSelectionRangeChanged(qint64 startMs, qint64 stopMs);   // 用户拖出选区
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
private:
    struct Range { double min; double max; };
    QRectF plotRect() const;                 // 绘图区（扣除轴边距与概览条）
    const QVector<Signal>& displayPoints() const;
    double msToX(qint64 ms) const;
    double xToMs(double x) const;
    double intensityToY(double v) const;
    double yToIntensity(double y) const;
    void updateYRange();
    void drawAxes(QPainter& p);
    void drawCurve(QPainter& p);
    void drawPeaks(QPainter& p);
    void drawSelection(QPainter& p);
    void drawCrosshair(QPainter& p);
    void drawOverview(QPainter& p);

    Chromatogram* m_chrom = nullptr;
    QList<Peak> m_peaks;
    qint64 m_visibleStartMs = 0;
    qint64 m_visibleStopMs = 0;
    Range m_y{ 0.0, 1.0 };
    bool m_selecting = false;
    bool m_panning = false;
    bool m_overviewDragging = false;
    QPoint m_dragLast;
    qint64 m_selStartMs = 0;
    qint64 m_selStopMs = 0;
    QPoint m_hover{ -1, -1 };                // 悬停像素（-1 无效）
    bool m_crosshairEnabled = true;
    bool m_overviewVisible = true;
};

} // namespace cdsw
