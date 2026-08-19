# ui 线程记忆

## 2026-08-18 CDS 1.0 UI 升级（P1–P6，feat/ui-industrial）
- **做了什么**：把可运行的 MainWindow 骨架升级为工业级现代 UI。P1 现代暗色主题（theme.qss 全控件 + ThemeColors 色板 + applyTheme）；P2 色谱图专业版（轴/网格/刻度/抗锯齿+渐变填充/峰标注 P#+RT/十字线提示/中键平移/双击复位/概览条拖动/空态）；P3 可停靠工作区（QDockWidget 峰表/方法编辑/信息/日志 + View 菜单 toggle + QSettings 几何与停靠持久化）；P4 方法编辑器 v2（Copy 复制步骤 + 方法 JSON 存读）；P5 InfoView/LogView 停靠 + 状态栏永久标签 + sigLogMessage 事件日志；P6 main.cpp 产品化（High-DPI、org/app/version、restoreWorkspace/saveWorkspace、loadDemoData 启动演示数据、--e2e 保持）。
- **关键经验（避免返工）**：
  - **AUTORCC + 静态库**：qrc 对象会被链接器丢弃，资源加载失败。必须 `extern int qInitResources_ui();`（全局声明）+ `::qInitResources_ui()` 显式调用（在 cdsw 命名空间内 `Q_INIT_RESOURCE(ui)` 会解析成 cdsw::qInitResources_ui 找不到符号）。
  - **Qt5 图标/渐变 include**：QApplication 在 `QtWidgets/qapplication.h`（不在 QtGui）；QLinearGradient 在 `QtGui/qbrush.h`；`QApplication::setStyleSheet/styleSheet` 是**实例**成员（用 `qApp->`），不是静态。
  - **QTest::mouseMove 在 offscreen 不投递 mouseMoveEvent**（Qt5 offscreen 平台怪癖，QtTest 已知）：拖拽测试改用 `QApplication::sendEvent(&widget, QMouseEvent(MouseMove,...))` 直投。
  - **.ui 生成 Ui::* 在全局命名空间**（前向声明须在 cdsw 外）；promote 的 cdsw 类在 .ui 用完全限定名 `cdsw::Xxx`（两次踩坑，见 M6 记忆）。
  - **QSettings 实例**：`QSettings(org, app)` 构造即可读写，`appSettings()` 返回局部实例（拷贝），测试不受真实设置影响。
  - **图表坐标**：plotRect 扣轴边距（左 64/右 14/上 12/下 26 + 概览 34）；msToX/xToMs/intensityToY/yToIntensity 全部以 plotRect 为准，xToMs 夹到 plot 范围防轴区误拖。
- **为什么这么设计**：对标 MODULE_07（Dock ≈ e4 perspective、方法树 ≈ MethodTreeViewer、Info/Log ≈ 视图部分）；开闭原则（新增只加类/资源，不动冻结接口）；core 系模块零改动（红线）。
- **验证**：ui_tests 16 用例 offscreen 全绿；全量构建 0 error（待 P7 合回后主 checkout 复跑）；`--e2e` exit 0。
- **下一步**：P7 独立审查（Critical 全修）→ 合回 main → 全量验证 → push GitHub。
- **卡点/依赖**：无。协调事项见 `docs/uncertainties-2026-08-18-overnight.md`（显示名仅 IFilter 有、QTest mouseMove 怪癖、未做插件系统/采集 UI）。

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
