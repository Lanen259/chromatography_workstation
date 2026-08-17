# MODULE_05 — Quantification Model（定量模型层）

> **状态：🔶 框架版 → ✅ 已回填（模板插件定量器 ✅，ChemClipse 定量算法与数据模型 ✅ 源码已确认）**
> 本仓库（OpenChrom 社区版）自带 4 个 PeakQuantifier 插件（内标分配/内标引用/补偿定量/内标提取）；
> 「浓度计算核心」位于 ChemClipse `xxd.quantitation.supplier.chemclipse`，**完整源码已抓取并确认**。
>
> ChemClipse 源码根：`.fetch/chemclipse-src/plugins/`。下文 Source 缩写：
> `[quantitation]`=`org.eclipse.chemclipse.chromatogram.xxd.quantitation`、`[qc.supplier]`=`org.eclipse.chemclipse.chromatogram.xxd.quantitation.supplier.chemclipse`、
> `[model]`=`org.eclipse.chemclipse.model`、`[xxd.model]`=`org.eclipse.chemclipse.xxd.model`、`[msd.model]`=`org.eclipse.chemclipse.msd.model`、
> `[converter]`=`org.eclipse.chemclipse.converter`、`[numeric]`=`org.eclipse.chemclipse.numeric`。

---

## 1. 本层职责

- 把峰面积（积分结果）换算成**浓度**（由 ChemClipse 定量器 + 校准数据库完成）
- 维护**校准曲线**（标样浓度 vs 峰面积，存于 QuantitationCompound → ResponseSignals）
- 支持**外标**（LINEAR/QUADRATIC/QUADRATIC_CHEMSTATION/AVERAGE，回归在 QuantitationCalculatorMSD）与**内标**（ISTD，面积比法在 PeakQuantitationCalculatorISTD）
- 定量结果挂到峰上（`IPeak.getQuantitationEntries()`）
- **本仓库实际实现的定量相关工作**：为峰**打内标（ISTD）标签**、**打定量引用**、**事后补偿校准误差**、**导出/报告定量结果**

---

## 2. 已确认的模型触点（✅ 源码确认）

### 2.1 IPeak 上的定量接口（Source: `[model]/src/org/eclipse/chemclipse/model/core/IPeak.java`）

| 触点 | 签名 | 说明 |
|---|---|---|
| 定量条目 | `getQuantitationEntries()` / `addQuantitationEntry(IQuantitationEntry)` / `removeQuantitationEntry` / `removeQuantitationEntries(List)` / `addAllQuantitationEntries(...)` / `removeAllQuantitationEntries()` | 定量结果列表，类型 `org.eclipse.chemclipse.model.quantitation.IQuantitationEntry` |
| 内标 | `getInternalStandards()` / `addInternalStandard(IInternalStandard)` / `addInternalStandards(List)` / `removeInternalStandard` / `removeInternalStandards()` | 类型 `org.eclipse.chemclipse.model.quantitation.IInternalStandard` |
| 定量器名 | `getQuantifierDescription()` / `setQuantifierDescription(String)` | 记录谁做的定量（报告列 QUANTIFIER 读它） |
| 定量引用 | `getQuantitationReferences()` / `addQuantitationReference(String)` / `addQuantitationReferences(List)` / `removeQuantitationReference` / `removeQuantitationReferences()` | 交叉引用：**一串字符串（ISTD 名称）**，定量时作为过滤器（见 §6.2 doQuantify） |

### 2.2 模板插件自带的 4 个定量器（✅ 本机源码，plugin.xml + peaks/ 目录）

扩展点：**`org.eclipse.chemclipse.chromatogram.xxd.quantifier.peakQuantifierSupplier`**
（Source: `openchrom/plugins/net.openchrom.xxd.process.supplier.templates/plugin.xml`）

| 扩展点 id | 定量器名称 | 实现类 | 设置类 | 职责 |
|---|---|---|---|---|
| `...peaks.standards.assigner` | Peak Standards Assigner (ISTD) | `StandardsAssigner` | `StandardsAssignerSettings` | 按 RT 窗口/离子把 ISTD 元数据挂到峰 |
| `...peaks.standards.referencer` | Peak Standards Referencer (ISTD) | `StandardsReferencer` | `StandardsReferencerSettings` | 给峰打「对哪个 ISTD 定量」的引用 |
| `...peaks.compensation.quantifier` | Peak Compensation Quantifier | `CompensationQuantifier` | `CompensationQuantifierSettings` | 用实测/期望浓度修正已有定量结果 |
| `...peaks.standards.extractor` | Peak Standards Extractor (ISTD) | `StandardsExtractor` | `StandardsExtractorSettings` | 从色谱图 miscInfo 头提取 ISTD |

> 4 个类全部继承 ChemClipse `AbstractPeakQuantifier`（`org.eclipse.chemclipse.chromatogram.xxd.quantitation.core`），
> 实现 `quantify(List<IPeak>, IPeakQuantifierSettings, IProgressMonitor)` 重载族。

### 2.3 ChemClipse 定量模块（✅ 全部源码已确认，见 §5-§8）

| ChemClipse 插件 | 角色 | 关键文件 |
|---|---|---|
| `[quantitation]` | 定量 API：`IPeakQuantifier`/`PeakQuantifier`/`PeakQuantifierSupplier`/`PeakQuantifierSupport`/`AbstractPeakQuantifier`/`AbstractPeakQuantitationCalculator`/`PeakQuantifierProcessTypeSupplier`/settings | `src/org/eclipse/chemclipse/chromatogram/xxd/quantitation/{core,settings}/` |
| `[qc.supplier]` | 算法实现：`core/PeakQuantifierISTD.java`（ISTD 定量器入口）、`internal/core/PeakQuantitationCalculatorISTD.java`（内标面积比）、`internal/calculator/QuantitationCalculatorMSD.java`（校准曲线回归）、`io/DatabaseSupport.java`（DB 读写封装）、`preferences/PreferenceSupplier.java` | 见 §6-§8 |
| `org.eclipse.chemclipse.chromatogram.xxd.quantitation.ui` | 定量器偏好页（strategy 等） | `ui/preferences/PreferencePage.java` |
| `org.eclipse.chemclipse.chromatogram.xxd.quantitation.supplier.chemclipse.ui` | 供应商 UI：wizard `QuantitationCompoundSupport.java`（从峰创建定量化合物）、PreferencePage | 见 §7.3 |
| `[model]` | 数据模型：`IQuantitationEntry`/`AbstractQuantitationEntry`、`IInternalStandard`/`InternalStandard`、`CalibrationMethod`、`QuantitationFlag`、`ResponseOption`、`WeightingOption`、`IQuantitationDatabase`/`QuantitationDatabase`、`IQuantitationCompound`/`AbstractQuantitationCompound`、`IResponseSignal(s)`/`ResponseSignal(s)`、`IQuantitationSignal(s)`/`QuantitationSignal(s)`、`IQuantitationPeak(s)`、`QuantitationSupport`、identification window 族 | `src/org/eclipse/chemclipse/model/quantitation/` |
| `[xxd.model]` | `QuantitationCompound`（从校准峰生成响应表，`calculateSignalTablesFromPeaks`） | `src/org/eclipse/chemclipse/xxd/model/quantitation/QuantitationCompound.java` |
| `[msd.model]` | `QuantitationPeakMSD`（校准峰包装） | `src/org/eclipse/chemclipse/msd/model/implementation/QuantitationPeakMSD.java` |
| `[converter]` | 定量 DB 读写框架：`QuantDBConverter`/`AbstractQuantDBReader`/`AbstractQuantDBImportConverter`/`AbstractQuantDBExportConverter`/`QuantDBSupplier`…（**具体 .ocq 格式实现在闭源插件**，见 §7.4） | `src/org/eclipse/chemclipse/converter/quantitation/` |
| `[numeric]` | 回归求解：`Equations.createLinearEquation/createQuadraticEquation` + `GaussJordan`（正规方程最小二乘） | `src/org/eclipse/chemclipse/numeric/equations/`、`numeric/internal/gaussjordan/` |

