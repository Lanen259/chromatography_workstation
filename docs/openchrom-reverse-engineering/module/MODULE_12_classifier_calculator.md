# MODULE_12 — 分类器（Classifier）与计算器（Calculator）算法族（QC / 谱学计算）

> **状态：🟡 分析中（分类器扩展点/接口 ✅、ratios 4 分类器 ✅、Durbin-Watson ✅、WNC/molpeak 质谱分类器 ✅、molpeak 内置谱库与匹配 ✅、计算器扩展点/接口 ✅、峰分辨率 ✅、Stein/Dyson 噪声 ✅、ChromatogramSegmentation 分段来源 ✅、AMDIS 保留指数 ✅、分类器结果 UI 预览流 ✅；仅 AMDIS 自动标定 UI 编排 ❓）**
> 服务于自研 CDS Qt 工程 QC 与谱学计算模块（`core_processing` 下分类/计算子模块）。
> 严格区分：✅ 源码确认 vs ⚠️ 推测；所有结论附 `Source`。

---

## 0. 域定位：Classifier vs Calculator（为什么分开）

- **分类器（Classifier）**：对色谱/峰做「归类/评级」，**不改变色谱数据本身**，只产生「结果对象」+ 附加一个 `MeasurementResult` 挂到色谱上（transient，仅供查看）。接口 Javadoc 明示："The classification data is stored in the chromatogram, but the data is transient and only intended to be used for live inspection of the chromatogram."
  - Source: `org.eclipse.chemclipse.chromatogram.xxd.classifier/src/.../core/IChromatogramClassifier.java`（类头注释）
- **计算器（Calculator）**：**扩展/改写色谱数据**。接口 Javadoc 明示："A chromatogram calculator is used for example to calculate retention indices (RI) for scans and/or peaks. It extends the chromatographic data."（原本想归入 filter，但用户认为 filter 语义是「删除」，故单列一类）
  - Source: `org.eclipse.chemclipse.chromatogram.xxd.calculator/src/.../core/chromatogram/IChromatogramCalculator.java`（类头注释）
- 门面执行后都会 `chromatogram.setDirty(true)`（calculator 在门面 `ChromatogramCalculator.applyCalculator` 中统一打脏；classifier 不直接打脏）
  - Source: `...xxd.calculator/core/chromatogram/ChromatogramCalculator.java` L62

---

## 1. 分类器扩展点与接口（✅ 全部源码确认）

### 1.1 接口 `IChromatogramClassifier`

```java
public interface IChromatogramClassifier {
    IProcessingInfo<IChromatogramClassifierResult> applyClassifier(
        IChromatogramSelection chromatogramSelection,
        IChromatogramClassifierSettings chromatogramClassifierSettings,
        IProgressMonitor monitor);
    DataType[] getDataTypes();
}
```
- Source: `org.eclipse.chemclipse.chromatogram.xxd.classifier/src/.../core/IChromatogramClassifier.java`
- 骨架 `AbstractChromatogramClassifier`：持有 `DataType[] dataTypes`（构造传入，`getDataTypes()` 返回克隆）；提供 `validate(selection)` = 校验 selection/chromatogram 非空（null → error message 入 `IProcessingInfo`）
  - Source: `.../core/AbstractChromatogramClassifier.java`（构造器 + validateChromatogramSelection）

### 1.2 结果对象 `IChromatogramClassifierResult`

- 仅两个成员：`ResultStatus getResultStatus()`（枚举 `OK / UNDEFINED / EXCEPTION`）+ `String getDescription()`。子类可自行携带业务数据（见 ratios 的 `PeakRatioResult`、D-W 的 `DurbinWatsonClassifierResult`）。
  - Source: `.../result/IChromatogramClassifierResult.java` + `ResultStatus.java` + `AbstractChromatogramClassifierResult.java`
- `IMeasurementResult<?>` 挂载：`new MeasurementResult(String name, String identifier, String description, Object result)` → `chromatogram.addMeasurementResult(...)`。分类器结果普遍以此方式挂到色谱，供 UI 预览。
  - Source: `org.eclipse.chemclipse.model/src/.../implementation/MeasurementResult.java` L21（构造器四参）

### 1.3 门面 `ChromatogramClassifier` 与扩展点（双扩展点合并读取）

- 门面 static 方法：
  - `applyClassifier(selection, settings, classifierId, monitor)`：按 id 反射实例化 classifier → 调 `applyClassifier(...)`；找不到返回错误 `IProcessingInfo`。
  - `getChromatogramClassifierSupport()`：扫注册表填 `ChromatogramClassifierSupplier`（id/description/classifierName/classifierSettings→settingsClass）。
  - `getConfigurationElements()`：**同时读两个扩展点并合并**：
    - 旧版（legacy）：`org.eclipse.chemclipse.chromatogram.msd.classifier.chromatogramClassifierSupplier`（注释：Keep the legacy classifier extension point. Otherwise existing process methods would be broken.）
    - 新版（generic）：`org.eclipse.chemclipse.chromatogram.xxd.classifier.chromatogramClassifierSupplier`
- 元素属性：`id / description / classifierName / classifier`（实现类）/ `classifierSettings`（设置类，可选）
- Source: `...xxd.classifier/core/ChromatogramClassifier.java`（EXTENSION_POINT_LEGACY/GENERIC、getChromatogramClassifier、getConfigurationElement、getConfigurationElements）

### 1.4 设置绑定（settings）

- `IChromatogramClassifierSettings` 为标记接口；实现 `AbstractChromatogramClassifierSettings`（含 `@JsonProperty` 序列化 + 共享正则常量 `RE_START/RE_TEXT/RE_SEPARATOR/RE_NUMBER/RE_TRACE_PATTERN`）
  - Source: `.../settings/IChromatogramClassifierSettings.java` + `AbstractChromatogramClassifierSettings.java`
- **绑定方式**：扩展属性 `classifierSettings=` 指向一个 Jackson 注解 POJO，门面反射实例化取 `.class` 填进 supplier；运行时分类器再强转 `instanceof` 判断具体 settings 类型（否则取 `PreferenceSupplier.getSettingsXxx()` 默认值）。ratios 4 个分类器均按此「settings instanceof 判断 + 默认回退」模式写。
  - Source: 各 `*RatioClassifier.applyClassifier` 开头 + `.../preferences/PreferenceSupplier.java`
- 所有 ratios 设置以**多行字符串模板**保存（`@StringSettingsProperty(isMultiLine=true)`），形如 `名称|期望值|Warn限|Error限`，用 `util/*RatioValidator` 逐行校验后 `getList()` 解析成模型列表。即：**设置即「用例清单」，分类器逐峰匹配后填偏差**。
  - Source: `settings/TimeRatioSettings.java`、`TraceRatioSettings.java`、`model/TimeRatios.extractSettings`

### 1.5 统一写法模板（✅ 各分类器共性）

```text
applyClassifier(selection, settings, monitor)
 ├─ settings instanceof 具体类型 ? 用传入 : 取 PreferenceSupplier 默认
 ├─ validate(selection)                       // 空校验
 ├─ 业务计算 → 结果对象（如 PeakRatioResult）
 ├─ new MeasurementResult(name, id, desc, 结果) → chromatogram.addMeasurementResult(...)
 └─ processingInfo.setProcessingResult(结果)
```

---

## 2. ratios 4 分类器（✅ 全部源码确认）

插件：`openchrom/plugins/net.openchrom.xxd.classifier.supplier.ratios`（OpenChrom 社区，4 个分类器全部注册在**旧版扩展点** `...msd.classifier.chromatogramClassifierSupplier`，见其 plugin.xml）。
公共骨架 `AbstractRatioClassifier`：`super(DataType.MSD)` + `isPeakMatch(peak, ratio)` —— **按峰识别目标名匹配**：遍历 `peak.getTargets()`，比对 `IIdentificationTarget.getLibraryInformation().getName() == ratio.getName()`。
- Source: `core/AbstractRatioClassifier.java`（isPeakMatch）

