# MODULE_02 — Chromatogram Model（色谱模型层）

> **状态：🟢 核心已确认（IChromatogram / IChromatogramSelection / IScan / 信号层 / 父接口族 / 实现类内部字段 / 通知机制 ✅——全部接口定义逐字来自本机 ChemClipse 源码）**
> 回答「数据如何存储」。这是全系统的核心枢纽，被导入、处理、UI、报告全部引用。
> 本次深挖新增证据：**ChemClipse 完整源码已抓取到 `.fetch/chemclipse-src`，三大模型插件（`org.eclipse.chemclipse.model` / `msd.model` / `csd.model` / `wsd.model`）逐接口精读：IScan/ISignal/IPoint 完整定义、信号层 ITotalScanSignal(s)+normalize 算法+构造器、IChromatogramOverview 常量精确值、9 个父接口成员表、AbstractChromatogram*/AbstractScan*/AbstractIon 内部字段、IIon 常量与 Ion 实现、ChromatogramSelection*/UpdateNotifier 注册-通知全链路**。旧文档 C1~C8 ❓ 全部升级为 ✅（仅 `IPointOfInflection` 确认不存在于本版本源码）。

---

## 1. 核心接口：IChromatogram（✅ 源码确认）

文件：`.fetch/chemclipse-src/plugins/org.eclipse.chemclipse.model/src/org/eclipse/chemclipse/model/core/IChromatogram.java`（真实包 `org.eclipse.chemclipse.model.core`，属于 ChemClipse `org.eclipse.chemclipse.model`；旧摘录副本 `.fetch/sources/model/IChromatogram.java` 与之逐字一致）
接口 javadoc 自述："A chromatogram consists of several scans. This is the detector independent part of it."（色谱 = 若干扫描的有序集合；本接口是检测器无关部分）。

### 1.1 接口继承链（多重继承，✅ 逐字来自接口声明）

```java
public interface IChromatogram extends SegmentedMeasurement, IChromatogramOverview,
        IChromatogramPeaks, ISupplierEditHistory, IChromatogramBaseline, IUpdater,
        IChromatogramIntegrationSupport, IChromatogramProcessorSupport,
        ITargetSupplier, ITargetDisplaySettings
```

```text
IChromatogram
 ├─ SegmentedMeasurement           — 分段测量（RT 分段时间轴，多段方法/评价）
 ├─ IChromatogramOverview          — 概览视图（轻量模型，大数据快速预览）
 ├─ IChromatogramPeaks             — 峰集合宿主（getPeaks()，可增删/索引/遍历）
 ├─ ISupplierEditHistory           — 编辑历史（getEditHistory() → List<IEditInformation>，审计追踪）
 ├─ IChromatogramBaseline          — 基线（getBaseline / setBaseline）
 ├─ IUpdater                       — 通用更新接口
 ├─ IChromatogramIntegrationSupport — 积分支持
 ├─ IChromatogramProcessorSupport  — 处理器支持
 ├─ ITargetSupplier                — 目标（化合物标注）
 └─ ITargetDisplaySettings         — 目标显示设置
```

> ✅ 父接口成员表现在全部有 ChemClipse 接口原文（`org.eclipse.chemclipse.model/core|baseline|targets|updates` + `org.eclipse.chemclipse.support/history`，见 §1.4 成员表）。以下旧调用点语义与接口原文一致，保留作交叉证据：
> - `IChromatogramOverview`：定义常量 `SECOND_CORRELATION_FACTOR` / `MINUTE_CORRELATION_FACTOR`（RT 秒/分→毫秒换算因子），被 CSD CDF Reader 用于 `retention_unit` 单位换算 ✅（见 §6.1）；`readOverview()` 返回该接口，MSD CDF 用 TIC-only 轻量色谱实现 ✅。
> - `IChromatogramPeaks.getPeaks()`：返回**可变** List，调用点 `chromatogram.getPeaks().add(peak)`（PeakDetectorCSD / AMDIS / animl Reader）、`.size()`、`.indexOf()`、`.get(i)`、遍历 ✅。
> - `ISupplierEditHistory.getEditHistory()`：返回 `List<IEditInformation>`，Reader 用 `.addAll(Common.readAuditTrail(...))` / `.add(new EditInformation(...))` 回填审计轨迹 ✅。
> - `ITargetSupplier`：`ChromatogramCleaner.cleanTargets(ITargetSupplier)` 把它当"带化合物标注的宿主"统一清理 ✅。

### 1.2 关键成员/语义（✅ 逐条来自源码）

| 概念 | 方法 | 说明 |
|---|---|---|
| 扫描列表 | `getScans()` / `addScan(IScan)` / `addScans(List)` / `getScan(int)` / `removeScan(int)` / `removeScans(from,to)` / `replaceAllScans(List)` | 色谱 = 有序扫描集合；`getScans()` javadoc 明示"只用于遍历，改 List 可能出问题"；`removeScans(from,to)` **含两端**（javadoc 例：删 500~509 用 removeScans(500,509)）；`getScan` 无则返回 null |
| RT 制式 | 常量 `MIN_SCANDELAY=0`、`MAX_SCANDELAY=216000000`、`MIN_SCANINTERVAL=1`、`MAX_SCANINTERVAL=3600000`、`MIN/MAX_SCANS_PER_SECOND=0.1/20.0` | **RT 单位 = 毫秒（ms）**（注释明文：1h=1000ms×60×60；1min=1000ms×60）；扫描密度约束 0.1~20 扫/秒 |
| 峰集合 | `getPeaks()`（来自 IChromatogramPeaks） | 可变 List（实现=ObservedPeakList 本体），直接 add 生效但校验峰模型有效（§5.3）；remove 自动标 deleted；峰↔色谱有背引用（`IPeak.setChromatogram(null)` ✅） |
| 基线 | 继承 IChromatogramBaseline | 基线数据挂色谱，见 MODULE_03/04 |
| 噪声/S/N | `getNoiseCalculator()` / `setNoiseCalculator()` / `getSignalToNoiseRatio(float)` / `resetNoiseFactor()` | 噪声模型可插拔 |
| 保存标识 | `getConverterId()` / `setConverterId()` / `isFinalized()` / `setFinalized()` | 见 §7：converterId 记忆可回写格式；finalized 防覆盖 |
| 方法 | `getMethod()` → `IMethod` | 分析方法（仪器方法）对象 |
| 脏标记 | `setDirty(boolean)` | 处理管线改动模型的统一标记（峰检测后必置 true ✅） |
| 分段 | `getAnalysisSegments()` / `defineAnalysisSegment(IScanRange[, childs])` / `removeAnalysisSegment` / `updateAnalysisSegment` / `clearAnalysisSegments()` | 分析段（多段方法/评价）；childs 为子段集合 |
| 多色谱 | `getReferencedChromatograms()` / `addReferencedChromatogram` / `removeReferencedChromatogram` / `removeAllReferencedChromatograms` | 一文件多色谱；**也被当临时抽取结果容器**（高分辨/串联 MS 抽取的子色谱挂这里，用完 remove ✅） |
| 临时数据 | `getProcessDataMap()` | **会话内**附加数据，javadoc 明文"not saved and only valid for the current session" |
| 卸载 | `setUnloaded()` / `isUnloaded()` | 大数据释放内存标记（只标记不销毁对象） |
| 扫描周期 | `containsScanCycles()` / `getScanCycleScans(int)` | 多周期采集（如 DIA）；同周期扫描 TIC 模式下用求和信号显示 |
| 分离柱 | `getSeparationColumnIndices()` / `setSeparationColumnIndices` | 保留指数计算用 |
| 列信息 | `getColumnDetails()` / `setColumnDetails()` | 色谱柱文本描述；导入时默认允许解析（`isParseSeparationColumnEnabled()` 默认 true，可覆写为 false） |
| 主色谱 | `getMasterChromatogram()` | 可能返回 null |
| 文件 | `getFile()` | **default 返回 null**（IChromatogram 自身不存文件路径） |

### 1.3 默认方法实现（✅）

- `isParseSeparationColumnEnabled()` → `true`（可覆写：导入时不想解析分离柱就返回 false）
- `getFile()` → `null`（从 ISupplierEditHistory 链覆写；文件路径实际由具体类如 VendorChromatogram 持有，Reader 调 `setFile(file)` ✅）
- `defineAnalysisSegment(IScanRange range)` → 委托两参版，childs 传 `Collections.emptyList()`
- `clearAnalysisSegments()` → 把 `getAnalysisSegments()` 快照数组逐个 `removeAnalysisSegment`

### 1.4 父接口成员表（★ ✅ 逐字来自 ChemClipse 接口原文）

所有父接口已在本机源码精读。源文件均以 **接口原文** 为准：

**IChromatogramPeaks**（`model/core/IChromatogramPeaks.java`）
- `List<? extends IChromatogramPeak> getPeaks(int startRT, int stopRT)`：返回 RT 范围内、按峰起始 RT 排序的峰
- `List<? extends IChromatogramPeak> getPeaks()`：返回列表；javadoc 声称"改它不影响色谱峰表"，**但实际实现直接返回 ObservedPeakList 本体（§5.2），直接 add/remove 生效**——接口注释已过时，以实现为准（C-O 交叉证据成立）
- `default getPeaks(IRetentionTimeRange range)`：null→getPeaks()，否则委托两参版

