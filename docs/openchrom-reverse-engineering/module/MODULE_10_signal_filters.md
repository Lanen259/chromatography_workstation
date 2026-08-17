# MODULE_10 — 信号预处理滤波器族（平滑 / 基线扣除 / 归一化 / 零点 / 扫描操作）

> **状态：🟡 分析中（滤波器扩展点/接口 ✅、Savitzky-Golay 全链含卷积系数计算 ✅、baselinesubtract ✅、9 个简单滤波器 ✅、MSD 滤波 8 族含 splitter/centroiding 全算法 ✅、scan 套件 12 子滤波器全 ✅；遗留：VSD/ISD 域未展开 ❓）**
> 服务于自研 CDS Qt 工程 `core_processing` 滤波模块。严格遵守：✅ 源码确认 vs ⚠️ 推测，不许混。

---

## 1. 滤波扩展点与接口（✅ 全部源码确认）

### 1.1 滤波器接口（`org.eclipse.chemclipse.chromatogram.filter` 插件）

- **`IChromatogramFilter`**（`.../filter/core/chromatogram/IChromatogramFilter.java`）：
  - 核心方法：`IProcessingInfo<IChromatogramFilterResult> applyFilter(IChromatogramSelection chromatogramSelection, IChromatogramFilterSettings chromatogramFilterSettings, IProgressMonitor monitor)`
  - 配套校验：`validate(...)`（= `validateChromatogramSelection` + `validateFilterSettings`，见 `AbstractChromatogramFilter`）、`validateChromatogramSelection`（selection/chromatogram 非空）、`validateFilterSettings`（settings 非空）✅
- **`IChromatogramFilterMSD`**（`...msd.filter/core/chromatogram/`）——MSD 专用变体，`applyFilter(IChromatogramSelectionMSD, ...)`；实现骨架 `AbstractChromatogramFilterMSD`（同样三件套校验）✅
- **结果对象 `IChromatogramFilterResult`**：`ResultStatus`（OK / EXCEPTION）+ `getDescription()` 文本 ✅
- **门面 `ChromatogramFilter`**（static，`.../filter/core/chromatogram/ChromatogramFilter.java`）：
  - `applyFilter(selection, settings, filterId, monitor)`：按 filterId 从扩展注册表实例化 `IChromatogramFilter` → 调用 → 异常兜底 ✅
  - `getChromatogramFilterSupport()`：扫注册表，逐个构造 `ChromatogramFilterSupplier`（id/description/filterName/filterSettings→settingsClass）✅
  - 扩展点常量：`EXTENSION_POINT = "org.eclipse.chemclipse.chromatogram.filter.chromatogramFilterSupplier"`；元素属性：`id / description / filterName / filter / filterSettings`；`createExecutableExtension(FILTER)` 反射实例化 ✅
- **`IChromatogramFilterSupplier`**：`getId / getDescription / getFilterName / getSettingsClass / getLiteratureReferences` ✅

### 1.2 类型化扩展点（✅ plugin.xml + 各插件扩展声明）

| 域 | 扩展点 ID | 元素 | 说明 |
|---|---|---|---|
| 通用 | `org.eclipse.chemclipse.chromatogram.filter.chromatogramFilterSupplier` | `ChromatogramFilterSupplier` | 跨域基础 |
| MSD | `org.eclipse.chemclipse.chromatogram.msd.filter.chromatogramFilterSupplier` / `.peakFilterSupplier` / `.massSpectrumFilterSupplier` | ChromatogramFilterSupplier / PeakFilterSupplier / MassSpectrumFilterSupplier | 3 个扩展点 |
| CSD | `org.eclipse.chemclipse.chromatogram.csd.filter.chromatogramFilterSupplier` / `.peakFilterSupplier` | 同 | 2 个 |
| WSD | `org.eclipse.chemclipse.chromatogram.wsd.filter.chromatogramFilterSupplier` / `.peakFilterSupplier` | 同 | 2 个 |

- S-G 注册示例（`xxd.filter.supplier.savitzkygolay/plugin.xml`）：MSD 扩展 `...msd.filter.chromatogramFilterSupplier`，id=`...msd.filter.supplier.savitzkygolay`，filter=`...core.ChromatogramFilterMSD`，filterSettings=`...settings.ChromatogramFilterSettingsMSD`；CSD/WSD 各自同构；质谱版注册于 `...msd.filter.massSpectrumFilterSupplier`（id 后缀 `.massspectrum`）✅
- **settings 绑定**：扩展属性 `filterSettings=` 指向一个 Jackson 注解的 POJO（`@JsonProperty` 字段 + `@IntSettingsProperty/@FloatSettingsProperty/@StringSettingsProperty` 校验范围），运行时反射实例化（`createExecutableExtension(FILTER_SETTINGS)`）✅；序列化由 Jackson 完成（方法/方法文件持久化用）
- 新版还叠加 OSGi DS 注册：`SavitzkyGolaySmoothingFilter` `@Component(service = {Filter.class, IScanFilter.class, ITotalScanSignalsFilter.class})` —— 处理管线（MODULE_03）用它，经典扩展点（MODULE_08）用前者，双机制并存 ✅

### 1.3 统一写法模板（✅ 各简单滤波器共性）

```text
applyFilter(selection, settings, monitor)
 ├─ validate(selection, settings)            // 空校验
 ├─ 业务处理（逐扫描 或 操作 ITotalScanSignals）
 ├─ chromatogram.setDirty(true)              // 打脏标记
 └─ setProcessingResult(ChromatogramFilterResult(OK, 描述))
```

---

## 2. Savitzky-Golay 平滑（✅ 全链源码确认，Qt 可整段照搬）

插件 `org.eclipse.chemclipse.chromatogram.xxd.filter.supplier.savitzkygolay`（作者 Lorenz Gerber，参考文献 DOI 10.1021/ac60214a047 = Savitzky & Golay 1964）。

### 2.1 设置字段（`settings/ChromatogramFilterSettings` 系）

| 字段 | 基类 | CSD | MSD | WSD | ISD | 质谱版 |
|---|---|---|---|---|---|---|
| `order`（多项式阶数）| 默认 2，范围 2–5 | **范围 1–5** | 2–5 | 2–5 | 2–5 | 默认 3，2–5 |
| `width`（滤波宽度）| 默认 5，范围 5–51，**必须奇数** | 同 | 同 | 同 | 同 | 默认 21，5–51（不要求注解奇数，构造器会纠正）|
| `derivative`（导数阶）| 字段存在但 `@JsonIgnore`，默认 0 | **恒 0**（`getDerivative()` 返回 0，`setDerivative(≠0)` 仅 warn）| 恒 0 | 恒 0 | — | 恒 0 |
| `perIonCalculation` | — | — | 默认 **true**（逐离子）| — | — | — |

- **说明**：CSD/MSD/WSD 各 settings 子类重写 `getDerivative()=0` —— **色谱图平滑不支持导数输出**（MSD 的 `setDerivative` 明确 `logger.warn("Derivative is not supported")`）✅；导数支持只存在于底层 `SavitzkyGolayFilter` 类与质谱数组 API 中（但质谱版 settings 同样把 derivative 锁 0）✅
- 质谱版 `MassSpectrumFilterSettings.appliesToMassSpectrumTypes()` = `[PROFILE]`（仅轮廓谱）✅