| # | 分类器 | ID | 判什么（算法一句话） | 匹配键 | 输出字段 |
|---|---|---|---|---|---|
| 2.1 | **Time Ratio** | `net.openchrom.xxd.classifier.supplier.ratios.time` | 峰保留时间与期望 RT 的**相对偏差 %** | 峰识别名 | `deviation` |
| 2.2 | **Trace Ratio** | `...ratios.trace` | 两离子丰度比与期望比的偏差 % | 峰识别名 + testCase 离子对 | `ratio` + `deviation` |
| 2.3 | **Quant Ratio** | `...ratios.quant` | 定量浓度与期望浓度的相对偏差 % | 峰识别名 + quantitationName + 浓度单位 | `concentration` + `deviation` |
| 2.4 | **Qual Ratio** | `...ratios.qual` | 峰质量评级（leading/tailing + S/N + 对称性，各给 5 级） | 全部峰（无模板） | 3× `PeakQuality` |

### 2.1 Time Ratio（`core/TimeRatioClassifier.java`）
- 模板行：`Naphthalin | 3.45 | 5.0 | 15.0`（名称|期望RT[min]|Warn%|Error%）
- 算法：`retentionTimeMinutes = peak.getPeakModel().getRetentionTimeAtPeakMaximum() / MINUTE_CORRELATION_FACTOR(60000)`；`deviation = |expected - actual| / actual * 100`（%）。只在与模板名匹配的峰上计算。
- Source: `core/TimeRatioClassifier.calculateRatios` L63-83；`settings/TimeRatioSettings.java` 模板示例注释

### 2.2 Trace Ratio（`core/TraceRatioClassifier.java`）
- 模板行：`Naphthalin | 128:127 | 14.6 | 5.0 | 15.0`（名称|测试离子对 ref:target|期望比|Warn%|Error%）
- 算法：从 `IPeakModelMSD.getPeakMassSpectrum()` 取 `ExtractedIonSignal`；`intensityReference = getAbundance(ref)`、`intensityTarget = getAbundance(target)`；若两者均 >0 且期望比 ≠0：
  - `actualRatio = 100 / intensityReference * intensityTarget`（即 target 相对 ref 的百分比丰度）
  - `deviation = |100 - (100/expectedRatio * actualRatio)|`（相对期望比的绝对偏差，单位 %）
- 边界：ref 丰度=0 或期望比=0 时不计算（ratio/deviation 置 NaN）；`getTestCase().split(":")` 必须恰好 2 段。
- Source: `core/TraceRatioClassifier.calculateRatios` L70-117

### 2.3 Quant Ratio（`core/QuantRatioClassifier.java`）
- 算法：匹配峰后查 `peak.getQuantitationEntries()`，要求 `name == ratio.getQuantitationName()` **且** `concentrationUnit` 相等；取 `entry.getConcentration()`，`deviation = |expectedConc - conc| / conc * 100`。
- 附带 `QuantRatio.getResponseFactor()`：`offset = deviation/100`，若 `concentration <= expected` 则 `offset *= -1`；`return 1 + offset` —— 供用户修正定量结果的响应因子（**1 为无偏差，>1 偏高/低由符号决定**）。
- Source: `core/QuantRatioClassifier.getQuantitationEntry` + `model/quant/QuantRatio.java`（getResponseFactor）
- `QualRatio`（非 QualRatioClassifier）：注意 2.3 的 QuantRatio 与 2.4 的 QualRatio 是不同类，勿混淆。

### 2.4 Qual Ratio（`core/QualRatioClassifier.java`，唯一无模板、遍历全部峰）
三个子评级，各映射到枚举 `PeakQuality {VERY_GOOD("++"), GOOD("+"), ACCEPTABLE("~"), BAD("-"), VERY_BAD("--"), NONE("")}`：
1. **leading/tailing**（来自 `peakModel.getLeading()/getTailing()`，浮点阈值分级）：
   - 两者均 ≤1 → VERY_GOOD；≤2 → GOOD；≤3 → ACCEPTABLE；≤4 → BAD；≤5 → VERY_BAD；否则 NONE
2. **S/N**（来自 `IChromatogramPeak.getSignalToNoiseRatio()`）：
   - ≤1 → VERY_BAD；≤2 → BAD；≤4 → ACCEPTABLE；≤10 → GOOD；>10 → VERY_GOOD
3. **对称性**：`factor = |(tMax - tStart) / (tStop - tMax)|`；`factor>1 时取 1/factor`（钳到 [0,1]，1=完全对称）：
   - ≥0.88 → VERY_GOOD；≥0.68 → GOOD；≥0.45 → ACCEPTABLE；≥0.23 → BAD；否则 VERY_BAD
- 结果类 `QualRatioResult` 携带 `QualRatios`（`ArrayList<QualRatio>`，每个 `QualRatio` 含 peak 引用 + 3×PeakQuality）。
- Source: `core/QualRatioClassifier.java`（calculateLeadingTailing / calculateSignalToNoiseRatio / calculateSymmetry）+ `model/qual/PeakQuality.java`

**输出挂载**：4 个分类器均构造 `MeasurementResult` 并 `chromatogram.addMeasurementResult(...)`；processingInfo 结果 = `PeakRatioResult`/`QualRatioResult`。峰本身不直接存字段，只有 `ratio.setPeak(peak)` 引用 + 结果对象持有列表。
- Source: 各 `*RatioClassifier.applyClassifier` + `model/PeakRatioResult.java`

**导出模板**：插件还注册 3 个仅导出转换器（`.tir` Time / `.trr` Trace / `.qar` Quant，CSD+MSD 双注册，`isImportable=false`），导出当前峰比值清单供再次使用。
- Source: `plugin.xml`（msd/csd converter chromatogramSupplier 扩展）+ `core/*RatioExport.java`

---

## 3. Durbin-Watson 检验（✅ 源码确认）

插件：`org.eclipse.chemclipse.chromatogram.xxd.classifier.supplier.durbinwatson`（注册于旧版 classifier 扩展点，id=`...durbinwatson`）。

### 3.1 统计量公式（标准 DW 统计量，作用在「残差」上）
```
e(i)  = original(i) - smoothed(i)          // 残差 = 原值 - S-G 平滑值
denominator = Σ e(i)²
nominator   = Σ (e(i) - e(i-1))²           // i 从 1 起（一阶差分）
rating = nominator / denominator
```
- 这正是经典 Durbin-Watson 统计量 `d = Σ(Δe)² / Σe²`。**残差的自相关测度**：d≈2 表示残差近似白噪声（无自相关），d→0 表示残差强正自相关（平滑欠拟合），d→4 表示强负自相关（过拟合）。
- Source: `processor/DurbinWatsonProcessor.java` L86-112（calculateDurbinWatsonRating）

### 3.2 实际用途：**自动搜最佳 S-G 平滑参数**（不是基线漂移检测）
- 输入：整个选择区间的 TIC（`TotalScanSignalExtractor.getTotalScanSignals` → 每扫描 totalSignal 转 double[]）。
- 三重循环扫参：`derivative ∈ [0..5]`，`order ∈ [2..5]`，`width ∈ [5..51]（必须奇数）`，每组参数用 `SavitzkyGolayProcessor.smooth(values, filterSettings)` 平滑，算 DW rating，存为 `SavitzkyGolayFilterRating(rating, filterSettings)` 加入结果列表。
- 结果 `DurbinWatsonClassifierResult` 携带全部 `(rating, 参数组)`，UI 可用它推荐「残差最白噪声」的那组 S-G 参数（≈最佳平滑度）。
- 由此：**DW 分类器在本项目中定位 = 平滑参数自动优化器**，而非统计意义上的回归残差检验。
- Source: `processor/DurbinWatsonProcessor.java`（durbinWatsonMain 循环）+ `core/Classifier.java`（applyClassifier 挂 MeasurementResult，`IChromatogramResultDurbinWatson.NAME/IDENTIFIER`）+ `result/SavitzkyGolayFilterRating.java`
- 设置类 `ClassifierSettings extends AbstractChromatogramClassifierSettings` 为**空壳**（无字段），参数范围硬编码在 processor。
  - Source: `settings/ClassifierSettings.java`

---

## 4. 质谱分类器 WNC 与 molpeak（✅ 源码确认）

