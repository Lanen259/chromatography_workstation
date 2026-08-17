# OpenChrom 逆向工程 · 主控文档（MASTER — AI 必读）

> ## ⚠️ 使用规则（本文件是所有 AI 会话的第一读取文件）
> **任何 AI 在本仓库执行任务前，必须先完整读取本文件全文**（不得跳过、不得只读摘要）。
> 读取后按 **第 5 节「标准启动流程」** 核对当前状态，再开始对应操作。
> 完成任何一步后，必须更新 **第 8 节「主状态表」** 与 **第 9 节「下一步」**。

---

## 1. 项目是什么

对 **OpenChrom**（Java / Eclipse RCP 色谱数据系统）做**源码级逆向分析**，产出一套可供后续 AI 开发**自研 CDS（色谱数据系统 / 色谱工作站）**参考的知识库。

- 工作目录：`E:\My_project\QT\openchrom`
- 项目性质：**只分析，不修改任何 OpenChrom 业务代码**
- 交付物：`docs/reverse-engineering/` 下的 11 份逆向文档 + 本主控文件
- 文档语言：中文

## 2. 最终目标（任务书的 10 个总问题）

1. OpenChrom 的总体架构是什么？
2. 从启动到打开一个色谱数据文件，完整调用链？
3. Chromatogram / Signal / Peak 在代码中如何表示？
4. 数据从文件导入到 UI 显示，经过哪些模块？
5. 基线校正、峰检测、峰积分、定性、定量由哪些模块负责？
6. 插件机制是什么？
7. 数据分析模块之间如何通信？
8. UI 与分析引擎如何解耦？
9. 数据文件如何保存和加载？
10. 哪些部分适合自研 CDS 参考，哪些不应照搬？

## 3. 阶段与交付物（共 11 份文档）

| 文档 | 主题 | 当前状态 | 详见 |
|---|---|---|---|
| 00_repository_map.md | 仓库地图 + 分析规范（总纲） | 🔶 框架版 | 所有文档的证据格式、结论分级定义于此 |
| 01_startup_and_runtime.md | 启动与运行时 | 🔶 框架版 | — |
| 02_data_flow.md | 数据流 | 🔶 框架版 | — |
| 03_data_model.md | 核心数据模型 | 🔶 框架版 | — |
| 04_signal_processing_inventory.md | 信号处理清单 | 🔶 框架版 | — |
| 05_peak_engine.md | 峰引擎（重点） | 🔶 框架版 | — |
| 06_quantification.md | 定量 | 🔶 框架版 | — |
| 07_ui_architecture.md | UI 架构 | 🔶 框架版 | — |
| 08_plugin_architecture.md | 插件架构 | 🔶 框架版 | — |
| 09_testing_and_validation.md | 测试与验证 | 🔶 框架版 | — |
| 10_openchrom_reverse_engineering_summary.md | 逆向总结 | 🔶 框架版 | 结论汇总 + 可借鉴/不建议借鉴 |
| 11_workstation_composition.md | 工作站组成分析（功能模块→组件映射） | 🟢 已回填 | 供后续开发 agent 使用 |

### 3.1 模块级逆向文档（Qt/C++ 复刻主轴，见 `module/` 子目录）

按「数据从哪来 → 报告如何生成」的 6 个核心中间层组织，**数据模型 + 算法 + 工作流**优先：

| 文档 | 主题 | 当前状态 |
|---|---|---|
| module/MODULE_00_pipeline_map.md | 模块级总图（7 段链路 + 6 层 + 源码边界 + Qt 工程映射） | 🟢 已回填 |
| module/MODULE_01_raw_data.md | 原始数据层（导入导出） | 🟢 已回填 |
| module/MODULE_02_chromatogram_model.md | 色谱模型层（IChromatogram/Selection） | 🟢 已回填 |
| module/MODULE_03_processing_pipeline.md | 处理管线层（方法/滤波/执行引擎/设置序列化） | 🟢 已回填 |
| module/MODULE_04_peak_model.md | 峰模型层（IPeak*/一阶导数算法）★ | 🟢 已回填 |
| module/MODULE_05_quantification_model.md | 定量模型层（ISTD/校准曲线） | 🟢 已回填 |
| module/MODULE_06_report_model.md | 报告模型层（字段全集/生成器/核心框架） | 🟢 已回填 |
| module/MODULE_07_ui.md | UI 架构层（RCP 壳/方法编辑器/事件链） | 🟢 已回填 |
| module/MODULE_08_plugin_extension.md | 插件与扩展点架构层（37 扩展点注册表） | 🟢 已回填 |
| module/MODULE_09_peak_shape_model.md | 峰形模型层（PeakModel 数学/PeakBuilder） | 🟢 已回填 |
| module/MODULE_10_signal_filters.md | 信号预处理滤波器族（S-G 平滑/基线扣除/归一化/零点/MSD 滤波） | 🟢 已回填 |
| module/MODULE_11_identification.md | 定性鉴定层（IIdentificationTarget/相似度算法/NIST·MassBank） | 🟢 已回填 |
| module/MODULE_12_classifier_calculator.md | 分类器与计算器（ratios/Durbin-Watson/噪声/保留指数/峰分辨率） | 🟢 已回填 |

