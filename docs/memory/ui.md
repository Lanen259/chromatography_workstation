# ui 线程记忆

## 2026-08-18 ui 线程（M6 Phase A + B 完成，待合并）
- **做了什么**：契约 §4.6 界面层全部落地。`include/ui/`：`SelectionController`（契约冻结签名 + 增补 setChromatogram/setMethod）、`PeakTableModel`（QAbstractTableModel 适配 QList<Peak>）、`PeakTableView`（QTableView 子类）、`ChromatogramView`（自绘曲线 + 缩放/平移/选区）、`MethodEditorView`（方法步骤增删改）、`MainWindow`（菜单/工具栏/分栏装配 + io 导入 + report 导出）。`.ui` 文件：`MainWindow.ui`、`MethodEditorView.ui`。测试 `tests/t_ui.cpp` 7 槽 offscreen 全绿。
- **为什么这么设计**：
  - **风格 §8（UI 一律 .ui）**：复用页面（MainWindow/MethodEditorView）用 `.ui` + `setupUi()` 装配；自绘/复用控件（ChromatogramView/PeakTableView/MethodEditorView）以 promote 进 MainWindow.ui；C++ 只写信号槽/数据绑定。cdsw_add_module 不 glob .ui，需 `target_sources(ui PRIVATE src/*.ui)` 手动加（AUTOUIC 生成 ui_*.h）。
  - **命名空间坑（两次踩）**：① `.ui` 生成的 `Ui::*` 在**全局**命名空间，C++ 类里 `namespace Ui { class X; }` 前向声明必须写在 `namespace cdsw` **外面**，否则解析成 cdsw::Ui；② promote 的类在 cdsw 命名空间，`.ui` 里 `<class>` 必须用**完全限定名** `cdsw::ChromatogramView`，否则生成的 ui_MainWindow.h（全局作用域）报「does not name a type」。
  - **QSignalSpy 捕获自定义类型**：`sigPeaksUpdated(const QList<Peak>&)` 的 moc 参数名是 `"QList<Peak>"`，默认注册名带命名空间不匹配 → 捕获为空。测试内显式 `qRegisterMetaType<QList<cdsw::Peak>>("QList<Peak>")`（acq 先例同款）。
  - **SelectionController 桥接**：契约只冻结构造 (Selection*, ProcessingPipeline*, QObject*) + onChromatogramChanged + sigPeaksUpdated；execute 需要 chrom+method，故增补 setChromatogram/setMethod（open-closed，不改冻结项）。onChromatogramChanged：空守卫 → setDirty(true) → pipeline.execute(*method,*chrom) → emit sigPeaksUpdated(peaks)。真实管线冒烟：双高斯峰 + first_derivative_peak_detector → 检出 2 峰。
  - **MainWindow 所有权**：Chromatogram 用指针（setChromatogram 接受外部指针）；导入数据存 `m_chromData` 成员（所有者），importCsv 经 `io::ConverterRegistry::instance().importerFor` 载入后 setChromatogram(&m_chromData)。
  - **两阶段**（io 依赖兜底）：Phase A 实现视图 + SelectionController + MainWindow 骨架（无 io）；M4 合 main 后 Phase B 补导入接线。`ui/CMakeLists.txt` 从一开始就 `cdsw_add_module(ui core_model core_processing acq io report)`，io 空壳目标已存在故配置可过。
  - **offscreen 冒烟**：QTEST_MAIN（QApplication 必需）；`set_tests_properties(ui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")` 注入；ChromatogramView 用 QTest::mousePress/move/release 模拟拖拽选区 + zoomAt 测试缩放。
- **验证**：dev-ui 全量 `cmake --build build` 0 error；全套 ctest 6/6 绿（core_model/core_processing/acq/io/report/ui）；ui_tests 7 槽（init + 6 用例 + cleanup = 9 pass）offscreen 全绿：selectionControllerRunsPipeline / peakTableModelShowsPeaks / peakTableModelEmpty / chromatogramViewRendersSelectsAndZooms / methodEditorEditsSteps / mainWindowAssemblesAndRuns / mainWindowImportsCsv。接口纪律：ui 只 include core/core_processing/io/report 接口头；ui 是唯一 QtWidgets 模块。
- **下一步**：主控线程按治理规范 §3 合回 main（独立审查 + ctest 全绿后）。
- **卡点/依赖**：无代码卡点。**协调事项（主控需知）**：① MethodEditorView 的 Add 下拉 = core_processing Registry 全部算法 id（开闭，新算法自动出现）；② MainWindow.buildReportData 的 methodName 留空（Method 结构无名称字段；若要在报告里显示方法名需 core_model 契约加字段，登记进不确定清单）；③ 曲线数据源 processedPoints 优先、否则 signalPoints（契约 §4.1 语义）；④ ui 测试必须 offscreen（QWidget 需 QApplication）。
