# 项目总控 / 进度追踪（PROJECT.md）

> 本文件是**动态看板**：记录「谁在做什么、做到哪了、下一步是什么」。
> 静态规格（接口签名 / 依赖方向）在 `00_工程骨架与模块契约.md`，治理公约在 `开发治理规范.md`，开工铁律在 `CLAUDE.md`。
>
> **维护规则**：每次派发任务、模块合并、关键决策后，立即更新本文件对应小节。
> **所有 AI 线程开工前必读**：本文件 + `CLAUDE.md` + 契约 §0/§4。

## 0. 项目定位（一句话）

嵌入式 **Qt/C++ 色谱工作站（CDS）**，逆向 OpenChrom 得到架构与核心算法，6 模块 + Qt 主程序壳，多 agent 并行开发。

## 1. 里程碑总览

| 里程碑 | 内容 | 状态 | 验收 |
|---|---|---|---|
| M0 | 契约 + Qt 主工程骨架 + worktree 框架 | ✅ 2026-08-16 | Qt 工程 MinGW 全量构建通过 |
| M1 | **core_model**（领域模型，含测试）合并 main | ✅ 2026-08-18 | 自身 ctest 绿 |
| M2 | core_processing（处理引擎） | ✅ 2026-08-18 | 金标准信号测试绿 |
| M3 | acq（采集，自研无参考）—— 首块 = 实时反控协议接收链（[docs/protocol/HWSendData_实时反控协议.md](docs/protocol/HWSendData_实时反控协议.md)） | 🔨 M3a 已合 main；**M3b IPC 端点待开工** | mock 测试绿（协议复刻示例端到端绿）✅ |
| M4 | io（CSV 转换器） | ✅ 2026-08-18 | 回环/边界测试 9 用例绿 |
| M5 | report（报告器） | ✅ 2026-08-18 | 金样逐字节比对测试绿 |
| M6 | ui（视图） | ✅ 2026-08-18 | offscreen 冒烟 7 用例绿 |
| M7 | Qt 主工程集成 + 端到端 | ✅ 2026-08-18 | 全量构建 0 error + 全套 ctest 6/6 绿 + exe 可运行 + --e2e 端到端绿 |

## 2. 模块状态表

| 模块 | 分支 | worktree 目录 | 状态 | 下一步 | 参考逆向文档 | 阻塞 |
|---|---|---|---|---|---|---|
| core_model | dev-core_model | worktrees/core_model/ | ✅ 已合 main | M1 完成（接口+实现+测试全绿，2026-08-18 合回 main，commit 5b2d092） | MODULE_02, MODULE_04 | 无 |
| core_processing | dev-core_processing | worktrees/core_processing/ | ✅ 已合 main | M2 完成（§4.2 接口 + 5 内置算法 + Registry + ProcessingPipeline + 金标准测试全绿，2026-08-18 合回 main，commit e502f08） | MODULE_03, MODULE_04, MODULE_09, MODULE_10 | 无 |
| acq | dev-acq | worktrees/acq/ | ✅ 已合 main | M3a 实时反控协议接收链完成（`HwRealtimeReceiver` 解码 data3 0–17 + RingBuffer 覆盖最旧保新 + AcquisitionController + MockDevice/§5 复刻端到端，ctest 全绿，2026-08-18 合回 main）；**下一步 M3b：IPC 端点（QLocalServer 适配 CtrlPanel）** | 无（社区版无采集代码） | 无 |
| io | dev-io | worktrees/io/ | ✅ 已合 main | M4 完成（ImporterCsv/ExporterCsv + ConverterRegistry + 回环/空文件/坏行/表头变体/导出字节/注册表/失败路径/UTF-8+BOM 测试 9 用例全绿，2026-08-18 合回 main） | MODULE_01 | 无 |
| report | dev-report | worktrees/report/ | ✅ 已合 main | M5 完成（契约 §4.5 接口 + ReporterCsv 分区式 CSV + ReportRegistry + 金样逐字节比对 + 中文 UTF-8 用例，全绿，2026-08-18 合回 main） | MODULE_06 | 无 |
| ui | dev-ui | worktrees/ui/ | ✅ 已合 main | M6 完成（契约 §4.6 视图：ChromatogramView 自绘缩放/平移/选区 + PeakTableModel/View + MethodEditorView + MainWindow .ui 装配 + SelectionController 桥接 + io 导入接线；offscreen 冒烟 7 用例全绿 + 审查无 Critical，2026-08-18 合回 main） | MODULE_07, MODULE_08 | 无 |

> 逆向文档路径：`docs/openchrom-reverse-engineering/module/`（MODULE_XX 前缀）。
> 状态取值：待开工 → 进行中 → 待合并 → ✅ 已合 main。

## 3. 合并记录（时间线）