### 4.1 WNC（Water/Nitrogen/Carbon dioxide 比值）
插件：`org.eclipse.chemclipse.chromatogram.msd.classifier.supplier.wnc`，id=`...supplier.wnc`，`DataType.MSD`。
- 目标离子模板（默认，可改）：`Water:18; Nitrogen:28; Oxygen:32; Carbon Dioxide:44; Solvent Tailing:84; Column Bleed:207`（`TargetTraces.getDefault()`）。
  - Source: `settings/ClassifierSettings.java`（@JsonProperty defaultValue）
- 算法（`internal/core/support/Calculator.java`，核心照搬）：
  1. 选区内逐扫描累加每个 m/z 的丰度 → `Map<ion, sumAbundance>`（`extractIonValues`）
  2. `factorMax = 100 / max(所有离子累加值)`；`factorSum = 100 / sum(所有离子累加值)`
  3. 对每个目标离子：`percentageMaxIntensity = factorMax * value`（相对最高峰的 %），`percentageSumIntensity = factorSum * value`（相对总和的 %）
- 即：**WNC 给出各目标杂质离子在选定 RT 窗口内的「相对最大峰」与「相对总面积」百分比**。
- Source: `internal/core/support/Calculator.java`（calculateIonPercentages / extractIonValues / calculateFactorMax / calculateFactorSum / calculateAndSetIntensityValues）+ `core/Classifier.java`

### 4.2 molpeak（Lignin SGH + Carbohydrate 分类）
插件：`org.eclipse.chemclipse.msd.classifier.supplier.molpeak`，id=`...molpeak`，`DataType.MSD`。**同一插件还注册 3 个标识器**（peakIdentifier / massSpectrumIdentifier / libraryService），分类器依赖「先做基峰标识」。
- 流程（`core/Classifier.applyClassifier`）：
  1. 取全部 `IPeakMSD`；若无 S/G/H/C 标记 → 先跑自身 `PeakIdentifier.identify`（基于谱库标识，标记名 = `SYRINGYL / GUAIACYL / PHYDROXYPHENYL / CARBOHYDRATE`）
  2. 判断峰是否已积分（`integratedArea != 0`）：是则按面积计数，否则仅按出现计数（warn 提示）
  3. 对每个峰，找 identifier=`BasePeakIdentifier.IDENTIFIER` 的 target，按标记名累加 counterS/G/H/C，无匹配记 counterU
  4. `percent = counter / (S+G+H+C+U) * 100`，写进 `LigninRatios.getResults()`（含 "No Match"）
- Source: `classifier/BasePeakClassifier.java`（calculateLigninRatios / markerIsAvailable / arePeaksIntegrated）+ `core/Classifier.java` + `identifier/BasePeakIdentifier.java`（标记常量）
- 应用背景（林产化学）：S/G/H 为木质素三单体（紫丁香基/愈创木基/对羟苯基）比值，C 为碳水化合物。

### 4.3 molpeak 内置谱库与匹配细节（✅ 源码确认）

**主判定不是谱库匹配，而是「基峰 m/z 硬编码集合」**（`BasePeakIdentifier.getIdentification`）：取峰质谱的 base peak（`(int)massSpectrum.getBasePeak()`），查四个静态集合：
- SYRINGYL = {149, 154, 167, 181, 182, 192, 194, 208, 210}
- GUAIACYL = {109, 123, 136, 137, 138, 140, 150, 151, 152, 162, 164, 168, 178}
- PHYDROXYPHENYL = {94, 107, 108, 120, 121, 124, 134}
- CARBOHYDRATE = {29, 31, 39, 41, 42, 43, 44, 45, 46, 55, 56, 57, 58, 59, 60, 68, 69, 73, 81, 82, 84, 85, 87, 95, 96, 98, 114, 126, 142}
- 都不命中 → `NOT_FOUND`（"Not Found (SGH + C Identifier)"）。
- Source: `...molpeak/identifier/BasePeakIdentifier.java`（static 块 L84-105 + getIdentification L313-327）

**内置谱库（插件 `standards/` 目录，2 个 .msl 文件）**：
- `standards/gerberetal2012.msl`：约 100 张谱（Gerber et al. 2012, J. Anal. Appl. Pyrolysis 95:95-100），格式 `NAME/RI/RT/COMMENTS/SOURCE/NUM PEAKS/(mz inten)...`，其中 **COMMENTS 字段标注类别**：`P`(酚)、`H`(对羟苯基类酚)、`G`(guaiacyl)、`S`(syringyl)、`C`(carbohydrate)、`U`(unknown)。用途：NOT_FOUND 峰的**兜底全谱匹配库**。
- `standards/references.msl`：2 张参考谱 "Guaiacyl"（base peak 57）与 "Lorenzyl"。用途：`LibraryService` 返回 Guaiacyl/对羟苯基类别参考谱。
- `settings/S0165237012000137.ris` 是 **RIS 参考文献文件**（Gerber 2012，DOI 10.1016/j.jaap.2012.01.011），经 `AbstractBasePeakSettings.getLiteratureReferences` 加载为 `LiteratureReference`——**不是谱库**。
- Source: `...molpeak/standards/*.msl` + `settings/S0165237012000137.ris` + `settings/AbstractBasePeakSettings.java` L68-71

**未命中（NOT_FOUND）的二次匹配**：`PeakIdentifierFile` / `MassSpectrumIdentifierFile`（file 标识供应商）用 `gerberetal2012.msl` 做全谱比较，参数：`numberOfTargets=10`、`minMatchFactor=70.0`、`minReverseMatchFactor=70.0`、`usePreOptimization=false`、`thresholdPreOptimization=0.1`、`alternateIdentifierId=IDENTIFIER`。命中后 target 名=库谱名（如 "Guaiacol"），identifier 仍为 "SGH + C Identifier"。
- Source: `...molpeak/identifier/BasePeakIdentifier.setFileIdentifierSettings` L341-350

**阈值设置**（`AbstractBasePeakSettings`）：`limitMatchFactor` 默认 **80.0**（峰已有 matchFactor≥80 的 target 则不再标识，`LimitSupport.doIdentify` 判断）、`matchQuality` 默认 **80.0**（写入 `ComparisonResult` 四参）。设置类 `PeakIdentifierSettings`/`MassSpectrumIdentifierSettings` 继承；`Classifier.applyClassifier` 以 **null settings** 调 `PeakIdentifier.identify(peaks, null, monitor)` → `getPeakIdentifierSettings` 回退 `new PeakIdentifierSettings()`（默认）。
- Source: `...molpeak/settings/AbstractBasePeakSettings.java` + `core/PeakIdentifier.getPeakIdentifierSettings` L44-52 + `core/Classifier.applyClassifier` L45-75

**LibraryService 返回参考谱**（`getMassSpectra`）：SYRINGYL → 单离子谱 `m/z 156 / 1000`（`getSyringyl()` 硬编码）；GUAIACYL / PHYDROXYPHENYL → 从 `references.msl` 按 `name == nameTarget` 过滤返回（无 "p-Hydroxyphenyl" 谱则返回空列表）。
- Source: `...molpeak/identifier/BasePeakIdentifier.getMassSpectra` L234-278 + `getSyringyl` L288-294

**匹配流程小结**：基峰判定（主，O(1) 集合查）→ 未命中走文件库全谱匹配（辅，match 70/70）→ 分类器只对 target identifier="SGH + C Identifier" 的峰按标记名累加 S/G/H/C/U。

---

## 5. 计算器扩展点与接口（✅ 源码确认）

### 5.1 扩展点（`org.eclipse.chemclipse.chromatogram.xxd.calculator/plugin.xml` 定义两个）
| 扩展点 | 用途 | 元素 | 供应商属性 |
|---|---|---|---|
| `org.eclipse.chemclipse.chromatogram.xxd.calculator.chromatogramCalculatorSupplier` | 色谱图计算器（RI、分辨率等） | `ChromatogramCalculatorSupplier` | `id/description/calculatorName/calculator/calculatorSettings` |
| `org.eclipse.chemclipse.chromatogram.xxd.calculator.noiseCalculationSupplier` | 噪声计算器（S/N 用） | `NoiseCalculationSupplier` | `id/description/calculatorName/noiseCalculator` |

