// core_processing/src/FilterSavitzkyGolay.cpp —— Savitzky-Golay 平滑实现
#include "FilterSavitzkyGolay.h"

#include "WorkingSignal.h"

#include <algorithm>
#include <cmath>

namespace cdsw {

namespace {

// 高斯消元（列主元）解 A·x = b，A 为 n×n；奇异返回 false。
bool solveLinearSystem(QVector<QVector<double>> a, QVector<double> b, QVector<double>& x)
{
    const int n = a.size();
    x = QVector<double>(n, 0.0);
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int r = col + 1; r < n; ++r)
            if (std::fabs(a.at(r).at(col)) > std::fabs(a.at(pivot).at(col)))
                pivot = r;
        if (std::fabs(a.at(pivot).at(col)) < 1e-12)
            return false;
        if (pivot != col) {
            a.swapItemsAt(pivot, col);
            std::swap(b[pivot], b[col]);
        }
        for (int r = col + 1; r < n; ++r) {
            const double factor = a.at(r).at(col) / a.at(col).at(col);
            if (factor != 0.0) {
                for (int c = col; c < n; ++c)
                    a[r][c] -= factor * a.at(col).at(c);
                b[r] -= factor * b.at(col);
            }
        }
    }
    for (int r = n - 1; r >= 0; --r) {
        double s = b.at(r);
        for (int c = r + 1; c < n; ++c)
            s -= a.at(r).at(c) * x.at(c);
        x[r] = s / a.at(r).at(r);
    }
    return true;
}

} // namespace

FilterSavitzkyGolay::FilterSavitzkyGolay() = default;

void FilterSavitzkyGolay::configure(const QVariantMap& p)
{
    m_order = p.value(QStringLiteral("order"), 2).toInt();
    m_width = p.value(QStringLiteral("width"), 5).toInt();
    // OpenChrom SavitzkyGolayFilter 构造纠正（MODULE_10 §2.2）：
    //   width = max(5, 1+2*((w-1)/2))（强制奇数且 ≥5）；order = min(max(0,o),5,width-1)
    m_width = std::max(5, 1 + 2 * ((m_width - 1) / 2));
    m_order = std::min(std::max(0, m_order), std::min(5, m_width - 1));
}

QString FilterSavitzkyGolay::id() const { return QStringLiteral("sg_smooth"); }

QString FilterSavitzkyGolay::displayName() const { return QStringLiteral("Savitzky-Golay 平滑"); }

QVector<QVector<double>> FilterSavitzkyGolay::buildWeightRows() const
{
    const int n = m_width;
    const int o = m_order;
    const int p = (n - 1) / 2;

    // 设计矩阵 X[n][o+1]：行 i 的局部坐标 t = i-p，列为幂 0..o（Vandermonde）
    QVector<QVector<double>> X(n, QVector<double>(o + 1, 0.0));
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i - p);
        X[i][0] = 1.0;
        for (int j = 1; j <= o; ++j)
            X[i][j] = X.at(i).at(j - 1) * t;
    }

    // 法方程 A = X^T X（(o+1)×(o+1)）
    QVector<QVector<double>> A(o + 1, QVector<double>(o + 1, 0.0));
    for (int j = 0; j <= o; ++j)
        for (int k = 0; k <= o; ++k) {
            double s = 0.0;
            for (int i = 0; i < n; ++i)
                s += X.at(i).at(j) * X.at(i).at(k);
            A[j][k] = s;
        }

    // 逐列解单位向量 → (X^T X)^-1（Ainv）
    QVector<QVector<double>> Ainv(o + 1, QVector<double>(o + 1, 0.0));
    for (int j = 0; j <= o; ++j) {
        QVector<double> e(o + 1, 0.0);
        e[j] = 1.0;
        QVector<double> col;
        if (!solveLinearSystem(A, e, col))
            return QVector<QVector<double>>(); // 退化（实际对 Vandermonde 不可能）→ 不平滑
        for (int k = 0; k <= o; ++k)
            Ainv[k][j] = col.at(k);
    }

    // B = (X^T X)^-1 · X^T（(o+1)×n）：B[j][k] = sum_l Ainv[j][l]·X[k][l]
    QVector<QVector<double>> B(o + 1, QVector<double>(n, 0.0));
    for (int j = 0; j <= o; ++j)
        for (int k = 0; k < n; ++k) {
            double s = 0.0;
            for (int l = 0; l <= o; ++l)
                s += Ainv.at(j).at(l) * X.at(k).at(l);
            B[j][k] = s;
        }

    // 权重行 W[r][k] = phi(t_r)·B 第 r 行 = sum_j X[r][j]·B[j][k]
    QVector<QVector<double>> W(n, QVector<double>(n, 0.0));
    for (int r = 0; r < n; ++r)
        for (int k = 0; k < n; ++k) {
            double s = 0.0;
            for (int j = 0; j <= o; ++j)
                s += X.at(r).at(j) * B.at(j).at(k);
            W[r][k] = s;
        }
    return W;
}

double FilterSavitzkyGolay::dot(const QVector<double>& w, const QVector<Signal>& sig, int offset)
{
    double s = 0.0;
    for (int k = 0; k < w.size(); ++k)
        s += w.at(k) * sig.at(offset + k).intensity;
    return s;
}

void FilterSavitzkyGolay::apply(Chromatogram& chrom)
{
    const QVector<Signal>& src = workingSignal(chrom);
    QVector<Signal> out = src;
    // 点数不足 width 或空 → 无法平滑，原样写入 processed（保持「已处理副本」语义）
    if (src.size() < m_width) {
        chrom.setProcessedPoints(out);
        return;
    }

    const QVector<QVector<double>> W = buildWeightRows();
    if (W.isEmpty()) {
        chrom.setProcessedPoints(out);
        return;
    }

    const int n = src.size();
    const int p = (m_width - 1) / 2;
    for (int i = 0; i < n; ++i) {
        double v = 0.0;
        if (i < p) {
            v = dot(W.at(i), src, 0); // 首 p 点：窗口取最前 width 个
        } else if (i >= n - p) {
            const int r = i - (n - m_width); // 尾 p 点：窗口取最后 width 个，相对位置 r
            v = dot(W.at(r), src, n - m_width);
        } else {
            v = dot(W.at(p), src, i - p); // 中间：对称卷积核
        }
        out[i].intensity = v;
    }
    chrom.setProcessedPoints(out);
}

} // namespace cdsw
