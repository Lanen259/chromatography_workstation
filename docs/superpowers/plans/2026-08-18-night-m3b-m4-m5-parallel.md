# 2026-08-18 整夜并行调度计划 —— M3b(acq IPC) + M4(io CSV) + M5(report CSV)

> **前置（主控已完成，2026-08-18 晚）**：M1(core_model) + M2(core_processing) + M3a(acq) 已合 main（全量构建 0 error + ctest 3/3 绿）；四个 worktree（acq/io/report/ui）已同步到 `4f9db79`。M3b 传输方案已定案：**QLocalServer + acq 加 QtNetwork 例外**（CLAUDE.md 铁律 §2 + 契约 §8 规则 5）。
>
> **调度方式**：每个 worktree 开一个独立新对话（会话），粘贴对应小节指令（方框内整段文本），会话自主开发 + 自测 + commit，**全绿后停下等主控审查合并，禁止自行 `git merge main`**。
>
> **三条铁律**：① 模块间只通过接口说话；② 依赖只许朝下，core 系默认只许 QtCore（acq IPC 端点唯一 QtNetwork 例外）；③ 独立测试验证（自己的 ctest 全绿才允许合并）。

---

## 线程 1 —— M3b acq IPC 端点（worktree: `worktrees/acq`）

```text
你是独立开发线程，任务是实现 M3b（acq IPC 端点），在 worktrees/acq（分支 dev-acq）完成。

【背景】M1(core_model) + M2(core_processing) + M3a(acq 实时反控接收链) 已合入 main。
M3a 的 HwRealtimeReceiver（契约 §4.3b）是传输无关的：逐条 receive(d1,d2,d3) → 解码为事件。
M3b 给它接一个跨进程传输：QLocalServer 命名管道适配真实 CtrlPanel.exe。
主控已定案：acq 允许 QtNetwork（见 CLAUDE.md 铁律 §2 例外 + 契约 §8 规则 5），
但必须封装在传输适配器之后，HwRealtimeReceiver 等公开接口头保持 QtCore 纯。

【第一步】worktree 已同步到 main（如提示落后则 git merge main）。

【开工前必读，按序】
1. 仓库根 CLAUDE.md（三条铁律，注意 §2 的 acq QtNetwork 例外）
2. 开发治理规范.md
3. 编码风格规范.md
4. 00_工程骨架与模块契约.md 的 §0 / §4.3b / §8 规则 5 / §5 / §6
5. PROJECT.md §1 / §2（acq 行：M3b 下一步）
6. docs/protocol/HWSendData_实时反控协议.md §5（IPC 端点归属）+ §2 命令表（data3 0–17）

【实现范围】只许改 project/chromatography_workstation/acq/ 下的 include/ src/ tests/ CMakeLists.txt

【设计要点（自研，无逆向参考；先想清楚再动手）】
- 新增公开接口头 include/acq/transport.h（QtCore 纯）：
  class ITransport { public: virtual ~ITransport() = default;
                     virtual void send(long d1, long d2, long d3) = 0; };
  —— 语义：把命令三元组喂给下游（接 HwRealtimeReceiver::receive）。
- 实现 TransportLocalServer（src/，唯一允许 #include <QtNetwork> 的地方）：
  QLocalServer 监听命名管道（如 "cdsw.ctrlpanel"），收到字节流 → 按帧解析出 (d1,d2,d3) → 调 ITransport::send。
  帧格式建议（可调整，但必须写进设计文档）：每帧 1 字节魔数 0xCD + 3 × qint64 小端 (d1,d2,d3)，共 25 字节。
- 若对帧格式/生命周期有不确定，先在 docs/superpowers/specs/ 落一份设计文档（M3a 的 spec 是现成模板）。
- 测试（tests/）：
  · TransportLocalServer 回环：QLocalServer 起服务 + QLocalSocket 客户端连上发帧 → 断言 transport 收到的三元组正确（含边界：短帧丢弃、坏魔数丢弃）。
  · 端到端：socket → TransportLocalServer → HwRealtimeReceiver → 断言 sigChannelSample / sigAcquisitionStarted 等信号。
  · QLocalServer 可在 QCoreApplication（QTest 自带）下用，无需 QWidgets。

【验收】本模块 ctest 全绿；公开接口头（include/acq/*.h）零 QtNetwork，QtNetwork 只进 transport 实现；
D1 编译通过；协议语义与 §4.3b 一致（不改冻结签名）。

【ctest 注意】Windows 上先 export PATH="/d/Program_flies/qt_creat/APP/5.14.2/mingw73_64/bin:$PATH"
再 cd build && ctest --test-dir build --output-on-failure -R acq

【完成后】1. git add 本模块文件，提交格式 feat/test/acq?: ... 2. 更新 PROJECT.md §2 acq 行
3. 写 docs/memory/acq.md（追加 M3b）4. 返回一句话汇报：改了哪些文件、ctest 结果、有无卡点

【禁止】自行 git merge main、改契约 §4.3b 冻结签名、碰其他模块和 main-owned 文件。
```

