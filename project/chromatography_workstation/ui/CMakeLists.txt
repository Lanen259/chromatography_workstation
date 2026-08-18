# ui —— 界面层（契约 §4.6，唯一允许 QtWidgets 的模块）
cdsw_add_module(ui core_model core_processing acq io report)
find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets QUIET)
endif()
if(TARGET Qt6::Widgets)
    target_link_libraries(ui PUBLIC Qt6::Widgets)
elseif(TARGET Qt5::Widgets)
    target_link_libraries(ui PUBLIC Qt5::Widgets)
else()
    message(WARNING "未找到 Qt Widgets：ui 模块需在装有 Qt GUI 的环境编译（契约 §8.3 可 CDSW_BUILD_UI=OFF 跳过）")
endif()

# 复用页面 .ui（cdsw_add_module 不 glob .ui，手动加入；AUTOUIC 生成 ui_*.h）
target_sources(ui PRIVATE src/MethodEditorView.ui)
# Task 5 建好 MainWindow.ui 后追加：src/MainWindow.ui

# offscreen 冒烟：ui 测试在无显示环境跑（QT_QPA_PLATFORM=offscreen）
if(CDSW_BUILD_TESTS AND TARGET ui_tests)
    set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
