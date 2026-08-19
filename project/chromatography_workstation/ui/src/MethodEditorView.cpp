// ui/src/MethodEditorView.cpp —— 方法编辑器实现（.ui 装配 + 注册表步骤）
#include <ui/MethodEditorView.h>

#include <core_processing/interfaces.h>

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtablewidget.h>

#include "ui_MethodEditorView.h"

namespace cdsw {

MethodEditorView::MethodEditorView(QWidget* parent)
    : QWidget(parent), ui(new Ui::MethodEditorViewUi)
{
    ui->setupUi(this);

    // Add 候选 = 注册表全部算法 id（开闭：新算法注册即出现在下拉）
    Registry& reg = Registry::instance();
    QStringList ids = reg.availableFilterIds() + reg.availableBaselineIds()
        + reg.availablePeakDetectorIds() + reg.availableIntegratorIds()
        + reg.availableQuantifierIds();
    ids.removeDuplicates();
    ids.sort();
    ui->comboAlgorithm->addItems(ids);

    ui->tableParams->setColumnCount(2);
    ui->tableParams->setHorizontalHeaderLabels(
        { QStringLiteral("参数"), QStringLiteral("值") });

    connect(ui->btnAdd, &QPushButton::clicked, this, &MethodEditorView::onAddStep);
    connect(ui->btnCopy, &QPushButton::clicked, this, &MethodEditorView::onCopyStep);
    connect(ui->btnRemove, &QPushButton::clicked, this, &MethodEditorView::onRemoveStep);
    connect(ui->btnUp, &QPushButton::clicked, this, &MethodEditorView::onMoveUp);
    connect(ui->btnDown, &QPushButton::clicked, this, &MethodEditorView::onMoveDown);
    connect(ui->listSteps, &QListWidget::currentRowChanged, this, &MethodEditorView::onStepSelected);
    connect(ui->tableParams, &QTableWidget::cellChanged, this, &MethodEditorView::onParamEdited);
}

MethodEditorView::~MethodEditorView()
{
    delete ui;
}

void MethodEditorView::setMethod(Method* method)
{
    m_method = method;
    reloadSteps();
}

Method* MethodEditorView::method() const { return m_method; }

void MethodEditorView::reloadSteps()
{
    ui->listSteps->clear();
    if (!m_method)
        return;
    for (const ProcessingStep& step : m_method->steps)
        ui->listSteps->addItem(step.id);
    onStepSelected();
}

void MethodEditorView::onAddStep()
{
    if (!m_method)
        return;
    const QString id = ui->comboAlgorithm->currentText();
    if (id.isEmpty())
        return;
    m_method->steps.append(ProcessingStep{ id, QVariantMap() });
    ui->listSteps->addItem(id);
    ui->listSteps->setCurrentRow(ui->listSteps->count() - 1);   // 新步骤立即选中，参数表可见
    emit sigMethodChanged();
}

void MethodEditorView::onCopyStep()
{
    if (!m_method)
        return;
    const int row = ui->listSteps->currentRow();
    if (row < 0 || row >= m_method->steps.size())
        return;
    m_method->steps.insert(row + 1, m_method->steps.at(row));   // 深拷贝步骤（含参数）
    ui->listSteps->insertItem(row + 1, ui->listSteps->item(row)->text());
    ui->listSteps->setCurrentRow(row + 1);
    emit sigMethodChanged();
}

void MethodEditorView::onRemoveStep()
{
    if (!m_method)
        return;
    const int row = ui->listSteps->currentRow();
    if (row < 0 || row >= m_method->steps.size())
        return;
    m_method->steps.removeAt(row);
    delete ui->listSteps->takeItem(row);
    emit sigMethodChanged();
}

void MethodEditorView::onMoveUp()
{
    if (!m_method)
        return;
    const int row = ui->listSteps->currentRow();
    if (row <= 0 || row >= m_method->steps.size())
        return;
    m_method->steps.swapItemsAt(row, row - 1);
    ui->listSteps->insertItem(row - 1, ui->listSteps->takeItem(row));
    ui->listSteps->setCurrentRow(row - 1);
    emit sigMethodChanged();
}

void MethodEditorView::onMoveDown()
{
    if (!m_method)
        return;
    const int row = ui->listSteps->currentRow();
    if (row < 0 || row >= m_method->steps.size() - 1)
        return;
    m_method->steps.swapItemsAt(row, row + 1);
    ui->listSteps->insertItem(row + 1, ui->listSteps->takeItem(row));
    ui->listSteps->setCurrentRow(row + 1);
    emit sigMethodChanged();
}

void MethodEditorView::onStepSelected()
{
    if (!m_method)
        return;
    const int row = ui->listSteps->currentRow();
    if (row < 0 || row >= m_method->steps.size()) {
        ui->tableParams->setRowCount(0);
        return;
    }
    loadParamTable(row);
}

void MethodEditorView::loadParamTable(int stepRow)
{
    // 填表阻塞信号：避免每次 setItem 都触发 onParamEdited
    const QSignalBlocker blocker(ui->tableParams);
    const QVariantMap params = m_method->steps.at(stepRow).parameters;
    ui->tableParams->setRowCount(params.size());
    int r = 0;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it, ++r) {
        ui->tableParams->setItem(r, 0, new QTableWidgetItem(it.key()));
        ui->tableParams->setItem(r, 1, new QTableWidgetItem(it.value().toString()));
    }
}

void MethodEditorView::onParamEdited(int row, int column)
{
    if (!m_method || column != 1)
        return;
    const int stepRow = ui->listSteps->currentRow();
    if (stepRow < 0 || stepRow >= m_method->steps.size())
        return;
    QTableWidgetItem* keyItem = ui->tableParams->item(row, 0);
    QTableWidgetItem* valItem = ui->tableParams->item(row, 1);
    if (!keyItem || !valItem)
        return;
    m_method->steps[stepRow].parameters.insert(keyItem->text(), valItem->text());
    emit sigMethodChanged();
}

} // namespace cdsw