**IChromatogramBaseline**（`model/baseline/IChromatogramBaseline.java`）
- 常量 `String DEFAULT_BASELINE_ID = "Default"`（勿翻译）
- `IBaselineModel getBaselineModel()`（可修改）、`Set<String> getBaselineIds()`、`String getActiveBaseline()`、`void setActiveBaselineDefault()`、`void setActiveBaseline(String id)`（空/null 不允许）、`void removeBaseline(String id)`（Default 不可删）

**SegmentedMeasurement extends IMeasurement**（`model/core/SegmentedMeasurement.java`）
- `List<IAnalysisSegment> getAnalysisSegments()`：**只读视图**（实现返回 `Collections.unmodifiableList`）；分析段改由 `defineAnalysisSegment/removeAnalysisSegment/updateAnalysisSegment` 驱动
- `IMeasurement` 父接口（`model/core/IMeasurement.java`）：extends `IMeasurementInfo, IMeasurementResultSupport, IAdaptable`，default `getFile()→null`、`isDirty()→false`、`getModCount()→0`
- `IMeasurementInfo`（`model/core/IMeasurementInfo.java`）：headerData 键值 API（`get/put/removeHeaderData`、保护键、`getHeaderDataMap()` 不可变）+ 元数据（instrument/operator/date/miscInfo/shortInfo/detailedInfo/sampleName/sampleGroup/barcode/barcodeType/sampleWeight(+unit)/dataName/findings/tags）

**IChromatogramIntegrationSupport**（`model/core/IChromatogramIntegrationSupport.java`）
- `get/setIntegratorDescription(String)`、`double getChromatogramIntegratedArea()`、`double getBackgroundIntegratedArea()`、`double getPeakIntegratedArea()`（= Σ 各峰 `getIntegratedArea()`）、`void setIntegratedArea(List<IIntegrationEntry> chrom, List<IIntegrationEntry> bg, String desc)`、`getChromatogramIntegrationEntries()`/`getBackgroundIntegrationEntries()`、`removeAll{Background|Chromatogram}IntegrationEntries()`

**IChromatogramProcessorSupport**（`model/core/IChromatogramProcessorSupport.java`）
- `void fireUpdate(IChromatogramSelection)`：唯一成员。CSD/MSD 实现把 selection 强转成 `ChromatogramSelection{CSD|MSD}` 调 `.update(true)`（§7 全链路）

**ITargetSupplier**（`model/core/ITargetSupplier.java`）
- `Set<IIdentificationTarget> getTargets()`：唯一成员。实现为 `HashSet`，可变（ChromatogramCleaner 统一清理 C-T 交叉证据成立）

**ITargetDisplaySettings**（`model/targets/ITargetDisplaySettings.java`）
- `isShowPeakLabels/isShowScanLabels` + 对应 setter、`DisplayOption getDisplayOption()/setDisplayOption`、`int getRotation()/setRotation(int degree)`、`int getCollisionDetectionDepth()/setCollisionDetectionDepth`、`LibraryField getLibraryField()/setLibraryField`、`boolean isVisible(ITargetReference)`、`void setVisible(ITargetReference, boolean)`、`boolean isMapped(ITargetReference)`、`Map<String,Boolean> getVisibilityMap()`（不可变）、`void putAll(Map)`

**IUpdater**（`model/updates/IUpdater.java`）
- `void addChromatogramUpdateListener(IChromatogramUpdateListener)` / `void removeChromatogramUpdateListener(IChromatogramUpdateListener)`。javadoc：GUI 控制器向模型注册自己，模型变更时被通知

**ISupplierEditHistory**（`org.eclipse.chemclipse.support/history/ISupplierEditHistory.java`，注意包在 support 插件）
- `IEditHistory getEditHistory()`。`IEditHistory extends List<IEditInformation>`（即 audit trail 就是一个 List）；`IEditInformation`：`Date getDate()`、`String getDescription()`、`String getEditor()`、`ProcessSupplierEntry getProcessSupplierEntry()/setProcessSupplierEntry(...)`

**IChromatogramOverview extends IMeasurementInfo**（`model/core/IChromatogramOverview.java`，完整定义见 §4.3）

## 2. 核心接口：IChromatogramSelection（✅ 源码确认）

文件：`.fetch/chemclipse-src/plugins/org.eclipse.chemclipse.model/src/org/eclipse/chemclipse/model/selection/IChromatogramSelection.java`（真实包 `org.eclipse.chemclipse.model.selection`）

**职责：UI 与处理引擎之间的「当前查看/操作区域」对象。** 这是分析引擎与 UI 解耦的枢纽。

- 继承 `IChromatogramUpdateListener, IRetentionTimeRange, IScanRange`
- **RT 范围**：`getStartRetentionTime()` / `getStopRetentionTime()`（int，**毫秒**，javadoc 明文），`setRangeRetentionTime(start, stop[, validate])`、`setRanges(4 参)`（RT + 丰度范围）
- **丰度范围**：`getStartAbundance()` / `getStopAbundance()`（float）
- **选中对象**：`getSelectedScan()`/`setSelectedScan([update])`、`getSelectedPeak()`/`getSelectedPeaks()`、`getSelectedIdentifiedScan(s)`（已鉴定扫描）
- **UI 状态**：`isOverlaySelected()`/`setOverlaySelected`、`isLockOffset()`/`setLockOffset`（锁定 UI 显示偏移）、`getOffset()`/`resetOffset()`（Point）
- `reset()` / `reset(boolean fireUpdate)`：恢复为全色谱范围。javadoc 明文示例："若色谱存 5726 个扫描，reset 会把 startScan 置 1、stopScan 置 5726，丰度置 0~max，选中扫描/峰各取第一个"——**扫描号从 1 起 ✅**

### 2.1 setRangeRetentionTime → getStartScan/getStopScan 反查（✅）

`getStartScan()`/`getStopScan()` 是 **default 方法**，用 RT 反查扫描号：

```java
default int getStartScan() {
    int scanNumber = getChromatogram().getScanNumber(getStartRetentionTime());
    return (scanNumber > 0) ? scanNumber : -1;   // 反查失败返回 -1
}
```

- 即 `IChromatogram.getScanNumber(int retentionTime)`：RT→最近扫描号（1-based），查不到返回 ≤0 ✅
- 上游调用点：PeakDetectorCSD 反复 `chromatogramSelection.setRangeRetentionTime(s,e)` 后再 `getFirstDerivativeSlopes`；CDK 标识器 `chromatogram.getScanNumber(chromatogramSelection.getStartRetentionTime())` ✅
- `validatePeak(IPeak)` default：peak 为 null、或 `peak.isMarkedAsDeleted()`、或 `getChromatogram().getPeaks().isEmpty()` 时返回 null，否则原样返回 ✅

### 2.2 fireUpdateChange 通知机制（★ ✅ 全链路源码确认，不再 ❓）

**接口声明**（`model/selection/IChromatogramSelection.java`）：

```java
public interface IChromatogramSelection extends IChromatogramUpdateListener, IRetentionTimeRange, IScanRange
```

**注册-通知全链路（逐环有源码）**：

```
① 创建 Selection：AbstractChromatogramSelection(IChromatogram) 构造器末尾
     this.chromatogram.addChromatogramUpdateListener(this);   // Selection 自身即 IChromatogramUpdateListener ✅
② 模型变更广播：AbstractChromatogram.fireUpdateChange(forceReload)
     遍历 List<IChromatogramUpdateListener> updateSupport → 逐个 listener.update(forceReload)（SafeRunner 包裹）
③ Selection 响应：ChromatogramSelection{MSD|CSD}.update(forceReload)
     super.update(forceReload) 校验并重设 RT/丰度范围 + selectedScan
     fireUpdateChange(forceReload)                                    // 向 UI 层转发
④ UI 广播：fireUpdateChange(forceReload) → UpdateNotifier.update(this)   （静态工具，model/notifier/UpdateNotifier.java）
     IEventBroker eventBroker = Activator.getDefault().getEventBroker();   // Eclipse e4 事件总线
     eventBroker.send(IChemClipseEvents.TOPIC_CHROMATOGRAM_XXD_UPDATE_SELECTION, this);
     // topic 常量 = "chromatogram/xxd/load/chromatogramselection" ✅（support/events/IChemClipseEvents.java:55）
⑤ UI 订阅：Eclipse e4 部件用 @EventSubscription 监听该 topic → 刷新色谱视图
```

- **关键结论（纠正旧文 ❓）**：旧文引用的接口 javadoc 示例类 `org.eclipse.chemclipse.msd.model.notifier.ChromatogramSelectionUpdateNotifier` **在本版本源码中不存在**（grep 全树无此类）——javadoc 注释已过时。当前机制是静态工具类 **`UpdateNotifier`**（多个重载 `update(...)`：Selection/Peak/Scan/IdentificationTarget/EditHistory 等各发一个 topic）。
- **两条更新方向**：
  - UI→模型：`selection.setRangeRetentionTime(...)` 内部 `fireUpdateChange(false)` → ④ → 各 UI 部件刷新；`EnhancedScanMarkerEditor:348` 的 `fireUpdateChange(true)` 同理（C-Q 交叉证据成立）。
  - 模型→UI（处理管线改完模型后）：`chromatogram.fireUpdate(IChromatogramSelection)`（IChromatogramProcessorSupport 成员）→ CSD/MSD 实现强转 `(ChromatogramSelection{CSD|MSD})` 调 `.update(true)` → ③④ 全链路；`setDirty(true)` 只标记不改 UI。