- Source: 两插件 plugin.xml + `...xxd.calculator/core/chromatogram/ChromatogramCalculator.java`（门面，扩展点常量 + createExecutableExtension + getChromatogramCalculatorSupport）+ `.../core/noise/NoiseCalculator.java`（噪声门面，getNoiseCalculator / getNoiseCalculatorSupport）
- 注意：`xxd.calculator` 插件本身还经 **classifier 旧扩展点** 注册了 3 个分类器：`Noise Calculator (Chromatogram)`（设置噪声算法）、`Noise Segment Setter (Chromatogram)`（把当前选区标为噪声段）、`Column Parser (Chromatogram)`（从表头字段解析色谱柱信息）——分类器/计算器跨界共用。
  - Source: `...xxd.calculator/plugin.xml` + `core/noise/NoiseChromatogramClassifier.java` + `core/noise/NoiseSegmentSetter.java` + `core/column/ChromatogramColumnParser.java`

### 5.2 接口 `IChromatogramCalculator`
```java
public interface IChromatogramCalculator {
    IProcessingInfo<?> applyCalculator(IChromatogramSelection selection, IChromatogramCalculatorSettings settings, IProgressMonitor monitor);
    IProcessingInfo<?> validate(IChromatogramSelection selection, IChromatogramCalculatorSettings settings);
}
```
- 门面 `ChromatogramCalculator.applyCalculator` 执行后**统一 `chromatogram.setDirty(true)`**（calculator 会改写色谱数据）。
- Source: `.../core/chromatogram/IChromatogramCalculator.java` + `ChromatogramCalculator.java` L62

### 5.3 噪声接口 `INoiseCalculator`（S/N 计算的统一入口）
```java
public interface INoiseCalculator {
    void reset();
    String getId();
    String getName();
    float getNoiseFactor();                                  // 全局噪声因子（惰性计算）
    float getSignalToNoiseRatio(IChromatogram c, float intensity);
    List<INoiseSegment> getNoiseSegments(IChromatogram c, IProgressMonitor monitor);
}
```
- `AbstractNoiseCalculator` 只实现 `id` 存取（`setId` 由门面注入扩展 id）。Stein/Dyson 各自惰性算一次 `noiseFactor` 后缓存（`runCalculation` 标志 + `reset()` 复位）。
- Source: `org.eclipse.chemclipse.model/src/.../core/INoiseCalculator.java` + `AbstractNoiseCalculator.java`

---

## 6. 峰分辨率计算（✅ 源码确认）

插件：`org.eclipse.chemclipse.chromatogram.xxd.calculator.peak.resolution`（id=`...peak.resolutions`，注册于 `chromatogramCalculatorSupplier`）。
- **计算公式（IUPAC 定义，代码直接实现）**：
```
Rs = 2 * (tR2 - tR1) / (w1 + w2)
tRi = peakModel.getRetentionTimeAtPeakMaximum()   // 峰顶保留时间
wi  = peakModel.getWidthByInflectionPoints()      // 拐点宽度
```
  - Source: `org.eclipse.chemclipse.model/src/.../implementation/PeakResolution.java`（calculate()，注释引 Gold Book DOI 10.1351/goldbook.P04465）
- **输入**：峰列表中**相邻两峰**（`ChromatogramCalculator.applyCalculator` 迭代 `peaks`，当前峰与下一峰成对构造 `new PeakResolution(current, next)`，逐个连续），结果存 `PeakResolutionResult.getPeakResolutions()` 并挂 MeasurementResult。
  - Source: `...peak.resolution/core/ChromatogramCalculator.java`（applyCalculator(List, result) L58-67）
- **`getWidthByInflectionPoints()` 的含义**：默认 `getWidthByInflectionPoints(0.5f)` —— 取峰模型**两条拐点切线**与「50% 峰高水平基线」的交点 x 差（`p2.x - p1.x + 1`）。对高斯峰 ≈ 半峰宽 FWHM。**注意不是基线峰宽（4σ）**。
  - Source: `org.eclipse.chemclipse.model/src/.../core/AbstractPeakModelStrict.java`（getWidthByInflectionPoints / getWidthByInflectionPoints(float height)）
- 设置字段：`CalculatorSettings.formula` 枚举 `PeakResolutionFormula.IUPAC`（目前仅一个选项，default=IUPAC）——公式选择是「预留位」，实际只有一种。
  - Source: `...peak.resolution/core/PeakResolutionFormula.java` + `settings/CalculatorSettings.java`

---

## 7. 噪声估计：Stein 与 Dyson（✅ 全部源码确认，可照搬）

两类都依赖「**色谱分段**」测量结果 `ChromatogramSegmentation`（分析段列表）与段校验器；段噪声因子逐段算，再聚合为全局 `noiseFactor`。

### 7.1 段校验器 `SegmentValidatorClassic`（两法共用，✅）
- 输入段内信号值数组 + 均值；统计**越过均值线的次数 crossings**；`accept = crossings > (length+1)/2`（length = n-1）。
- 语义：噪声段应围绕均值高频振荡（过半位置穿越均值）；若穿越太少，认为该段属于峰（被拒绝）。
- Source: `org.eclipse.chemclipse.model/src/.../support/SegmentValidatorClassic.java` L31-71

### 7.2 Stein 法（`...supplier.noise.stein`，id=`...noise.stein`，S/N 公式注释 `S/N = Math.sqrt(intensity) * noiseFactor`）
- **段因子**：`segmentNoiseFactor = medianDeviationFromMedian(values) / sqrt(mean(values))`
  - `medianDeviationFromMedian` = median(|value(i) - median(values)|)（对 float[] 转 double 后计算，n<3 返回 0）
  - 引用文献：S.E. Stein, "An Integrated Method for Spectrum Extraction and Compound Identification from GC/MS Data"
- **聚合**：对 TIC 段 + **每个 m/z 离子段**全部算段因子（MSD 时遍历 `startIon..stopIon`），取全部段因子的**中位数**为全局 noiseFactor。
- **S/N**：`sqrt(intensity) * noiseFactor`（注意：噪声因子与 sqrt(强度) 相乘，S/N 随强度平方根增长）。
- 计算细节：`getNoiseSegments(chromatogram, monitor)`：非 MSD 时用 TIC + segmentation 段；MSD 时对 TIC_ION 段 + 逐离子段都算。段无效（校验拒绝）则跳过。
- Source: `...stein/core/NoiseCalculator.java`（calculateNoiseFactor / getNoiseSegments / getSignalToNoiseRatio）+ `org.eclipse.chemclipse.numeric/.../Calculations.getMedianDeviationFromMedian`

### 7.3 Dyson 法（`...supplier.noise.dyson`，id=`...noise.dyson`，注释 `S/N = intensity / noiseValue`）
- **段因子**：`segmentNoiseFactor = max(values) - min(values)`（段内峰-谷差，即峰峰值噪声）
- **聚合**：
  - 若存在用户选定的噪声段（`noiseSegment.isUse()==true`）→ 取这些段因子的**均值**（优先用户选择）
  - 否则 → 全部段因子**中位数**（并全部 `setUse(false)`）
  - 无噪声段时兜底 `chromatogram.getMinSignal()`
- **S/N**：`intensity / noiseFactor`（线性，S/N 与强度成正比）
- 额外：`getNoiseSegments` 会**写回** 两个测量结果（`ChromatogramSegmentation` 重建 + `NoiseSegmentMeasurementResult`），供 UI 展示噪声段。
- 引用文献：Norman Dyson, *Chromatographic Integration Methods*, 2nd ed.
- Source: `...dyson/core/NoiseCalculator.java`（calculateNoiseFactor / calculateNoiseFactor(segment) / getNoiseSegments / getSignalToNoiseRatio）
- 参考（同一接口家族的统计辅助，`Calculations.java`）：mean/median/min/max/variance/stddev/medianDeviationFromMedian 等（n<3 时 median=0，注意空输入防护）。

### 7.4 两类对比小结
| 项 | Stein | Dyson |
|---|---|---|
| 段噪声度量 | MAD（中位绝对偏差）/ sqrt(mean) | max − min（峰峰） |
| 全局聚合 | 全部段因子**中位数** | 用户段**均值**，否则全段**中位数** |
| S/N 公式 | sqrt(I) × factor | I / factor |
| 段来源 | TIC + 逐离子（MSD） | TIC（+用户选择段） |