> ✅ 证据来源说明（2026-08-17 更新）：ChemClipse 核心源码已全量本地化到 `.fetch/chemclipse-src/plugins/`（228 插件），本表所有模块文档的 ✅ 证据均可直接在本机 `openchrom/plugins/`（社区 68 插件）+ `.fetch/chemclipse-src/plugins/`（ChemClipse 核心）双源核对。

## 4. 铁律（任何 AI 不得违反，来自任务书）

- ❌ **不修改业务代码**、不重构、不新增功能、不为方便理解改源码
- ❌ 不凭文件名猜测功能（`XxxProcessor` 不代表它是处理器，必须读实现）
- ❌ 不把 README / 文档当源码事实（只作线索，再进源码验证）
- ❌ **不编造调用关系**（画调用图前必须逐个确认边）
- 每个重要结论必须附：`Source: 文件路径 / 类 / 方法`
- 无法确认的内容必须标 **「⚠️ 背景假设（待验证）」** 或 **「❓ 待验证」**
- 05_peak_engine.md 尤其要区分「✅ 源码确认」与「⚠️ 根据代码推测」
- 本文档 **第 4 节（铁律）与第 2 节（目标）不得修改**；其余章节可在状态变化时更新

## 5. 标准启动流程（每次新会话，按顺序执行）

1. **完整读取本文件**（强制，见文件头）。
2. 读取 `memory/` 索引（MEMORY.md 已自动加载；必要时读相关记忆文件）。
3. 核对仓库现状：
   - `git status` / 根目录 `ls`：确认源码是否已就位
   - 若源码未就位：本仓库是空仓库，origin 指向 `git@github.com:OpenChrom/openchrom.git`，需先克隆（建议浅克隆 `--depth 1`）——**克隆属外部网络动作，先征询用户**
4. 读取 `00_repository_map.md`，确定当前处于哪个 Phase、哪些结论已回填。
5. 按文档序号顺序执行（00 → 01 → … → 10），或按用户指定的子集续接。
6. 任务结束前：更新本文件第 8 节状态表 + 第 9 节下一步。

## 6. 标准分析工作流（针对单个 Phase）

1. **定位**：用 Grep/Glob/CodeGraph（若仓库有 `.codegraph/`）找到相关源码文件，禁止只靠文件名下结论。
2. **取证**：读出关键类与方法体，记录真实调用关系（发起方法 → 被调方法 → 参数 → 返回）。
3. **回填**：把结论写回对应文档，格式如下，并同步 `00` 的「证据登记表」：

```text
### 结论标题
- 结论：<一句话>
- 状态：✅ 源码确认 / ⚠️ 推测
Source:
- 文件: <相对路径>
- 类: <ClassName>
- 方法: <methodName()>
- 证据说明: <关键行或调用关系>
```

4. **标注**：无法确认的一律标「❓ 待验证」，不猜、不填假值。
5. **交叉引用**：结论涉及其他 Phase 时，在对应文档（02/03/04/05/06/07/08）互链。
6. **大范围搜索时**：可派 Explore 子代理并行，但主结论必须由执行 AI 亲自核对后再写入。

## 7. 文档地图（谁依赖谁）

```text
00（总纲 + 证据格式）
 ├─ 01 启动/插件加载 ─┬─ 08 插件架构
 ├─ 02 数据流 ────────┼─ 03 数据模型
 ├─ 04 信号处理清单 ──┼─ 05 峰引擎 ─┐
 ├─ 06 定量 ──────────┤             ├─ 10 总结
 ├─ 07 UI ────────────┼─ 09 测试 ───┘
 └─ 08 插件架构 ──────┘

module/（Qt/C++ 复刻主轴，跨 Phase 分层）
 MODULE_00 总图
  ├─ MODULE_01 Raw Data ──┬─ MODULE_02 Chromatogram Model
  ├─ MODULE_03 Processing ─┼─ MODULE_04 Peak Model ★
  ├─ MODULE_05 Quantification
  └─ MODULE_06 Report Model
```