- **补充**：`IChromatogramSelectionUpdateNotifier`（model/notifier）是另一接口（`updateSelection(IChromatogramSelection, boolean)`），供 UI 层主动请求刷新 Selection 用，与 Selection 自身的 fireUpdateChange 无直接关系。
- **Selection 自校验**：`update(forceReload)` 会重算 start/stop 丰度（越界回退到 min/max signal）并重设 `selectedScan`；`IChromatogramSelection.validatePeak(IPeak)` default 确认 peak==null / `isMarkedAsDeleted()` / 色谱峰表空 → 返回 null。

## 3. IScan 与扫描数据（★ ✅ 接口定义逐字来自 `model/core/IScan.java` + 构造点交叉确认）

`IScan extends ISignal, IAdaptable, Serializable, ITargetSupplier`。javadoc："Scans are data points per interval from the detector."（扫描 = 检测器按间隔输出的数据点）。

### 3.0 ISignal / IPoint 完整定义（✅）

**ISignal**（`model/core/ISignal.java`，"可画在 X/Y 图上的任意信号"）：
```java
public interface ISignal {
    double TOTAL_INTENSITY = 0.0d;
    String TOTAL_INTENSITY_DESCRIPTION = "TIC";
    double getX();
    double getY();
}
```
- 实现语义：`AbstractScan` 里 `getX()=retentionTime`、`getY()=getTotalSignal()`；`IPeak` 里 `getX()=峰顶 RT(ms)`、`getY()=峰丰度+背景丰度`（C-D 交叉证据成立）。

**IPoint**（`org.eclipse.chemclipse.numeric/core/IPoint.java`，注意包在 numeric 插件，非 model）：
```java
public interface IPoint {
    double getX(); void setX(double x);
    double getY(); void setY(double y);
}
```
- 实现类 `numeric/core/Point.java`：`Point(double x, double y)` + equals/hashCode/toString。
- **IPointOfInflection：本版本 ChemClipse 源码中不存在该接口**（grep 全 .fetch 树 0 命中）。峰拐点相关 API 在 `IPeakModel`/`AbstractPeakModel`（`areInflectionPointsAvailable()`、`getWidthByInflectionPoints()`、`calculateInflectionPointEquations()` 内部计算），无独立点类型。

**IScan 完整成员（✅ 逐字）**：
- 父链字段：`getParentChromatogram()/setParentChromatogram(IChromatogram)`（无宿主返回 null）；`getScanNumber()/setScanNumber(int)`（**不在色谱中 = 0**，只允许 ≥0）；`getRetentionTime()/setRetentionTime(int)`（**毫秒**，≥0 才存）；`get/setRetentionTimeColumn1()`、`get/setRetentionTimeColumn2()`（GCxGC/LCxLC）；`get/setRelativeRetentionTime()`（RRT，毫秒）；`getRetentionIndex()`/`setRetentionIndex(float)`（≥0）；`hasAdditionalRetentionIndices()`、`get/setRetentionIndex(SeparationColumnType, ...)`、`Map<SeparationColumnType,Float> getRetentionIndicesTyped()`（惰性 EnumMap）；`float getTotalSignal()`（无信号返回 0）；`int getTimeSegmentId()/setTimeSegmentId`（默认 1）；`int getCycleNumber()/setCycleNumber`（**默认 1**，同 cycle 扫描在 TIC 显示中求和）；`boolean isDirty()/setDirty(boolean)`；`String getIdentifier()/setIdentifier`；`void adjustTotalSignal(float totalSignal)`（"把所有成分平移使总信号=给定值"）
- 继承 `ITargetSupplier.getTargets()` → `Set<IIdentificationTarget>`（可变 HashSet，实现见 AbstractScan）

### 3.1 已确认的 IScan 字段/方法（✅ 接口原文 + 构造点双证据）

| 字段/方法 | 类型 | 证据 |
|---|---|---|
| `getRetentionTime()` / `setRetentionTime(int)` | **int，毫秒** | CDF CSD：`scan.setRetentionTime(retentionTime); retentionTime += in.getScanInterval();`（延迟+间隔累加，间隔已是 ms）；animl：`scan.setRetentionTime(Math.round(retentionTimes.get(i)))`；PeakDetectorCSD：`chromatogram.getScan(i).getRetentionTime()`；mzdb 写入用 int |
| `getTotalSignal()` | **float** | VendorScan(CSD) 的 `private float totalSignal`；`new VendorScan(in.getIntensity(i))` 传入 float；PeakSupport `new ScanCSD(scan.getTotalSignal())`；PeakDetectorCSD `scan.getTotalSignal()` |
| `getScanNumber()` | int，**1-based** | mzMLb `spectrum.setIndex(BigInteger.valueOf(scanMSD.getScanNumber() - 1))`；mzdb `buffer.putInt(scanMSD.getScanNumber())`；CDF MSD `getMassSpectrum(scan)` 校验 `scan < 1` |
| `getTargets()` | `List<IIdentificationTarget>`（可变） | CDF peak table：`vendorChromatogram.getScan(scanNumber).getTargets().add(identificationTarget)`；gaml Reader：`chromatogram.getScan(...).getTargets().add(...)` |
| `setTotalSignal(float)` / `adjustTotalSignal(float)` | void | VendorScan(CSD) 实现；接口层只有 `adjustTotalSignal(float)`（`IScan.java`），CSD 扫描无 setTotalSignal 接口方法（ScanCSD 内部私有 setTotalSignal） |
| 抽象基类 | `AbstractScanCSD` / `AbstractScanMSD` | extends `AbstractScan`（`model/core/AbstractScan.java`）；字段与行为见 §5.2 |

### 3.2 CSD 扫描（单通道）：float totalSignal（✅）

`VendorScan(CSD)`（extends `AbstractScanCSD`）字段**只有** `float totalSignal`。构造：
- `VendorScan(int retentionTime, float totalSignal)`：设 RT + 信号
- `VendorScan(float totalSignal)`：**只设信号**，RT 之后由 `setRetentionTime` 补

> ⚠️ 注意：`VendorScan(float)` 构造不调 `super()` 的 RT 参数也不 setRetentionTime，CDF Reader 用此构造 + 显式 `setRetentionTime`。

### 3.3 MSD 扫描（质谱）：m/z + 丰度离子列表（✅）

- `VendorScan(MSD)`（extends `AbstractScanMSD`）：是 **IMassSpectrum**（getIons()/addIon/removeIon 语义），`makeDeepCopy()` 深拷贝离子 `new VendorIon(ion.getIon(), ion.getAbundance())`
- `VendorIon`（extends `AbstractIon`）：`(double ion)`、`(double ion, float abundance)`——**m/z 为 double，丰度为 float**
- CDF MSD `getMassSpectrum`：`double mz = valueArrayIon[position]; float intensity = valueArrayAbundance[position]; if(intensity > 0) addIon(...)` → `new VendorIon(mz, intensity); massSpectrum.addIon(ion, false)`——**强度为 0 的离子不存**（省内存）
- MSD 扫描另有**提取离子信号**：`scanMSD.getExtractedIonSignal().getAbundance(ion)`（PeakSupport）。**`IExtractedIonSignal` 完整定义 ✅**（`msd.model/xic/IExtractedIonSignal.java`）：`setAbundance(IIon[, removePrevious])`、`setAbundance(int ion, float ab[, removePrevious])`、`float getAbundance(int ion)`、`getNumberOfIonValues()`、`float getTotalSignal()`、`int getIonMaxIntensity()`、`getMaxIntensity/getMinIntensity/getNthHighestIntensity(n)/getMeanIntensity/getMedianIntensity`、`get/setRetentionTime(int ms)`、`get/setRetentionIndex(float)`、`int getStartIon()/getStopIon()`、`IIonRange getIonRange()`、`normalizeIntensity([base])`/`normalizeVector([base])`、常量 `ION_NOT_SET=0`。**实现类 `ExtractedIonSignal` 用 `float[] abundanceValues` 按标称质量轴连续存储**（`position = 标称m/z - startIon`，见 §5.2）——XIC 计算的基础 ✅

### 3.4 WSD 扫描（波长）：类似 MSD

`IScanWSD.getExtractedWavelengthSignal().getAbundance(wavelength)`（PeakSupport）——波长轴版提取信号。**`IExtractedWavelengthSignal` 完整定义 ✅**（`wsd.model/xwc/IExtractedWavelengthSignal.java`）：与 IExtractedIonSignal 同构，`setAbundance(IScanSignalWSD[, remove])`/`setAbundance(int wavelength, float ab[, remove])`、`getAbundance(int wavelength)`、`getStartWavelength()/getStopWavelength()`、`IWavelengthRange getWavelengthRange()`、`normalize([base])` 等。`IScanWSD` 另持有 `List<IScanSignalWSD>`（getScanSignals/addScanSignal/removeScanSignal）+ `getExtractedSingleWavelengthSignal(float)` + `getTotalSignal(excluded)`（完整定义 ✅，`wsd.model/core/IScanWSD.java`）