### 7.5 ChromatogramSegmentation 分段来源（✅ 源码确认）

**定义**：`org.eclipse.chemclipse.model/results/ChromatogramSegmentation.java` —— 继承 `AnalysisSegmentMeasurementResult<ChromatogramSegment>`，即一个挂到色谱上的 `IMeasurementResult`（`isVisible()=false`，不出现在结果下拉预览）。构造 `new ChromatogramSegmentation(chromatogram, segmentWidth)` 时即切好全部段（不可变列表）。
- Source: `model/results/ChromatogramSegmentation.java` L31-35

**谁创建 / 谁读取**：
- 创建点 1：`NoiseChromatogramSupport.applyNoiseSettings(chromatogram, settings, monitor)` —— `new ChromatogramSegmentation(chromatogram, settings.getSegmentWidth())`；若该宽度下 `noiseCalculator.getNoiseSegments()` 返回空，则 `segmentWidth = SegmentWidth.getLower(segmentWidth)` **逐级降宽重试**（17→15→…→5），直到找到段或 0（0 时返回 null = "Can't find any noise segments"）。
- 创建点 2：`NoiseChromatogramSupport.addNoiseSegment(selection, useOnlyNewSegment)` —— 用户把当前选区标记为噪声段时 `new ChromatogramSegmentation(chromatogram, noiseSegment.getWidth())`（选区须奇数宽且 ≥5）。
- **Stein/Dyson 的 `getNoiseSegments` 只读取** `chromatogram.getMeasurementResult(ChromatogramSegmentation.class)`，自己不创建。
- 触发链：`NoiseChromatogramClassifier`（legacy 分类器 "Noise Calculator (Chromatogram)"，DataTypes MSD/WSD/CSD）`applyClassifier` → `applyNoiseSettings`；`NoiseSegmentSetter`（"Noise Segment Setter"）→ `addNoiseSegment`。
- Source: `...xxd.calculator/core/noise/NoiseChromatogramSupport.java` L53-85 + `NoiseChromatogramClassifier.java` L39-60 + `NoiseSegmentSetter.java`

**分段算法（等点数，非等时间/非自适应）**：`AnalysisSupport.getChromatogramSegments(chromatogram, width)` → `initializeAnalysisSegments(numberOfScans, 1, width, ...)`：
- `last = numberOfScans % width`；`parts = (numberOfScans - last) / width`
- 生成 parts 个段，第 i 段 `[startScan, startScan+width-1]`（`AnalysisSegment` 构造 `stopScan = startScan + segmentWidth - 1`），起点从扫描 1 递增；若 `last>0` 末尾追加一个**短段**（不足 width）。
- 每段 = `ChromatogramAnalysisSegment`：持 chromatogram 引用 + startScan/stopScan，`getStartRetentionTime()/getStopRetentionTime()` 由扫描换算（`getScan(startScan).getRetentionTime()`）。
- 前置约束：`width ≥ 3` 且 `numberOfScans ≥ width`，否则抛 `AnalysisSupportException`。
- Source: `model/support/AnalysisSupport.java`（getChromatogramSegments L135-138 + initializeAnalysisSegments L97-133）+ `model/support/AnalysisSegment.java` L24-30 + `model/support/ChromatogramSegment.java`

**段宽设置**：`NoiseChromatogramClassifierSettings.segmentWidth`（JsonProperty defaultValue "7"、合法**奇数 5..19**、ODD_NUMBER 校验），运行时默认 = `PreferenceSupplier.getSelectedSegmentWidth()` → 偏好默认 **"9"**（`DEF_SEGMENT_WIDTH`，MIN=5 / MAX=19）。`SegmentWidth` 现为工具类（枚举已废弃），`getLower` 在 {5,7,9,11,13,15,17,19} 中取小于当前的最大值。
- Source: `...xxd.calculator/settings/NoiseChromatogramClassifierSettings.java` L37-41 + `preferences/PreferenceSupplier.java` L29-32,78-81 + `support/model/SegmentWidth.java` L65-78

**分段在色谱图上的叠加绘制**：`AnalysisSegmentPaintListener`（swtchart `ICustomPaintListener`，`drawBehindSeries()=true`）把每段画成**半透明矩形 + 段界竖线**（alpha=100，选中段 +50 并加粗），颜色方案 ANALYSIS/NOISE；`AnalysisSegmentMeasurementResultAdapterFactory` 把 `AnalysisSegmentMeasurementResult` 适配出 `ICustomPaintListener` / `ITreeContentProvider` / `ColumnDefinitionProvider` / `ISelectionChangedListener`（选中段交互），经 `org.eclipse.chemclipse.ux.extension.xxd.ui/plugin.xml` adapters 扩展注册。
- Source: `ux.extension.xxd.ui/segments/AnalysisSegmentPaintListener.java`（paintControl L54-103）+ `adapters/AnalysisSegmentMeasurementResultAdapterFactory.java`（getAdapter L43-63）

---

## 8. AMDIS 保留指数（CalcRI）（✅ 全部源码确认）

插件：`org.eclipse.chemclipse.chromatogram.xxd.calculator.supplier.amdiscalri`（id=`...amdiscalri`，注册于 `chromatogramCalculatorSupplier`；另有 RI Reset 计算器 + 峰/质谱烷烃标识器 + .cal 导出转换器）。

### 8.1 核心公式（线性插值，AMDIS 手册原式，✅）
```
RIcomp = RIlo + (RIhi − RIlo) × (RTact − RTlo) / (RThi − RTlo)
```
- `separationColumnIndices` = TreeMap<**保留时间RT**（ms）, IRetentionIndexEntry{rt, ri, name}>；对给定 RT，取 `floorEntry(RT)` / `ceilingEntry(RT)` 两个最邻近标准点插值；只有两端都存在才计算（否则 RI=0 = 缺失标记）。
- 反向：`calculateRetentionTime(ri)` 同样线性插值（RI→RT）。
- **线性插值，不是对数**（Kovats 定义下同系烷烃用 log RT 内插，但 OpenChrom 按 AMDIS 手册用线性）。
- Source: `org.eclipse.chemclipse.model/src/.../support/RetentionIndexMath.java`（calculateRetentionIndex / calculateRetentionTime / calculateX，注释引 AMDIS 手册公式）
- 适用对象：**每个扫描**（`scan.setRetentionIndex(ri)`，MSD 还同步写 `optimizedMassSpectrum`）+ **每个峰顶扫描**（`peak.getPeakModel().getPeakMaximum()`）。
  - Source: `impl/RetentionIndexCalculator.calculateIndex(IChromatogram, ISeparationColumnIndices)`

### 8.2 标定点来源（三种策略 `CalculatorStrategy {AUTO, CHROMATOGRAM, FILES}`，✅）
1. **CHROMATOGRAM**：直接用色谱自身携带的 `getSeparationColumnIndices()`（来自导入或上次存储）。
2. **FILES**：读 AMDIS `*.cal` 校准文件（`CalibrationFileReader → AMDISConverter.parse`）。格式：
   - 列头：`#COLUMN_NAME=DB5`、`#COLUMN_LENGTH=...`、`#COLUMN_DIAMETER=...`、`#COLUMN_PHASE=...`
   - 数据行：`<RT分钟> <RI> <net> <S/N> <名称...>`（如 `10.214 1600.0 100 981 Hexadecane`），RT 乘 60000 转 ms
   - 按**色谱柱名称**（其次分离类型名，最后 DEFAULT）匹配校准文件
3. **AUTO**：先色谱内，空则回退 FILES。
- 设置字段：`calibrationFile(.cal)`、`calculatorStrategy`(default FILES)、`useDefaultColumn`(default true)、`processReferencedChromatograms`(default true)。
- Source: `settings/CalculatorSettings.java` + `impl/CalculatorStrategy.java` + `io/AMDISConverter.java`（L90-105 数据行解析）

