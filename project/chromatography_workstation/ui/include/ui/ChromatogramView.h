// ui/include/ui/ChromatogramView.h —— 色谱曲线视图（契约 §4.6：缩放/平移/选区）
#pragma once
#include <core_model/Chromatogram.h>
#include <core_model/Peak.h>
#include <QtWidgets/qwidget.h>
namespace cdsw {

// 自绘曲线控件：读 Chromatogram 指针（processedPoints 优先，否则 signalPoints），
// 滚轮缩放 X 轴、左键拖拽出选区（emit sigSelectionRangeChanged）、setSelectionRange 外部写回。
class ChromatogramView : public QWidget {
    Q_OBJECT
public:
    explicit ChromatogramView(QWidget* parent = nullptr);
    void setChromatogram(Chromatogram* chrom);
    void setPeaks(const QList<Peak>& peaks);
    void setSelectionRange(qint64 startMs, qint64 stopMs);   // 外部写回（如 Selection 广播）
    void zoomAt(qint64 centerMs, double factor);             // factor>1 放大
    qint64 visibleStartMs() const;
    qint64 visibleStopMs() const;
    qint64 selectionStartMs() const;
    qint64 selectionStopMs() const;
signals:
    void sigSelectionRangeChanged(qint64 startMs, qint64 stopMs);   // 用户拖出选区
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
private:
    const QVector<Signal>& displayPoints() const;
    double msToX(qint64 ms) const;
    qint64 xToMs(int x) const;
    Chromatogram* m_chrom = nullptr;
    QList<Peak> m_peaks;
    qint64 m_visibleStartMs = 0;
    qint64 m_visibleStopMs = 0;
    bool m_selecting = false;
    QPoint m_dragStart;
    qint64 m_selStartMs = 0;
    qint64 m_selStopMs = 0;
};

} // namespace cdsw