- **核心数据模型**（03）被 02/04/05/06/07 引用，最先回填。
- **处理清单**（04）与**峰引擎**（05）是分析重点。
- **模块级系列**（module/）以「数据模型 + 算法 + 工作流」为轴，是自研 CDS 的首选入口。
- 每份文档头部的「当前状态」徽章，是全仓库完成度的单一事实来源。

## 8. 主状态表（每次执行后更新）

| 文档 | 状态 | 最近更新 | 回填进度（✅/⚠️/❓） | 备注 |
|---|---|---|---|---|
| 00_repository_map.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | 源码已就位（社区插件 68 个，见 11） |
| 01_startup_and_runtime.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 02_data_flow.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 03_data_model.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 04_signal_processing_inventory.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 05_peak_engine.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | 重点 Phase |
| 06_quantification.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 07_ui_architecture.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 08_plugin_architecture.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 09_testing_and_validation.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | — |
| 10_openchrom_reverse_engineering_summary.md | 🔶 框架版 | 2026-08-12 | 0 / 0 / 0 | 待前序回填 |
| 11_workstation_composition.md | 🟢 已回填 | 2026-08-15 | 多 ✅ / 少 ⚠️❓ | 见 README 第 3 节 |
| module/MODULE_00_pipeline_map.md | 🟢 已回填 | 2026-08-16 | 多 ✅ / 少 ❓ | Qt 工程模块映射表已加入（§8） |
| module/MODULE_01_raw_data.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | CDF/TSD/匹配器/8 格式全链 + ocx 原生格式（ZIP/VERSION entry/版本化 Reader_1001~1502/SCANPROXIES 懒加载）✅（R-A~R-AN） |
| module/MODULE_02_chromatogram_model.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | IChromatogram/Selection + IScan/ISignal 接口定义 + normalize 公式/双 NORMALIZATION_BASE + UpdateNotifier→EventBroker ✅（C-A~C-AG） |
| module/MODULE_03_processing_pipeline.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | 引擎循环/ProcessEntry/ProcessExecutionContext/.ocm 双格式/GCMethod ✅，wrapper=IProcessTypeSupplier ✅，设置序列化 SettingsClassParser/JSONSerialization/Profile ✅（P-A~P-AI） |
| module/MODULE_04_peak_model.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | 峰检测基类（Threshold 映射/连续 3 步/峰顶首次过零/NORMALIZATION_BASE=100000）+ 基线/SNIP/PeakType 13 值/积分 ✅（PK-A~PK-BK） |
| module/MODULE_05_quantification_model.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | ISTD 面积比公式/校准曲线 GaussJordan 回归/CalibrationMethod 5 值/STANDARD_COMPENSATION_FACTOR=1.0 ✅（Q-A~Q-Z）；.ocq 格式与 DB 调用方仍闭源 ❓ |
| module/MODULE_06_report_model.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | 核心框架基类/Delimiter/时间因子/UI 消费链 ✅（RP-A~RP-Y）；设置 JSON 落盘归 process 模块 |
| module/MODULE_07_ui.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | 方法编辑器/设置表单 Adapters.adapt/事件链 UpdateNotifier→IEventBroker/工作台 ✅（UI-A~UI-L+新行） |
| module/MODULE_08_plugin_extension.md | 🟢 已回填 | 2026-08-16 | 多 ✅ / 少 ❓ | 37 扩展点/136 块/三域 matcher ✅（EP-A~EP-O） |
| module/MODULE_09_peak_shape_model.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | 峰形模型（拐点=|斜率|最大段/切线峰宽/tailing 双路径/PeakBuilder 三变体）✅（PK-BI~…） |
| module/MODULE_10_signal_filters.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | 滤波器接口/S-G 全链/baselinesubtract/9 简单滤波/MSD 8 族 + splitter 6 族（按极性/分辨率/类型分组建 referenced 子图）/centroiding/scan 套件 10+ 子滤波 ✅（SF-A~SF-AW）；VSD/ISD 域细节 ❓ |
| module/MODULE_12_classifier_calculator.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | ratios 4 分类器/Durbin-Watson/WNC/molpeak(+内置库)/峰分辨率 Rs=2Δt/Σw/噪声 Stein·Dyson/分段来源/AMDIS RI/UI 预览流 全链 ✅（CC-A~CC-AI）；仅 AMDIS 自动标定 UI 编排 ❓ |
| module/MODULE_11_identification.md | 🟢 已回填 | 2026-08-17 | 多 ✅ / 少 ❓ | IIdentificationTarget 全链（ITarget 标记接口→AbstractTarget→AbstractIdentificationTarget）+ 相似度三公式 + NIST 进程 + MassBank + DatabasesCache/ComparatorCache 缓存 + Best Match 排序 ✅（ID-A~ID-AD）；ComparatorCache:65 疑 bug ❓ |