### 2.2 算法类 `SavitzkyGolayFilter`（✅ `processor/SavitzkyGolayFilter.java` 完整）

**构造器纠正（可直接照搬）：**
```text
width      = max(5, 1 + 2*((width-1)/2))        // 强制奇数且 ≥5
order      = min(max(0, order), 5, width-1)     // 0..5 且 < width
derivative = min(max(0, derivative), order)     // 0..order
```

**卷积系数计算（运行时最小二乘，非查表）：**
1. `getNormalEquations(width, order)`：法方程矩阵 = `width × (1+order)`，行 i 的 x 位置 `t1 = -p .. p`（p=(width-1)/2），列 j 为幂指数 `t2 = 0..order`，元素 `= t1^t2`（`generateNormalMatrix`：`Math.pow(t1[i][j], t2[i][j])`）✅
2. 解法：`QRDecomposition(normalEquations)` 取 Q/R → `SingularValueDecomposition` 取秩 r → `q2 = Q 前 r 列`，`r3 = R 前 r 行` → `weights = LU(r3)^-1 · (q2^T · I_width)` → 得 `weights[order+1][width]` ✅（依赖 Apache Commons Math3：LUDecomposition/QRDecomposition/SingularValueDecomposition/MatrixUtils/StatUtils/CombinatoricsUtils）
3. `filterCoefficients = weights[derivative]`（取第 derivative 行作卷积核）✅
4. `derivativeCoefficients`：derivative==0 时全 1；>0 时按 `calculateCoefficients` 的组合乘积公式计算（用于导数缩放）✅
5. `getFactorialAdjustedFilterCoefficients()`：`coeff * factorial(derivative)`（导数阶乘校正，供显示用）✅

**三区处理 `apply(ticValues[])`（✅）：**
```text
processStart: 前 p 行     y[i] = Σ_j x[j] · (uStart[i] × startStopWeights[·][j])    // p=(width-1)/2
processMiddle: i ∈ [p, n-p)  y[i] = Σ_{k=-p..p} x[i+k] · filterCoefficients[k+p] · derivativeCoefficients[0]
processEnd:   后 p 行     y[i] = Σ_j x[n-width+j] · (uStop[m] × startStopWeights[·][j])
```
- 中间段 = 标准对称卷积核；**首尾 p 个点用 uStart/uStop（法方程前/后 p 行）加权** —— 边界不平移、不截断，保留原始点数 ✅

### 2.3 处理流程（`SavitzkyGolayProcessor` + 两个 Operation，✅）

**TIC 路径 `smooth(IChromatogramSelection, validatePositive, settings, monitor)`：**
1. `TotalScanSignalExtractor.getTotalScanSignals(selection, validatePositive)` 取选择区总信号
2. `double[] sgTic = smooth(totalScanSignals, settings)` → `new SavitzkyGolayFilter(order,width,derivative).apply(...)`
3. 逐扫描 `scan.adjustTotalSignal((float)sgTic[i])`；**`validatePositive && intensity<=0` → 置 `0.1f`**（MSD 不允许 0/负；FID 允许负值）✅
4. `chromatogram.setDirty(true)`；结果 "Smoothed N scans." ✅

**MSD 逐离子路径 `SavitzkyGolayPerIonOperation`（✅ 默认 perIon=true）：**
1. 深拷贝全部扫描（undo 快照）→ `new ExtractedMatrix(selection)` 建 `double[scans][ions]`（**行=扫描，列=离子，仅名义质量**；高分辨谱抛 `IllegalArgumentException` → 结果 EXCEPTION "High Resolution Data is not supported."）✅
2. `SavitzkyGolayProcessor.apply(matrix, settings)`：**逐列（每离子一个 S-G）平滑**，`<0 → 0.0` 钳位 ✅
3. `extractedMatrix.updateSignal()` 把矩阵写回扫描（0 丰度离子不建）✅

**MSD 总信号路径 `SavitzkyGolayTotalScanSignalOperation`（perIon=false 时）：**
- `getTotalScanSignals(selection, true)` → `apply(signals, settings)` → `setNegativeTotalSignalsToZero()` → 逐扫描 `adjustTotalSignal` ✅

### 2.4 质谱平滑（`MassSpectrumFilter` + `calculator/FilterSupplier`，✅）

- `applyFilter(List<IScanMSD>, settings)`：`new FilterSupplier().applySavitzkyGolayFilter(massSpectra, derivative, order, width)`
- `FilterSupplier.applySavitzkyGolayFilter`：对每个质谱，离子按 m/z 排序 → 取丰度数组 → S-G 平滑 → 逐离子 `setAbundance`（超限则跳过并 warn）✅
- 质谱数组版：`SavitzkyGolayProcessor.apply(double[][] matrix, ...)`（`matrix[scan][ion]` 语义下按列平滑，负值钳 0）——与 PerIonOperation 同一实现 ✅

### 2.5 扩展注册（✅ plugin.xml）

| 注册 | filter 类 | settings 类 | 扩展点 |
|---|---|---|---|
| MSD | `core.ChromatogramFilterMSD` | `settings.ChromatogramFilterSettingsMSD` | `...msd.filter.chromatogramFilterSupplier` |
| CSD | `core.ChromatogramFilterCSD` | `settings.ChromatogramFilterSettings` | `...csd.filter.chromatogramFilterSupplier` |
| WSD | `core.ChromatogramFilterWSD` | `settings.ChromatogramFilterSettings` | `...wsd.filter.chromatogramFilterSupplier` |
| 质谱 | `core.MassSpectrumFilter` | `settings.MassSpectrumFilterSettings` | `...msd.filter.massSpectrumFilterSupplier` |

- `core/ChromatogramFilterMSD.process`（✅）：`settings.getPerIonCalculation()` ? PerIonOperation : TotalScanSignalOperation，经 Eclipse undo/redo 框架执行（`OperationHistoryFactory`）✅
- OSGi 管线版：`core/SavitzkyGolaySmoothingFilter` 实现 `IScanFilter`（质谱）/`ITotalScanSignalsFilter`（总信号）两接口 ✅

---

## 3. baselinesubtract 基线扣除（✅ 全链源码确认，交叉引用 MODULE_04 §3.3 / PK-AU）

插件 `org.eclipse.chemclipse.chromatogram.xxd.filter.supplier.baselinesubtract`。

