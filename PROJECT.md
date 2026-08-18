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
| M1 | **core_model**（领域模型，含测试）合并 main | 🔨 待开工 | 自身 ctest 绿 |
| M2 | core_processing（处理引擎） | 🔨 代码完成测试全绿，待合并 | 金标准信号测试绿 ✅ |
| M3 | acq（采集，自研无参考）—— **首块 = 实时反控协议接收链**（[docs/protocol/HWSendData_实时反控协议.md](docs/protocol/HWSendData_实时反控协议.md)） | ⏳ 待开工 | mock 测试绿（协议复刻示例端到端绿） |
| M4 | io（CSV 转换器） | ⏳ 待开工 | 回环测试绿 |
| M5 | report（报告器） | ⏳ 待开工 | 金样测试绿 |
| M6 | ui（视图） | ⏳ 待开工 | offscreen 冒烟绿 |
| M7 | Qt 主工程集成 + 端到端 | ⏳ 待开工 | 全模块 ctest 绿 + 可运行 exe |

## 2. 模块状态表

| 模块 | 分支 | worktree 目录 | 状态 | 下一步 | 参考逆向文档 | 阻塞 |
|---|---|---|---|---|---|---|
| core_model | dev-core_model | worktrees/core_model/ | ✅ 已合 main | M1 完成（接口+实现+测试全绿，2026-08-18 合回 main，commit 5b2d092） | MODULE_02, MODULE_04 | 无 |
| core_processing | dev-core_processing | worktrees/core_processing/ | ✅ 已合 main | M2 完成（§4.2 接口 + 5 内置算法 + Registry + ProcessingPipeline + 金标准测试全绿，2026-08-18 合回 main，commit e502f08） | MODULE_03, MODULE_04, MODULE_09, MODULE_10 | 无 |
| acq | dev-acq | worktrees/acq/ | ✅ 已合 main | M3a 实时反控协议接收链完成（`HwRealtimeReceiver` 解码 data3 0–17 + RingBuffer 覆盖最旧保新 + AcquisitionController + MockDevice/§5 复刻端到端，ctest 全绿，2026-08-18 合回 main）；**下一步 M3b：IPC 端点（QLocalServer 适配 CtrlPanel）** | 无（社区版无采集代码） | 无 |
| io | dev-io | worktrees/io/ | 待开工 | M4 CSV 转换器（ImporterCsv/ExporterCsv + ConverterRegistry + 回环测试） | MODULE_01 | 无（M1/M2/M3a 已合，可并行） |
| report | dev-report | worktrees/report/ | 待开工 | M5 CSV 报告器（IReporter + ReportRegistry + 金样测试） | MODULE_06 | 无（M1/M2/M3a 已合，可并行） |
| ui | dev-ui | worktrees/ui/ | 待开工 | M6 | MODULE_07, MODULE_08 | 等接口稳定 |

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

## 当前唯一阻塞点 → M1 core_model

其他 5 个模块都依赖 `core_model` 的头文件，所以 **M1 是现在唯一要开的线程**。
M1 全绿合并回 main 后，M2/M3/M4/M5 可并行开线程，M6 等接口稳定后开。