| 提交 | 内容 |
|---|---|
| b1763e7 | 初始化：工程契约 + worktree 框架 |
| 8a133e4 | M0：根 CMake 骨架 + 模块目录结构 |
| bc2ede5 | M0a：6 模块并入 Qt 主工程（方案 A） |
| a1518fa | 契约 §0 接口规则 + 仓库根 CLAUDE.md |
| acee7c1 | doc：OpenChrom 逆向知识库（31 文档，收官基线） |
| ac136b9 | doc(style)：UI 设计规范 §8（.ui 优先）+ 契约 §4.6 + 提交在途的 M1 冻结口径 |
| 5b2d092 | M1：core_model 领域模型（§4.1 接口 + 实现 + QTest）+ cdsw_module.cmake AUTOMOC 修复 合回 main |
| 3db1ba5 | doc：M1 合回后的状态更新（PROJECT.md 看板 / 治理规范 D7 人工审查门禁 / CLAUDE.md 必读顺序） |
| e502f08 | M2：core_processing 处理引擎（§4.2 接口 + Registry + ProcessingPipeline + 5 内置算法 + 金标准测试）合回 main |
| c6d9bbb | M3a：acq 实时反控协议接收链（HwRealtimeReceiver 解码 data3 0–17 + RingBuffer + AcquisitionController + QTest）合回 main |
| fe0b550 | M5：report 报告器（§4.5 接口 + ReporterCsv + ReportRegistry + 金样测试 + UTF-8 显式 codec 修复）合回 main |
| 6b2065e | M4：io 转换器（§4.4 接口 + ImporterCsv/ExporterCsv + ConverterRegistry + 回环测试 + UTF-8/BOM 修复）合回 main |
| 0eefc99 | M6：ui 界面层（§4.6 视图 + SelectionController + MainWindow .ui 装配 + 审查改进）合回 main |
| c018ae2 | M7：Qt 主工程集成（main.cpp 装配 MainWindow + --e2e 无头自检 + 移除 widget.*）|
| 4a35a46 | CDS 1.0：工业级 UI 升级（feat/ui-industrial 合回）——现代暗色主题 + 色谱图专业版 + 可停靠工作区 + 方法 JSON 存读 + Info/Log 视图 + 演示数据/High-DPI/布局持久化 |

## 4. 决策日志

| 日期 | 决策 |
|---|---|
| 2026-08-16 | 形态 = 嵌入式 Qt/C++ CDS，复刻 OpenChrom |
| 2026-08-16 | 方案 A：6 模块并入 Qt 主工程，app 模块取消 |
| 2026-08-16 | 架构原则：模块间只通过接口说话（OpenChrom 扩展点思想） |
| 2026-08-16 | 依赖方向：ui → core/acq/io/report → core_processing → core_model |
| 2026-08-16 | 并行：每模块一 worktree 分支，ctest 全绿才合并回 main |
| 2026-08-16 | acq 无逆向参考（社区版无采集代码），需自研 |
| 2026-08-18 | 实时反控协议（HWSendData 语义，data3 0–17）纳入 M3 首块；接收侧由我方定制 `HwRealtimeReceiver`（传输无关），规格见 docs/protocol/ |
| 2026-08-18 | M2(core_processing) + M3a(acq) 合回 main（全量构建 0 error + ctest 3/3 绿）；io/report/ui worktree 已同步 main，M4/M5 可并行开工 |
| 2026-08-18 | 整夜计划：M3b(acq IPC 端点) + M4(io CSV) + M5(report CSV) 三路并行；合并一律等主控人工审查放行 |
| 2026-08-18 | **M3b 传输方案 = QLocalServer + acq 加 QtNetwork 例外**（改 CLAUDE.md 铁律 §2 + 契约 §8 规则 5）：IPC 封装在传输适配器后，`HwRealtimeReceiver` 保持 QtCore 纯 |
| 2026-08-18 | 整夜自主执行（用户预授权）：M5(report) → M4(io 兜底，dev-io 空闲) → M6(ui) → M7(集成) 全部完成合 main：全量构建 0 error + 全套 ctest 6/6 绿 + exe 可运行 + `--e2e` 端到端绿 |
| 2026-08-18 | **Qt5 QTextStream 默认 codec = codecForLocale()（中文 Windows=GBK）**：io/report 导入导出显式 `setCodec(UTF-8)`，中文金样用例锁死字节一致性 |
| 2026-08-18 | **CDS 1.0 工业级 UI 升级（feat/ui-industrial）**：现代暗色主题 + 色谱图专业版（轴/网格/峰标/十字线/概览）+ 可停靠工作区（QDockWidget + 布局持久化）+ 方法 JSON 存读 + Info/Log 视图 + 演示数据/High-DPI；只动 ui + main.cpp（core 系零改动）；独立审查无 Critical |

## 5. 合并顺序（不可违反）

```
core_model → {core_processing, acq, io, report 四路并行} → ui → Qt 主工程集成
```

## 6. AI 线程开工 SOP（每个 worktree = 一个独立线程）

1. **锁定范围**：确认分配给你的 worktree（`worktrees/<模块>/`），只许改 `project/chromatography_workstation/<模块>/` 下的 `include/ src/ tests/ CMakeLists.txt`。
2. **读规则**：仓库根 `CLAUDE.md`（三条铁律）+ 契约 `00_工程骨架与模块契约.md` 的 §0 / §4（你模块的接口签名）/ §5 / §6。
3. **读状态**：本文件 §1 / §2 确认你的任务与阻塞项。
4. **读参考**：§2 表中你模块对应的逆向文档。
5. **实现**：实现接口 + 写测试 + 跑通 `ctest` 全绿。
6. **回报**：更新本文件 §2 你那一行的「状态 / 下一步」；commit 用规范前缀 `feat/test/doc(<模块>): ...`。
7. **停下等合并**：禁止自行 `git merge main`、禁止改契约签名、禁止碰其他模块文件——全绿后等我（主控线程）按 §5 顺序合并回 main。

---

## 当前状态（2026-08-18 整夜完成）

**M1 + M2 + M3a + M4(io) + M5(report) + M6(ui) 已合 main，M7 集成完成：全量构建 0 error + 全套 ctest 6/6 全绿 + exe 可运行。**
- 完整链路已通：导入(io) → 管线(core_processing) → 报告(report) → 曲线显示(ui)。
- `chromatography_workstation.exe --e2e` 无头自检全链路（exit 0 绿）。
- **唯一剩余**：M3b（acq IPC 端点，QLocalServer 适配 CtrlPanel）在 `worktrees/acq/` 待开工（独立线程）。
