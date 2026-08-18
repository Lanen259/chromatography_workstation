#include <core_model/Selection.h>

namespace cdsw {

void Selection::setRange(qint64 startMs, qint64 stopMs)
{
    m_startMs = startMs;
    m_stopMs = stopMs;
    emit sigSelectionChanged();
}

qint64 Selection::startMs() const
{
    return m_startMs;
}

qint64 Selection::stopMs() const
{
    return m_stopMs;
}

void Selection::setPeak(const Peak& peak)
{
    m_peak = peak;
}

const Peak& Selection::peak() const
{
    return m_peak;
}

} // namespace cdsw