### 8.3 自动判烷烃碳数（供「自动生成标定」用，✅）
- 烷烃 RI = **碳数 × 100**（C8 → 800）。识别名形如 `C8 (Octane)`；`getAlkaneNumber` 用正则 `C(\d+)` 提取碳数，或按 **CAS 号 / IUPAC 名**查内置表（C1–C32，含 `74-82-8→Methane`…`544-85-4→Dotriacontane`）。
- Source: `impl/RetentionIndexCalculator.java`（ALKANE_PREFIX/REGEX、getAlkaneNumber、getAlkanesByCAS）
- 内置烷烃库标识由 `AlkaneIdentifier` 用「文件谱库标识器」匹配（`PeakIdentifierFile`，库文件内嵌插件资源）；设置：`numberOfTargets / minMatchFactor / minReverseMatchFactor`。
  - Source: `impl/AlkaneIdentifier.java`（transferAlkaneSettings）

### 8.4 缺失烷烃外推（`RetentionIndexExtrapolator`，可选，✅）
- 若启用外推（`extrapolateLeft/Right`），用已知相邻两烷烃（RT/RI 两点）按**均匀分格**补出中间缺失碳数的 RT：
  - `unit = (RT_C2 − RT_C1) / (c2 − c1)`；缺失烷烃 C_target 的峰宽 `width = (RT1+RT2)/2`，`halfWidth = width/2`
  - 左外推 `start = RT_C1 − unit*(c1−target) − halfWidth`；右外推 `start = RT_C2 + unit*(target−c2) − halfWidth`；中段 `start = RT_C1 + unit*(target−c1) − halfWidth`
  - `RT_target = start + width/2`，RI = target*100，名称标 `-> Extrapolated`
- 另有 `deriveMissingIndices`：在「峰 RT↔RI 映射」上线性插补缺失 100 整倍数 RI 的 RT（`-> Derived`）。
- Source: `impl/RetentionIndexExtrapolator.java`（extrapolateMissingAlkaneRanges / calculateAlkane / getStartRetentionTime）+ `impl/RetentionIndexSupport.deriveMissingIndices`

### 8.5 保留指数「映射/应用」与 reset
- 没有独立的 `retentionindexmapper` 滤波器插件（搜索 `.fetch/` 与 `openchrom/plugins/` 均无 `*retentionindexmapper*`）；RI 映射职责由本插件 `RetentionIndexCalculator` 承担（扫描/峰顶逐个 `setRetentionIndex`）。另有 RI Reset 计算器（id=`...resetri`）把全部 RI 清 0（可选清 `separationColumnIndices`）。
  - Source: `...amdiscalri/plugin.xml`（第二个 ChromatogramCalculatorSupplier）+ `impl/RetentionIndexCalculator.resetIndex`

---

## 9. 分类器/计算器结果 UI 预览流（✅ 全部源码确认）

### 9.1 触发：分类器作为「方法过程」经过程链运行

- 分类器没有独立菜单按钮，而是通过**方法编辑器过程链**触发：`ChromatogramClassifierProcessTypeSupport`（OSGi `@Component(service=IProcessTypeSupplier.class)`，category = `ICategories.CHROMATOGRAM_CLASSIFIER`）把注册表里的每个分类器包装成 `ChromatogramClassifierProcessorSupplier`（继承 `ChromatogramSelectionProcessSupplier`，id/name/description/settingsClass/dataTypes 均取自分类器供应商）。
- 其 `apply(selection, processSettings, messageConsumer, monitor)` 直接调 `classifier.applyClassifier(chromatogramSelection, processSettings, monitor)` 并把 `IProcessingInfo` 消息汇入 `messageConsumer`。即用户在建方法时添加 "Chromatogram Classifier" 步骤 → 方法运行器按 MODULE_03 的引擎循环执行 → 分类结果以 `MeasurementResult` 挂到色谱。
- Source: `...xxd.classifier/core/ChromatogramClassifierProcessTypeSupport.java`（L33-79）

### 9.2 预览宿主：MeasurementResultsPart + ExtendedMeasurementResultUI

- 色谱编辑器侧栏部件 `MeasurementResultsPart`（订阅 `TOPIC_CHROMATOGRAM_XXD_UPDATE_SELECTION`，色谱选中变化即刷新）→ 内含 `ExtendedMeasurementResultUI`。
- `ExtendedMeasurementResultUI.setInput(chromatogram)`：下拉框列出 `chromatogram.getMeasurementResults()` 中 **`isVisible()==true`** 的结果（按名称排序，首项 "No Selection"）；工具按钮：删除选中 / 删除全部 / 复制结果 id 到剪贴板。
- 选中一个结果 → `MeasurementResultUI.update(measurementResult)`：先 `Adapters.adapt(measurementResult, T)`，失败再 `Adapters.adapt(measurementResult.getResult(), T)`，取 `IStructuredContentProvider` / `ITableLabelProvider` / `ColumnDefinitionProvider` / `ISelectionChangedListener`，填充 `ExtendedTableViewer` 的列与行 —— **结果预览 = 表格**（非图）。
- Source: `ux.extension.xxd.ui/parts/MeasurementResultsPart.java` + `swt/ExtendedMeasurementResultUI.java` + `swt/MeasurementResultUI.java`（update L46-74 / adaptTo L76-88）

### 9.3 ratios 适配（community `net.openchrom.xxd.classifier.supplier.ratios.ui`）

- `plugin.xml` 在 `IPeakRatios` / `QualRatios` 上注册 `org.eclipse.core.runtime.adapters` 工厂：
  - `PeakRatiosAdapterFactory` → 共享 `PeakRatioContentProvider`（**RESULTS 模式只显示 `getPeak()!=null` 的行**，SETTINGS 模式显示全部模板行）+ 按子类型分发的 `TimeRatioLabelProvider` / `TraceRatioLabelProvider` / `QuantRatioLabelProvider` + `TimeRatioResultTitles` / `TraceRatioResultTitles` / `QuantRatioResultTitles`（列定义）+ 共享 `PeakRatioSelectionListener`。
  - Qual 同构：`QualRatiosAdapterFactory` → `QualRatioContentProvider` / `QualRatioLabelProvider` / `QualRatioTitles` / `QualRatioSelectionListener`。
- **选中行 → 色谱图回链**：`PeakRatioSelectionListener.handleSelection` 取行数据 `IPeakRatio.getPeak()` → `UpdateNotifierUI.update(display, peak)` → 色谱图跳到该峰。
- molpeak（chemclipse `.ui`）同样在 `ILigninRatios` 上注册 Content/Label/Columns 适配器，进同一张预览表。
- Source: `...ratios.ui/plugin.xml` + `internal/provider/PeakRatiosAdapterFactory.java` + `PeakRatioContentProvider.java`（getElements L42-70）+ `PeakRatioSelectionListener.java`（L48-56）+ `molpeak.ui/plugin.xml`

### 9.4 图表叠加（分析段/噪声段）

- 选中结果 combo 时还触发 `MeasurementResultNotification.select(measurementResult)`（`AbstractNotifications` 广播），注释明示「用于给色谱图取 `ICustomPaintListener` 画分析段」。分段/噪声段在图上的叠加见 §7.5（`AnalysisSegmentPaintListener`）。
- 即 **预览 = 结果表（行↔峰回链） + 图上半透明段叠加**；无独立图形化分类结果图。

---

## 10. Qt 移植要点（面向自研 CDS）

### 10.1 可直接 C++ 化的算法（推荐做）
| 算法 | Qt 实现建议 |
|---|---|
| 峰分辨率 Rs | 直接公式 `2*(tR2-tR1)/(w1+w2)`；w 取 FWHM（拐点切线×50% 高）或标准 4σ 基线宽二选一，文档注明口径 |
| Stein/Dyson 噪声 | 纯数值算法（MAD、mean、median、max-min），Qt 一行 qSort/qMedian 可写；需先有「色谱分段」概念（可先用固定等宽分段，或用基线段代替） |
| SegmentValidatorClassic | 均值穿越计数，纯循环，照搬 |
| ChromatogramSegmentation 等宽分段 | 按扫描序固定等宽切段：`parts=scans/width`、`stop=start+width-1`、余数为末段；段宽默认 9（奇数 5~19），无噪声段时逐级降宽重试。Qt 用 `QVector<Segment{startScan, stopScan, startRT, stopRT}>` 即可，从等宽起步 |
| AMDIS RI（线性插值） | `std::map<int, RIEntry>` 取 lower_bound/upper_bound 两端点线性插值；.cal 解析按空格分隔即可 |
| Time/Quant/Trace/Qual 4 比值分类器 | 全部是纯算术 + 峰元数据读取，无第三方依赖，照搬；Qual 的 leading/tailing/S/N/对称阈值直接移植 |
| Durbin-Watson 统计量 | 公式两行；需要先有 S-G 平滑器（Qt 工程已有，见 MODULE_10） |
| WNC 离子百分比 | 需要 MSD XIC 累加能力，Qt 若有逐扫描离子信号则照搬 |

