// ui/src/ChromatogramView.cpp —— 色谱曲线视图实现（专业版：轴/网格/标注/十字线/概览）
#include <ui/ChromatogramView.h>

#include <ui/Theme.h>

#include <QtCore/qline.h>
#include <QtCore/qrect.h>
#include <QtGui/qbrush.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>

#include <algorithm>
#include <cmath>

namespace cdsw {

namespace {

constexpr double kZoomFactor = 1.2;       // 每格滚轮的缩放比
constexpr double kMinWindowMs = 1.0;      // 可见窗最小宽度，防除零
constexpr int kTargetXTicks = 6;          // X 轴目标刻度数
constexpr int kTargetYTicks = 5;          // Y 轴目标刻度数
constexpr double kYHeadroom = 1.05;       // Y 顶部留白系数
constexpr int kOverviewHeight = 34;       // 概览条高度
constexpr int kDecimateMax = 2000;        // 曲线/概览抽稀上限

// 1/2/5 × 10^n 的“好看”步长
double niceStep(double span, int target)
{
    const double raw = span / target;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;
    const double n = (norm < 1.5) ? 1.0 : (norm < 3.5) ? 2.0 : (norm < 7.5) ? 5.0 : 10.0;
    return n * mag;
}

QString fmtTick(double v)
{
    if (std::abs(v) < 1e-9)
        return QStringLiteral("0");
    const double a = std::abs(v);
    const int decimals = (a >= 10.0) ? 0 : (a >= 1.0) ? 1 : 2;
    return QString::number(v, 'f', decimals);
}

} // namespace

ChromatogramView::ChromatogramView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(240, 140);
    setMouseTracking(true);   // 无按键也收 mouseMove（十字线）
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

QRectF ChromatogramView::plotRect() const
{
    const double kLeft = 64, kRight = 14, kTop = 12, kBottom = 26;
    const double overview = m_overviewVisible ? kOverviewHeight : 0.0;
    return QRectF(kLeft, kTop,
                  qMax(0.0, width() - kLeft - kRight),
                  qMax(0.0, height() - kTop - kBottom - overview));
}

void ChromatogramView::setChromatogram(Chromatogram* chrom)
{
    m_chrom = chrom;
    fitToData();
}

void ChromatogramView::fitToData()
{
    const QVector<Signal>& pts = displayPoints();
    if (pts.isEmpty()) {
        m_visibleStartMs = 0;
        m_visibleStopMs = 1;
    } else {
        m_visibleStartMs = pts.first().retentionTimeMs;
        m_visibleStopMs = qMax(pts.last().retentionTimeMs, m_visibleStartMs + 1);
    }
    updateYRange();
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
    updateYRange();
    update();
}

void ChromatogramView::setCrosshairEnabled(bool on)
{
    m_crosshairEnabled = on;
    if (!on)
        m_hover = QPoint(-1, -1);
    update();
}

void ChromatogramView::setOverviewVisible(bool on)
{
    m_overviewVisible = on;
    update();
}

qint64 ChromatogramView::visibleStartMs() const { return m_visibleStartMs; }
qint64 ChromatogramView::visibleStopMs() const { return m_visibleStopMs; }
qint64 ChromatogramView::selectionStartMs() const { return m_selStartMs; }
qint64 ChromatogramView::selectionStopMs() const { return m_selStopMs; }

double ChromatogramView::msToX(qint64 ms) const
{
    const QRectF plot = plotRect();
    const double window = double(m_visibleStopMs - m_visibleStartMs);
    if (window <= 0.0)
        return plot.left();
    return plot.left() + (double(ms - m_visibleStartMs) / window) * plot.width();
}

double ChromatogramView::xToMs(double x) const
{
    const QRectF plot = plotRect();
    const double window = double(m_visibleStopMs - m_visibleStartMs);
    if (plot.width() <= 0.0)
        return m_visibleStartMs;
    x = qBound(plot.left(), x, plot.right());
    return m_visibleStartMs + ((x - plot.left()) / plot.width()) * window;
}

double ChromatogramView::intensityToY(double v) const
{
    const QRectF plot = plotRect();
    const double span = m_y.max - m_y.min;
    if (span <= 0.0)
        return plot.bottom();
    return plot.bottom() - ((v - m_y.min) / span) * plot.height();
}

double ChromatogramView::yToIntensity(double y) const
{
    const QRectF plot = plotRect();
    if (plot.height() <= 0.0)
        return m_y.min;
    return m_y.min + ((plot.bottom() - y) / plot.height()) * (m_y.max - m_y.min);
}

void ChromatogramView::updateYRange()
{
    const QVector<Signal>& pts = displayPoints();
    m_y = Range{ 0.0, 1.0 };
    bool have = false;
    for (const Signal& s : pts) {
        if (s.retentionTimeMs < m_visibleStartMs || s.retentionTimeMs > m_visibleStopMs)
            continue;
        if (!have) {
            m_y.min = m_y.max = s.intensity;
            have = true;
        } else {
            m_y.min = qMin(m_y.min, s.intensity);
            m_y.max = qMax(m_y.max, s.intensity);
        }
    }
    if (have) {
        if (m_y.max - m_y.min <= 0.0)
            m_y.max = m_y.min + 1.0;
        m_y.max *= kYHeadroom;
        if (m_y.min > 0.0)
            m_y.min = 0.0;                    // 色谱通常从 0 看
    }
}

QPointF ChromatogramView::mapToData(const QPoint& pos) const
{
    if (!plotRect().contains(pos))
        return QPointF(qQNaN(), qQNaN());
    return QPointF(xToMs(pos.x()), yToIntensity(pos.y()));
}

// ---------- 绘制 ----------

void ChromatogramView::paintEvent(QPaintEvent*)
{
    const ThemeColors tc = ThemeColors::dark();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), tc.window);

    if (displayPoints().isEmpty()) {
        p.setPen(tc.textDim);
        p.drawText(rect(), Qt::AlignCenter, tr("导入 CSV 或加载演示数据"));
        return;
    }

    drawAxes(p);
    drawCurve(p);
    drawPeaks(p);
    drawSelection(p);
    drawCrosshair(p);
    drawOverview(p);
}