---

## 3. StandardsAssigner（内标分配）数据流（★ 重点，✅ 全部源码确认）

### 3.1 AssignerStandard 全部字段（Source: `.../model/AssignerStandard.java` + `model/AbstractSetting.java`）

| 字段 | 类型 | 默认 | 语义 |
|---|---|---|---|
| `positionStart` / `positionStop` | double | 0.0 | RT 窗口边界，**单位由 positionDirective 决定**（默认分钟）；内部存 double，用时换算为 ms |
| `positionDirective` | enum | RETENTION_TIME_MIN | `RETENTION_TIME_MIN`(min) / `RETENTION_TIME_MS`(ms) / `RETENTION_INDEX`(RI) |
| `name` | String | "" | 内标物质名（写入 InternalStandard.name；同时作为 equals/hashCode 键） |
| `concentration` | double | 0.0 | 内标已知浓度 |
| `concentrationUnit` | String | "" | 浓度单位（ppm/mg/L…） |
| `compensationFactor` | double | `IInternalStandard.STANDARD_COMPENSATION_FACTOR` | 响应因子/补偿因子（**默认 1.0，见 §5.3 确认**） |
| `tracesIdentification` | String | "" | 离子/波长串（如 `"104 103"`），空=全谱 TIC |

- 文本序列化（`AssignerStandards.extractSetting`，9 段，分隔符 `|`，条目间 `;`）：
  `positionStart | positionStop | name | concentration | concentrationUnit | compensationFactor | tracesIdentification | positionDirective`
  示例：`10.52 | 10.63 | Styrene | 10.5 | mg/L | 1.0 | 104 103`
  （Source: `AssignerStandards.java`、`StandardsAssignerListUtil.EXAMPLE_SINGLE`、`AbstractTemplateListUtil`）
- 校验规则（Source: `util/StandardsAssignerValidator.java`）：start≥0、stop>start、name 非空、concentration>0、unit 非空、factor>0。

### 3.2 RT 窗口匹配逻辑（Source: `peaks/StandardsAssigner.java` + `AbstractSetting.getRetentionTime*` + `util/TracesUtil.java`）

```
retentionTimeMs = resolve(setting.position*, directive, RetentionIndexMap)   // min→*60000，ms→原值，RI→RI映射查表
要求 startRetentionTime > 0 且 startRetentionTime < stopRetentionTime
对每个峰：
  1) isPeakMatch: peakModel.getRetentionTimeAtPeakMaximum() ∈ [start, stop]   // 用峰最高点RT判定，闭区间
  2) peak.getIntegratedArea() > 0                                            // 面积必须 >0，否则 warn "The peak area is 0."
  3) TracesUtil.isTraceMatch(peak, traces)：
       traces 空 → 恒 true（TIC）
       CSD → 恒 true
       MSD/WSD → 取 peakModel.getPeakMaximum() 扫描，traces 中每个离子/波长都必须存在才 true
```

### 3.3 结果写回峰（✅ 源码确认）

`assignPeak(...)` 命中后（Source: `StandardsAssigner.java` 第 96-105 行）：

```java
InternalStandard internalStandard = new InternalStandard(name, concentration, concentrationUnit, compensationFactor);
peak.addInternalStandard(internalStandard);
```

**注意：StandardsAssigner 只挂 ISTD 元数据，不计算浓度、不调用 addQuantitationEntry。**
浓度计算由 ChemClipse 定量器（`PeakQuantifierISTD`/`PeakQuantitationCalculatorISTD`，见 §6）结合峰上 ISTD 完成。

> 反向导出：`io/StandardsExport.java` 读 `peak.getInternalStandards()`，把每个 ISTD 写回 `AssignerStandard`
> （RT 窗口取峰起点±deltaLeft / 终点+deltaRight，traces 取峰最大扫描前 N 个），即「标注 ↔ 导出模板」闭环。

---

## 4. 另外 3 个定量器的职责与字段（✅ 源码确认）

### 4.1 StandardsExtractor（内标提取，从文件头）

- 设置字段（`StandardsExtractorSettings`）：仅 `concentrationUnit`（默认 `"ppm"`，Source: `PreferenceSupplier.DEF_STANDARDS_EXTRACTOR_CONCENTRATION_UNIT`）。
- 数据流（`StandardsExtractor.assignPeaks`）：
  1. 从色谱图 `getMiscInfo()` 用正则 `(IS:)(\d+)(:)(\d+\.?\d{0,5})` 提取 `referenceId` + `concentration`（IS 头格式：`IS:<referenceIdentifier>:<concentration>`，单位不入头）。
  2. 在峰的识别目标里找 `LibraryInformation.getReferenceIdentifier() == referenceId` 的目标。
  3. 命中且面积>0 → 用目标名称构造 `InternalStandard(name, concentration, unit, STANDARD_COMPENSATION_FACTOR)` → `peak.addInternalStandard(...)`。

### 4.2 StandardsReferencer（内标引用）

- 设置字段（`StandardsReferencerSettings` → `AssignerReference`，Source: `model/AssignerReference.java`）：
  `positionStart/positionStop/positionDirective`（继承）+ `internalStandard`（目标 ISTD 名，非空）+ `identifier`（源识别名，可空）。
  文本示例：`10.52 | 10.63 | Toluene (ISTD) | Styrene (Target or Empty)`（Source: `StandardsReferencerListUtil.EXAMPLE`）。
- 匹配语义（`StandardsReferencer.referencePeak`，三种形态）：
  - `start>0 且 start<stop`（RT 窗口有效）：
    - `identifier=""` → 窗口内**所有峰** `peak.addQuantitationReference(internalStandard)`；
    - `identifier≠""` → 窗口内**且峰识别目标名 == identifier** 的峰，加引用。
  - `start=0 且 stop=0`（`AbstractSetting.FULL_RETENTION_TIME`）→ 全色谱图按 identifier 匹配加引用。
- 结果：**只写 `peak.addQuantitationReference(String)`**，不写定量条目、不写内标对象。
  反向导出：`io/ReferencerExport.java` 读 `peak.getQuantitationReferences()` 生成 `AssignerReference`（identifier 取峰最佳识别名；无 identifier 时用峰 RT 窗口）。

### 4.3 CompensationQuantifier（补偿定量，✅ 唯一会在本仓库写 QuantitationEntry 的类）