### 10.2 建议**做但简化**的部分
- **输出挂载**：不必仿 `MeasurementResult`+扩展注册表双通道。Qt 直接让算法返回结构体（`struct RatioResult {峰id; 名称; 期望; 实测; 偏差%}`）存进峰/色谱的「QC 结果表」，UI 用表格视图展示即可。
- **结果预览 UI**：不必仿 `MeasurementResult` 适配器注册表。Qt 让分类算法直接返回结果表结构，`QTableView` + 一个 model 即可；选中行 → 色谱图 `setCursor`/选中对应峰（对照 `PeakRatioSelectionListener` 的 `UpdateNotifierUI.update(peak)` 回链语义）；噪声段/分析段叠加 = 图上半透明矩形，Qt 的 `QGraphicsScene`/自定义 paint 覆盖即可。
- **模板字符串设置**：可保留 `名称|期望|Warn|Error` 多行文本编辑（用户习惯），解析器照搬；或直接做成表格行编辑，两者等价。
- **分类器/计算器扩展点**：Qt 侧若用「注册表 + id 分发」做（仿 MODULE_08 的插件表思路）可以；若只内置 5-6 个 QC 算法，**直接硬编码一个 switch 分发 + 设置结构体**更务实。

### 10.3 建议不做（QC 类可有可无）
- **Durbin-Watson 自动搜 S-G 参数**：三重循环（6×4×24 组合×全谱平滑）在 Qt 里性能可控但收益低——S-G 参数一般由用户经验设定或固定默认。可留作「参数建议」离线工具，不进实时管线。
- **molpeak 木质素分类**：主判定=基峰 m/z 硬编码集合（S/G/pH/C），未命中再用内置库 gerberetal2012.msl 全谱兜底（match 70/70）；强依赖「先标识后分类」流程，超出通用 CDS 范畴。除非产品面向林产/木质素分析，否则跳过（库与阈值细节已文档化于 §4.3，真要移植可只抄基峰集合）。
- **RI 缺失烷烃外推/派生**（Extrapolator）：只在「标样峰不全」时用，算法细节较多、收益边际；先做 8.1 线性插值核心 + .cal 读取即可，外推按需再加。

### 10.4 需要先有的前置数据（依赖核对）
- 峰模型需提供：`tR_at_peak_max`、`FWHM/拐点宽`、`leading/tailing`、`start/stop/center RT`、`integratedArea`（Qual/分辨率/Time 用）。
- 色谱需提供：`getPeaks()` 顺序（须按 RT 排序，分辨率按相邻峰计算）、`扫描RT→峰顶扫描`、每扫描 TIC（D-W/噪声用）、MSD 每扫描每 m/z 丰度（Trace/WNC/Stein 用）。
- 峰识别（`targets` 列表，含名称）是 ratios 3 个模板类分类器的匹配前提（Time/Trace/Quant 按峰名匹配）。Qt 侧对应「鉴定结果表」。

---

