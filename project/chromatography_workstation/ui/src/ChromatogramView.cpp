// ui/src/ChromatogramView.cpp —— 色谱曲线视图实现（自绘：折线 + 峰标记 + 选区阴影）
#include <ui/ChromatogramView.h>

#include <QtGui/qpainter.h>
#include <QtGui/qevent.h>
#include <QtGui/qpolygon.h>

namespace cdsw {

namespace {

constexpr double kZoomFactor = 1.2;        // 每格滚轮的缩放比
constexpr double kMinWindowMs = 1.0;       // 可见窗最小宽度，防除零

} // namespace

ChromatogramView::ChromatogramView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(200, 100);
}

const QVector<Signal>& ChromatogramView::displayPoints() const
{
    if (m_chrom && !m_chrom->processedPoints().isEmpty())
        return m_chrom->processedPoints();
    if (m_chrom)
        return m_chrom->signalPoints();
    static const QVector<Signal> kEmpty;
    return kEmpty;
}

void ChromatogramView::setChromatogram(Chromatogram* chrom)
{
    m_chrom = chrom;
    // 默认可见全谱
    if (m_chrom) {
        m_visibleStartMs = m_chrom->startTimeMs();
        m_visibleStopMs = qMax(m_chrom->stopTimeMs(), m_visibleStartMs + qint64(1));
    } else {
        m_visibleStartMs = 0;
        m_visibleStopMs = 1;
    }
    update();
}

void ChromatogramView::setPeaks(const QList<Peak>& peaks)
{
    m_peaks = peaks;
    update();
}

void ChromatogramView::setSelectionRange(qint64 startMs, qint64 stopMs)
{
    m_selStartMs = qMin(startMs, stopMs);
    m_selStopMs = qMax(startMs, stopMs);
    update();
}

void ChromatogramView::zoomAt(qint64 centerMs, double factor)
{
    if (factor <= 0.0)
        return;
    const double window = double(m_visibleStopMs - m_visibleStartMs);
    const double newWindow = qMax(window / factor, kMinWindowMs);
    const double half = newWindow / 2.0;
    const double center = double(centerMs);
    const qint64 maxMs = m_chrom ? m_chrom->stopTimeMs() : m_visibleStopMs;
    qint64 start = qint64(qMax(0.0, center - half));
    qint64 stop = start + qint64(newWindow);
    if (stop > maxMs) {                       // 夹上界：可见窗不漂出谱尾
        stop = maxMs;
        start = qMax<qint64>(0, maxMs - qint64(newWindow));
    }
    m_visibleStartMs = start;
    m_visibleStopMs = qMax(stop, start + 1);
    update();
}

qint64 ChromatogramView::visibleStartMs() const { return m_visibleStartMs; }
qint64 ChromatogramView::visibleStopMs() const { return m_visibleStopMs; }
qint64 ChromatogramView::selectionStartMs() const { return m_selStartMs; }
qint64 ChromatogramView::selectionStopMs() const { return m_selStopMs; }

double ChromatogramView::msToX(qint64 ms) const
{
    const double window = double(m_visibleStopMs - m_visibleStartMs);
    if (window <= 0.0)
        return 0.0;
    return (double(ms - m_visibleStartMs) / window) * width();
}

qint64 ChromatogramView::xToMs(int x) const
{
    const double window = double(m_visibleStopMs - m_visibleStartMs);
    if (width() <= 0)
        return m_visibleStartMs;
    return m_visibleStartMs + qint64((double(x) / width()) * window);
}

void ChromatogramView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    const QVector<Signal>& pts = displayPoints();
    if (pts.isEmpty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }

    // 可见范围内点连折线；Y 轴范围取自「可见显示点」（processedPoints 滤波后强度范围才是真实绘制范围）
    QPolygonF poly;
    double minY = 0.0, maxY = 1.0;
    bool haveVisible = false;
    for (const Signal& s : pts) {
        if (s.retentionTimeMs < m_visibleStartMs || s.retentionTimeMs > m_visibleStopMs)
            continue;
        if (!haveVisible) {
            minY = maxY = s.intensity;
            haveVisible = true;
        } else {
            minY = qMin(minY, s.intensity);
            maxY = qMax(maxY, s.intensity);
        }
    }
    if (haveVisible && maxY - minY <= 0.0)
        maxY = minY + 1.0;
    if (!haveVisible) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, tr("No data in range"));
        return;
    }
    for (const Signal& s : pts) {
        if (s.retentionTimeMs < m_visibleStartMs || s.retentionTimeMs > m_visibleStopMs)
            continue;
        const double x = msToX(s.retentionTimeMs);
        const double y = height() - ((s.intensity - minY) / (maxY - minY)) * height();
        poly << QPointF(x, y);
    }
    p.setPen(QPen(Qt::blue, 1.0));
    p.drawPolyline(poly);

    // 选区阴影
    if (m_selStopMs > m_selStartMs) {
        const double x1 = msToX(m_selStartMs);
        const double x2 = msToX(m_selStopMs);
        p.fillRect(QRectF(x1, 0, x2 - x1, height()), QColor(0, 0, 255, 40));
    }

    // 峰标记（apex 处小竖线）
    p.setPen(QPen(Qt::red, 1.0));
    for (const Peak& peak : m_peaks) {
        const double x = msToX(peak.apexRTMs);
        p.drawLine(QPointF(x, height() - 4), QPointF(x, height()));
    }
}

void ChromatogramView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_dragStart = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void ChromatogramView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_selecting) {
        setSelectionRange(xToMs(m_dragStart.x()), xToMs(event->pos().x()));
    }
    QWidget::mouseMoveEvent(event);
}

void ChromatogramView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        const qint64 a = xToMs(m_dragStart.x());
        const qint64 b = xToMs(event->pos().x());
        setSelectionRange(a, b);
        // 单击（零宽选区）不广播：最小阈值避免误触「选区变化→重跑管线」
        if (m_selStopMs > m_selStartMs)
            emit sigSelectionRangeChanged(m_selStartMs, m_selStopMs);
    }
    QWidget::mouseReleaseEvent(event);
}

void ChromatogramView::wheelEvent(QWheelEvent* event)
{
    // 滚轮上滚放大、下滚缩小，以光标为不动点
    const double factor = (event->angleDelta().y() > 0) ? kZoomFactor : (1.0 / kZoomFactor);
    zoomAt(xToMs(event->pos().x()), factor);
    event->accept();
}

} // namespace cdsw
