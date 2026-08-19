// ui/include/ui/InfoView.h —— 信息视图（色谱元数据键值表）
#pragma once
#include <QtWidgets/qwidget.h>

namespace Ui { class InfoViewUi; }

namespace cdsw {

class Chromatogram;

class InfoView : public QWidget {
    Q_OBJECT
public:
    explicit InfoView(QWidget* parent = nullptr);
    ~InfoView() override;
    void setChromatogram(const Chromatogram* chrom);   // 填充元数据
    void setPeaks(int count);                          // 峰数行
private:
    void setRow(int row, const QString& key, const QString& value);
    Ui::InfoViewUi* ui;
};

} // namespace cdsw