状态含义：🔶 框架版（仅结构，无源码结论）｜🟡 分析中｜🟢 已回填｜✅ 完成

## 9. 下一步（当前阻塞与行动项）

**当前状态：✅ 模块系列 13 份全部深挖完成并经主会话核对（含收尾轮：MODULE_10 splitter/centroiding/scan 套件、MODULE_11 ITarget 链/缓存、MODULE_12 分段/molpeak/UI 预览均已解）。全部 27 个 .md 已同步到 Qt 工程 `E:\My_project\QT\chromatography_workstation\docs\openchrom-reverse-engineering\`。逆向阶段收官，可开始 Qt 工程搭建。**

**已完成（2026-08-17 ChemClipse 深挖轮，12 子 agent + 收尾 3 子 agent + 主会话逐一核对）**：抓取 chemclipse 全量源码（codeload 67.6MB，228 插件本地化）→ 并行深挖 13 份模块文档。已核对通过：MODULE_01 ocx（ZIP/VERSION/版本化 Reader/SCANPROXIES 懒加载）、MODULE_02 模型接口（IScan/ISignal 定义、normalize 公式、双 NORMALIZATION_BASE 1000/100000）、MODULE_03 引擎+设置（applyProcessEntries 循环、ProcessEntry=processorId+profile→JSON、.ocm 旧 ZIP/新 MTH 魔数+GZIP+ObjectStream、SettingsClassParser/JSONSerialization/Profile）、MODULE_04 峰检测基类（Threshold 映射 0.0005~0.5、连续 3 步、峰顶首次过零、NORMALIZATION_BASE=100000）+ 基线/SNIP/PeakType 13 值/积分、MODULE_05 定量（ISTD 面积比公式、校准曲线 GaussJordan 回归、CalibrationMethod 5 值）、MODULE_06 报告核心（基类只含 validate、Delimiter、时间因子 1000/60000/3600000）、MODULE_07 UI 内部（方法编辑器、SettingsUI Adapters.adapt、UpdateNotifier→IEventBroker→DataUpdateSupport、工作台）、MODULE_09 峰形模型（拐点=|斜率|最大段、切线峰宽、tailing 双路径、PeakBuilder 三变体）、MODULE_10 滤波器族（S-G 法方程 QR/LU 卷积系数 + splitter 6 族 + scan 套件）、MODULE_11 鉴定（Alfassi/熵/距离三公式、NIST 进程、MassBank、ITarget 链、DatabasesCache）、MODULE_12 分类器/计算器（ratios 4 分类器、D-W、峰分辨率 Rs=2Δt/Σw、Stein/Dyson 噪声、AMDIS 线性 RI、分段、UI 预览）。（每份均已抽查源码行，如 ISTD L102、SNIP L37、DurbinWatson L108、PeakResolution L71、Threshold L59、ITarget L31、DatabasesCache L108、AnalysisSupport L45）

**剩余 ❓（全部低优先，均不影响 Qt 起步；后两项为闭源，开源无法取证）**：MODULE_10 VSD/ISD 域细节、MODULE_11 ComparatorCache:65 疑 bug、MODULE_12 AMDIS 自动标定 UI 编排、`.ocq` 序列化格式（闭源）、`PeakQuantifier` 数据库调用方（闭源）。

**下一步（用户准备开启）**：在自研 Qt 工程 `E:\My_project\QT\chromatography_workstation` 按 [MODULE_00 §8](module/MODULE_00_pipeline_map.md) 的 Qt 模块映射（core_model/core_processing/acq/io/report/ui）并行开发；逆向文档已镜像到 `docs/openchrom-reverse-engineering/`。

## 10. 维护规则（谁来改本文件）

- **允许更新**：第 5/8/9 节（状态、下一步）——由每次执行任务的 AI 在任务结束时更新。
- **允许增补**：第 7 节（文档地图）、第 3 节（交付物表状态徽章）。
- **禁止修改**：第 1/2/4 节（项目定位、目标、铁律）。
- 每次更新追加或修改「最近更新」日期列，不重写历史事实。