---

## 线程 2 —— M4 io CSV 转换器（worktree: `worktrees/io`）

```text
你是独立开发线程，任务是实现 M4（io 导入导出转换器），在 worktrees/io（分支 dev-io）完成。

【背景】M1(core_model) + M2(core_processing) + M3a(acq) 已合入 main。
你的契约 §4.4 只依赖 core_model 的 Chromatogram，已可用。io worktree 已同步 main。

【第一步】worktree 已同步到 main（如提示落后则 git merge main）。

【开工前必读，按序】
1. 仓库根 CLAUDE.md（三条铁律）
2. 开发治理规范.md
3. 编码风格规范.md
4. 00_工程骨架与模块契约.md 的 §0 / §4.4（你的接口签名，冻结）/ §5 / §6
5. PROJECT.md §1 / §2（io 行：M4 CSV 转换器）
6. docs/openchrom-reverse-engineering/module/MODULE_01_raw_data.md（CSV 语义参考）

【实现范围】只许改 project/chromatography_workstation/io/ 下的 include/ src/ tests/ CMakeLists.txt
- §4.4 的 include/io/converters.h 逐字实现：ImportResult / IChromatogramImporter / IChromatogramExporter / ConverterRegistry
- 起步实现 ImporterCsv + ExporterCsv + ConverterRegistry 注册，一个格式一个类

【CSV 格式建议（自研，可调整但写进设计/测试）】
- 默认两列：retentionTimeMs,intensity（第一行可选表头，支持 '#' 注释行）
- 导入：解析 → 填 Chromatogram 的 signalPoints + RT 网格（scanDelayMs/scanIntervalMs 从首末点推断）+ name(文件名) + converterId("io.csv")
- 导出：Chromatogram → 同格式 CSV（scanDelayMs/scanIntervalMs 落到元数据行或注释）

【测试】回环测试绿：构造固定 Chromatogram → ExporterCsv 导出 → ImporterCsv 再导入 → 信号逐点一致；
边界：空文件、坏行/缺列、表头变体、含注释。空 chromatography 与多段点均覆盖。

【验收】本模块 ctest 全绿：回环测试绿；D1 编译通过；纯 QtCore（QFile/QTextStream 即可），禁止 QtWidgets

【ctest 注意】Windows 上先 export PATH="/d/Program_flies/qt_creat/APP/5.14.2/mingw73_64/bin:$PATH"
再 cd build && ctest --test-dir build --output-on-failure -R io

【完成后】1. git add 本模块文件，提交格式 feat/test/io?: ... 2. 更新 PROJECT.md §2 io 行
3. 写 docs/memory/io.md 4. 返回一句话汇报：改了哪些文件、ctest 结果、有无卡点

【禁止】自行 git merge main、改契约 §4.4 冻结签名、碰其他模块和 main-owned 文件。
```

---

## 线程 3 —— M5 report CSV 报告器（worktree: `worktrees/report`，指令由协调会话已发，此处留档）

```text
（协调会话 2026-08-18 已发完整指令，要点复述）：
§4.5 的 include/report/reporters.h 逐字实现：ReportData / IReporter / ReportRegistry；
起步 ReporterCsv（固定 ReportData → CSV：表头 + 峰表 + 定量结果行）；
ReportRegistry 注册，availableFormats/reporterFor 按格式名分发；
金样测试（固定 ReportData → CSV 与 golden 文本逐字节比对，含空峰/无定量边界）；
参考 MODULE_06_report_model.md；纯 QtCore；ctest -R report 全绿。
```

---

## 早间验收清单（主控用）

1. 每分支 `git log` 核对提交、`git status` 确认干净。
2. 每模块 `ctest -R <模块>` 全绿（先 `export PATH` 加 Qt bin）。
3. 主 checkout 全量构建 0 error + ctest 3/3（并入新模块后 4/5/6 个套件）。
4. 按顺序合回：acq(M3b) / io(M4) / report(M5) —— 每合一分支先解决 PROJECT.md 冲突（各线程会改 §2 各自行）。
5. 更新 PROJECT.md §1/§2 + 决策日志；更新 docs/memory/。