## 4. 信号层：ISignal / ITotalScanSignal / ITotalScanSignals / TotalScanSignalsModifier（★ ✅ 完整定义）

> 本层全部接口/实现在本机 `org.eclipse.chemclipse.model/signals/`，逐字精读。PeakDetectorCSD 的真实用法（C-C）与接口原文一致。

### 4.1 ISignal（通用点接口，✅ 完整定义见 §3.0）

`model/core/ISignal.java`：`double getX()/getY()` + 常量 `TOTAL_INTENSITY=0.0d`、`TOTAL_INTENSITY_DESCRIPTION="TIC"`。IPeak 的默认实现展示了语义：`getX()`=峰顶 RT(ms)、`getY()`=峰丰度+背景丰度。

### 4.2 ITotalScanSignal（✅ 接口原文 `model/signals/ITotalScanSignal.java`）

| 方法 | 返回 | 说明 |
|---|---|---|
| `getRetentionTime()` / `setRetentionTime(int)` | int | **毫秒** |
| `getRetentionIndex()` / `setRetentionIndex(float)` | float | |
| `getTotalSignal()` | float | |
| `setTotalSignal(float)` | void | 自动校验只接受正值 |
| `setTotalSignal(float, boolean validatePositive)` | void | false=可存负值（对应负信号专用类 `ExtendedTotalScanSignal`） |
| `makeDeepCopy()` | `ITotalScanSignal` | 深拷贝 |

- 实现 `AbstractTotalScanSignal`：`private int retentionTime; private float retentionIndex; private float totalSignal;`（毫秒/float/float）。`TotalScanSignal(rt, ri, ts[, validatePositive])` 构造时逐字段校验 ≥0。

### 4.3 ITotalScanSignals（✅ 接口原文 `model/signals/ITotalScanSignals.java`）

`extends Iterable<Integer>`（迭代产出**扫描号**）。**常量 `float NORMALIZATION_BASE = 1000.0f`**（`TotalScanSignalsModifier.normalize(signals)` 单参版默认基准；⚠️ PeakDetectorCSD 实际传入的是 `BasePeakDetector.NORMALIZATION_BASE = 100000.0f`——见 §4.5）。

| 方法 | 说明 |
|---|---|
| `IChromatogram getChromatogram()` | 信号来源色谱 |
| `void add(ITotalScanSignal)` | 尾部追加 |
| `ITotalScanSignal getTotalScanSignal(int scan)` | **扫描号 1-based**，越界/null 返回 null |
| `default getNextTotalScanSignal(int)` / `getPreviousTotalScanSignal(int)` | 相邻扫描（`getTotalScanSignal(±1)`） |
| `default getFirstTotalScanSignal()` / `getLastTotalScanSignal()` | = get(startScan)/get(stopScan) |
| `isEmpty()` / `size()` | |
| `getStartScan()` / `getStopScan()` | 起止扫描号 |
| `default getMaxSignal()` / `getMinSignal()` | 用 `Calculations.getMax/Min(float[])` |
| `makeDeepCopy()` | 深拷贝 |
| `List<ITotalScanSignal> getTotalScanSignals()` | **返回副本**（新增/删除必须走 add） |
| `List<ITotalScanSignal> getTotalScanSignalList()` | **不可变视图** |
| `default getMaxTotalScanSignal()` / `getMinTotalScanSignal()` | 用 TotalScanSignalComparator |
| `default setNegativeTotalSignalsToZero()` / `setPositiveTotalSignalsToZero()` / `setTotalSignalsAsAbsoluteValues()` | 批量调整 |
| `default float[] getValues()` / `getValues(IScanRange)` | 导出 float 数组 |

**`TotalScanSignals(IChromatogramSelection)` 构造器（✅ 从 Selection 提取的精确算法，`model/signals/TotalScanSignals.java`）**：

```java
public TotalScanSignals(IChromatogramSelection chromatogramSelection) {
    chromatogram = chromatogramSelection.getChromatogram();
    startScan = chromatogram.getScanNumber(chromatogramSelection.getStartRetentionTime()); // RT→扫描号(1-based)
    stopScan  = chromatogram.getScanNumber(chromatogramSelection.getStopRetentionTime());
    for(int scan = startScan; scan <= stopScan; scan++) {
        IScan supplierScan = chromatogram.getScan(scan);
        if(supplierScan != null) {
            signals.add(new TotalScanSignal(supplierScan.getRetentionTime(),
                                            supplierScan.getRetentionIndex(),
                                            supplierScan.getTotalSignal()));
        }
    }
}
```
即：**Selection 的 RT 范围 → `getScanNumber` 反查扫描号区间 → 逐扫描拷贝 (RT, RI, totalSignal) 三元组**。
`getTotalScanSignal(scan)` 内部：`scan` 必须在 `[startScan, stopScan]`，`correction = startScan-1`，`signals.get(scan - correction - 1)`——**1-based 索引归一化**。其他构造器：`(int numberOfScans)`（start=1, stop=n）、`(startScan, stopScan)`（自动排序/截 0）、各带 IChromatogram 版本。

### 4.4 TotalScanSignalsModifier（✅ 接口原文 `model/signals/TotalScanSignalsModifier.java`，全静态）

**`normalize(signals, base)` 精确算法**（一阶导峰检测前用）：
```java
if(totalIonSignals == null) return;
if(base < 1.0f) return;
double max = totalIonSignals.getMaxSignal();      // 全部信号中的最大值
double factor = (max != 0.0d) ? base / max : 0.0d; // 缩放因子 = base / max
for(ITotalScanSignal signal : signals) {
    signal.setTotalSignal((float)(factor * signal.getTotalSignal())); // 每个信号 × factor
}
```
即 **`signal' = signal × (base / maxSignal)`**——把最大信号缩放到 `base`，其余等比缩放。PeakDetectorCSD 用 `base = 100000.0f`（BasePeakDetector.NORMALIZATION_BASE，`chromatogram.xxd.peak.detector.supplier.firstderivative/core/BasePeakDetector.java:37`），把整条 TIC 曲线放大到 10⁵ 量级后求一阶导。
其他静态方法：`normalize(signals)`（默认 1000）、`meanNormalize`（除以 |均值|）、`medianNormalize`（除以中位数）、`multiply`/`divide`、`calculateMovingAverage(windowSize)`（滑窗均值，注意索引 1-based：`getTotalScanSignal(i+j+1)`）、`unitSumNormalize(areaSum)`。

### 4.5 IChromatogramOverview（✅ 完整定义 `model/core/IChromatogramOverview.java`）

extends `IMeasurementInfo`，javadoc："当色谱扫描不应被解析时使用（文件概览 / 色谱叠加方法）"。

**常量精确值（确认旧文推测 1000/60000 ✅）**：
```java
double SECOND_CORRELATION_FACTOR = 1000.0d;    // 1ms * 1000 = 1s
double MINUTE_CORRELATION_FACTOR = 60000.0d;   // 1ms * 1000 * 60 = 1min
double HOUR_CORRELATION_FACTOR   = 3600000.0d; // 1h（新增，旧文未列）
```
**成员**：`float getMinSignal()`、`float getMaxSignal()`、`float getMaxSignal(boolean condenseCycleNumberScans)`（cycle 合并时更高）、`int getStartRetentionTime()/getStopRetentionTime()`（ms）、`int getScanInterval()`、`void setScanInterval(int ms)`、`void setScanInterval(float scansPerSecond)`（`ms = round(1000/sps)`，范围 0.1~20）、`int getScanDelay()`、`void setScanDelay(int ms)`、`int getScanNumber(int retentionTime)`（**floor 语义**：45003ms→scan34、47790ms→scan35 区间的 47790→34；无匹配返回 0；实现是线性扫描，注释 TODO 说等间隔时可用公式但未启用）、`int getScanNumber(float retentionTime)`（**分钟单位**，`×1000×60` 转 ms）、`int getNumberOfScans()`、`String getName()`、`void setFile(File)`（javadoc：只应在 msd.model 使用）、`File getFile()`、`float getTotalSignal()`（**= Σ 全部扫描总信号**）、`void recalculateRetentionTimes()`（delay + 逐累加 interval，尾部追加不重算）、`void recalculateScanNumbers()`。

> 保留旧结论：**社区插件里 Grep 不到任何 `ITotalScanSignal|TotalScanSignals|TotalScanSignalsModifier` 引用**——信号层只在 ChemClipse 内部处理算法中使用，不暴露给社区插件。⚠️ Qt 移植时信号层属内部实现，不必 1:1 复刻（但 `normalize` 公式与 Selection→信号提取是通用算法，建议保留）。

## 5. 检测器家族与实现类（✅ 接口引用 + 内部字段全部源码确认）

### 5.1 检测器特异模型接口（✅ 大量 import + 泛型证据）

社区插件直接 import 并使用：