- `core/ChromatogramFilter.applyFilter`：validate → `BaselineSubtractProcessor.removeBaseline(selection)` → `setDirty(true)` → 结果 "The baseline was successfully removed." ✅
- **`processor/BaselineSubtractProcessor.removeBaseline`（✅ 完整方法体）**：
```text
startScan = chromatogram.getScanNumber(selection.getStartRetentionTime())
stopScan  = chromatogram.getScanNumber(selection.getStopRetentionTime())
baselineModel = chromatogram.getBaselineModel()          // ★ 用色谱图已挂载的基线模型
for i in startScan..stopScan:
    backgroundSignal = baselineModel.getBackground(scan.getRetentionTime())   // 段内两点线性插值
    if NaN: continue
    adjustedSignal = scan.getTotalSignal() - backgroundSignal
    if adjustedSignal > 0: scan.adjustTotalSignal(adjustedSignal)
    else: scansToRemove.add(i)                             // ≤0 → 整扫描标记删除
批量 removeScan（offset 校正）→ selection.reset(false) → recalculateScanNumbers()
→ getPeaks().clear()   // 清除已检出峰
→ baselineModel.removeBaseline()  // 删除基线模型
```
- **设置：`settings/ChromatogramFilterSettings` 空壳，无参数** —— 无覆写选项，行为固定为"选择区减背景 + 删≤0扫描 + 清峰 + 删基线"（破坏性操作）✅
- 同插件的 `core/ChromatogramSubtractor`（✅ 目录确认存在，非滤波器入口）：主色谱 − 扣减色谱逐扫描相减（MSD/WSD 逐离子/波长相减，≤0 删除）——见 MODULE_04 §3.3 已述 ✅

---

## 4. 简单滤波器（✅ 每个算法均完整读实现）

统一架构：`extends AbstractChromatogramFilter`（mediannormalizer 例外，`extends AbstractChromatogramSignalFilter` 直接操作 `ITotalScanSignals`）。

| 滤波器 | 设置 | 算法一句话（✅ 源码） |
|---|---|---|
| **invert** | 空 | 全色谱逐扫描 `adjusted = chromatogram.getMaxSignal() - scan.getTotalSignal()`（**作用于整个色谱图，不只选择区**）|
| **multiplier**（含 divisor）| `multiplier` float 默认 1（1e-12..MAX）| `ModifierChromatogramFilter`：选择区总信号 `×/÷ factor`（TotalScanSignalsModifier.multiply/divide，负→0），逐扫描回写，**≤0 → 0.1f 下限** |
| **rtshifter**（shift/stretch/gapfiller/scandensity）| `millisecondsShift` int 默认 0（±任意）、`shiftAllScans` bool 默认 true | `RetentionTimeShifter`：`newRT = RT + shift`；newRT 越出相邻扫描边界（左=前一扫描 RT，右=后一扫描 RT）→ **删扫描**，否则 `setRetentionTime(newRT)`；随后 `removeMarkedScans` + `adjustScanDelayAndRetentionTimeRange` |
| **normalizer** | `normalizationBase` float 默认 **1000**（≥1）| 选择区 `TotalScanSignalsModifier.normalize(signals, base)`：`factor = base / max`，`signal ×= factor`（峰值缩放到 base）；回写 ≤0 → 0.1f |
| **meannormalizer** | 空 | 选择区 `meanNormalize`：`mean = |均值(所有信号)|`（0 抛异常），`signal /= mean`，负→0；回写 ≤0 → 0.1f |
| **mediannormalizer** | 空 | 选择区 `medianNormalize`：排序取中位数（偶数→两中值均值，0 抛异常），`signal /= median`，负→0 |
| **unitsumnormalizer** | 空 | 选择区 `unitSumNormalize(signals, areaSumIntensity)`，`areaSumIntensity = chromatogram.getTotalSignal()`（全色谱总信号），`signal /= areaSum`；回写无 0.1f 下限 |
| **zeroset**（CSD/WSD）| 空 | `minSignal < 0` 时全色谱逐扫描 `adjusted = totalSignal + (-minSignal)`（整体上移，最低点归 0）；**仅 CSD/WSD 注册** |
| **scan**（套件）| 见下 | 扫描操作套件（非单一"范围截取"，见 4.1）|

> 归一化公共实现（✅ `model/signals/TotalScanSignalsModifier.java`）：`normalize`（base/max 缩放）、`meanNormalize`（|均值| 除）、`medianNormalize`（中位数除）、`multiply/divide`、`calculateMovingAverage`（居中窗口滑动平均）、`unitSumNormalize`（总面积除）——本任务 6 个滤波器全部复用它。

### 4.1 scan 插件（✅ 全部 12 个子滤波器算法已读）

`org.eclipse.chemclipse.chromatogram.xxd.filter.supplier.scan` —— 扫描操作工具族。**注册方式（修正此前 ⚠️ 猜测）**：plugin.xml 以经典 `chromatogramFilterSupplier` 扩展点注册 **11 条**；仅 `FilterScanSelector` 走 OSGi `@Component(service=IProcessTypeSupplier.class)` + `OSGI-INF/*.xml` —— **不是**"本版基本走 OSGi 管线"，主流仍是经典扩展点 ✅

- **`calculator/ScanProcessor`**（✅，被 Density/Duplicator 复用）：
  - `recalculateScans(chromatogram, scanIntervalTarget, mergeScans)` —— 按目标扫描间隔重建网格：RT 已存在→保留原扫描；缺失→**相邻扫描两点线性插值**（`Equations.createLinearEquation(...).calculateY(rt)`）生成新 CSD/MSD/WSD 扫描（MSD 可选合并前后离子、0 丰度剔除）✅
  - `increaseScansDensity(chromatogram, n, mergeScans)` —— 相邻扫描间线性插入 **n 个等分点**（RT 与总信号同时线性内插，`parts = n+1`）✅