- 设置字段（`CompensationQuantifierSettings` → `CompensationSetting`，Source: `model/CompensationSetting.java`）：
  `name`（目标物名）、`internalStandard`（内标名）、`expectedConcentration`（内标期望浓度）、`concentrationUnit`、`targetUnit`（空=匹配所有单位）、`adjustQuantitationEntry`（true=原地替换原条目，false=另加 `[adjusted]` 条目）。
  文本示例：`Substance A | Styrene | 1.0 | mg/L | false | ppm`（Source: `CompensationQuantListUtil.EXAMPLE_SINGLE`）。
- 数据流（`CompensationQuantifier.compensateQuantification`）：
  1. `getMeasuredConcentration`：找「识别目标名 == internalStandard」的峰，取其 `getQuantitationEntries()` 中 name==内标名 && (targetUnit 空 || 单位==concentrationUnit) 的条目的**浓度平均值**。
  2. `compensationFactor = 100 / measuredConcentration * expectedConcentration / 100 = expected / measured`。
  3. 对每个峰：取出 name==setting.name（可带单位过滤）的**既有定量条目**，逐个 `concentration * factor` 生成新条目：
     `new QuantitationEntry(name, group, adjustedConcentration, unit(保留原单位), area)`，并拷贝
     `chemicalClass` / `calibrationMethod` / `usedCrossZero` / `description` / `signal`（Source: `createAdjustedQuantitationEntry`）。
     描述追加 `"Adjusted with factor X based on <ISTD> [<unit>]"`。
  4. `adjustQuantitationEntry=true` 时先 `peak.removeQuantitationEntries(旧条目)` 再 `addAllQuantitationEntries(新条目)`；否则直接追加（新 name = 原名 + `" [adjusted]"`）。

> 意义：外标法定量结果若内标实测浓度偏离期望值，用 `期望/实测` 比值整体缩放目标物浓度，做基质补偿。

---

## 5. IQuantitationEntry / IInternalStandard 接口定义（✅ 完整源码）

> 两接口在 ChemClipse `org.eclipse.chemclipse.model`（源码已抓取）。旧文档 §5.1/§5.2 的「用法反推」全部升级为接口定义。
> 实现：`[model]/src/org/eclipse/chemclipse/model/implementation/QuantitationEntry.java` → `model/quantitation/AbstractQuantitationEntry.java`。

### 5.1 IQuantitationEntry 接口完整成员（Source: `[model]/.../model/quantitation/IQuantitationEntry.java`）

| 成员 | 签名 | 语义/实现细节 |
|---|---|---|
| 信号（legacy） | `double getSignal()` / `void setSignal(double)` | getSignal 返回 `signals` 首元素；**空列表时返回 `ISignal.TOTAL_INTENSITY`(0.0)**（Source: `AbstractQuantitationEntry`） |
| 信号列表 | `List<Double> getSignals()` / `void setSignals(List<Double>)` | 推荐用法；setSignal 即清空后 add 单个 |
| 名称 | `String getName()` | 化合物名（**ISTD 定量时=内标名**，见 §6.2） |
| 分组 | `String getGroup()` | 无 setter，仅构造传入（用于重复实验） |
| 化学类别 | `String getChemicalClass()` / `setChemicalClass(String)` | |
| 浓度 | `double getConcentration()` | 无 setter，构造传入 |
| 浓度单位 | `String getConcentrationUnit()` | 无 setter |
| 面积 | `double getArea()` | 无 setter |
| 校准方法 | `String getCalibrationMethod()` / `setCalibrationMethod(String)` | 存枚举 `.toString()`（如 `"ISTD"`/`"LINEAR"`） |
| 强制过零 | `boolean getUsedCrossZero()` / `setUsedCrossZero(boolean)` | **实现默认 true**（与化合物默认 crossZero=true 一致）；ISTD 定量器显式置 false |
| 描述 | `String getDescription()` / `setDescription(String)` / `appendDescription(String)` | append 去重，分隔符 `" \| "`（Source: `AbstractQuantitationEntry.DESCRIPTION_DELIMITER`） |
| 定量标记 | `QuantitationFlag getQuantitationFlag()` / `setQuantitationFlag(...)` | 默认 `NONE` |

- 构造器（Source: `[model]/.../model/implementation/QuantitationEntry.java`）：
  `QuantitationEntry(String name, double concentration, String concentrationUnit, double area)` 与
  `QuantitationEntry(String name, String group, double concentration, String concentrationUnit, double area)`。
- 除 signals/chemicalClass/calibrationMethod/usedCrossZero/description/quantitationFlag 外**无 setter → 不可变**。
- equals/hashCode 基于 area、calibrationMethod、chemicalClass、concentration、concentrationUnit、group、name、signals、usedCrossZero（不含 description/flag）。

### 5.2 QuantitationFlag 枚举全部值（Source: `[model]/.../model/quantitation/QuantitationFlag.java`）

| 值 | label() | shortcut() | 含义 |
|---|---|---|---|
| `NONE` | "" | "" | 无标记（默认） |
| `ZERO` | "0" | "Z" | 浓度为 0 |
| `NEGATIVE` | "< 0" | "N" | 浓度为负 |
| `LOWER_MIN_AREA` | "< Min Area" | "L" | 面积低于校准下限 |
| `HIGHER_MAX_AREA` | "> Max Area" | "H" | 面积高于校准上限 |

> 报告 writer（`ConfigurableReportWriter`）读 `.label()`。本算法路径（MSD 计算器）目前**不主动 setQuantitationFlag**（只写 description）。

### 5.3 IInternalStandard 接口完整成员（Source: `[model]/.../model/quantitation/IInternalStandard.java`）

| 成员 | 签名 | 语义 |
|---|---|---|
| 常量 | **`double STANDARD_COMPENSATION_FACTOR = 1.0d`** | **旧文档「≈1.0 反推」已确认精确值 =1.0**（Source: `IInternalStandard.java` L17） |
| 名称 | `String getName()` / `setName(String)` | 空字符串防 null（Source: `InternalStandard`） |
| 浓度 | `double getConcentration()` | |
| 浓度单位 | `String getConcentrationUnit()` | |
| 补偿因子 | `double getCompensationFactor()` | |
| 响应因子 | `double getResponseFactor()` | **= 1/compensationFactor；factor≤0 时返回 0.0**（Source: `InternalStandard.getResponseFactor`） |
| 化学类别 | `String getChemicalClass()` / `setChemicalClass(String)` | |

- 构造器（Source: `InternalStandard.java`）：`InternalStandard(name, concentration, concentrationUnit)`（factor 默认=STANDARD_COMPENSATION_FACTOR）与 `InternalStandard(name, concentration, concentrationUnit, compensationFactor)`。
- equals/hashCode 基于 **name、concentration、concentrationUnit**（**不含 compensationFactor**）。
- 旧文档 §3.1 反推的 compensationFactor 默认 1.0 ✅ 由 `STANDARD_COMPENSATION_FACTOR=1.0d` 确认。

### 5.4 报告出口（✅，保留旧 §5.3 结论）

