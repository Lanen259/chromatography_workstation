// ui/include/ui/LogView.h —— 日志视图（只读，时间戳追加）
#pragma once
#include <QtWidgets/qwidget.h>

namespace Ui { class LogViewUi; }

namespace cdsw {

class LogView : public QWidget {
    Q_OBJECT
public:
    explicit LogView(QWidget* parent = nullptr);
    ~LogView() override;
    void appendMessage(const QString& message);   // 追加 [HH:mm:ss] 行
    void clear();
    QString toPlainText() const;                  // 测试用
private:
    Ui::LogViewUi* ui;
};

} // namespace cdsw