- **`calculator/DensityCalculator`**（⚠️ 修正）：算法存在（sps→目标间隔→factor→加/减密度/微调对齐），但**全代码库无任何调用 → 本版死代码**；真实密度处理由 `FilterScanDensity` 直接内联完成（见下）✅
- **`core/FilterScanClipper`**（✅）：按扫描号列表（空格分隔字符串）删除指定扫描，`recalculateScanNumbers` + 选择区 reset ✅
- **`core/FilterScanSelector`**（✅，OSGi `IProcessTypeSupplier`，settings=`ScanSelectorOption`+`scanSelectorValue`）：按 扫描号/RT(ms)/RT(min)/保留指数 换算目标扫描号，在选择区内则 `chromatogramSelection.setSelectedScan(scan)`，否则抛 `FilterException` warn ✅
- **`core/FilterScanDensity`**（✅）：设置 `scansPerSecond`(默认 2, 1–1000)、`mergeScans`(true)、`processReferencedChromatograms`(true)；`scanIntervalTarget = round(1000/sps)`，当前间隔≠目标时 `recalculateScans` 重建网格 → MSD 剔 0 丰度离子 → `replaceAllScans` → 重算间隔/延迟/循环号；可选递归处理 referenced 色谱图 ✅
- **`core/FilterScanMerger`**（✅，experimental，设置空壳）：两两相邻扫描合并到 **RT 中点**，`totalSignal = (s1+s2)/2`（均值）；MSD 离子取并集（`addIons(..., true)` 同 m/z 丰度叠加）、WSD/VSD 按波长/波数累加；`newScanInterval = scanInterval × 2`，`replaceAllScans` 扫描数减半 ✅
- **`core/FilterCleaner`**（✅，设置空壳）：删选择区空扫描——MSD `isEmpty()`（先 `enforceLoadScanProxy`）/ CSD `getTotalSignal()==0` / WSD `getScanSignals().isEmpty()`；`removeScan` 带 removeCounter 偏移校正 → `recalculateScanNumbers` ✅
- **`core/FilterDuplicator`**（✅）：设置 `mergeScans`(true)；`increaseScansDensity(chromatogram, 1, mergeScans)` 每对相邻扫描间插 **1 个线性中点** → `replaceAllScans` → 复用 `FilterDelayInterval`（`resetRetentionTimes=false`）✅
- **`core/FilterRemover`**（✅）：设置 `scanRemoverPattern`(默认 "XO"，regex `^[OX]+`)；`ScanRemoverPattern` 解析 X=删/O=留，对选择区扫描**循环匹配**模式（到尾回绕，`position` 归零）；选择区外扫描一律保留 → `replaceAllScans` ✅
- **`core/FilterDelayInterval`**（✅）：设置 `resetRetentionTimes`(false)；`ChromatogramSupport.calculateScanIntervalAndDelay` 重算扫描延迟/间隔，可选 `recalculateRetentionTimes` ✅
- **`core/FilterBufferIdentifier`**（✅）：设置 `bufferOption`(BUFFER_TARGERTS/RESTORE_TARGERTS/CLEAR_BUFFER)；BUFFER=把选择区扫描的鉴定 target 集合拷贝进 `BufferedScanTargets`（`TreeMap<RT, Set<IIdentificationTarget>>`）作为 `IMeasurementResult` 挂在色谱上；RESTORE=按 RT 区间回写；CLEAR=删缓冲 ✅
- **`core/FilterDeleteIdentifier`**（✅）：设置 `deleteScanIdentifications`(默认 false 确认开关)；选择区逐扫描 `scan.getTargets().clear()` ✅
- **`core/FilterObfuscator`**（✅）：设置 `scans`(true)、`peaks`(false)；脱敏——MSD 全离子替换为 1 个随机 m/z(18–250) 单离子（保留总信号）；VSD 单信号随机波数 660–4000（红外/拉曼类型保留）；WSD 单信号随机波长 380–750；峰则脱敏峰最高点扫描 ✅
- **`core/FilterRetentionIndexSelector`**（✅）：设置 `searchColumn`(默认色谱柱型)、`caseSensitive`、`removeWhiteSpace`、`matchPartly`、`separationColumnTypeFallback`、`retentionIndexOption`(AUTO/MIN/MEAN/MEDIAN/MAX)、`deleteUnrelatedIndices`；对选择区已鉴定扫描/峰：`ColumnIndexSupport.getColumnIndexMarker` 匹配列标记 → 排序 → 按选项取 RI（AUTO=最接近当前保留指数的标记）→ `libraryInformation.setRetentionIndex`；可选删不匹配标记 ✅

---

## 5. MSD 滤波器（✅ 8 族全部核心算法已读）

### 5.1 CODA（`...msd.filter.supplier.coda`，✅ 完整算法）

- 设置 `FilterSettings.codaThreshold` float 默认 **0.75**（0–1，step 0.05）；移动平均窗硬编码 `MOVING_AVERAGE_WINDOW = 5`；参考文献 Windig/Phalp/Payne 1996（DOI 10.1021/ac960435y）
- **MCQ 计算 `CodaCalculator.getMCQValue(signals, windowSize, ion)`（✅）**：
  1. 取该离子每扫描丰度数组（缺失→0）
  2. `scaleToEuclidianLength`：除以向量欧氏长度
  3. `smooth(values, window)`：窗口 5 的**移动平均**（只填前 n-window+1 个）
  4. `scaleToStandardizedLength`：z-score `(v-mean)/std`
  5. `mcq = (1/√(scans-window)) × Σ(euclid[i] × standardized[i])` —— 相似度（噪声离子两者正交→mcq 低）
- **过滤**（`MassChromatographicQualityResult.calculateMassChromatographicQuality` + `core/ChromatogramFilterMSD.applyCodaFilter`）：逐名义离子算 mcq，**mcq < codaThreshold → 加入 excludedIons**，然后逐扫描 `massSpectrum.removeIons(excludedIons)` ✅；另计算 `dataReductionValue = mcq≥阈值离子数 / 总离子数`
- 用途：**自动识别并剔除"质量色谱与平滑后标准化信号不相关"的离子**（噪声/背景离子），保留峰相关离子

### 5.2 denoising（`...msd.filter.supplier.denoising`，✅ 完整算法）

- 设置（`settings/FilterSettings`）：`ionsToRemove`（默认 "18;28;84;207"）、`ionsToPreserve`（默认 "103;104"）、`adjustThresholdTransitions`（true）、`numberOfUsedIonsForCoefficient`（默认 1，1–20）、`segmentWidth`（默认 13，5–19 奇数）
- **`Denoising.applyDenoisingFilter`（✅ 五步）**：
  - A. 按 `ionsToRemove` 把指定离子丰度置 0（全选择区）
  - B. 可选 `ExtractedIonSignalsModifier.adjustThresholdTransitions`（Stein 等：处理 0↔非0 阈值跳变）
  - C. `msd.model.noise.Calculator.getNoiseSegments(signals, ionsToPreserve, segmentWidth)` 切噪声段
  - D. 逐段：段内各扫描减去"噪声质谱 × 相关系数"；**相关系数 = 取噪声丰度 Top N 离子（N=numberOfUsedIonsForCoefficient）的 `scanAbundance/noiseAbundance` 均值**（`calculateCoefficient`）；`abundance - factor×noiseAbundance`，≤0 → 0；段边界按 `width/2` 错开防重叠（`calculateLeadingScans/TailingScans`）
  - E. 经 `DenoiseOperation`（undo 友好）写回；噪声质谱列表挂在 `IMeasurementResult` 上供 UI 查看 ✅
- 用途：**扣除"噪声段"背景质谱**（如柱流失），对选择区整体去噪

### 5.3 ionremover（`...msd.filter.supplier.ionremover`，✅）

- 设置：`ionsToRemove` 字符串（默认 "18 28 84 207"，空格分隔）+ `markedTraceModus`（INCLUDE=删列表内离子 / **EXCLUDE=保留列表内、删其余**）
- `ScanProcessor.apply(List<IScanMSD>, settings)`：`TraceFactory.parseTraces` 解析 m/z 列表 → `MarkedTraces` → 逐质谱 `massSpectrum.removeIons(markedIons)` ✅
- 三个入口：`ChromatogramFilter`（色谱全选择区）、`MassSpectrumFilter`（单质谱）、`PeakFilter`（峰）——同一算法 ✅
- 用途：**按 m/z 名单删离子**（本底/溶剂峰）

### 5.4 subtract（`...msd.filter.supplier.subtract`，✅ 完整算法）

- 设置：`subtractMassSpectrum`（IScanMSD 对象）、`useNominalMasses`（默认 true）、`useNormalize`（默认 true）
- **`SubtractCalculator`（✅）**：
  1. `getMassSpectrumMap`：深拷贝扣除谱 → 可选 `normalize(100)`（峰高归一化到 100）→ 名义模式取 `ExtractedIonSignal` 逐整数 m/z（丰度>0）；否则保留精确 m/z → `Map<m/z, 丰度>`
  2. `adjustIntensityValues`：对目标谱逐离子：`useNormalize` → `abundance -= (subIntensity/100) × abundance`（相对扣除）；否则 `abundance -= subIntensity`（绝对扣除）；`≤0 → 删除该离子` ✅
  3. 色谱入口额外 `chromatogram.getChromatogramIntegrationEntries().clear()` + `getBackgroundIntegrationEntries().clear()`（积分结果失效）✅