- CSV：`ConfigurableReportWriter` 导出 QUANTIFIER（`getQuantifierDescription()`）、INTERNAL_STANDARD_*（取第一个 ISTD）、QUANTITATION_*（取第一个条目）、QUANTITATION_REFERENCE（取第一个引用）。
- Excel 模板：`ExcelTemplateReportWriter` 同名占位符。
- 模板 TSV 报告：`templates/core/ReportWriter.java` 的 CONCENTRATIONS 列聚合峰集合的定量条目。
- 峰表 UI：`templates.ui/.../swt/peaks/ExtendedPeakReviewUI.java` 的 `updateSelection` 显示峰首个定量条目 `name + 浓度 + 单位`。

---

## 6. ChemClipse 定量器与校准算法（✅）

### 6.1 定量器 API（Source: `[quantitation]/src/org/eclipse/chemclipse/chromatogram/xxd/quantitation/core/`）

- **IPeakQuantifier**（`IPeakQuantifier.java`）：4 个 `quantify(...)` 重载（单峰/峰列表 × 带设置/不带设置）+ `getLegacyIDs()`。`AbstractPeakQuantifier` 只实现 `getLegacyIDs()` 返回空列表。
- **PeakQuantifier**（静态门面）：`quantify(peak|peaks, [settings,] peakQuantifierId, monitor)` 按 id 从扩展点实例化 `IPeakQuantifier` 并调用；成功后若峰是 `IChromatogramPeak` 则 `chromatogram.setDirty(true)`。`getPeakQuantifierSupport()` 扫描扩展点填充 `PeakQuantifierSupport`；`getPeakQuantifier(id)` 实例化实现类。
- **扩展点**：`org.eclipse.chemclipse.chromatogram.xxd.quantifier.peakQuantifierSupplier`，属性 `id`/`description`/`peakQuantifierName`/`peakQuantifier`(class)/`peakQuantifierSettings`(class，可选)。
- **PeakQuantifierProcessTypeSupplier**（OSGi `IProcessTypeSupplier`，类别 `ICategories.PEAK_QUANTIFIER`）：把每个已注册定量器暴露为方法编辑器（Method Editor）步骤；`apply()` → 取选区峰 → `PeakQuantifier.quantify(peaks, settings, id, monitor)`；`matchesId` 兼容 legacy id。
- 设置：`IPeakQuantifierSettings`（继承 `IProcessSettings`）→ `AbstractPeakQuantifierSettings`（继承 `AbstractProcessSettings`）。**`PeakQuantifierSettings`/`PeakDatabaseSettings` 均为空标记类**（无字段，Source: `[qc.supplier]/.../settings/`）。

### 6.2 ISTD 内标法定量（✅ 公式确认）

**入口** `[qc.supplier]/.../core/PeakQuantifierISTD.java`（plugin.xml 注册 id=`org.eclipse.chemclipse.chromatogram.xxd.quantitation.supplier.chemclipse.peak.istd`，名 "Peak Quantifier (ISTD)"；legacy id=`...msd.quantitation.supplier.chemclipse.peak.istd`）。4 个 quantify 重载全部委托 `PeakQuantitationCalculatorISTD.quantify(peaks)`；另提供 `quantifySelectedPeak`/`quantifyAllPeaks(IChromatogramSelection)`。

**算法** `[qc.supplier]/.../internal/core/PeakQuantitationCalculatorISTD.java`：

```
quantify(List<IPeak> peaks):
  1) internalStandardPeaks = peaks 中 getInternalStandards() 非空的峰（ISTD 峰与目标峰必须在同一列表）
  2) 对每个峰 peakToQuantify：quantifyPeak(internalStandardPeaks, peakToQuantify)

quantifyPeak(istdPeaks, targetPeak):
  对每个 istdPeak（要求 isAreaValid：双方 getIntegratedArea() > 0）：
    对 istdPeak.getInternalStandards() 里每个 internalStandard（name 记 nameOfStandard）：
      若 doQuantify(targetPeak, nameOfStandard)：          // 引用过滤器
        A_ISTD = istdPeak.getIntegratedArea()             // 内标峰面积
        C_ISTD = internalStandard.getConcentration()      // 内标已知浓度
        unit   = internalStandard.getConcentrationUnit()
        factor = internalStandard.getCompensationFactor() // 补偿因子
        A_target = targetPeak.getIntegratedArea()         // 目标峰面积
        C_target = (C_ISTD / A_ISTD) * A_target * factor  // ★ 内标面积比公式
        entry = new QuantitationEntry(nameOfStandard, C_target, unit, A_target)
        entry.setSignal(ISignal.TOTAL_INTENSITY)          // 0.0（TIC）
        entry.setCalibrationMethod(CalibrationMethod.ISTD.toString())  // "ISTD"
        entry.setUsedCrossZero(false)
        entry.setChemicalClass(istd chemicalClass)
        targetPeak.addQuantitationEntry(entry)
```

- **面积比公式（确认）**：`C_target = (A_target / A_ISTD) × C_ISTD × compensationFactor`（代码按 `(C_ISTD/A_ISTD)×A_target` 求值，数学等价）。ISTD 浓度直接参与比值，**不使用校准数据库、不做回归**。
- **引用过滤器**（Source: `[quantitation]/.../core/AbstractPeakQuantitationCalculator.doQuantify`）：`targetPeak.getQuantitationReferences()` **为空 → 全量定量**；非空 → 仅当引用列表**含 nameOfStandard** 才定量。即 StandardsReferencer 打的引用就是这里的白名单。
- **面积校验**：`isAreaValid`（同文件）= 目标峰与内标峰面积都 >0。
- 条目 **name = 内标名**（不是独立的目标物名）；一个内标峰上挂多个 ISTD 会生成多条条目；一条 ISTD 会作用于所有被引用它的峰。
- `quantifySelectedPeak`/`quantifyAllPeaks`：ISTD 峰从整个色谱图（MSD/CSD 分别取 `getPeaks()` 过滤）收集，目标峰取选择集（MSD/CSD）。

### 6.3 校准曲线回归：QuantitationCalculatorMSD（✅ 算法，调用方 ❓）

**算法** `[qc.supplier]/.../internal/calculator/QuantitationCalculatorMSD.java`（实现 `IQuantitationCalculatorMSD`）：

```
calculateQuantitationResults(peak, compound):
  峰面积必须 > 0
  QuantitationSupport(peak) 校验积分与化合物一致：
    compound.isUseTIC() → validateTIC() → 单条 TIC 条目（signal=TOTAL_INTENSITY，area=峰面积）
    否则 XIC：
      selectedIons = compound.getQuantitationSignals().getSelectedSignals()  // isUse=true 的信号
      validateXIC(selectedIons)（TIC 积分也算通过）
      对每个 ion：
        totalSignal = 峰提取质谱总信号；abundance = 该离子丰度
        若峰是 TIC 积分 → percentageIonAbundance = (1/totalSignal)*abundance
                            条目面积 = 峰面积 × percentageIonAbundance   // 只有 TIC 一部分属于该离子
        否则            → 条目面积 = QuantitationSupport.getIntegrationArea(ion)  // XIC 直接取离子积分面积
        → getQuantitationEntry(ion, compound, 面积)

getQuantitationEntry(signal, compound, integratedArea):
  name/chemicalClass/concentrationUnit 来自 compound；isCrossZero = compound.isCrossZero()
  minResponse/maxResponse = responseSignals.getMin/MaxResponseValue(signal)   // 该信号下校准点响应范围
  switch compound.getCalibrationMethod():
    LINEAR:   eq = responseSignals.getLinearEquation(signal, isCrossZero)
              C = eq.calculateX(integratedArea)
              超出 min/max 响应只写 description（"< min"/"> max"），仍给浓度
    QUADRATIC / QUADRATIC_CHEMSTATION:
              eq = responseSignals.getQuadraticEquation(signal, isCrossZero)
              超出 min/max → 只写 description，C 保持 0（不外推）
              否则 C = eq.calculateX(integratedArea)
    AVERAGE:  factor = responseSignals.getAverageFactor(signal, isCrossZero)
              C = factor × integratedArea
    ISTD:     break（不在此路径计算）
  entry = new QuantitationEntry(name, C, unit, integratedArea)
  entry.setSignal(signal); setCalibrationMethod(calibrationMethod.toString())
  setUsedCrossZero(isCrossZero); setChemicalClass(chemicalClass); setDescription(description)
```

