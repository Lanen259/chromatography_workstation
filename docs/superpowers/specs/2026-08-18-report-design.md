# M5 report —— 报告生成器设计文档（2026-08-18）

> 里程碑 M5：报告生成器。接口签名冻结于契约 §4.5；设计依据 `docs/openchrom-reverse-engineering/module/MODULE_06_report_model.md`。本文档记录**非冻结点**的设计决策（ReportData 三个公开类型逐字照契约实现）。

## 1. 范围

- 只交付 `project/chromatography_workstation/report/` 下的 `include/ src/ tests/ CMakeLists.txt`，外加主控授权的三份文档：本设计文档 + `PROJECT.md §2` + `docs/memory/report.md`。
- 起步实现 **ReporterCsv**（CSV 报告器）+ ReportRegistry。后续照 OpenChrom `net.openchrom.*.report.supplier.*` 的写法加 PDF/Excel 等，一个格式一个类。
- 纯 QtCore，零 UI（report 是 core 系模块，禁止 QtWidgets；.ui 要求只适用 ui 模块）。
- 依赖：core_model 的 `Peak` + core_processing 的 `QuantEntry`，只 include 两个模块的 `include/` 接口头，禁止 include 别家 `src/`。

## 2. 组件与数据流

```
ReportData（样品名/方法名/采集时间/峰表/定量结果）
   │ IReporter::generate(ReportData, filePath)     ← 由 ReportRegistry.reporterFor(format) 取实例
   ▼
ReporterCsv::generate → 打开文件 → 写三个 CSV 节 → bool
   │
   ▼
CSV 报告文件（表头节 + 峰表节 + 定量节，LF 行尾，UTF-8）
```

- `include/report/reporters.h`：`ReportData` / `IReporter` / `ReportRegistry`（契约 §4.5 逐字实现）。
- `src/ReporterCsv.h/.cpp`：`IReporter` 的 CSV 实现。
- `src/ReportRegistry.cpp`：单例 + 内置注册（沿用 core_processing `Registry` 的工厂表模式，见 §3）。

## 3. 设计决策（非冻结点）

| 决策 | 定案 | 理由 |
|---|---|---|
| 头文件编译增补 | `reporters.h` 补 `#include <QtCore/qdatetime.h>` | `ReportData::acquiredAt` 是 `QDateTime` 值成员，接口头需完整类型；不引 QtCore 之外，不改任何冻结签名。与 core_processing 头文件「编译必要增补」注释先例同模式。 |
| CSV 布局 | **分区式**（2026-08-18 用户拍板）：表头 name/value 节 → 空行 → 峰表 → 空行 → 定量表 | 对应任务原文「表头 + 峰表 + 定量结果行」；人读性好、diff 清晰，峰表/定量表各自列数一致。 |
| 行尾 | **LF**：`QFile` 以 `QIODevice::WriteOnly`（不带 `Text` 标志）打开，`QTextStream` 写 `\n` 原样 | 不带 Text 标志避免 Windows 自动转 `\r\n`，保证金样逐字节比对跨平台确定。 |
| 编码 | QTextStream 默认 UTF-8 | 字段含中文也可字节确定。 |
| 数值格式 | 强度/面积/浓度一律 `QString::number(x, 'f', 6)`（6 位定点）；RT 输出**分钟** = `ms / 60000.0` | `QString::number` 与 locale 无关（恒 `.` 小数点），跨机字节一致；RT 分钟对齐 MODULE_06 的 `MINUTE_CORRELATION_FACTOR`。 |
| 时间格式 | `acquiredAt.toString(Qt::ISODate)` | 构造时用 `Qt::LocalTime` 规格（默认）→ Qt5 下输出 `2026-08-18T10:30:00` 不带时区后缀，任何机器一致。 |
| 峰排序 | 峰表**按 apexRTMs 升序**排序后编号 1..N | 对齐 MODULE_06：OpenChrom `PeakRetentionTimeComparator(SortOrder.ASC)` 排序后重编号。 |
| 定量顺序 | 定量表保持 ReportData 给定顺序 | 定量条目本身是结果列表，顺序有意义（对齐峰时由调用方保证）。 |
| 空边界 | peaks 空 / quantEntries 空 → 各自表头行**仍打印**、无数据行 | 文件仍可解析、节结构稳定；MODULE_06 打印表头由 `printResultsHeader` 控制（默认 true）。 |
| RFC4180 引号 | 字段含 `,` / `"` / `\n` 时加双引号包裹，内部 `"` 翻倍 | MODULE_06 §3.3：CSVFormat.RFC4180 引号规则。 |
| 注册表 | `ReportRegistry::instance()` 首次调用注册内置 ReporterCsv；`registerReporter(factory)` 探针取 `formatName()` 作键；`availableFormats()` 返回排序键；`reporterFor()` 未注册返回 `nullptr` | 沿用 core_processing `Registry` 模式（QHash 工厂表 + 探针取 id + 首调注册），开闭原则：新增格式 = 新增实现类 + 注册一行。 |
| 所有权 | `reporterFor()` 每次返回新实例（堆对象），调用方负责 `delete` | 与 core_processing `createXxx()` 语义一致。 |
| 错误处理 | `generate()` 打开/写文件失败 → 返回 `false`，不抛异常 | 对应 OpenChrom `AbstractChromatogramReportGenerator.validate()` 的失败语义（null/不可建/不可写 → 失败）。 |