- 用途：**扣除参考背景谱/空白谱**（归一化模式下按强度比例扣，最常用）

### 5.5 xpass（`...msd.filter.supplier.xpass`，✅ 核心算法）

- **`filter/XPassFilter`（✅）**：
  - `nominalize(spectrum)`：`CondenseMassSpectrumCalculator` 把高分辨离子**合并到名义 m/z**（丰度叠加），重建谱（`ChromatogramFilterNominalize` 逐扫描调它，可选递归引用谱）
  - `applyHighPass(spectrum, n)`：按丰度**降序**排序，**保留 Top n，删其余**（`HighPassIonsFilter` 峰级，`numberHighest` 默认 5，settings 限定 CENTROID 谱）
  - `applyLowPass(spectrum, n)`：按丰度**升序**排序，保留 Bottom n（`LowPassScanFilter` 谱级）
- 另有 `CutOffMassSpectrumFilter`/`CutOffScanFilter`（m/z 阈值截断）、`HighPass/LowPassScanFilter`、`NominalizeScanFilter/PeakFilter` ✅
- 用途：**谱精简**——高/低丰度离子保留 n 个、名义化、m/z 截断

### 5.6 centroiding（`...msd.filter.supplier.centroiding`，✅ 全链确认，峰来源已定位）

- `core/MassSpectrumFilter.centroidisation`（✅ 完整方法体）：validate（须 `IStandaloneMassSpectrum` 且 `MassSpectrumType.PROFILE` 且 `getPeaks()` 非空）→ `removeAllIons()` → 把每个 `IMassSpectrumPeak`（`getIon`/`getAbundance`）转成 `new Ion(mz, abundance)` 加入 → `getPeaks().clear()` → `setMassSpectrumType(CENTROID)` → `setDirty(true)` ✅
- **轮廓峰来源（❓ 已定位，与"局部最大值/阈值峰检测"无关）**：`IStandaloneMassSpectrum.getPeaks()` 是 `AbstractStandaloneMassSpectrum` 里一个普通 `ArrayList<IMassSpectrumPeak>`（字段 `peaks`，无 setter，返回可变列表）。全代码库仅两处往它填充 `new MassSpectrumPeak`：**CML 转换器** `MassSpectraReader.readPeaks` 与 **mMass 转换器** `AbstractMassSpectrumReader.readPeakList` —— 均是从数据文件自带的 `<peaklist>/<peak>` 元素**读入**峰列表；另有 xpass 插件的 `DeleteMassSpectrumPeaksOperation`（undo 友好的增删）与 UI 展示部件（`MassSpectrumPeakListPart`）。**OpenChrom 没有任何"由轮廓谱自动挑峰"的算法** —— 本滤波是纯粹的"已存峰列表 → 离子"转换器，峰必须先在数据文件/内存中已存在 ✅
- 注册（plugin.xml）：MSD `massSpectrumFilterSupplier`，filterName="Centroiding"，settings=`MassSpectrumFilterSettings.appliesToMassSpectrumTypes()=[PROFILE]`（描述 "Converts profile mass spectra into a centroided ones."）✅
- 用途：把轮廓谱转质心谱（前提是谱内已挂载峰列表）

### 5.7 backfolding（`...msd.filter.supplier.backfolding`，✅ 完整算法）

- 文献：Ghosh & Anderegg 1989（差分 GC-MS）；Pool, deLeeuw, van de Graaf 1996/1997
- 设置：`getNumberOfBackfoldingRuns()`（循环次数）、`getMaximumRetentionTimeShift()`（最大 RT 平移，计算用 ×2）
- **`BackfoldingShifter.shiftIons`（✅）**：
  1. 逐离子取总信号；`deltaSignal = next - actual`（一阶差分，允许负值）
  2. 差分信号拆正/负两份 → 各自用一阶导数峰检测（`PeakDetectorSettings`+`Threshold.OFF`）找峰顶 → 正/负峰顶 RT 差 = `deltaPeakDistances`（过滤 > maxDistance×2）→ **取中位数 ÷2 = 每次平移量**
  3. 差分信号**正值右移 +ΔRT、负值左移 −ΔRT**，取绝对值后按新 RT 叠加（`extractedIonSignalsShifted.add(ion, |abundance|, rt, merge=false)`）✅
  4. 每 run 对整个离子区间重复；写回扫描（`removeAllIons` + 重建）✅
- 用途：**差分 GC-MS 分辨率增强**（把差分信号折叠回峰形，峰锐化）——实验性算法

### 5.8 splitter（`...msd.filter.supplier.splitter`，✅ 6 滤波器全算法已读）

**共性**：6 个滤波器均 `extends AbstractChromatogramFilterMSD`，经 MSD `chromatogramFilterSupplier` 扩展点注册（id 后缀 `.msx/.sim/.tandemms/.highresms/.nominalms/.polarity`）。**拆分一律不是按时间窗**，而是按厂商扫描元数据把一次进样的扫描分组，**每组新建一个 `ChromatogramMSD` referenced 子色谱图**（`chromatogramMSD.addReferencedChromatogram(...)`），主色谱不变（**唯一例外：Polarity 是破坏性的**，见下）。`model/VendorScan`（深拷贝质谱）存在但**全插件无引用 → 死代码** ✅

| 滤波器 | 设置字段 | 算法一句话（✅ 源码） |
|---|---|---|
| **MSx**（Splitter (MS1,MS2,...)）| `FilterSettingsMSx` 空壳 | 按 `massSpectrometer`（IRegularMassSpectrum 的 short 字段，MS 级数 1/2/3...）分组 → 键升序 → **每级一个 referenced 子色谱图** |
| **SIM**（Splitter (SCAN, SIM)）| `limitIons` int 默认 5（≥1）| `getNumberOfIons() <= limitIons` → 判 SIM，否则 SCAN；生成 **两个** referenced 子色谱图 |
| **Polarity**（Splitter (Polarity)）| 空壳 | `chromatogramMSD.getScans().removeIf(scan → Polarity.NEGATIVE)`：**负离子扫描从原色谱移走**进 referenced（正的留在原图）——**破坏性** |
| **NominalMS**（Splitter (Nominal MS)）| `enforceFullTimeRange` bool 默认 false | 选择区内（或强制全 RT 范围）逐扫描，从 `getExtractedIonSignal()` 按**名义 m/z 整数 bin** 复制 `intensity>0` 离子到新 `RegularMassSpectrum` → 一个 referenced 子色谱图（全名义质量） |
| **HighResMS**（Splitter (High Resolution)）| `specificTraces` 字符串（如 `400.01627` 或 `400.01627±5ppm`）、`binning` ppm 默认 10、`separateTraces` bool 默认 true、`headerField`（默认 DATA_NAME）、`enforceFullTimeRange` false | `HighResolutionSupport.extractHighResolutionData`：逐扫描逐离子 `trace.matches(ion)` 匹配高分辨 trace → 按 (trace, RT) **累加丰度**；再逐 trace（`separateTraces=true` 每 trace 一图，否则合一图且强制全时间范围）建 referenced：每 RT 一扫描含 `Ion(trace.mz, 累加丰度)`；`enforceFullTimeRange=false` 时无信号 RT 跳过 |
| **TandemMS**（Splitter (MS/MS)）| `specificTraces`（如 `267 > 159.0 @15` = 母>子@碰撞能）、`condenseOption`（STANDARD/COARSE/SENSITIVE，RT 十进制取整调和微小偏差）、`separateTraces` true、`headerField`、`enforceFullTimeRange` false | `TandemDataSupport.extractTandemData`：逐扫描匹配 `IIonMSn` 离子的 `(Q1 母, Q3 子, collisionEnergy)` 三要素 → 按 (RT, trace) 收集；`separateTraces=false` 时 `condenseTraces` 按 `parent_daughter_CE` 键**合并同离子丰度**（保留 Q1/Q3 分辨率与 transitionGroup）→ 建 referenced 子色谱图 |