- **回归实现（✅ 普通最小二乘，无加权）**：
  - `ResponseSignals.getLinearEquation(signal, isCrossZero)`（Source: `[model]/.../model/quantitation/ResponseSignals.java`）：
    取该 signal 的所有 `ResponseSignal(concentration, response)` 点；`isCrossZero=true` 时**先插入点 (0,0)**；调
    `Equations.createLinearEquation(points)`。
  - `Equations.createLinearEquation(IPoint[])`（Source: `[numeric]/.../numeric/equations/Equations.java`）：
    拟合 `f(x)=ax+b`，解正规方程 `AtA·x = AtB`（`GaussJordan.AtA/AtB` + `solve`，部分主元高斯消元，Numerical Recipes 算法）。
  - `getQuadraticEquation`：拟合 `f(x)=ax²+bx+c`，同样的正规方程最小二乘（Source: `Equations.java` L193-232）。
  - 求逆（浓度从响应反算）：
    - `LinearEquation.calculateX(y) = (y−b)/a`，**a==0 返回 NaN**（Source: `LinearEquation.java`）。
    - `QuadraticEquation.calculateX(y) = (c−y)/(−0.5×(b+√(b²−4a(c−y))))`，分母 0 时返回 0（Source: `QuadraticEquation.java`）。
  - `getAverageFactor`（Source: `ResponseSignals.java`）= 平均浓度 / 平均响应 `x̄/ȳ`（无点或 ȳ==0 返回 0）；过零同样插入 (0,0) 点再算均值。
- **QuantitationSupport**（Source: `[model]/.../model/quantitation/QuantitationSupport.java`）：从 `peak.getIntegrationEntries()` 构建 `Map<signal, area>`；`isTotalSignalIntegrated()` = 仅含 `ISignal.TOTAL_INTENSITY` 一个条目；`validateXIC` 要求积分信号集**包含**全部定量离子（TIC 积分恒过）。
- ⚠️ **调用方不在开源代码库**：全仓库仅 `QuantitationCalculatorMSD`/`IQuantitationCalculatorMSD` 自引用，无开源调用点 → 它服务于**闭源**的「定量数据库」峰定量器（`PeakDatabaseQuantifier` 类不在本仓库）。其**算法本身可溯源**，但「谁调用、怎么匹配化合物/峰」仍 ❓。
- 相关偏好（Source: `[qc.supplier]/.../preferences/PreferenceSupplier.java`）：`P_QUANTITATION_STRATEGY` ∈ {NONE("None"), RT("Retention Time"), REFS("References"), NAME("Name")}、RT 偏差 ±0.5 min、RI 偏差 ±10 index、`P_SELECTED_QUANTITATION_DATABASE`。

### 6.4 枚举全部值与语义

**CalibrationMethod**（Source: `[model]/.../model/quantitation/CalibrationMethod.java`）：

| 值 | label() | 分组 | 算法（§6.3） |
|---|---|---|---|
| `LINEAR` | "Linear" | 外标 | 最小二乘 `f(x)=ax+b`，超范围仍外推（仅告警） |
| `QUADRATIC` | "Quadratic (Classic)" | 外标 | 最小二乘 `f(x)=ax²+bx+c`，超范围不计算（C=0） |
| `QUADRATIC_CHEMSTATION` | "Quadratic (ChemStation - experimental)" | 外标 | 同二次（`isQuadraticMethod()` 含两者） |
| `AVERAGE` | "Average" | 外标 | `C = (x̄/ȳ) × area` |
| `ISTD` | "Internal Standard" | **仅内标** | `getInternalCalibrationOptions()` 只含 ISTD；算法见 §6.2 |

> 外标选项 = {AVERAGE, LINEAR, QUADRATIC, QUADRATIC_CHEMSTATION}（`getExternalCalibrationOptions`）；注释明确 "ISTD is used for internal standards only. All other are used for external calibration."

**ResponseOption**（Source: `[model]/.../model/quantitation/ResponseOption.java`）：`COMPENSATION_FACTOR`("Compensation Factor") / `RESPONSE_FACTOR`("Response Factor")。当前全代码库仅定义、无算法引用（`InternalStandard` 的 `getResponseFactor()`=1/补偿因子 是唯一响应因子语义）。

**WeightingOption**（Source: `[model]/.../model/quantitation/WeightingOption.java`）：`STANDARD`("Standard") / `ONE_OVER_X`("1/x")。**全代码库仅定义文件自引用，无任何调用点 → 校准回归未实现加权**（grep 全仓库仅 1 个文件命中）。

---

## 7. 定量数据库与校准点存储（✅）

### 7.1 模型层次（Source: `[model]/.../model/quantitation/`）

```
IQuantitationDatabase (extends Set<IQuantitationCompound>)   // HashSet 实现 QuantitationDatabase
  ├─ File getFile()/setFile、String getConverterId()/setConverterId()   // 由 DB converter 设置
  ├─ String getOperator()/setOperator、getDescription()/setDescription
  ├─ List<String> getCompoundNames()
  ├─ IQuantitationCompound getQuantitationCompound(String name)   // 可能返回 null
  └─ boolean containsQuantitationCompund(String name)            // 原拼写如此

IQuantitationCompound (extends Serializable, Comparable)
  ├─ name / chemicalClass / concentrationUnit
  ├─ IRetentionTimeWindow getRetentionTimeWindow()   // RetentionTimeWindow: retentionTime(ms) + ±allowedNegative/PositiveDeviation
  ├─ IRetentionIndexWindow getRetentionIndexWindow() // RetentionIndexWindow: retentionIndex + ±偏差
  ├─ boolean isUseTIC()/setUseTIC
  ├─ IQuantitationSignals getQuantitationSignals()   // 定量离子（相对响应），TreeSet<IQuantitationSignal>
  ├─ IResponseSignals getResponseSignals()           // 校准点 List<IResponseSignal>
  ├─ CalibrationMethod getCalibrationMethod()/setCalibrationMethod
  ├─ boolean isCrossZero()/setUseCrossZero           // 默认 true
  ├─ List<IQuantitationPeak> getQuantitationPeaks()  // 标样峰（含已知浓度）
  ├─ void calculateSignalTablesFromPeaks()           // 由标样峰生成上面两个表
  └─ void setQuantitationSignalTIC()                 // 只留 TIC 定量信号
```

