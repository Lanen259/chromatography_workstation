# 不确定清单 —— 2026-08-18 整夜自主开发

> 记录「拿不准但按最优判断继续」的决策；待用户复核。固定格式：日期 / 问题 / 查证过程 / 已采取取舍 / 待用户确认。

## 2026-08-18 / M4(io) 兜底接管 dev-io
- **问题**：M6(MainWindow 导入) 依赖 io 的 ConverterRegistry，而 io 未合 main；worktrees/io 属 M4 线程。
- **查证过程**：`git -C worktrees/io status --short` 干净、`git log main..dev-io` 无提交、io/include/io/ 仅 .gitkeep → 完全空闲。契约/M4 指令/整夜 plan 均在案。
- **已采取取舍**：按用户「M4 依赖兜底」规则自行完成 M4(io)（子 agent 在 dev-io 独写），合回 main 后再做 M6 Phase B。
- **待用户确认**：M4 线程确未在其它会话启动（避免双写）。本会话派发时确认空闲。

## 2026-08-18 / Qt5 QTextStream 默认编码
- **问题**：报告/导出写文件时默认编码假设 UTF-8 是否成立。
- **查证过程**：独立审查子 agent 指出 Qt5 QTextStream 默认 codec = codecForLocale()（中文 Windows = GBK），Qt6 才默认 UTF-8。项目是 Qt 5.14.2。
- **已采取取舍**：M5 已显式 `setCodec(UTF-8)` 并补中文金样用例（先红后绿）；M4(io) 子 agent brief 同样要求显式 UTF-8。
- **待用户确认**：无（已修复，测试锁死）。

## 2026-08-18 / QSignalSpy 捕获自定义类型
- **问题**：QSignalSpy 捕获 `QList<cdsw::Peak>` 参数失败（peaks.size()==0）。
- **查证过程**：acq 记忆先例：moc 记录参数名 "QList<Peak>"，默认注册名带命名空间不匹配。
- **已采取取舍**：测试内 `qRegisterMetaType<QList<cdsw::Peak>>("QList<Peak>")` 显式按 moc 名注册；SelectionController.h 加 Q_DECLARE_METATYPE。
- **待用户确认**：无。

## 2026-08-18 / MainWindow.ui 提升类命名空间
- **问题**：`.ui` 里 promote 的 ChromatogramView/PeakTableView/MethodEditorView 在 cdsw 命名空间，生成的 ui_MainWindow.h（全局命名空间）报「does not name a type」。
- **查证过程**：uic 支持 customwidget 的完全限定类名。
- **已采取取舍**：.ui 里用 `cdsw::ChromatogramView` 等完全限定名。
- **待用户确认**：无。

## 2026-08-18 / M6 设计取舍（非冻结处）
- **问题**：契约 §4.6 只冻结 SelectionController 签名与视图职责描述，具体 API/交互自定。
- **查证过程**：MODULE_07（图框选→写回 selection→广播、方法树、QMainWindow 对应 e4）；编码风格 §8。
- **已采取取舍**：视图 API 照设计文档；MainWindow 的 `buildReportData` 中 methodName 留空（Method 结构无名称字段）；Phase A 不含 io 导入（导入菜单先挂上、Phase B 接线）。
- **待用户确认**：methodName 是否需要在 Method 结构加名称字段（涉及 core_model 契约变更 → 需主控登记）。

## 2026-08-18 / M6 曲线视图数据源
- **问题**：ChromatogramView 画 processedPoints 还是 signalPoints。
- **查证过程**：契约 §4.1「原始信号永不改、processedPoints 为滤波/基线后副本」。
- **已采取取舍**：processedPoints 非空用它、否则 signalPoints（设计文档 §3）。
- **待用户确认**：无。

## 2026-08-18 / M6 审查 Non-critical 取舍
- **问题**：独立审查 8 项 Non-critical，哪些修。
- **查证过程**：无 Critical；改进按「便宜且提升正确性」筛。
- **已采取取舍**：修了 4 项（选区握手/zoom 上界/零宽选区不广播/Y 轴取显示点 + Add 自动选中）；未修 2 项（MethodEditor 参数键名改列被忽略——边缘 UX；CMakeLists 链接 acq——契约依赖方向本就如此）；未补测 2 项（wheel 分支/QPainter 裁剪——覆盖已够冒烟）。
- **待用户确认**：MethodEditor 键名编辑语义若需支持，后续加。

## 2026-08-18 / M7 集成与 E2E 形态
- **问题**：契约 §4.7 主程序壳如何装配 6 模块、E2E 怎么验证「exe 可运行」。
- **查证过程**：治理规范合回门禁需「exe 可运行 + 端到端绿」；GUI exe 交互难以自动化。
- **已采取取舍**：main.cpp 装配 ui MainWindow（持有 io 导入 + core_processing 管线 + report 导出 + 曲线显示）；移除 Qt Creator 模板 widget.*；加 `--e2e` 无头自检 flag：写样本 CSV → importCsv(io) → setMethod/runMethod(core_processing) → exportCsv(report) → 断言峰表 2 行 + 报告含峰表头 → exit 0。`QT_QPA_PLATFORM=offscreen ./chromatography_workstation.exe --e2e` 验证「exe 可运行 + 端到端绿」。
- **待用户确认**：无。

## 2026-08-18 / CDS 1.0 UI 升级（P1–P6 决策）
- **问题**：工业级/可上线/OpenChrom 级别 UI 升级的范围与取舍。
- **查证过程**：MODULE_07（Dock/方法树/Info/Log/事件总线）、编码风格 §8、契约 §4.6 冻结面。
- **已采取取舍**：① 只动 ui 模块 + main.cpp（core 系一行未动）；② 现代暗色主题 theme.qss + ChartPalette（与 QSS 同源色板）；③ 图表专业版（轴/网格/峰标/十字线/概览/平移/双击复位）；④ QDockWidget 可停靠工作区 + View 菜单 + QSettings 布局持久化；⑤ 方法 JSON 存读（顶层 name 字段承载方法名，Method 结构本身无名称字段——core_model 不改）；⑥ InfoView/LogView 停靠面板 + 状态栏；⑦ 演示数据启动加载 + High-DPI + `--e2e` 自检。
- **待用户确认**：① 方法步骤「显示名」仅 IFilter 接口有（契约 §4.2 其他接口只有 id），列表暂显示 id，未统一取显示名——若需要需 core_processing 接口加 displayName（冻结面变更，需主控拍板）；② QTest::mouseMove 在 offscreen 下不投递 mouseMoveEvent（Qt5 offscreen 平台已知怪癖），测试改用 QApplication::sendEvent 直投，非产品代码问题；③ 未做插件系统/真实采集 UI/报告列编辑（OpenChrom 完整功能面远超本次，属后续里程碑）。