| 接口 | 证据 |
|---|---|
| `org.eclipse.chemclipse.csd.model.core.IChromatogramCSD` | 所有 CSD converter Reader 的 `read()` 返回类型；`AbstractChromatogramImportConverter<IChromatogramCSD>` 泛型；`ChromatogramImportConverterCSD` |
| `org.eclipse.chemclipse.msd.model.core.IChromatogramMSD` | `ChromatogramReaderMSD.read()` 返回 `IChromatogramMSD`；AMDIS、MassShiftDetector 全量使用 |
| `org.eclipse.chemclipse.wsd.model.core.IChromatogramWSD` | animl/abif/cdf 的 WSD Reader |
| `IChromatogramSelectionCSD`/`ChromatogramSelectionCSD` | PeakSupport：`new ChromatogramSelectionCSD(chromatogramCopy)`；PeakDetectorCSD 参数类型 |
| `IChromatogramPeakCSD/MSD/WSD`、`IPeakCSD/MSD/WSD`、`IScanCSD/MSD/WSD` | PeakSupport 的 instanceof 分派 + 泛型加峰 |

→ 运行时 `chromatogram instanceof IChromatogramMSD/CSD/WSD/VSD` 分派是**常规模式**（PeakSupport.addPeak / extractPeakByScanRange），检测器特异性通过"接口族 + 扫描子类型"表达。

### 5.2 实现类（✅ 构造点引用，内部结构见 §5.3）

| 实现类（ChemClipse） | 使用证据 |
|---|---|
| `org.eclipse.chemclipse.model.implementation.Chromatogram` | IChromatogramSelection 的 import（javadoc 指向）✅ |
| `org.eclipse.chemclipse.csd.model.implementation.ChromatogramCSD` | PeakSupport `new ChromatogramCSD()` |
| `org.eclipse.chemclipse.csd.model.implementation.ScanCSD` | PeakSupport `new ScanCSD(scan.getTotalSignal())` + `setRetentionTime` |
| `org.eclipse.chemclipse.csd.model.implementation.ChromatogramPeakCSD` | PeakTransfer import |
| `org.eclipse.chemclipse.msd.model.implementation.ChromatogramPeakMSD` | PeakSupport import + `instanceof ChromatogramPeakMSD peakMSD` |
| `org.eclipse.chemclipse.model.implementation.IdentificationTarget` | CDF/gaml Reader 建目标 |
| Vendor 插件子类：`VendorChromatogramCSD extends AbstractChromatogramCSD`、`VendorChromatogram extends AbstractChromatogramMSD`、`VendorScan extends AbstractScanCSD/MSD`、`VendorIon extends AbstractIon` | 本节各文件 ✅ |

> ✅ 结论：**模型实现（抽象基类 + 具体类）在 ChemClipse 编译单元里；社区插件通过"Vendor 子类继承 ChemClipse 抽象基类"实现数据格式适配，只写字段级代码**（如 VendorChromatogramMSD 只加 `Date dateOfExperiment`）。抽象基类内部存储已全部精读（§5.3）。

### 5.3 实现类内部结构（★ ✅ 逐字段源码确认）

**AbstractChromatogram**（`model/core/AbstractChromatogram.java`，所有色谱的存储核心）：
- `private final List<IScan> scans = new ArrayList<>();` ← **扫描 = List<IScan>，就是它**（无数组/无 map；CSD/MSD 子类都复用，不再另存）
- `addScan(IScan)`：`scan.setParentChromatogram(this)` + `scan.setScanNumber(++lastScan)`（**1-based**）+ `list.add(scan)`
- `getScan(int)`：`list.get(--position)`，越界返回 null；`getScans()` 返回**本体 List**（javadoc 警告"只用于遍历，改 List 可能出问题"）
- `getScanNumber(int rt)`：**线性扫描**取 floor（`rt > 当前扫描RT → return --scan`），越界/无 scanInterval 返回 0；等间隔快速公式被注释（"TODO optimize"）未启用
- `getTotalSignal()`：**实时求和**（Σ `scan.getTotalSignal()`），无缓存字段；`getMinSignal/getMaxSignal` 同样遍历
- `setDirty(boolean)`：`modCount++` / `modCount=0`；`isDirty() = modCount != 0`（**脏标记 = 修改计数**）
- `getConverterId()`：`finalized ? "" : converterId`（finalized 时强制返回 ""，与 §7 语义一致）
- 其他字段：`scanDelay=4500`、`scanInterval=1000`（默认）、`finalized=false`、`file=null`、`Map<String,IBaselineModel> baselineModelMap`（构造时放 `DEFAULT_BASELINE_ID→new BaselineModel(this)`）、`List<IChromatogramUpdateListener> updateSupport`、`IEditHistory editHistory=new EditHistory()`、`List<IChromatogram> referencedChromatograms`、`ObservedPeakList<? extends IChromatogramPeak> peaks`、`Set<IIdentificationTarget> identificationTargets`、`List<ChromatogramAnalysisSegment> analysisSegments`、`Map<String,Object> processDataMap`
- `fireUpdateChange(forceReload)`（protected）：遍历 updateSupport 逐个 `listener.update(forceReload)`（SafeRunner 包裹，单监听异常不影响其他）
- **峰表 ObservedPeakList**（`model/implementation/ObservedPeakList.java`，extends ArrayList）：`add/addAll` 校验 `hasValidPeakModel`（有拐点时 `getWidthByInflectionPoints() > 0` 否则拒绝）；`remove/clear` 自动 `peak.setMarkedAsDeleted(true)` 并同步 `PeakRetentionTimeMap`（RT 区间查询索引）。→ **`getPeaks().add(peak)` 直接改本体，但无效峰模型会被拒收**（比旧文"可变 List 直接 add"更精确）
- `recalculateRetentionTimes()`：`actual=scanDelay`，逐 scan `setRetentionTime(actual); actual += scanInterval`，末尾 `fireUpdateChange(true)`

**AbstractChromatogramCSD**（`csd.model/core/AbstractChromatogramCSD.java`）：无新扫描存储；只重写 `getScan(int)→IScanCSD`（instanceof 校验）、`getPeaks()` 强转 `List<IChromatogramPeakCSD>`、`fireUpdate(IChromatogramSelection)→((ChromatogramSelectionCSD)sel).update(true)`、噪声计算器工厂（`NoiseCalculator.getNoiseCalculator(id)`）
**AbstractChromatogramMSD**（`msd.model/core/AbstractChromatogramMSD.java`）：同上复用 scans；新增字段 `IIonTransitionSettings ionTransitionSettings`、`ImmutableZeroIon`、`IScanMSD combinedMassSpectrum`；重写 `getScan(int[, excludedIons])`、`getNumberOfScanIons()`（Σ 各扫描离子数）、`getMin/MaxIonAbundance()`、`getStart/StopIon()`、`getIonTransitionSettings()`、`get/setCombinedMassSpectrum()`；`fireUpdate→((ChromatogramSelectionMSD)sel).update(true)`

**AbstractScan**（`model/core/AbstractScan.java`，extends AbstractSignal）：字段 `retentionTime=0`、`retentionTimeColumn1/2=0`、`relativeRetentionTime=0`、`retentionIndex=0`、`Map<SeparationColumnType,Float> additionalRetentionIndices=null`（惰性 EnumMap）、`scanNumber=0`、`timeSegmentId=1`、`cycleNumber=1`、`Set<IIdentificationTarget> identificationTargets`、`isDirty=false`、`identifier=""`、`transient IChromatogram parentChromatogram`。**ISignal 实现：`getX()=retentionTime`、`getY()=getTotalSignal()`**。`setRetentionTime` 等 setter 带 ≥0 校验 + `setDirty(true)`
**AbstractScanCSD**（`csd.model/core/AbstractScanCSD.java`）：空类（仅 extends AbstractScan implements IScanCSD）；**`ScanCSD` 加唯一字段 `private float totalSignal`**，`getTotalSignal()` 返回字段、`adjustTotalSignal()` 直接赋值 → **CSD 总信号 = 存储字段（非求和）**
**AbstractScanMSD**（`msd.model/core/AbstractScanMSD.java`）：核心字段 `private List<IIon> ionsList`（初值 `new ArrayList<>(200)`）；`getTotalSignal()` **实时遍历 Σ ion.getAbundance()**；`getIons()` 返回不可变视图（增删走 addIon/removeIon）；`addIon(IIon[, checked])/addIon(boolean addIntensity, IIon)`——同 m/z 时按 `addIntensity` 决定**加和**还是**取大**（`checkIntensityCollisions()` 默认 true）；`getIon(int)` 按标称 m/z 聚合并返回新 Ion；`getHighest/LowestAbundance/Ion` 用 IonCombinedComparator（MZ_FIRST / ABUNDANCE_FIRST），空谱返回 `ImmutableZeroIon`；`getExtractedIonSignal([start,stop])` 动态求离子界再建数组；`normalize(base=100.0f)` 每离子 ×(base/最高丰度)；`adjustTotalSignal` 用 `correctionFactor = ((100/currentTotal)*target)/100` 逐离子缩放；`isMeasurementSIM()` = 离子数 ≤10；`isHighResolutionMS()` = >3000 离子或采样判断小数位
**AbstractIon**（`msd.model/core/AbstractIon.java`）：**只有两个字段 `private double ion; private float abundance;`**；`getIon()/setIon(double)`、`getAbundance()/setAbundance(float)`（setter 拒绝负值返回 false）；静态工具 `getIon(double)→int`（IonRoundMethod 取整）、`getIon(double, int precision)`（最大 6 位）、`getAbundance(float)→int`。**`Ion`**（`msd.model/implementation/Ion.java`）仅三个构造器：`(double)`、`(double, float)`、`(IIon)`。`ImmutableZeroIon`（`implementation/ImmutableZeroIon.java`）：只读零离子（覆盖 setter 抛异常），空谱哨兵
> ⚠️ **R6 修正**：旧文档 R6 引用的 `AbstractIon.addIon/getTotalSignal` 实际并不存在——`addIon` 与 `getTotalSignal` 都在 **`AbstractScanMSD`**（质谱扫描）上。AbstractIon 只有 m/z/丰度读写。