## 11. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| CC-A | IChromatogramClassifier 接口（applyClassifier 签名 + getDataTypes）+ 结果 IChromatogramClassifierResult（ResultStatus+description）| xxd.classifier/core/IChromatogramClassifier.java + result/IChromatogramClassifierResult.java + ResultStatus.java + AbstractChromatogramClassifierResult.java | ✅ |
| CC-B | 分类器门面双扩展点（legacy msd + generic xxd 合并读取）+ createExecutableExtension + getChromatogramClassifierSupport | xxd.classifier/core/ChromatogramClassifier.java（EXTENSION_POINT_LEGACY/GENERIC、getConfigurationElements）| ✅ |
| CC-C | 分类器统一写法：settings instanceof + 默认回退 + MeasurementResult 挂载 + setProcessingResult | 各 *RatioClassifier.applyClassifier + MeasurementResult.java L21 | ✅ |
| CC-D | AbstractRatioClassifier.isPeakMatch（按峰识别名匹配）| net.openchrom.xxd.classifier.supplier.ratios/core/AbstractRatioClassifier.java | ✅ |
| CC-E | Time Ratio：deviation=|期望RT−实际RT|/实际RT×100；模板 `名称|RT|Warn|Error` | ...ratios/core/TimeRatioClassifier.calculateRatios + settings/TimeRatioSettings.java | ✅ |
| CC-F | Trace Ratio：actualRatio=100/ref×target；deviation=|100−(100/expectedRatio×actualRatio)|；模板 `名称|ref:target|期望比|Warn|Error` | ...ratios/core/TraceRatioClassifier.calculateRatios L70-117 + settings/TraceRatioSettings.java | ✅ |
| CC-G | Quant Ratio：按 quantitationName+单位匹配；deviation=|期望−实测|/实测×100；getResponseFactor=1±dev/100 | ...ratios/core/QuantRatioClassifier + model/quant/QuantRatio.java | ✅ |
| CC-H | Qual Ratio：leading/tailing 5 级阈值、S/N 5 级阈值、对称因子=|L−R|/|R−L|钳[0,1] 5 级；PeakQuality 枚举 | ...ratios/core/QualRatioClassifier.calculateLeadingTailing/calculateSignalToNoiseRatio/calculateSymmetry + model/qual/PeakQuality.java | ✅ |
| CC-I | ratios 4 分类器注册于 legacy 扩展点 + 模板导出转换器(.tir/.trr/.qar) | net.openchrom.xxd.classifier.supplier.ratios/plugin.xml | ✅ |
| CC-J | Durbin-Watson：d=Σ(ei−ei−1)²/Σei²，e=原值−S-G平滑；三重扫参 derivative/order/width | durbinwatson/processor/DurbinWatsonProcessor.java（calculateDurbinWatsonRating / durbinWatsonMain）+ core/Classifier.java + settings/ClassifierSettings.java（空壳）| ✅ |
| CC-K | WNC：选区内逐 m/z 累加 → %max=100×v/max、%sum=100×v/sum；默认 trace Water:18/…/Column Bleed:207 | wnc/internal/core/support/Calculator.java + settings/ClassifierSettings.java + core/Classifier.java | ✅ |
| CC-L | molpeak：先标识 S/G/H/C → 面积或计数 → 各类占比%；同插件注册 3 标识器 | molpeak/classifier/BasePeakClassifier.calculateLigninRatios + core/Classifier.java + identifier/BasePeakIdentifier.java | ✅ |
| CC-M | IChromatogramCalculator 接口 + 门面（扩展点 chromatogramCalculatorSupplier + 执行后 setDirty）+ 噪声扩展点 noiseCalculationSupplier | xxd.calculator/core/chromatogram/ChromatogramCalculator.java + IChromatogramCalculator.java + core/noise/NoiseCalculator.java + plugin.xml | ✅ |
| CC-N | INoiseCalculator 接口（reset/getId/getName/getNoiseFactor/getSignalToNoiseRatio/getNoiseSegments）+ AbstractNoiseCalculator | model/core/INoiseCalculator.java + AbstractNoiseCalculator.java | ✅ |
| CC-O | 峰分辨率 Rs=2(tR2−tR1)/(w1+w2)；w=getWidthByInflectionPoints()=50%高宽；相邻峰成对 | model/implementation/PeakResolution.calculate + model/core/AbstractPeakModelStrict.getWidthByInflectionPoints + calculator.peak.resolution/core/ChromatogramCalculator.applyCalculator | ✅ |
| CC-P | Stein：段因子=MAD/sqrt(mean)；全局=中位数；S/N=sqrt(I)×factor；引用 Stein GC/MS 文献 | noise.stein/core/NoiseCalculator.java + numeric/Calculations.getMedianDeviationFromMedian | ✅ |
| CC-Q | Dyson：段因子=max−min；用户段均值否则全段中位数；S/N=I/factor；引用 Dyson 文献 | noise.dyson/core/NoiseCalculator.java | ✅ |
| CC-R | SegmentValidatorClassic：均值穿越数 > (n−1+1)/2 才接受 | model/support/SegmentValidatorClassic.java L31-71 | ✅ |
| CC-S | AMDIS RI 线性插值 RIcomp=RIlo+(RIhi−RIlo)(RT−RTlo)/(RThi−RTlo)；floor/ceiling 最近标定点 | model/support/RetentionIndexMath.calculateRetentionIndex | ✅ |
| CC-T | RI 应用到扫描+峰顶扫描（setRetentionIndex，MSD 同步 optimized MS）| amdiscalri/impl/RetentionIndexCalculator.calculateIndex(IChromatogram, ISeparationColumnIndices) | ✅ |
| CC-U | .cal 文件格式（#COLUMN_* 列头 + `RT RI net S/N name` 数据行）+ 3 策略（AUTO/CHROMATOGRAM/FILES）+ 设置字段 | amdiscalri/io/AMDISConverter.parse + impl/CalculatorStrategy.java + settings/CalculatorSettings.java | ✅ |
| CC-V | 烷烃 RI=碳数×100；正则/CAS/IUPAC 查表；内置烷烃库标识（PeakIdentifierFile）| amdiscalri/impl/RetentionIndexCalculator.getAlkaneNumber/getAlkanesByCAS + AlkaneIdentifier.transferAlkaneSettings | ✅ |
| CC-W | 缺失烷烃外推（unit=ΔRT/ΔC，左/中/右外推公式）与派生态（deriveMissingIndices）| amdiscalri/impl/RetentionIndexExtrapolator + RetentionIndexSupport.deriveMissingIndices | ✅ |
| CC-X | 无独立 retentionindexmapper 滤波器；RI 映射由 amdiscalri 承担；另有 RI Reset 计算器 | 搜索 *.fetch/ 与 openchrom/plugins/ 无 *retentionindexmapper*；amdiscalri/plugin.xml + RetentionIndexCalculator.resetIndex | ✅ |
| CC-Y | ChromatogramSegmentation = 挂色谱的 AnalysisSegmentMeasurementResult（isVisible=false）；由 NoiseChromatogramSupport.applyNoiseSettings / addNoiseSegment 创建，Stein/Dyson 只读取不创建 | model/results/ChromatogramSegmentation.java + xxd.calculator/core/noise/NoiseChromatogramSupport.java | ✅ |
| CC-Z | 分段算法=按扫描序固定等宽切段：last=scans%width、parts=(scans-last)/width、末段可短；段=startScan/stopScan(=start+width−1)+RT；width≥3 且 scans≥width | model/support/AnalysisSupport.getChromatogramSegments/initializeAnalysisSegments + AnalysisSegment + ChromatogramSegment | ✅ |
| CC-AA | 段宽默认 9（PreferenceSupplier DEF_SEGMENT_WIDTH），合法奇数 5..19；设置默认 "7"；无噪声段时 SegmentWidth.getLower 在 {5,7,9,11,13,15,17,19} 逐级降宽重试 | xxd.calculator/settings/NoiseChromatogramClassifierSettings + preferences/PreferenceSupplier + support/model/SegmentWidth.getLower | ✅ |
| CC-AB | 触发链：NoiseChromatogramClassifier（legacy 分类器，MSD/WSD/CSD）→ applyNoiseSettings；NoiseSegmentSetter → addNoiseSegment（用户选区奇数宽≥5） | xxd.calculator/core/noise/NoiseChromatogramClassifier.java + NoiseSegmentSetter.java + NoiseChromatogramSupport.addNoiseSegment | ✅ |
| CC-AC | 分段图上叠加：AnalysisSegmentPaintListener（swtchart ICustomPaintListener，半透明矩形+段界竖线，drawBehindSeries）；AdapterFactory 适配 4 类型并经 plugin.xml adapters 注册 | ux.extension.xxd.ui/segments/AnalysisSegmentPaintListener + adapters/AnalysisSegmentMeasurementResultAdapterFactory + plugin.xml | ✅ |
| CC-AD | molpeak 主判定=基峰 m/z 硬编码集合（S/G/pH/C 四组）非谱库；未命中→PeakIdentifierFile 用 gerberetal2012.msl 全谱兜底（targets=10、minFwd/Rev=70.0、preOptimization=false） | molpeak/identifier/BasePeakIdentifier.getIdentification + setFileIdentifierSettings | ✅ |
| CC-AE | molpeak 内置库：standards/gerberetal2012.msl（≈百谱，COMMENTS 标 P/H/G/S/C/U）、standards/references.msl（Guaiacyl/Lorenzyl）；S0165237012000137.ris=文献 RIS 非谱库；limitMatchFactor=80/matchQuality=80 | molpeak/standards/*.msl + settings/AbstractBasePeakSettings + settings/S0165237012000137.ris | ✅ |
| CC-AF | molpeak LibraryService：SYRINGYL→单离子谱 m/z156/1000；GUAIACYL/pH→references.msl 按名过滤 | molpeak/identifier/BasePeakIdentifier.getMassSpectra + getSyringyl | ✅ |
| CC-AG | 分类器经方法编辑器过程链触发：ChromatogramClassifierProcessTypeSupport（IProcessTypeSupplier，category=CHROMATOGRAM_CLASSIFIER）→ ChromatogramClassifierProcessorSupplier.apply → applyClassifier | xxd.classifier/core/ChromatogramClassifierProcessTypeSupport.java | ✅ |
| CC-AH | 结果预览=表格：MeasurementResultsPart（订阅 XXD_UPDATE_SELECTION）→ ExtendedMeasurementResultUI（combo 列 isVisible 结果）→ MeasurementResultUI.update 经 Adapters.adapt(result/result.getResult()) 取 Content/Label/Columns/Selection 填 ExtendedTableViewer | ux.extension.xxd.ui/parts/MeasurementResultsPart.java + swt/ExtendedMeasurementResultUI.java + swt/MeasurementResultUI.java | ✅ |
| CC-AI | ratios.ui 适配：PeakRatiosAdapterFactory（RESULTS 只显示 peak!=null 行）+ 各 RatioLabelProvider/ResultTitles + PeakRatioSelectionListener→UpdateNotifierUI.update(peak) 回链色谱图；Qual 同构；molpeak.ui 在 ILigninRatios 上同机制 | net.openchrom.xxd.classifier.supplier.ratios.ui/plugin.xml + internal/provider/* + molpeak.ui/plugin.xml | ✅ |

### 待验证 / 缺项（❓）
| # | 缺口 |
|---|---|
| CC-? | AMDIS CalcRI 在色谱导入时「自动标定」的 UI 编排（AlkanePatternDetectorMSD 全流程用到峰检测+积分+标识，Qt 是否按此编排视产品取舍）|

---

## 附：本模块涉及的插件清单（供复刻检索）

- `net.openchrom.xxd.classifier.supplier.ratios`（+ `.ratios.ui`）— OpenChrom 社区
- `org.eclipse.chemclipse.chromatogram.xxd.classifier` / `.ui` — 分类器核心（接口/门面/扩展点）
- `org.eclipse.chemclipse.chromatogram.xxd.classifier.supplier.durbinwatson`（+ `.ui`）
- `org.eclipse.chemclipse.chromatogram.msd.classifier.supplier.wnc`（+ `.ui`）
- `org.eclipse.chemclipse.msd.classifier.supplier.molpeak`（+ `.ui`）
- `org.eclipse.chemclipse.chromatogram.xxd.calculator`（+ `.ui`）— 计算器/噪声核心
- `org.eclipse.chemclipse.chromatogram.xxd.calculator.peak.resolution`（+ `.ui`）
- `org.eclipse.chemclipse.chromatogram.xxd.calculator.supplier.noise.stein` / `.dyson`
- `org.eclipse.chemclipse.chromatogram.xxd.calculator.supplier.amdiscalri`（+ `.ui`）