void ChromatogramView::drawAxes(QPainter& p)
{
    const ThemeColors tc = ThemeColors::dark();
    const QRectF plot = plotRect();
    p.setPen(QPen(tc.border, 1.0));
    p.drawRect(plot);

    // X 轴：分钟刻度 + 网格
    const double startMin = m_visibleStartMs / 60000.0;
    const double stopMin = m_visibleStopMs / 60000.0;
    const double stepMin = niceStep(stopMin - startMin, kTargetXTicks);
    p.setPen(tc.textDim);
    for (double t = std::ceil(startMin / stepMin) * stepMin; t <= stopMin + 1e-9; t += stepMin) {
        const double x = msToX(qint64(t * 60000.0));
        if (x < plot.left() || x > plot.right())
            continue;
        p.setPen(QPen(tc.grid, 1.0, Qt::DotLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(tc.textDim);
        p.drawText(QRectF(x - 20, plot.bottom() + 4, 40, 18), Qt::AlignCenter, fmtTick(t));
    }

    // Y 轴：强度刻度 + 网格
    const double stepY = niceStep(m_y.max - m_y.min, kTargetYTicks);
    for (double v = std::ceil(m_y.min / stepY) * stepY; v <= m_y.max + 1e-9; v += stepY) {
        const double y = intensityToY(v);
        if (y < plot.top() || y > plot.bottom())
            continue;
        p.setPen(QPen(tc.grid, 1.0, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(tc.textDim);
        p.drawText(QRectF(0, y - 8, plot.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, fmtTick(v));
    }
    p.setPen(tc.textDim);
    p.drawText(QRectF(0, 0, width(), 14), Qt::AlignCenter, tr("Intensity"));
}

void ChromatogramView::drawCurve(QPainter& p)
{
    const ThemeColors tc = ThemeColors::dark();
    const QVector<Signal>& pts = displayPoints();
    const QRectF plot = plotRect();

    QPolygonF poly;
    for (const Signal& s : pts) {
        if (s.retentionTimeMs < m_visibleStartMs || s.retentionTimeMs > m_visibleStopMs)
            continue;
        poly << QPointF(msToX(s.retentionTimeMs), intensityToY(s.intensity));
    }
    if (poly.size() < 2)
        return;

    p.save();
    p.setClipRect(plot);

    // 曲线下沿渐变填充
    QLinearGradient grad(plot.topLeft(), plot.bottomLeft());
    grad.setColorAt(0.0, tc.curveFill);
    grad.setColorAt(1.0, QColor(tc.curveFill.red(), tc.curveFill.green(), tc.curveFill.blue(), 0));
    QPainterPath path;
    path.addPolygon(poly);
    path.lineTo(poly.last().x(), plot.bottom());
    path.lineTo(poly.first().x(), plot.bottom());
    path.closeSubpath();
    p.fillPath(path, grad);

    p.setPen(QPen(tc.curve, 1.6));
    p.drawPolyline(poly);
    p.restore();
}

void ChromatogramView::drawPeaks(QPainter& p)
{
    const ThemeColors tc = ThemeColors::dark();
    const QRectF plot = plotRect();
    p.setFont(QFont(font().family(), qMax(7, font().pointSize() - 1)));

    int label = 1;
    for (const Peak& peak : m_peaks) {
        if (peak.apexRTMs < m_visibleStartMs || peak.apexRTMs > m_visibleStopMs) {
            ++label;
            continue;
        }
        const double x = msToX(peak.apexRTMs);
        const double y = intensityToY(qMax(peak.peakHeight, m_y.min));
        p.setPen(QPen(tc.peak, 1.0));
        p.drawLine(QPointF(x, y - 5), QPointF(x, y + 5));
        const QString text = QStringLiteral("P%1  %2")
                                 .arg(label)
                                 .arg(peak.apexRTMs / 60000.0, 0, 'f', 2);
        const QRectF box(x - 30, qMax(plot.top() + 2.0, y - 26.0), 60, 15);
        p.setPen(tc.peak);
        p.drawText(box, Qt::AlignHCenter | Qt::AlignTop, text);
        ++label;
    }
}

void ChromatogramView::drawSelection(QPainter& p)
{
    const ThemeColors tc = ThemeColors::dark();
    if (m_selStopMs <= m_selStartMs)
        return;
    const QRectF plot = plotRect();
    const double x1 = msToX(m_selStartMs);
    const double x2 = msToX(m_selStopMs);
    p.fillRect(QRectF(x1, plot.top(), x2 - x1, plot.height()), tc.overview);
    p.setPen(QPen(tc.accent, 1.0));
    p.drawLine(QPointF(x1, plot.top()), QPointF(x1, plot.bottom()));
    p.drawLine(QPointF(x2, plot.top()), QPointF(x2, plot.bottom()));
}

void ChromatogramView::drawCrosshair(QPainter& p)
{
    if (!m_crosshairEnabled || m_hover.x() < 0)
        return;
    const QRectF plot = plotRect();
    if (!plot.contains(m_hover))
        return;
    const ThemeColors tc = ThemeColors::dark();
    const double x = m_hover.x();
    const double y = m_hover.y();
    p.setPen(QPen(tc.crosshair, 1.0, Qt::DashLine));
    p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

    const QPointF data = mapToData(m_hover);
    const QString text = QStringLiteral("RT %1 min\nI %2")
                             .arg(data.x() / 60000.0, 0, 'f', 3)
                             .arg(data.y(), 0, 'g', 6);
    QRectF box(x + 10, qMax(plot.top() + 2.0, y - 30.0), 120, 30);
    p.setPen(QPen(tc.border, 1.0));
    p.setBrush(tc.surface);
    p.drawRoundedRect(box, 4, 4);
    p.setPen(tc.text);
    p.drawText(box.adjusted(6, 2, -6, -2), Qt::AlignLeft | Qt::AlignVCenter, text);
}

void ChromatogramView::drawOverview(QPainter& p)
{
    if (!m_overviewVisible)
        return;
    const ThemeColors tc = ThemeColors::dark();
    const QVector<Signal>& pts = displayPoints();
    if (pts.isEmpty())
        return;
    const QRectF plot = plotRect();
    const QRectF strip(plot.left(), plot.bottom() + 8, plot.width(), kOverviewHeight - 10);
    if (strip.width() <= 0)
        return;
    p.fillRect(strip, tc.surface);
    p.setPen(QPen(tc.border, 1.0));
    p.drawRect(strip);

    // 全谱抽稀折线
    const int stride = qMax(1, pts.size() / qMax(1, int(strip.width())));
    QPolygonF poly;
    const double fullStart = pts.first().retentionTimeMs;
    const double fullStop = qMax(double(pts.last().retentionTimeMs), fullStart + 1.0);
    const double minI = m_chrom ? m_chrom->minIntensity() : 0.0;
    const double maxI = m_chrom ? m_chrom->maxIntensity() : 1.0;
    const double iSpan = qMax(maxI - minI, 1e-9);
    for (int i = 0; i < pts.size(); i += stride) {
        const double x = strip.left() + (pts.at(i).retentionTimeMs - fullStart) / (fullStop - fullStart) * strip.width();
        const double y = strip.bottom() - ((pts.at(i).intensity - minI) / iSpan) * strip.height();
        poly << QPointF(x, y);
    }
    p.setPen(QPen(tc.curve, 1.0));
    p.drawPolyline(poly);

    // 可见窗矩形
    const double v1 = strip.left() + (m_visibleStartMs - fullStart) / (fullStop - fullStart) * strip.width();
    const double v2 = strip.left() + (m_visibleStopMs - fullStart) / (fullStop - fullStart) * strip.width();
    p.fillRect(QRectF(v1, strip.top(), qMax(1.0, v2 - v1), strip.height()), tc.overview);
    p.setPen(QPen(tc.accent, 1.0));
    p.drawRect(QRectF(v1, strip.top(), qMax(1.0, v2 - v1), strip.height()));
}

// ---------- 交互 ----------

void ChromatogramView::mousePressEvent(QMouseEvent* event)
{
    const QRectF plot = plotRect();
    const QRectF strip(plot.left(), plot.bottom() + 8, plot.width(), kOverviewHeight - 10);
    if (m_overviewVisible && strip.contains(event->pos())) {
        m_overviewDragging = true;
        m_dragLast = event->pos();
        return;
    }
    if (event->button() == Qt::LeftButton && plot.contains(event->pos())) {
        m_selecting = true;
        m_dragLast = event->pos();
        setSelectionRange(xToMs(m_dragLast.x()), xToMs(m_dragLast.x()));
    } else if (event->button() == Qt::MiddleButton && plot.contains(event->pos())) {
        m_panning = true;
        m_dragLast = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void ChromatogramView::mouseMoveEvent(QMouseEvent* event)
{
    m_hover = event->pos();
    update();
    if (m_selecting) {
        setSelectionRange(xToMs(m_dragLast.x()), xToMs(event->pos().x()));
    } else if (m_panning) {
        const qint64 deltaMs = qint64((event->pos().x() - m_dragLast.x())
                                      * (m_visibleStopMs - m_visibleStartMs) / qMax(1.0, plotRect().width()));
        const qint64 span = m_visibleStopMs - m_visibleStartMs;
        qint64 start = m_visibleStartMs - deltaMs;
        const qint64 maxMs = m_chrom ? m_chrom->stopTimeMs() : m_visibleStopMs;
        start = qBound<qint64>(0, start, qMax<qint64>(0, maxMs - span));
        m_visibleStartMs = start;
        m_visibleStopMs = start + span;
        m_dragLast = event->pos();
        updateYRange();
    } else if (m_overviewDragging) {
        // 概览条拖动：以指针位置为视窗中心
        const QRectF plot = plotRect();
        const QRectF strip(plot.left(), plot.bottom() + 8, plot.width(), kOverviewHeight - 10);
        const QVector<Signal>& pts = displayPoints();
        if (!pts.isEmpty() && strip.width() > 0) {
            const double fullStart = pts.first().retentionTimeMs;
            const double fullStop = qMax(double(pts.last().retentionTimeMs), fullStart + 1.0);
            const double frac = (event->pos().x() - strip.left()) / strip.width();
            const qint64 span = m_visibleStopMs - m_visibleStartMs;
            qint64 center = qint64(fullStart + frac * (fullStop - fullStart));
            qint64 start = center - span / 2;
            const qint64 maxMs = qint64(fullStop);
            start = qBound<qint64>(0, start, qMax<qint64>(0, maxMs - span));
            m_visibleStartMs = start;
            m_visibleStopMs = start + span;
            updateYRange();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void ChromatogramView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        setSelectionRange(xToMs(m_dragLast.x()), xToMs(event->pos().x()));
        // 单击（零宽选区）不广播：避免误触「选区变化→重跑管线」
        if (m_selStopMs > m_selStartMs)
            emit sigSelectionRangeChanged(m_selStartMs, m_selStopMs);
    } else if (m_panning) {
        m_panning = false;
        unsetCursor();
    }
    m_overviewDragging = false;
    QWidget::mouseReleaseEvent(event);
}

void ChromatogramView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        fitToData();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ChromatogramView::wheelEvent(QWheelEvent* event)
{
    // 滚轮上滚放大、下滚缩小，以光标为不动点
    const double factor = (event->angleDelta().y() > 0) ? kZoomFactor : (1.0 / kZoomFactor);
    zoomAt(qint64(xToMs(event->pos().x())), factor);
    event->accept();
}

void ChromatogramView::leaveEvent(QEvent*)
{
    m_hover = QPoint(-1, -1);
    update();
}

} // namespace cdsw