### 5.4 检测器特异接口完整成员（★ ✅ 接口原文）

| 接口 | 关键成员（接口原文） |
|---|---|
| `IChromatogramMSD extends IChromatogram, IChromatogramPeaksMSD`（`msd.model/core`） | `IScanMSD getScan(int, IMarkedTraces<ITrace> excludedIons)`（深克隆后剔除离子）、`@Override IScanMSD getScan(int)`、`int getNumberOfScanIons()`、`void enforceLoadScanProxies(IProgressMonitor)`（代理扫描按需加载）、`float getMinIonAbundance()/getMaxIonAbundance()`、`double getStartIon()/getStopIon()`、`IIonTransitionSettings getIonTransitionSettings()`、`void setCombinedMassSpectrum(IScanMSD)`/`getCombinedMassSpectrum()`（全谱合并概要） |
| `IChromatogramCSD extends IChromatogram, IChromatogramPeaksCSD`（`csd.model/core`） | 仅 `@Override IScanCSD getScan(int)`——**CSD 色谱无新增成员**（单通道，全靠父接口） |
| `IChromatogramWSD extends IChromatogram, IChromatogramWSDBaseline, IChromatogramPeaksWSD`（`wsd.model/core`） | `@Override IScanWSD getScan(int)`、`Set<Float> getWavelengths()`（全部波长集合） |
| `IScanMSD extends IScan, IMassSpectrumCloneable, IMassSpectrumNormalizable, IIonProvider` | 见 §5.3 AbstractScanMSD 对应方法（getTotalSignal(excluded)、getExtractedIonSignal([start,stop])、getBasePeak/getBasePeakAbundance、getLowest/HighestIon/Abundance、getIonBounds、addIon 4 重载/removeIon 4 重载、getIon(int/double/2 参)、adjustIons(percentage)、getMassSpectrum(excluded)、hasIons、enforceLoadScanProxy、set/getOptimizedMassSpectrum、isMeasurementSIM/isTandemMS/isHighResolutionMS/checkIntensityCollisions、adjustTotalSignal） |
| `IScanCSD extends IScan`（`csd.model/core`） | **空接口**（标记类型） |
| `IScanWSD extends IScan` | getScanSignal(int/float)、addScanSignal/removeScanSignal/deleteScanSignals/getNumberOfScanSignals、getExtractedWavelengthSignal([start,stop])、getExtractedSingleWavelengthSignal(float)、hasScanSignals、getWavelengthBounds、getTotalSignal(excluded) |
| `IChromatogramPeaks{CSD|MSD|WSD} extends IChromatogramPeaks` | 仅把 `getPeaks()` 泛型收窄为 `List<IChromatogramPeak{CSD|MSD|WSD}>`（桥接：实现类强转 super.getPeaks()） |

## 6. 模型如何被构造（★ 关键：Reader 生命周期）

### 6.1 CSD CDF 完整构造流程（✅ 逐行读自 ChromatogramReaderCSD + AbstractCDFChromatogramArrayReader）

```
ChromatogramImportConverterCSD(extends AbstractChromatogramImportConverter<IChromatogramCSD>)
  └─ convert(File, monitor)
       └─ IChromatogramCSDReader reader = new ChromatogramReaderCSD();
       └─ IChromatogramCSD chromatogram = reader.read(file, monitor);   // 返回类型 IChromatogramCSD ✅
            └─ readChromatogram(...)
                 ├─ new VendorChromatogramCSD()   // extends AbstractChromatogramCSD
                 ├─ setChromatogramEntries():
                 │    ├─ setScanDelay(scanDelay≥0 取 0) / setScanInterval(interval)
                 │    ├─ setConverterId("net.openchrom.csd.converter.supplier.cdf")   // 记忆可回写格式 ✅
                 │    ├─ setFile(file) / setDate(creationDate) / setOperator(operator)
                 ├─ RT 换算（见下）
                 ├─ for i in 0..scans-1:
                 │    VendorScan scan = new VendorScan(in.getIntensity(i));  // 只带 float 信号
                 │    scan.setRetentionTime(retentionTime);                  // int ms
                 │    retentionTime += in.getScanInterval();                 // 等间隔累加
                 │    chromatogram.addScan(scan);                            // 逐个挂载 ✅
                 └─ in.readPeakTable(chromatogram)  // 峰表→每峰按 RT 反查 scan 挂 IdentificationTarget
```

**RT 单位换算（✅，秒/分→毫秒）**：
- 读 CDF 全局属性 `retention_unit`：`"seconds"/"Seconds"/"s"` → `IChromatogramOverview.SECOND_CORRELATION_FACTOR`；`"minutes"/.../ "time in minutes"` → `MINUTE_CORRELATION_FACTOR`；否则（含无属性）因子 = 1（已毫秒）。
- `scanDelay = (int)(actual_delay_time * factor)`；`scanInterval = (int)(actual_sampling_interval * factor)`。
- DataApex 变体（存在 `actual_run_time_length`）：`scanInterval = (int)((runTimeLength - delayTime) * factor / (scans-1))`。
- `scanInterval == 0` → 兜底 **200 ms**。
- ⚠️ 因子常量精确值已确认（✅）：`IChromatogramOverview.SECOND_CORRELATION_FACTOR = 1000.0d`、`MINUTE_CORRELATION_FACTOR = 60000.0d`（另 HOUR=3600000.0d，§4.5）；MSD 的 `getScanAcquisitionTime` 直接 `(int)(value * 1000)`，即假定秒。
- 格式魔数：文件头 3 字节 `"CDF"`（`isValidFileFormat`）。

### 6.2 MSD CDF 构造流程（✅ 抽样全流程）

```
ChromatogramReaderMSD(extends AbstractChromatogramMSDReader)
  └─ read() → IChromatogramMSD
       └─ new VendorChromatogram()   // extends AbstractChromatogramMSD
       ├─ setConverterId("net.openchrom.msd.converter.supplier.cdf")
       ├─ setScanDelay/Interval/setFile/setDate/setDateOfExperiment/setMiscInfo/setOperator
       └─ for i in 1..n:
            VendorScan massSpectrum = in.getMassSpectrum(i);  // 质谱扫描
            chromatogram.addScan(massSpectrum);
```

- `getMassSpectrum(scan)`（CDFChromtogramArrayReader）：
  - 越界检查 `scan < 1 || scan > n` → NoSuchScanStored（**扫描号 1-based ✅**）
  - 用 `point_count` + `scan_index` 切出本扫描的离子段；`double mz` + `float intensity`；**intensity>0 才存** → `new VendorIon(mz, intensity); massSpectrum.addIon(ion, false)`
  - `massSpectrum.setRetentionTime(getScanAcquisitionTime(scan))` = `(int)(value * 1000)`（秒→毫秒，此路径**硬编码 ×1000**，不同于 CSD 的 retention_unit 换算）
- **readOverview()（✅ 大数据预览的轻量模型）**：不读全质谱，每扫描只建 **1 个 `VendorIon(IIon.TIC_ION)`**（丰度 = 文件总信号 `getTotalSignal(i)`）→ TIC-only 色谱；返回 `IChromatogramOverview`。这印证了 `IChromatogramOverview` 的"概览/轻量"职责。

### 6.3 第二个 CSD Reader（animl，✅ 构造模式一致性）

`ChromatogramReader(CSD/animl)`：`IScan scan = new VendorScan(signals.get(i)); scan.setRetentionTime(Math.round(retentionTimes.get(i))); chromatogram.addScan(scan);`——**同样的三连**（构造扫描→设 RT→addScan）。另有 `chromatogram.getEditHistory().addAll(...)` 回填审计轨迹；峰表用 `getScanNumber(start/endTime)` 反查范围 + `PeakBuilderCSD.createPeak(chromatogram, scanRange, true)` 建峰 + `chromatogram.getPeaks().add(chromatogramPeak)`。

## 7. 生命周期与所有者（✅ 全局调用点证据）