- 公共收尾：Nominal/HighRes/Tandem 的 referenced 子图均 `ChromatogramSupport.calculateScanIntervalAndDelay` 重算扫描间隔/延迟 ✅

### 5.9 msd 质谱归一化（`...msd.filter.supplier.normalizer`，✅）

- `MassSpectrumModifier.meanNormalize/medianNormalize`：**每张质谱内部**按其离子丰度均值/中位数除（`abundance/avg`，负→0）；`MassSpectrumFilter` 按 `AveragesType.MEAN/MEDIAN` 选择 ✅
- 与 chromatogram 级 meannormalizer 的区别：后者按**扫描间总信号均值**归一（跨扫描），本款按**谱内离子丰度**归一（谱内相对）✅

---

## 6. Qt/C++ 移植要点（⚠️ 设计笔记）

- **滤波接口**（core_processing）：
  - `IChromatogramFilter.applyFilter(selection, settings, monitor) → IProcessingInfo<IChromatogramFilterResult>` 在 Qt 对应：
    `virtual FilterResult apply(const ChromatogramSelection &sel, const FilterSettings &cfg)`，`FilterResult{ ResultStatus status; QString description; }`
  - **注册机制**：Java 扩展点（字符串 id + 反射实例化 + Jackson settings）→ Qt 用 `QHash<QString, std::function<std::unique_ptr<ChromatogramFilter>()>>` 工厂表 + `QHash<QString, std::shared_ptr<FilterSettings>>`（或直接每个 filter 自带 settings 结构）。设置序列化（方法保存）用 `QJsonObject`。
  - 数据流：Qt 色谱数据建议 `QSharedPointer<Chromatogram>` + 扫描数组；滤波器只操作 `[startScan..stopScan]` 区间内扫描并回写，最后打 dirty 标记。
- **Savitzky-Golay 移植**（core_processing 可直接照搬）：
  - **不查表**：系数在运行时由法方程最小二乘解出（宽度≤51×51 的小矩阵，Qt 用 Eigen / 自写高斯消元即可）。必须复刻三处纠正：`width=max(5,1+2*((w-1)/2))`（强制奇数）、`order=min(max(0,o),5,width-1)`、`derivative=min(max(0,d),order)`。
  - 三区处理：中间 `y[i]=Σ c[k]·x[i+k]`（对称卷积核，`p=(width-1)/2`）；**首尾 p 点分别用"法方程前/后 p 行 × 起始权重"加权**（OpenChrom 独特之处，边界不平移不截断，点数不变）。
  - 若嫌法方程求解复杂，**可用经典 S-G 系数表硬编码**（5/7/9/11/13/15 点 × 阶 2/3/4 的标准表，等价结果），Qt 直接 `QVector<double>` 存核。
  - 设置范围：order 2–5（CSD 允许 1）、width 5–51 奇数、**derivative 对色谱图恒 0**（自研 CDS 可先只做 0 阶平滑）。
  - **MSD 逐离子 vs TIC**：默认逐离子（`perIonCalculation=true`）→ 按每离子一维数组独立平滑（矩阵列方向），负值钳 0；TIC 路径 MSD 负值置 0.1f 下限。CSD/WSD 走 TIC。⚠️ 高分辨（每扫描离子数 > m/z 范围宽度+10）当前不支持。
- **baselinesubtract**：逐扫描 `y' = totalSignal - baselineModel.getBackground(rt)`（BaselineModel 见 MODULE_04 §3.1，两点线性插值）；`y'≤0 → 删扫描`；随后 `peaks.clear()` + `baselineModel.removeBaseline()`。Qt 直接搬。
- **简单滤波器**一行一个（QVector<float> 操作）：
  - invert：`y' = maxSignal - y`（全谱，非选择区）
  - multiplier/divisor：`y' = y × factor`（选择区，负→0，回写 ≤0→0.1f）
  - rtshifter：`rt' = rt + Δms`；越出相邻扫描 RT → 删扫描
  - normalizer：`y' = y × (base/max)`（默认 1000）；meannormalizer/mediannormalizer：`y' = y / |mean| 或 /median`；unitsumnormalizer：`y' = y / 色谱总信号`
  - zeroset：`min<0 → y' = y - min`
  - scan：按目标间隔线性插值重建扫描网格（`ScanProcessor.recalculateScans`，目标间隔 = 1000/扫描频率）；`DensityCalculator` 本版死代码，勿移植
- **MSD 滤波器算法一句话移植**：
  - CODA：每离子 mcq = 欧氏归一×平滑×z-score 的点积 ÷√(n-window)；`mcq<thr` 删离子 —— 纯数值，可移植（win=5）
  - denoising：噪声段质谱 × 相关系数（Top N 离子 scan/noise 比均值）逐离子扣减
  - ionremover：m/z 名单 INCLUDE/EXCLUDE 删除（`QSet<int>`）
  - subtract：参考谱 Map<m/z,丰度>，归一化相对扣 / 绝对扣，≤0 删离子
  - xpass：丰度排序留 Top n / Bottom n；名义化合并
  - centroiding：**纯"已存峰列表→离子"转换器**（`QVector<MassSpectrumPeak{mz,abundance}> → Ions`，替换原离子 + 类型置 CENTROID）。⚠️ **OpenChrom 本身没有自动轮廓峰检测**（峰列表仅 CML/mMass 文件自带）——若 Qt 要"真正的质心化"，需自研局部最大值 + 信噪比/丰度阈值峰挑选算法，不能照搬本插件
  - splitter：按厂商元数据分组建 **referenced 子色谱图**（Qt 用 `QList<QSharedPointer<Chromatogram>> referencedChromatograms`）；Polarity 变体破坏性（负离子扫描移出主图）；HighRes/Tandem 的 m/z±ppm / 母>子@CE 匹配与 (trace,RT) 丰度累加可移植；MSx 按 MS 级数、SIM 按离子数阈值即可
  - scan 套件：ScanMerger 两两合并到 RT 中点（信号取均值）；Duplicator 用 `increaseScansDensity(1)` 相邻插 1 个线性中点；ScanDensity 目标间隔 `1000/sps` 重建网格（线性插值）；Cleaner 删空扫描；Remover 用 X/O 循环模式；DelayInterval 重算 scanDelay/interval；BufferIdentifier/DeleteIdentifier 操作扫描鉴定 target（Qt 侧为 `QHash<Scan*, QVector<Target>>`）
  - backfolding：一阶差分 → 正/负峰距中位数定平移 → 正右移负左移合并（实验性，非必需）
  - ⚠️ 优先移植 S-G + baselinesubtract + normalizer 族 + ionremover + CODA（开箱即用）；backfolding/denoising 按需。
