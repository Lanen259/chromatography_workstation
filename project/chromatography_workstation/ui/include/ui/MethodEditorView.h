// ui/include/ui/MethodEditorView.h —— 方法编辑器（契约 §4.6：Method.steps 增删改）
#pragma once
#include <core_model/Method.h>
#include <QtWidgets/qwidget.h>

// .ui 生成的 Ui::* 在全局命名空间（AUTOUIC ui_MethodEditorView.h），前向声明须在 cdsw 外
namespace Ui { class MethodEditorViewUi; }

namespace cdsw {

// 面板：注册表算法下拉 + 步骤列表 + 上移/下移/删除 + 参数表（k/v 写回 QVariantMap）。
// 编辑对象 = setMethod 传入的 Method 指针（不拷贝、不持有所有权）。
class MethodEditorView : public QWidget {
    Q_OBJECT
public:
    explicit MethodEditorView(QWidget* parent = nullptr);
    ~MethodEditorView() override;
    void setMethod(Method* method);
    Method* method() const;
signals:
    void sigMethodChanged();
private slots:
    void onAddStep();
    void onCopyStep();
    void onRemoveStep();
    void onMoveUp();
    void onMoveDown();
    void onStepSelected();
    void onParamEdited(int row, int column);
private:
    void reloadSteps();
    void loadParamTable(int stepRow);
    Ui::MethodEditorViewUi* ui;
    Method* m_method = nullptr;
};

} // namespace cdsw