| 维度 | 结论 | 证据 |
|---|---|---|
| 谁创建 | 数据供应商 Reader：`new VendorChromatogram{CSD}` → 逐 scan addScan → 返回 `IChromatogram{CSD/MSD}`；`ChromatogramImportConverter{CSD}` 泛型 `IProcessingInfo<IChromatogramCSD>` 收尾 | ChromatogramReaderCSD / MSD、ChromatogramImportConverterCSD ✅ |
| 处理中"另建" | PeakSupport 用 `new ChromatogramCSD()` + `new ScanCSD(...)` 复制出一份"可丢弃的处理副本"（临时高分辨/串联抽取用），完事从 `getReferencedChromatograms()` 移除并置 null 释放 | PeakSupport.createChromatogramCopy ✅ |
| 谁修改 | 处理管线（filter/processor/峰检测）→ 改完 `setDirty(true)` | PeakDetectorCSD.detectPeaks 尾部；AMDIS `chromatogramSelection.getChromatogram().setDirty(true)`；RetentionIndexMapper ✅ |
| UI 侧脏标 | 编辑器把脏态委托给 `editorProcessor.setDirty(true)`（编辑历史记录） | EnhancedScanMarkerEditor / TraceDataComparisonUI ✅ |
| 谁读取 | UI editor/view、报告生成器（遍历 `getPeaks()`、`getScan(i).getTotalSignal()`） | ConfigurableReportWriter / ExcelTemplateReportWriter / PeakDetectorChart ✅ |
| 谁销毁 | JVM 生命周期；`setUnloaded()` 释放数据（非销毁对象）；临时子色谱引用移除 + 置 null | PeakSupport line 770-771、787-788 ✅ |
| 变更通知 | Selection 自注册为 chromatogram 的 listener → `fireUpdateChange(forceReload)` → `UpdateNotifier.update(this)` → e4 EventBroker topic 广播；处理管线用 `chromatogram.fireUpdate(selection)` 反向触发（§2.2 全链路） | AbstractChromatogramSelection.java / UpdateNotifier.java / IChemClipseEvents.java:55 + EnhancedScanMarkerEditor:348 ✅ |
| converterId | Reader 导入时写入自己的 id（**记忆"这份数据能由谁回写"**）；不可写格式写 `""`（WSD abif 注释："If the chromatogram shall be exportable, set the id otherwise its null or ''"）；`getConverterId()` 空 → UI 触发"另存为"选择器 | CDF/GAML/ARW/ABIF Reader ✅ |
| finalized | `isFinalized()/setFinalized()`；javadoc 明文："finalized 色谱不能被保存覆盖，converter id 被置 "" 以让软件询问如何保存" | IChromatogram.java ✅ |
| setUnloaded | 大数据释放内存标记；`isUnloaded()` 查询；实现见 AbstractChromatogram.setUnloaded()（置位不销毁对象），社区插件无直接调用 | AbstractChromatogram.java:791-800 ✅ |

## 8. PeakRegionParameter（✅ 反卷积区域参数）

`net.openchrom.xxd.base.model.PeakRegionParameter`（openchrom 社区代码，完整可读）：

- 字段：`List<IPoint> points`（IPoint = RT×强度，double），`add`/`remove` 后**按 X（RT）自动排序**
- `isValid()`：**≥3 个点**才有效
- `getStart()` = 首点、`getStop()` = 末点、`getProposedMaxima()` = 中间点集合
- 用途：用户在地图上点选峰的起/止/候选极大值 → 传给反卷积服务

```java
// IDeconvolutionService（xxd.base.ui）
IProcessingInfo<List<? extends IPeak>> calculate(IChromatogram chromatogram, PeakRegionParameter peakRegionParameter, IProgressMonitor monitor);
```

→ 它是一份"UI 手绘峰区 → 反卷积请求"的参数载体，复用 `IPoint`/`ISignal` 的点语义 ✅。

## 9. 待回填清单（✅ 旧 ❓ 已全部升级，仅 1 项保留 ❓）

| # | 问题 | 状态 |
|---|---|---|
| C1 | IScan / ISignal / ITotalScanSignal / ITotalScanSignals / IExtractedIonSignal 接口定义 | ✅ 全部升级（§3.0/§4.2-4.3/§3.3）。`IPointOfInflection` **不存在**于本版本源码（grep 0 命中），拐点 API 在 IPeakModel |
| C2 | IChromatogramPeaks / IChromatogramBaseline / SegmentedMeasurement / IChromatogramIntegrationSupport / IChromatogramProcessorSupport / ITargetSupplier / ITargetDisplaySettings / IUpdater 成员表 | ✅ 全部升级（§1.4）；ISupplierEditHistory 在 support 插件（§1.4） |
| C3 | IChromatogramOverview 常量精确值 | ✅ `SECOND=1000.0d`、`MINUTE=60000.0d`、`HOUR=3600000.0d`（§4.5） |
| C4 | AbstractChromatogram{CSD|MSD}、AbstractScan{CSD|MSD}、AbstractIon 内部字段 | ✅ 全部升级（§5.3） |
| C5 | IChromatogramMSD/CSD/WSD、IScanMSD/CSD、IExtractedIonSignal/IExtractedWavelengthSignal 成员 | ✅ 全部升级（§5.4/§3.3-3.4） |
| C6 | *UpdateNotifier / IChromatogramUpdateListener 注册-通知机制 | ✅ 全链路升级（§2.2）：Selection 自注册为 listener → chromatogram.fireUpdateChange → selection.update → UpdateNotifier → e4 IEventBroker topic `chromatogram/xxd/load/chromatogramselection`。旧 javadoc 的 ChromatogramSelectionUpdateNotifier 类已不存在 |
| C7 | 模型序列化（.ocm/.ocx） | ❓ 未做（属 MODULE_01，本机有 ocx converter 插件源码但未精读） |
| C8 | `NORMALIZATION_BASE` 与 `TotalScanSignalsModifier.normalize` 算法 | ✅ 双常量：`ITotalScanSignals.NORMALIZATION_BASE=1000.0f`（默认）+ `BasePeakDetector.NORMALIZATION_BASE=100000.0f`（一阶导峰检测实际用）；算法 `signal' = signal × (base/max)`（§4.4） |

## 10. Qt/C++ 移植要点（core_model 模块，⚠️ 设计笔记）

- **Scan 结构**：RT 用 `qint64`（毫秒），总信号 `float`；MSD 加 `QVector<Ion{mz double, abundance float}>`（写入时跳过 abundance==0 的离子，参照 §3.3）；扫描号 = 容器索引+1（1-based）。避免 Java 式装箱。
- **Chromatogram 结构**：`QVector<Scan>` + 峰列表（`QList<QSharedPointer<Peak>>`）+ 基线表 + 元数据（converterId、file、date、operator、scanDelay、scanInterval、editHistory、referencedChromatograms）。
- **构造流程照抄 Reader 模式**：`CoreModelReader::read()` 即模型工厂——new 空 Chromatogram → 算 RT 序列（delay + interval 累加）→ 逐 scan 构造 + `setRetentionTime` + `addScan` → 写 converterId/file/date → 返回检测器子类（`ChromatogramCSD`/`ChromatogramMSD` 继承基类）。RT 单位换算（秒/分→毫秒、无则默认 200ms 间隔）在 Reader 层做，模型层只存毫秒。
- **Selection/通知**：`IChromatogramSelection` + `fireUpdateChange` ≈ Qt 的 `QAbstractItemModel`（`dataChanged`/`modelReset`）——**这是 UI 与分析引擎解耦的最佳落点**，建议自研 CDS 直接以 `QAbstractItemModel` 作为色谱/峰表的数据视图；`setRangeRetentionTime` → `getScanNumber(rt)` 反查封装为 `scanNumberAtRetentionTime(qint64)`（查不到返回 -1）。
- **脏标记**：处理管线（峰检测/滤波）末尾统一置 dirty；编辑器脏态经 `QUndoStack` 或保存状态 flag。
- **converterId**：`QString converterId`——导入时写回写插件 id，空串 = 需"另存为"；finalized 色谱禁止覆盖保存。
- **多色谱引用**：`getReferencedChromatograms` → `QList<QSharedPointer<Chromatogram>>`；临时抽取子色谱挂这里，用完移除释放（参照 PeakSupport）。
- **概览模式**：TIC-only 轻量模型（读 overview 时不展开质谱/波长全谱），对应 `IChromatogramOverview` 职责。
- **信号层算法（新增 ✅）**：`normalize(signals, base)` = `每个信号 × (base / max信号)`（基准 1000 或 100000），是谱峰检测归一化的标准公式，Qt 里可直接照抄；`TotalScanSignals(selection)` 的提取逻辑（Selection RT → `getScanNumber` 反查扫描号 → 逐扫描拷贝 RT/RI/totalSignal）可作为通用"从选区取曲线"工具。
- **通知机制（新增 ✅）**：Qt 侧不用 Eclipse e4 EventBroker，映射为信号槽即可：`Chromatogram` 提供 `scansChanged(int modCount)`（对应 `fireUpdateChange`），`ChromatogramSelection` 构造时 connect 自己 + 提供 `selectionChanged()`（对应 `fireUpdateChange→UpdateNotifier`）；UI 部件 connect 两路。注意 Selection 本身是 chromatogram 的 listener（自注册），模型变更会反向触发 Selection 自校验并二次广播——Qt 里用 `QObject` 父子 + 信号转发天然等价。
- **getScanNumber（新增 ✅）**：实现是**线性扫描取 floor**（含 `retentionTime==stopRT` 特判返回末扫描号），注释里的等间隔公式被 TODO 注释掉了。Qt 移植时**应改用二分**（`std::lower_bound`），数据量大时是明显优化点；注意该语义要求扫描号 1-based 返回、无匹配返回 0（与 Selection 的 -1 哨兵区分）。
- **峰表（新增 ✅）**：ObservedPeakList 的 add 校验（无有效峰模型拒绝）与 remove 自动标记 deleted 值得保留；`PeakRetentionTimeMap` 是 RT 区间查峰的索引，Qt 可用 `std::multimap<int RT, Peak*>` 或排序容器实现。
- **m/z 存储（新增 ✅）**：MSD 扫描 = `QVector<Ion>`（Ion 为 `double mz; float abundance;`），`getTotalSignal()` 是**实时求和**而非字段（大量调用时建议 Qt 加缓存）；CSD 扫描 = 单 `float` 字段直存。提取离子信号 `IExtractedIonSignal` = `float[]` 连续数组，下标 = 标称 m/z − startIon（Qt 用 `QVector<float>`，按需整段分配）。