## 4. CSV 字节规格（金样比对基准）

```
Sample Name,<sampleName>
Method Name,<methodName>
Acquired At,<acquiredAt ISO>
Number of Peaks,<N>
Number of Quantitation Entries,<M>
<空行>
Peak Number,Apex RT (min),Start RT (min),Stop RT (min),Height,Area
<每峰一行：编号,apex/60000,start/60000,stop/60000,height,area>   （按 apexRTMs 升序）
<空行>
Component,Apex RT (min),Area,Concentration,Unit
<每定量条目一行：componentName,apex/60000,area,concentration,unit>
```

数值列全部 `'f',6` 定点；RT 为分钟；`<空行>` 即一个 `\n`。

## 5. 测试计划（QTest，一文件一组，QTEST_APPLESS_MAIN）

1. **golden 金样**：固定 ReportData（峰**乱序传入**验证排序）→ `ReporterCsv::generate` 到 `QTemporaryDir` → 读回文件字节与 `tests/data/golden_report.csv`（CMake 宏注入目录）逐字节 `QCOMPARE`。
2. **空峰 + 无定量**：空 ReportData → 与预期字面量比对（表头节 + 峰表头行 + 定量表头行，无数据行）。
3. **有峰无定量**：峰表有数据行，定量表仅表头行。
4. **RFC4180 引号**：样品名/组分名含 `,`、`"`、换行 → 读回比对预期转义输出。
5. **注册表分发**：`availableFormats()` 含 `"CSV"`；`reporterFor("CSV")` 非空且 `formatName()=="CSV"`、`fileSuffix()==".csv"`；`reporterFor("PDF")==nullptr`。
6. **失败路径**：不可写/非法路径 → `generate` 返回 `false`。

> 金样文本**手写推导**（不用生成器输出回抄），保证不出现「金样由同一段代码生成」的循环验证。测试数据取干净值：如 apex 75000ms=1.25min、60000ms=1.00min、90000ms=1.50min、52000.0/104000.0，便于人工复核。

## 6. 验收映射

| DoD | 达成 |
|---|---|
| D1 编译 | `cmake --build build --target report`（含 report_tests） |
| D2 测试 | `ctest --test-dir build --output-on-failure -R report` 全绿 |
| D3 注册 | cdsw_add_module 自动 `add_test(report_tests)` + `ctest -N` 核对 |
| D4 纪律 | 只 include `include/core_model/` `include/core_processing/`，纯 QtCore，不碰其他模块 src |
| D6 文档 | 更新 `PROJECT.md §2` + 写 `docs/memory/report.md` |