- 默认值（Source: `AbstractQuantitationCompound.java`）：`useTIC=true`、`calibrationMethod=LINEAR`、`useCrossZero=true`。
- **校准点单元** `ResponseSignal(signal, concentration, response)`（Source: `ResponseSignal.java`）：signal 为离子 m/z 或 `0.0`(TIC)；`response` 可改（`setResponse`），concentration 不可改。`IResponseSignals extends List<IResponseSignal>`（同一 signal 可有多个点）。
- **定量离子单元** `IQuantitationSignal`（Source: `IQuantitationSignal.java` + `AbstractQuantitationSignal.java`）：`signal` / `relativeResponse`（默认 `ABSOLUTE_RELATIVE_RESPONSE=100.0`）/ `uncertainty`（默认 0.0）/ `isUse()`；`QuantitationSignals` 是 TreeSet，`getSelectedSignals()` 返回 `isUse()==true` 的信号列表。
- **校准峰包装** `IQuantitationPeak`（Source: `IQuantitationPeak.java` + `AbstractQuantitationPeak.java`）：`concentration` / `concentrationUnit` / `referencePeak`(IPeak)。实现 `QuantitationPeakMSD(IPeakMSD, concentration, concentrationUnit)`（Source: `[msd.model]/.../msd/model/implementation/QuantitationPeakMSD.java`）。

### 7.2 校准点如何生成：calculateSignalTablesFromPeaks（✅，Source: `[xxd.model]/.../xxd/model/quantitation/QuantitationCompound.java`）

```
清空 getQuantitationSignals() 与 getResponseSignals()
若 isUseTIC()：createTablesTIC
  对每个 IQuantitationPeak：
    concentration = quantitationPeak.getConcentration()
    QuantitationSupport(peak).validateTIC() 通过
    response = QuantitationSupport.getIntegrationArea(TOTAL_INTENSITY)   // TIC 积分面积
    首峰先 add QuantitationSignal(TOTAL_INTENSITY=0.0, ABSOLUTE_RELATIVE_RESPONSE=100.0)
    add ResponseSignal(0.0, concentration, response)
否则：createTablesXIC
  对每个峰（须 IPeakMSD）：
    提取质谱中丰度>0 的所有离子作为 selectedIons；validateXIC 通过
    对每个 ion：
      percentageIonAbundance = abundance / totalSignalMassSpectrum
      首峰 add QuantitationSignal(ion, percentageIonAbundance)
      response = QuantitationSupport.getIntegrationArea(ion)
      TIC 积分 → add ResponseSignal(ion, concentration, response × percentageIonAbundance)
      XIC 积分 → add ResponseSignal(ion, concentration, response)
```

> 即：**compound → 一组标样峰（浓度已知）→ 每个峰的积分面积作为响应 → 生成 (signal, concentration, response) 校准点表**。
> 一个化合物同一 signal 下有多点 → 供 §6.3 回归取点。TIC 模式固定一个 signal(0.0) 一条回归。

### 7.3 化合物创建（UI wizard，✅ Source: `[qc.supplier].ui/.../ui/internal/wizards/QuantitationCompoundSupport.java`）

```
create(peak, name, concentration, concentrationUnit, chemicalClass):
  RT/RI 取峰最高点扫描；new QuantitationCompound(name, unit, retentionTime)
  setChemicalClass
  若 RT>0：RT 窗口 allowedNegative/PositiveDeviation = 偏好(默认0.5 min) × MINUTE_CORRELATION_FACTOR(60000) → ms
  若 RI>0：RI 窗口偏差 = 偏好(默认10 index)
  加 QuantitationPeakMSD(peak, concentration, unit) 到 getQuantitationPeaks()
  setQuantitationSignalTIC()

merge(db, peak, name, concentration):   // 给已有化合物追加一个标样点
  取 db.getQuantitationCompound(name)，再 add QuantitationPeakMSD(peak, concentration, 化合物单位)
```

### 7.4 数据库读写（✅ 框架 / ❓ 文件格式）

- 扩展点：**`org.eclipse.chemclipse.converter.quantitationDatabaseSupplier`**（Source: `[converter]/plugin.xml`，含 schema；属性 id/description/filterName/fileExtension/fileName/importConverter/exportConverter/isExportable/isImportable/magicNumber/…）。
- 门面 `QuantDBConverter`（Source: `[converter]/.../converter/quantitation/QuantDBConverter.java`）：
  - `convert(File, monitor)`：遍历所有 supplier 依次尝试，首个成功的返回 IQuantitationDatabase。
  - `convert(File, converterId, monitor)`：按 id 实例化 `IQuantDBImportConverter` 导入。
  - `convert(File, db, converterId, monitor)`：按 id 实例化 `IQuantDBExportConverter` 导出。
  - 默认 id `org.eclipse.chemclipse.xxd.converter.supplier.chemclipse.quantitationDatabaseSupplier`；默认文件名 **`QuantitationDatabase.ocq`**、扩展名 `*.ocq`。
- 抽象/接口（`AbstractQuantDBReader`(空)、`AbstractQuantDBImportConverter`、`AbstractQuantDBExportConverter`、`IQuantDBReader`、`IQuantDBImportConverter`、`IQuantDBExportConverter`、`IQuantDBSupplier`/`QuantDBSupplier`、`IQuantDBConverterSupport`/`QuantDBConverterSupport`）。
- 封装 `DatabaseSupport`（Source: `[qc.supplier]/.../io/DatabaseSupport.java`）：`load()` = 读偏好里选中的 DB 文件 → `QuantDBConverter.convert`；`save(db)` = 用 db.getFile()+getConverterId() 导出。
- **❓ 具体 `.ocq` 序列化格式**：本开源仓库只有扩展点声明，**没有实现该扩展点的插件**（grep `extends AbstractQuantDBImportConverter` 仅命中抽象类自身）。真正的读写在闭源插件 `org.eclipse.chemclipse.xxd.converter.supplier.chemclipse`（产品二进制），格式未知。

---

## 8. 定量结果回写峰（addQuantitationEntry 完整数据流，✅）

```
方法编辑器（PeakQuantifierProcessTypeSupplier.apply）
  → PeakQuantifier.quantify(peaks, settings, id, monitor)
    → IPeakQuantifier 实例（扩展点实例化）
      ├─ ISTD 路径：PeakQuantifierISTD.quantify → PeakQuantitationCalculatorISTD
      │     收集 ISTD 峰 → doQuantify 引用过滤 → 面积比公式 → new QuantitationEntry
      │     → setSignal/setCalibrationMethod("ISTD")/setUsedCrossZero(false)/setChemicalClass
      │     → peakToQuantify.addQuantitationEntry(entry)          // ★ 挂到峰
      └─ DB 路径（闭源）→ QuantitationCalculatorMSD.calculateQuantitationResults
            按化合物回归 → getQuantitationEntry(...) → setSignal/calibrationMethod/crossZero/chemicalClass/description
            → 返回 List<IQuantitationEntry>（由闭源调用方 addQuantitationEntry）
  成功后：若峰为 IChromatogramPeak → chromatogram.setDirty(true)   // 标记保存
报告/UI 读 peak.getQuantitationEntries() + getInternalStandards() + getQuantifierDescription()
```