## 11. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| C-A | IChromatogram 接口全貌（继承链/常量/成员/默认方法） | .fetch/sources/model/IChromatogram.java | ✅ |
| C-B | Selection 事件机制 + RT→scan 反查默认方法 | .fetch/sources/model/IChromatogramSelection.java (fireUpdateChange/getStartScan/getStopScan/validatePeak) | ✅ |
| C-C | 信号层真实用法（TotalScanSignals/Modifier/ITotalScanSignal） | .fetch/sources/firstderivative/PeakDetectorCSD.java | ✅ |
| C-D | ISignal 语义（getX/getY 默认实现） | .fetch/sources/model/IPeak.java | ✅ |
| C-E | 检测器模型插件清单 | .fetch/chemclipse_tree.json | ✅ 存在性 |
| C-F | 模型实现类存在 | IChromatogramSelection.java import + PeakSupport.java (ChromatogramCSD/ScanCSD/ChromatogramPeakMSD) | ✅ |
| C-G | CSD CDF Reader 构造流程 + RT 单位换算 + converterId | ChromatogramReaderCSD.java / AbstractCDFChromatogramArrayReader.java | ✅ |
| C-H | CSD Vendor 扫描/色谱字段（float totalSignal / 空实现） | VendorScan.java / VendorChromatogramCSD.java | ✅ |
| C-I | MSD CDF Reader 构造流程（质谱扫描/离子/TIC 概览/×1000） | ChromatogramReaderMSD.java / CDFChromtogramArrayReader.java | ✅ |
| C-J | MSD Vendor 模型（VendorScan/VendorIon/VendorChromatogram+dateOfExperiment） | net.openchrom.msd.converter.supplier.cdf/model/*.java | ✅ |
| C-K | 导入链泛型返回 IChromatogramCSD/MSD（检测器特异） | ChromatogramImportConverterCSD.java | ✅ |
| C-L | 实现类构造点（new ChromatogramCSD/ScanCSD + addScan + setRetentionTime） | PeakSupport.java (createChromatogramCopy) | ✅ |
| C-M | setDirty 时机（峰检测/编辑后） | PeakDetectorCSD.java:172,226 / AMDIS PeakDetectorELU:47 / EnhancedScanMarkerEditor:80 | ✅ |
| C-N | setConverterId 语义（可写=真实 id，不可写="") | 多 Reader（CDF 写 id；GAML/ARW/ABIF 写 ""） | ✅ |
| C-O | getPeaks() 可变 List（add/size/indexOf/遍历） | PeakDetectorCSD:87 / ConfigurableReportWriter / ExcelTemplateReportWriter | ✅ |
| C-P | referencedChromatograms 作临时抽取容器 | PeakSupport.java:770-771,787-788 | ✅ |
| C-Q | fireUpdateChange UI 调用点 | EnhancedScanMarkerEditor.java:348 | ✅ |
| C-R | PeakRegionParameter（≥3 点、按 X 排序、start/stop/maxima） | net.openchrom.xxd.base/model/PeakRegionParameter.java + IDeconvolutionService.java | ✅ |
| C-S | getEditHistory 审计轨迹回填 | animl/mzdb Reader `.getEditHistory().add(All)(...)` | ✅ |
| C-T | ITargetSupplier 用法（cleanTargets 统一清理） | ChromatogramCleaner.java:92 | ✅ |
| C-U | IScan 字段（RT int ms / totalSignal float / scanNumber 1-based / targets 可变） | §3 交叉证据（Reader 构造 + 写器 + 峰表） | ✅ 用法 |
| C-V | RT 单位毫秒制式（常量 + javadoc + 换算） | IChromatogram.java 常量 / IChromatogramSelection.java javadoc / 两 CDF Reader | ✅ |
| C-W | IChromatogramOverview 常量精确值（SECOND=1000.0d/MINUTE=60000.0d/HOUR=3600000.0d）+ 完整成员表（getScanNumber floor 语义/getScanInterval 双 setter/readOverview 职责） | model/core/IChromatogramOverview.java | ✅ |
| C-X | IScan 完整接口定义（继承链 ISignal+IAdaptable+Serializable+ITargetSupplier、RT 毫秒/scanNumber/cycleNumber/时间段/RI typed/标识/adjustTotalSignal 全成员） | model/core/IScan.java + core/AbstractScan.java | ✅ |
| C-Y | ISignal（TOTAL_INTENSITY=0.0d/TIC、getX/getY）与 IPoint（numeric/core，double x/y 读写）完整定义；IPointOfInflection 不存在于本版本源码 | model/core/ISignal.java / numeric/core/IPoint.java / 全树 grep | ✅ |
| C-Z | 信号层完整定义：ITotalScanSignal（RT/RI/totalSignal + setTotalSignal 双重载 + makeDeepCopy）、ITotalScanSignals（Iterable<Integer>、NORMALIZATION_BASE=1000.0f、11 个 default 方法）、TotalScanSignals(IChromatogramSelection) 提取算法（getScanNumber 反查+逐扫描拷贝三元组）、1-based 索引归一化 | model/signals/ITotalScanSignal.java / ITotalScanSignals.java / TotalScanSignals.java / AbstractTotalScanSignal.java | ✅ |
| C-AA | normalize 精确公式 `signal'=signal×(base/max)` + 双 NORMALIZATION_BASE（ITotalScanSignals=1000.0f 默认 / BasePeakDetector=100000.0f 实际）+ mean/median/movingAverage 等变体 | model/signals/TotalScanSignalsModifier.java / chromatogram.xxd.peak.detector.supplier.firstderivative/core/BasePeakDetector.java:37 | ✅ |
| C-AB | 9 父接口成员表：IChromatogramPeaks（getPeaks(int,int) 区间查询）/IChromatogramBaseline（多基线 id 模型+Default）/SegmentedMeasurement（只读 analysisSegments）/IntegrationSupport/ProcessorSupport（fireUpdate 唯一成员）/ITargetSupplier（Set targets）/ITargetDisplaySettings（标签/旋转/碰撞/可见性 map）/IUpdater（add/remove listener）/ISupplierEditHistory（support 插件，IEditHistory extends List） | model/core|baseline|targets|updates + support/history 各接口原文 | ✅ |
| C-AC | AbstractChromatogram 内部存储：`List<IScan> scans`（addScan 设 parent+scanNumber 1-based）、getScanNumber 线性 floor（等间隔公式被 TODO）、getTotalSignal 实时求和、modCount 型 dirty、finalized→converterId=""、ObservedPeakList 峰表（add 校验/remove 标记 deleted/PeakRetentionTimeMap）、fireUpdateChange 遍历 updateSupport | model/core/AbstractChromatogram.java + implementation/ObservedPeakList.java | ✅ |
| C-AD | 扫描/离子内部字段：AbstractScan 全字段、ScanCSD 仅 float totalSignal（字段直存）、AbstractScanMSD `List<IIon> ionsList`（getTotalSignal 求和）、AbstractIon 仅 double ion+float abundance、Ion 三构造器、ImmutableZeroIon 哨兵；**R6 修正：addIon/getTotalSignal 在 AbstractScanMSD 而非 AbstractIon** | csd.model/core/AbstractScanCSD.java + csd.model/implementation/ScanCSD.java + msd.model/core/AbstractScanMSD.java + AbstractIon.java + implementation/Ion.java | ✅ |
| C-AE | 检测器特异接口成员：IChromatogramMSD（getScan(excluded)/组合质谱/离子界/代理加载）、IChromatogramCSD（仅 getScan 收窄）、IChromatogramWSD（getWavelengths）、IScanMSD（离子增删改查+SIM/串联/高分辨判定）、IScanCSD 空、IScanWSD、IExtractedIonSignal/WavelengthSignal 全成员 + ExtractedIonSignal 用 float[] 标称质量轴存储 | msd.model/core + csd.model/core + wsd.model/core + msd.model/xic/ExtractedIonSignal.java | ✅ |
| C-AF | UpdateNotifier 通知全链路：Selection 构造器自注册 `chromatogram.addChromatogramUpdateListener(this)` → chromatogram.fireUpdateChange→listener.update(forceReload) → ChromatogramSelection{MSD|CSD}.update→fireUpdateChange → UpdateNotifier.update(this)→e4 IEventBroker.send("chromatogram/xxd/load/chromatogramselection", this)；旧 javadoc 的 ChromatogramSelectionUpdateNotifier 类已不存在 | model/selection/AbstractChromatogramSelection.java + notifier/UpdateNotifier.java + updates/IChromatogramUpdateListener.java + msd/csd.model selection/*.java + support/events/IChemClipseEvents.java:55 | ✅ |
| C-AG | IIon 常量精确值（TIC_ION=0.0d、TIC_DESCRIPTION="TIC"、ZERO_INTENSITY=0.0f）+ IIon 继承链（IIonSerializable/IAdaptable/Comparable）+ Ion 实现类构造器 | msd.model/core/IIon.java + IIonSerializable.java + implementation/Ion.java | ✅ |