- **不建议照搬**：Eclipse undo/redo 框架（`OperationHistory`）、OSGi DS 双注册、扩展点反射机制（Qt 用工厂表替代）、splitter 的厂商元数据依赖。

## 7. 待回填清单（❓）

> 本任务 3 项遗留（splitter 拆分逻辑 / centroiding 峰来源 / scan 套件子滤波器）已于本次全部回填（见 §5.6、§5.8、§4.1，证据 SF-AE~SF-AW）。

| # | 问题 |
|---|---|
| SF-? | VSD/ISD 滤波器族（chromatogram.vsd.filter / msd.filter 之外）未展开（不在本任务范围） |
| SF-? | splitter 的 `HighResolutionSupport`/`TandemDataSupport` 之外，`separateTraces=false` 的多 trace 合并行为仅按源码推断（`enforceFullTimeRange` 被强制 true），未见 UI 实测 |

## 8. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| SF-A | IChromatogramFilter 接口（applyFilter 签名 + 三件套校验）| org.eclipse.chemclipse.chromatogram.filter/core/chromatogram/IChromatogramFilter.java + AbstractChromatogramFilter.java | ✅ |
| SF-B | 门面 ChromatogramFilter（扩展点常量 + createExecutableExtension + getChromatogramFilterSupport）| .../filter/core/chromatogram/ChromatogramFilter.java | ✅ |
| SF-C | IChromatogramFilterSupplier / Support / IChromatogramFilterMSD 骨架 | .../filter/core/chromatogram/IChromatogramFilterSupplier.java + ...msd.filter/core/chromatogram/AbstractChromatogramFilterMSD.java + IChromatogramFilterMSD.java | ✅ |
| SF-D | 类型化扩展点（msd/csd/wsd 的 chromatogram/peak/massSpectrumFilterSupplier）| 三插件 plugin.xml + org.eclipse.chemclipse.chromatogram.filter/plugin.xml | ✅ |
| SF-E | S-G 设置字段（order/width/derivative/perIonCalculation + 各域范围与默认值）| xxd.filter.supplier.savitzkygolay/settings/ChromatogramFilterSettings.java + CSD/MSD/WSD.java + MassSpectrumFilterSettings.java | ✅ |
| SF-F | SavitzkyGolayFilter 构造纠正（width 奇数化/order 钳位/derivative 钳位）| .../processor/SavitzkyGolayFilter.java (constructor) | ✅ |
| SF-G | 卷积系数 = 法方程 QR/SVD/LU 求解（非查表）；filterCoefficients=weights[derivative]；derivativeCoefficients | 同 SF-F (calculateConvolutionWeights / getNormalEquations / calculateCoefficients) | ✅ |
| SF-H | 三区处理（start/middle/end，首尾 uStart/uStop 加权，中间对称卷积）| 同 SF-F (apply / processStart / processMiddle / processEnd) | ✅ |
| SF-I | TIC 路径（TotalScanSignalExtractor → smooth → adjustTotalSignal；MSD ≤0→0.1f）| .../processor/SavitzkyGolayProcessor.smooth(IChromatogramSelection,...) | ✅ |
| SF-J | MSD 逐离子路径（ExtractedMatrix 矩阵列平滑、负值钳 0、高分辨拒绝）| .../calculator/operations/SavitzkyGolayPerIonOperation.java + msd.model/matrix/ExtractedMatrix.java + SavitzkyGolayProcessor.apply(double[][],...) | ✅ |
| SF-K | MSD 总信号路径（setNegativeTotalSignalsToZero + adjustTotalSignal）| .../calculator/operations/SavitzkyGolayTotalScanSignalOperation.java | ✅ |
| SF-L | 质谱平滑（FilterSupplier 逐谱排序平滑 + setAbundance 越限跳过）| .../calculator/FilterSupplier.java + core/MassSpectrumFilter.java | ✅ |
| SF-M | OSGi 管线版（Filter/IScanFilter/ITotalScanSignalsFilter）| .../core/SavitzkyGolaySmoothingFilter.java | ✅ |
| SF-N | S-G 扩展注册（4 条，MSD/CSD/WSD/质谱）| xxd.filter.supplier.savitzkygolay/plugin.xml | ✅ |
| SF-O | baselinesubtract（getBaselineModel→逐扫描减背景→≤0删扫描→清峰+删基线；设置空壳）| xxd.filter.supplier.baselinesubtract/processor/BaselineSubtractProcessor.removeBaseline + core/ChromatogramFilter + settings/ChromatogramFilterSettings | ✅ |
| SF-P | invert（maxSignal - y 全谱）| xxd.filter.supplier.invert/core/ChromatogramFilter.applyFilter(私有) | ✅ |
| SF-Q | multiplier/divisor（ModifierChromatogramFilter ×÷ factor；≤0→0.1f）| xxd.filter.supplier.multiplier/core/ModifierChromatogramFilter + Multiplier/DivisorChromatogramFilter + settings | ✅ |
| SF-R | rtshifter（RT±shift，越界删扫描，邻扫边界）| xxd.filter.supplier.rtshifter/core/ChromatogramFilterShift + internal/support/RetentionTimeShifter + settings/FilterSettingsShift | ✅ |
| SF-S | 四个归一化（normalize base/max；meanNormalize /median；unitSumNormalize /totalSignal）+ 公共实现 | model/signals/TotalScanSignalsModifier.java + 4 个 xxd.filter.supplier.*normalizer/core/ChromatogramFilter.java | ✅ |
| SF-T | zeroset（min<0 整体上移，仅 CSD/WSD）| xxd.filter.supplier.zeroset/core/ChromatogramFilterCSD.java + WSD | ✅ |
| SF-U | scan 套件（recalculateScans 线性插值重建网格 / increaseScansDensity / DensityCalculator / FilterScanClipper / FilterScanSelector）| xxd.filter.supplier.scan/calculator/ScanProcessor.java + DensityCalculator.java + core/FilterScanClipper.java + FilterScanSelector.java | ✅ |
| SF-V | CODA（MCQ 公式 + 阈值选排除离子 + removeIons；窗口=5；阈值默认0.75）| ...msd.filter.supplier.coda/calculator/CodaCalculator.java + MassChromatographicQualityResult.java + core/ChromatogramFilterMSD.java + settings/FilterSettings.java + numeric/statistics/Calculations.java(smooth/scaleToEuclidianLength/scaleToStandardizedLength) | ✅ |
| SF-W | denoising（5 步：删指定离子→阈值跳变→噪声段→相关系数扣减→写回）| ...msd.filter.supplier.denoising/internal/core/support/Denoising.java + settings/FilterSettings.java + core/ChromatogramFilter.java | ✅ |
| SF-X | ionremover（名单 INCLUDE/EXCLUDE 删离子；3 入口）| ...msd.filter.supplier.ionremover/core/ScanProcessor.java + settings/ChromatogramFilterSettings.java + core/MassSpectrumFilter.java + core/PeakFilter.java | ✅ |
| SF-Y | subtract（参考谱 Map<m/z,丰度>；归一化相对扣/绝对扣；≤0 删离子；清积分）| ...msd.filter.supplier.subtract/calculator/SubtractCalculator.java + settings/ChromatogramFilterSettings.java | ✅ |
| SF-Z | xpass（nominalize 名义化 / high-pass 留 Top n / low-pass 留 Bottom n；CENTROID 限定）| ...msd.filter.supplier.xpass/filter/XPassFilter.java + core/HighPassIonsFilter.java + core/LowPassScanFilter.java + settings/HighPassFilterSettings.java + core/ChromatogramFilterNominalize.java | ✅ |
| SF-AA | centroiding（PROFILE 轮廓峰→离子→CENTROID）| ...msd.filter.supplier.centroiding/core/MassSpectrumFilter.centroidisation | ✅ |
| SF-AB | backfolding（一阶差分→正/负峰距中位数→正右移负左移合并）| ...msd.filter.supplier.backfolding/detector/BackfoldingShifter.java + core/ChromatogramFilterMSD.java | ✅ |
| SF-AC | splitter 结构确认（6 滤波器 + VendorScan）——实现细节已由 SF-AE~SF-AK 补齐 | ...msd.filter.supplier.splitter/core/*.java（结构）| ✅ |
| SF-AD | msd 质谱归一化（谱内 mean/median 除）| ...msd.filter.supplier.normalizer/MassSpectrumModifier.java + core/MassSpectrumFilter.java | ✅ |
| SF-AE | splitter MSx（按 massSpectrometer 分组 → 每 MS 级一个 referenced 子图）| ...splitter/core/ChromatogramFilterMSx.java (applyFilter) + settings/FilterSettingsMSx | ✅ |
| SF-AF | splitter SIM（离子数 ≤ limitIons → SIM，否则 SCAN；两个 referenced 子图）| ...splitter/core/ChromatogramFilterSIM.java + settings/FilterSettingsSIM | ✅ |
| SF-AG | splitter Polarity（NEGATIVE 扫描 removeIf 移入 referenced，破坏性）| ...splitter/core/ChromatogramFilterPolarity.java (splitByPolarity) | ✅ |
| SF-AH | splitter NominalMS（EnforceFullTimeRange；ExtractedIonSignal 名义 m/z 复制离子）| ...splitter/core/ChromatogramFilterNominalMS.java + settings/FilterSettingsNominalMS | ✅ |
| SF-AI | splitter HighResMS（TraceHighResMSD m/z±ppm 匹配；按 (trace,RT) 累加丰度；separateTraces 每 trace 一图）| ...splitter/core/ChromatogramFilterHighResMS.java + settings/FilterSettingsHighResMS + msd.model/support/HighResolutionSupport.java | ✅ |
| SF-AJ | splitter TandemMS（IIonMSn 父/子/碰撞能匹配；CondenseOption RT 取整调和；condenseTraces 合并同离子丰度）| ...splitter/core/ChromatogramFilterTandemMS.java + settings/FilterSettingsTandemMS + msd.model/support/TandemDataSupport.java | ✅ |
| SF-AK | splitter 注册 6 条（MSD 扩展点）；model/VendorScan 无引用（死代码）| ...splitter/plugin.xml + model/VendorScan.java | ✅ |
| SF-AL | centroiding 峰来源：getPeaks()=普通 ArrayList，仅 CML/mMass 转换器从文件峰列表读入；无自动峰检测；滤波=纯"峰列表→离子"转换器 | msd.model/core/AbstractStandaloneMassSpectrum.java(getPeaks) + msd.converter.supplier.cml/.../MassSpectraReader.java(readPeaks) + msd.converter.supplier.mmass/.../AbstractMassSpectrumReader.java(readPeakList) + centroiding/core/MassSpectrumFilter.java + settings/MassSpectrumFilterSettings.java + plugin.xml | ✅ |
| SF-AM | scan 套件注册方式修正：plugin.xml 经典扩展点 11 条 + FilterScanSelector 走 OSGi | xxd.filter.supplier.scan/plugin.xml + OSGI-INF/...FilterScanSelector.xml + core/FilterScanSelector.java(@Component) | ✅ |
| SF-AN | scan FilterScanMerger（两两合并到 RT 中点、总信号均值、离子并集、间隔×2）| ...scan/core/FilterScanMerger.java + settings/FilterSettingsScanMerger | ✅ |
| SF-AO | scan FilterScanDensity（目标间隔 1000/sps → recalculateScans 重建网格 → 剔 0 丰度离子）+ DensityCalculator 无引用死代码 | ...scan/core/FilterScanDensity.java + settings/FilterSettingsScanDensity + calculator/DensityCalculator.java(全库无调用) | ✅ |
| SF-AP | scan FilterCleaner（MSD isEmpty / CSD signal==0 / WSD 无信号 → 删扫描）| ...scan/core/FilterCleaner.java + settings/FilterSettingsCleaner | ✅ |
| SF-AQ | scan FilterDuplicator（increaseScansDensity(1) 相邻插 1 线性中点 + 复用 DelayInterval）| ...scan/core/FilterDuplicator.java + calculator/ScanProcessor.increaseScansDensity + settings/FilterSettingsDuplicator | ✅ |
| SF-AR | scan FilterRemover（ScanRemoverPattern X=删/O=留 循环匹配）| ...scan/core/FilterRemover.java + model/ScanRemoverPattern.java + settings/FilterSettingsRemover | ✅ |
| SF-AS | scan FilterDelayInterval（calculateScanIntervalAndDelay + 可选 recalculateRetentionTimes）| ...scan/core/FilterDelayInterval.java + settings/FilterSettingsDelayInterval | ✅ |
| SF-AT | scan FilterBufferIdentifier（BUFFER/RESTORE/CLEAR：扫描 target 缓冲到 IMeasurementResult）| ...scan/core/FilterBufferIdentifier.java + model/BufferedScanTargets.java + settings/FilterSettingsBufferIdentifier | ✅ |
| SF-AU | scan FilterDeleteIdentifier（选择区扫描 targets.clear()）| ...scan/core/FilterDeleteIdentifier.java + settings/FilterSettingsDeleteIdentifier | ✅ |
| SF-AV | scan FilterObfuscator（MSD 随机 m/z 18–250 单离子 / VSD 随机波数 660–4000 / WSD 随机波长 380–750）| ...scan/core/FilterObfuscator.java + settings/FilterSettingsObfuscator | ✅ |
| SF-AW | scan FilterRetentionIndexSelector（列标记匹配 → AUTO/MIN/MEAN/MEDIAN/MAX 选 RI → 写回 + 可选删无关标记）| ...scan/core/FilterRetentionIndexSelector.java + settings/FilterSettingsRetentionIndexSelector + model/RetentionIndexOption.java | ✅ |