- `IPeak.addQuantitationEntry` 等全部方法在 `[model]/.../model/core/IPeak.java`（§2.1）。
- `ISignal.TOTAL_INTENSITY = 0.0d`（Source: `[model]/.../model/core/ISignal.java`），TIC 信号键恒为 0.0。

---

## 9. 数据流位置（更新版）

```text
（1）内标打标：                 （2）内标引用：                （3）浓度计算（ChemClipse, ✅）：
StandardsAssigner 按RT窗口/离子   StandardsReferencer 按RT窗口     PeakQuantifierISTD / PeakQuantitationCalculatorISTD
  命中峰  ────────────────►      或识别名 → addQuantitationReference  ────►  C_target = (A_target/A_ISTD)×C_ISTD×factor
  addInternalStandard(ISTD)         "该峰对 Toluene 定量"                 （或用校准DB的 QuantitationCalculatorMSD，闭源调用）
       │                                 │                                        │
       └────────── StandardsExtractor（从文件头 IS: 提取，等价入口）              ▼
（4）补偿（可选）：CompensationQuantifier                                   IPeak.addQuantitationEntry(entry)
  factor = expected / measured  → 缩放既有条目浓度，重建 QuantitationEntry        │
                                                                            ▼
（5）报告：CSV / Excel / 模板 TSV / 峰表 UI 读 getQuantitationEntries + getInternalStandards + getQuantifierDescription
```

---

## 10. Qt/C++ 移植要点（core_processing 定量子模块，✅ 公式已确认）

- **数据模型**（对应 Java IQuantitationEntry / IInternalStandard，接口定义见 §5）：

```cpp
enum class QuantitationFlag { NONE=0, ZERO, NEGATIVE, LOWER_MIN_AREA, HIGHER_MAX_AREA };
// label(): "" / "0" / "< 0" / "< Min Area" / "> Max Area"

struct QuantitationEntry {
    QString compound;             // getName()（ISTD 定量时=内标名！）
    QString group;                // getGroup()（构造传入，不可改）
    double  concentration = 0.0;  // 无 setter
    QString concentrationUnit;    // 无 setter
    double  area = 0.0;           // 无 setter
    QString chemicalClass;        // setChemicalClass
    QString calibrationMethod;    // setCalibrationMethod（"LINEAR"/"QUADRATIC"/.../“ISTD”）
    bool    usedCrossZero = true; // 注意默认 true（Java 实现即 true）
    QString description;          // appendDescription 用 " | " 分隔、去重
    QVector<double> signals;      // 空列表时 getSignal()==0.0(TIC)
    QuantitationFlag flag = QuantitationFlag::NONE;
};

struct InternalStandard {
    QString name;                 // equals 键之一
    double  concentration = 0.0;
    QString concentrationUnit;
    double  compensationFactor = 1.0;  // STANDARD_COMPENSATION_FACTOR = 1.0（已确认）
    QString chemicalClass;
    double  responseFactor() const { return compensationFactor > 0.0 ? 1.0/compensationFactor : 0.0; }
};
```

- **峰上挂载**：`QVector<QuantitationEntry>`, `QVector<InternalStandard>`, `QVector<QString> quantitationReferences`（ISTD 名白名单）, `QString quantifierDescription`。
- **ISTD 定量（✅ 公式确认，直接照搬）**：
  ```
  对每个挂有 ISTD 的峰（面积>0）作为内标峰：
    对其中每个 InternalStandard（name）：
      若 quantitationReferences 为空 或 包含 name：        // doQuantify
        若 目标峰面积>0：
          C = (istd.getConcentration() / istdPeak.area) * targetPeak.area * istd.compensationFactor;
          entry = { compound: name, concentration: C, unit: istd.unit,
                    area: targetPeak.area, signal: [0.0], calibrationMethod: "ISTD",
                    usedCrossZero: false };
  ```
- **外标校准曲线（✅ 最小二乘，无加权）**：
  - 校准点：`struct CalibrationPoint { double signal; double concentration; double response; }`，按 signal 分组（TIC 恒为 0.0）。
  - `isCrossZero` 时在回归前**插入 (0,0) 点**。
  - 线性：正规方程最小二乘 `f(x)=ax+b`；反算 `C=(area−b)/a`，a==0 → NaN。
  - 二次：`f(x)=ax²+bx+c`；反算 `C=(c−area)/(−0.5×(b+√(b²−4a(c−area))))`。
  - Average：`factor = mean(浓度)/mean(响应)`，`C = factor × area`。
  - 超 min/max 响应：LINEAR 仍外推（记描述）；QUADRATIC 不计算（C=0，记描述）。
  - `WeightingOption`（Standard / 1/x）在 Java 算法中**未实现**——Qt 侧同样先不做加权。
- **校准数据库**：`QMap<QString /*compound*/, QMap<double /*signal*/, QVector<CalibrationPoint>>>` + 自写最小二乘即可起步（对应 QuantitationDatabase → QuantitationCompound → ResponseSignals）。
- **RT 窗口匹配算法**（照搬 AssignerStandard 语义，✅）：见旧 §3.2——窗口存 double + PositionDirective{min,ms,RI}，统一换算 ms；匹配峰最高点 RT ∈ [start,stop] 闭区间且面积>0；离子过滤按 TracesUtil 语义。
- **补偿定量**（✅ 公式已确认）：`factor = expected / measured`；measured 为 ISTD 峰上匹配条目浓度平均；目标条目 `concentration × factor` 重建，可选「替换原条目」或「追加 [adjusted]」。
- **策略接口**：`AbstractQuantifier { virtual Result quantify(PeakList&, Settings&) }`；子类 ISTD / External（回归）/ Normalization。
- **定量 DB 文件**：Java 用 `.ocq`（闭源格式 ❓）；Qt 侧建议自定 JSON/SQLite 存 `compound → 校准点`，不必兼容 .ocq。

---

## 11. 证据登记表

