// ui/src/LogView.cpp —— 日志视图实现（只读文本，时间戳追加）
#include <ui/LogView.h>

#include <QtCore/qdatetime.h>
#include <QtWidgets/qtextedit.h>

#include "ui_LogView.h"

namespace cdsw {

LogView::LogView(QWidget* parent) : QWidget(parent), ui(new Ui::LogViewUi)
{
    ui->setupUi(this);
    ui->text->setReadOnly(true);
}

LogView::~LogView()
{
    delete ui;
}

void LogView::appendMessage(const QString& message)
{
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                  message);
    ui->text->append(line);
}

void LogView::clear()
{
    ui->text->clear();
}

QString LogView::toPlainText() const
{
    return ui->text->toPlainText();
}

} // namespace cdsw