| # | 结论 | Source | 状态 |
|---|---|---|---|
| Q-A | IPeak 定量条目/内标/定量引用接口 | .fetch/sources/model/IPeak.java（本仓库）+ `[model]/src/org/eclipse/chemclipse/model/core/IPeak.java` | ✅ |
| Q-B | IQuantitationEntry/IInternalStandard 所在包 | 同上（import org.eclipse.chemclipse.model.quantitation.*）| ✅ |
| Q-C | 定量模块定位 | .fetch/chemclipse_tree.json (xxd.quantitation*) | ✅ 存在性 |
| Q-D | 定量算法实现 | chemclipse.xxd.quantitation.supplier.chemclipse（✅ 源码已抓取，见 Q-Q/Q-R/Q-S） | ✅ |
| Q-E | 模板插件 4 个定量器扩展点 id + 类/设置绑定 | templates/plugin.xml（peakQuantifierSupplier 扩展）| ✅ |
| Q-F | AssignerStandard 全字段 + RT 窗口匹配（峰最高点∈闭区间、面积>0、离子过滤） | model/AssignerStandard.java、model/AbstractSetting.java、peaks/StandardsAssigner.java、util/TracesUtil.java、util/StandardsAssignerValidator.java | ✅ |
| Q-G | ISTD 写入峰：`addInternalStandard(InternalStandard)`，不写定量条目 | peaks/StandardsAssigner.java、peaks/StandardsExtractor.java | ✅ |
| Q-H | 定量引用写入：`addQuantitationReference(ISTD名)`（窗口/识别名/全谱三种形态） | peaks/StandardsReferencer.java | ✅ |
| Q-I | 补偿定量：factor=expected/measured，重建 QuantitationEntry（组/单位/面积保留，附加 adjusted 说明） | peaks/CompensationQuantifier.java | ✅ |
| Q-J | IQuantitationEntry 字段用法（name/concentration/unit/area/group/chemicalClass/calibrationMethod/usedCrossZero/description/signal/flag） | CompensationQuantifier + CSV/Excel 报告 writer + QuantRatioClassifier + ReportWriter | ✅ 用法 → 接口见 Q-N |
| Q-K | IInternalStandard 字段用法（name/concentration/unit/compensationFactor/chemicalClass + STANDARD_COMPENSATION_FACTOR） | StandardsAssigner/StandardsExtractor/StandardsExport + 报告 writer | ✅ 用法 → 接口见 Q-P |
| Q-L | 校准曲线/回归/ISTD 面积比公式 | chemclipse_tree.json → supplier.chemclipse（internal/calculator、core/PeakQuantifierISTD）| ✅ 见 Q-Q/Q-R/Q-S |
| Q-M | 定量器调用入口（方法编辑器/向导 UI）与设置 UI 流 | chemclipse_tree.json → xxd.quantitation.ui、supplier.chemclipse.ui | ✅ 见 Q-W/Q-V |
| Q-N | IQuantitationEntry 接口完整成员（getSignals/setSignals + 全部 getter/setter；usedCrossZero 默认 true；signal 空列表返回 TIC；appendDescription 用 " \| " 去重） | `[model]/src/org/eclipse/chemclipse/model/quantitation/IQuantitationEntry.java` + `AbstractQuantitationEntry.java` + `[model]/.../model/implementation/QuantitationEntry.java` | ✅ |
| Q-O | QuantitationFlag 枚举 5 值（NONE/ZERO/NEGATIVE/LOWER_MIN_AREA/HIGHER_MAX_AREA，含 label 与 shortcut） | `[model]/.../model/quantitation/QuantitationFlag.java` | ✅ |
| Q-P | IInternalStandard 完整接口：**STANDARD_COMPENSATION_FACTOR=1.0d（精确）**、getResponseFactor=1/因子（factor≤0→0）、构造器、equals 基于 name/concentration/unit | `[model]/.../model/quantitation/IInternalStandard.java` + `InternalStandard.java` | ✅ |
| Q-Q | ISTD 定量：**C_target=(C_ISTD/A_ISTD)×A_target×factor**；条目 name=内标名；signal=TIC、calibrationMethod="ISTD"、usedCrossZero=false；doQuantify 引用白名单过滤；isAreaValid 双方面积>0；ISTD 峰来自同一列表/整图 | `[qc.supplier]/.../internal/core/PeakQuantitationCalculatorISTD.java` + `[quantitation]/.../core/AbstractPeakQuantitationCalculator.java` + `[qc.supplier]/.../core/PeakQuantifierISTD.java` + `[qc.supplier]/plugin.xml` | ✅ |
| Q-R | QuantitationCalculatorMSD：TIC/XIC 分支、XIC 百分比面积、LINEAR/QUADRATIC/QUADRATIC_CHEMSTATION/AVERAGE、crossZero、min/max 响应、描述文案 | `[qc.supplier]/.../internal/calculator/QuantitationCalculatorMSD.java` + `[model]/.../model/quantitation/QuantitationSupport.java` | ✅ |
| Q-S | 回归=正规方程最小二乘（无加权）；crossZero 插入(0,0)；LinearEquation.calculateX=(y−b)/a(a=0→NaN)；Quadratic.calculateX 公式；Average 因子=x̄/ȳ | `[model]/.../model/quantitation/ResponseSignals.java` + `[numeric]/.../numeric/equations/Equations.java` + `LinearEquation.java` + `QuadraticEquation.java` + `[numeric]/.../numeric/internal/gaussjordan/GaussJordan.java` | ✅ |
| Q-T | CalibrationMethod 5 值（LIN/QUAD/QUAD_CHEM/AVG/ISTD，外标=前4、内标=仅ISTD）、ResponseOption 2 值、WeightingOption 2 值 | `[model]/.../model/quantitation/CalibrationMethod.java` + `ResponseOption.java` + `WeightingOption.java` | ✅ |
| Q-U | WeightingOption 全代码库仅定义无引用 → 加权未实现 | grep `WeightingOption`（仅 1 文件命中） | ✅ |
| Q-V | 定量数据库模型：IQuantitationDatabase(Set+文件/操作员/描述)、IQuantitationCompound（RT/RI 窗口、useTIC、quantitationSignals、responseSignals、quantitationPeaks、crossZero 默认 true）、ResponseSignal(signal,concentration,response)、QuantitationSignal、QuantitationPeakMSD；校准点由 calculateSignalTablesFromPeaks 从标样峰生成（TIC/XIC 两种） | `[model]/.../model/quantitation/{IQuantitationDatabase,QuantitationDatabase,IQuantitationCompound,AbstractQuantitationCompound,IResponseSignals,ResponseSignal(s),IQuantitationSignal(s),QuantitationSignal(s),IQuantitationPeak,AbstractQuantitationPeak,RetentionTimeWindow,RetentionIndexWindow}.java` + `[xxd.model]/.../xxd/model/quantitation/QuantitationCompound.java` + `[msd.model]/.../msd/model/implementation/QuantitationPeakMSD.java` + `[qc.supplier].ui/.../ui/internal/wizards/QuantitationCompoundSupport.java` | ✅ |
| Q-W | 定量器注册/调用链：扩展点 peakQuantifierSupplier → PeakQuantifier 静态门面（成功后 setDirty）→ PeakQuantifierSupport/Supplier → PeakQuantifierProcessTypeSupplier（方法编辑器 IProcessTypeSupplier）；设置类为空标记 | `[quantitation]/.../core/{IPeakQuantifier,PeakQuantifier,PeakQuantifierSupport,PeakQuantifierSupplier,PeakQuantifierProcessTypeSupplier,AbstractPeakQuantifier}.java` + `[qc.supplier]/plugin.xml` + `[qc.supplier]/.../settings/PeakQuantifierSettings.java` | ✅ |
| Q-X | 定量 DB 读写框架：扩展点 quantitationDatabaseSupplier、QuantDBConverter 门面、默认 .ocq 文件名与 converter id、Abstract 读写类；**具体 .ocq 序列化格式在闭源插件（本仓库无实现扩展）** | `[converter]/.../converter/quantitation/*` + `[converter]/plugin.xml` + `[qc.supplier]/.../io/DatabaseSupport.java` | ✅ 框架 / ❓ 格式 |
| Q-Y | QuantitationCalculatorMSD 的调用方（数据库峰定量器 PeakDatabaseQuantifier）不在开源代码库中 | grep `QuantitationCalculatorMSD\|calculateQuantitationResults`（无外部调用点） | ❓ |
| Q-Z | ISignal.TOTAL_INTENSITY=0.0d（TIC 信号键）；MINUTE_CORRELATION_FACTOR=60000.0 | `[model]/.../model/core/ISignal.java` + `IChromatogramOverview.java` | ✅ |
