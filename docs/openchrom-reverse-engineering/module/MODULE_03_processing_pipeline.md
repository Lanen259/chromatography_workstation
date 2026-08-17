# MODULE_03 — Processing Pipeline（处理管线层）

> **状态：🟢 核心链路已确认（本机 ChemClipse 完整源码，引擎层 ✅ 深挖完毕）**
> 回答「数据如何进入分析引擎 / 分析引擎如何处理 / 结果如何反馈 UI」的工作流骨架。
> 本版依据本机源码重写：tracecompare / templates / massshiftdetector / cms ✅；**处理引擎 `org.eclipse.chemclipse.processing`、方法模型、`.ocm` 序列化、旧式算法包装器全部 ✅**（§7 为全链证据）；**设置序列化框架（SettingsClassParser / JSONSerialization / ISerializationService）✅**（§8 为全链证据）。

---

## 1. 管线概念（⚠️ 背景假设 + ✅ 证据）

色谱数据处理不是单个算法，而是一个**有序步骤序列（Processing Method）**：
`滤波器 → 基线校正 → 峰检测 → 积分 → 定性 → 定量`，每步一个可配置处理器。

证据支撑：
- `IChromatogram extends IChromatogramProcessorSupport`（✅ IChromatogram.java）
- OpenChrom 有 `net.openchrom.xxd.process.supplier.templates`（方法模板）与 `xxd.processor.supplier.tracecompare`（多色谱处理器），都是「处理步骤」插件的形态 ✅
- ChemClipse 有 `org.eclipse.chemclipse.processing`（执行引擎）与 `org.eclipse.chemclipse.chromatogram.method.model`（方法模型）✅（目录树）

> ✅ 已确认（本版新增，见 §7）：方法模型 = `ProcessMethod`/`ProcessEntry`（`processing.methods` 包）；引擎 `AbstractProcessEntryContainer.applyProcessEntries` 按序迭代、按 id 经 `IProcessSupplierContext` 查 supplier、settings 以 **Jackson JSON 字符串按 profile 内嵌**于 entry；进度经 `ProcessExecutionContext` 内的 `IProgressMonitor`；异常捕获后记 error message 并**跳过该步继续**（无事务/回滚）。

**本版新增的机制级确认：**
- **处理步骤有「两套注册与执行机制」并存**（✅ 本机源码）：
  1. **经典 Eclipse 扩展点**（plugin.xml 里的 `*.peakDetectorSupplier` / `*.peakIdentifier` / `*.peakIntegratorSupplier` / `*.peakQuantifierSupplier` / `chromatogramReportSupplier` / `chromatogramFilterSupplier` 等）——算法实现类实现 ChemClipse 接口（`IPeakDetectorMSD`、`IPeakIdentifier`…），方法签名是 `detect/integrate/identify(selection, settings, monitor) → IProcessingInfo<?>`。
  2. **OSGi DS 服务 `IProcessTypeSupplier`**（`@Component(service={IProcessTypeSupplier.class})`）——面向「处理方法（ProcessMethod）」引擎，步骤实现 `IProcessSupplier`，执行签名 `apply(IChromatogramSelection, Settings, ProcessExecutionContext) → IChromatogramSelection`（✅ 已从 `IChromatogramSelectionProcessSupplier` 源码确认，见 §5/§7.7）。
- 两种机制的典型例子：templates 的 `PeakDetector`（扩展点，见 §6）与 `NameSynonymReplacer`（DS 服务，见 §5）——同一个插件里并存。

## 2. 处理步骤类型与插件清单（✅ 目录树 + 本机源码）

### 2.1 滤波器（Filter）——ChemClipse `xxd.filter` + 各 supplier

| 插件 | 功能（⚠️ 按名称推断，需读实现） |
|---|---|
| xxd.filter.supplier.savitzkygolay | Savitzky-Golay 平滑 |
| xxd.filter.supplier.baselinesubtract | 基线扣除 |
| xxd.filter.supplier.normalizer / meannormalizer / mediannormalizer / unitsumnormalizer | 各种归一化 |
| xxd.filter.supplier.multiplier | 乘系数 |
| xxd.filter.supplier.rtshifter | RT 平移 |
| xxd.filter.supplier.invert / zeroset / scan | 反转 / 归零 / 扫描操作 |
| msd.filter.supplier.coda / denoising / backfolding / xpass / subtract / ionremover / splitter / centroiding | MSD 专用（去噪/去卷积/离子过滤等）|

**templates 自带的滤波器（✅ 本机源码）：**
- `ChromatogramFilterRetentionIndexMapper`（`chromatogram/ChromatogramFilterRetentionIndexMapper.java`）——保留指数映射/抽取，扩展点 `org.eclipse.chemclipse.chromatogram.filter.chromatogramFilterSupplier`，settings = `FilterSettingsRetentionIndexMapper`。

### 2.2 峰检测器（Peak Detector）——templates 见 §6，其余见 MODULE_04

### 2.3 计算器（Calculator）——ChemClipse `xxd.calculator.*`

`xxd.calculator`、`xxd.calculator.peak.resolution`（峰分辨率）、`supplier.amdiscalri`（AMDIS 保留指数）、`supplier.noise.{dyson,stein}`（噪声算法）✅

### 2.4 OpenChrom 自有处理器（本仓库源码，✅ 本版全部深挖）

| 插件 | 入口类 / 签名（✅ 本机源码） | 注册方式 |
|---|---|---|
| xxd.processor.supplier.tracecompare | core/DataProcessor.java：`getTrackStatistics(IProcessorModel)` → `List<TrackStatistics>`（见 §3）| 无 plugin.xml；纯库，被 `.ui` 插件直接调用 |
| chromatogram.msd.processor.supplier.massshiftdetector | core/MassShiftDetector.java：`calculateIonCertainties(IChromatogramMSD, IChromatogramMSD, IProcessorSettings, IProgressMonitor)` → `CalculatedIonCertainties`；文件后缀 `.mdp` | 无 plugin.xml；纯库 |
| xxd.process.supplier.templates | 两种机制（扩展点 + DS 服务），见 §5/§6 | plugin.xml + OSGi DS |
| msd.process.supplier.cms | core/MassSpectraDecomposition.java：`decompose(IMassSpectra, IMassSpectra, boolean, PrintStream)` → `DecompositionResults`；core/MassSpectraCorrelation.java | 无 plugin.xml；纯库 |

> ✅ 重要模式确认：`tracecompare` 与 `massshiftdetector` 都使用**处理器自带 versioned 模型（v1000）+ 专属 Reader/Writer 持久化配置**（tracecompare 是 JAXB XML `.otc`，见 §4）——处理方法的序列化是「每个处理器自持 schema」，复刻时值得注意。

## 3. ★ tracecompare：DataProcessor 完整调用链（✅ 本机源码全链）

**重要修正**：`core/DataProcessor.java` **没有 `execute` 方法**。它只是「模型→统计结果」的纯计算 + 文件辅助类；真正的数据装载（读色谱、抽通道）在 `.ui` 插件的 `DataProcessorUI extends DataProcessor` 里。

### 3.1 数据流全链（UI 侧驱动）

```
UI 向导/编辑器（WizardProcessor / TraceCompareEditorUI）
  → MeasurementModelData.loadTrackModel(IProcessorModel, track, analysisType, sampleGroup, referenceGroup)
      ├─ dataProcessorUI.getMeasurementFileList(processorModel, SAMPLE/REFERENCE, group)
      │     → DataProcessor.getMeasurementFiles(dir, ext, pattern)   // 按文件名前缀匹配组
      ├─ dataProcessorUI.extractMeasurementsData(files, type)         // 真正读数据
      │     ├─ MeasurementImportRunnable + ProgressMonitorDialog     // 后台读色谱
      │     └─ 每 IChromatogram：Track 1 = 主色谱；Track 2..n = getReferencedChromatograms()
      │           → extractMeasurement(): ExtractedWavelengthSignalExtractor（ChemClipse wsd.model.xwc）
      │             → x = retentionTime, y = abundance(wavelength)；Reference 侧 y 取负（镜像，isMirrorReferenceData）
      │           → ISeriesData 存入 Map<track, Map<wavelength, ISeriesData>>
      └─ 构造/复用 ReferenceModel_v1000 → SampleModel_v1000 → TrackModel_v1000（含 scanVelocity=PreferenceSupplier.getScanVelocity()）
```

- 入口 `IProcessorModel` 由 UI 构造；**「逐 track 对比」= 视觉叠加比对**（sample 曲线 vs 镜像 reference 曲线），人工在 UI 上把每条 track 标记为 skipped / evaluated / matched，结果**回写 TrackModel_v1000 字段**（isSkipped/isEvaluated/isMatched），最终持久化进 `.otc` 文件。**没有数值型自动匹配评分**。
- `DataProcessor.getTrackStatistics(IProcessorModel)`：遍历 `referenceModels.values()` → 每个 reference 下所有 sample 的 track 聚合 → `TrackStatistics`，按 `TrackStatisticComparator` 以 matchProbability **降序**排序。
- `TrackStatistics`（model/TrackStatistics.java，✅）：字段 `sampleGroup/referenceGroup/tracks/evaluated/skipped/matched`；`addTrackModel(ITrackModel)` 计数；`getMatchProbability() = 100.0d / tracks * matched`。
- 图片输出：`getImageName(model, sampleGroup, referenceGroup, sampleTrack, referenceTrack)` 生成 `{imageDirectory}/{sampleGroup}-{sampleTrack}_vs_{referenceGroup}-{referenceTrack}.png`。
- `getWavelength(IChromatogram)`：取第一张 scan 的 `IScanSignalWSD.getWavelength()`（WSD 专用）。
- 文件名分组：`getSampleGroup(fileName)` 用 PreferenceSupplier 的正则 `(.*)(A)(\d+)(\.)(DFM)` 提取组号。

### 3.2 输入/输出契约总结

| 项 | 内容 | Source |
|---|---|---|
| 处理器文件扩展名 | `.otc`（`PROCESSOR_FILE_EXTENSION`）| DataProcessor.java:41 |
| 计算入口 | `getTrackStatistics(IProcessorModel)` → `List<TrackStatistics>` | DataProcessor.java:128 |
| 单 reference 入口 | `getTrackStatistics(IReferenceModel)` → `TrackStatistics` | DataProcessor.java:140 |
| 逐 track 数据装载 | `DataProcessorUI.extractMeasurementsData` / `extractMeasurement` | DataProcessorUI.java:97,146 |
| 统计聚合 | `TrackStatistics.addTrackModel` + 排序 | TrackStatistics.java:53 / TrackStatisticComparator.java |

## 4. ★ 版本化模型 + 序列化（✅ 本机源码全链）

这是「处理方法如何持久化」的最直接范本：**JAXB XML**。

### 4.1 v1000 模型字段（`model/v1000/*.java`，全部 ✅）

- `ProcessorModel_v1000 implements IProcessorModel`，`@XmlRootElement(name="TraceCompare")`：
  `version`（默认 `"1.0.0.0"`）/ `detectorType` / `imageDirectory` / `sampleDirectory` / `referenceDirectory` / `calculatedResult` / `generalNotes` / `Map<String, ReferenceModel_v1000> referenceModels`
- `ReferenceModel_v1000 implements IReferenceModel`：`referenceGroup` / `referencePath` / `Map<String, SampleModel_v1000> sampleModels`
- `SampleModel_v1000 implements ISampleModel`：`sampleGroup` / `samplePath` / `Map<Integer, TrackModel_v1000> trackModels`（key = track 号）
- `TrackModel_v1000 implements ITrackModel`：`sampleTrack` / `referenceTrack` / `scanVelocity` / `startRetentionTime` / `stopRetentionTime` / `startIntensity` / `stopIntensity` / `isSkipped` / `isEvaluated` / `isMatched` / `notes` / `pathSnapshot`

### 4.2 序列化格式（io/*.java，✅）

- `ProcessorModelWriter.write(File, IProcessorModel)`：`JAXBContext.newInstance(四个 v1000 类)` → `Marshaller` + `JAXB_FORMATTED_OUTPUT=true` → `marshal(model, file)`。**输出为格式化 XML**。
- `ProcessorModelReader.read(File)`：同 Context 的 `Unmarshaller.unmarshal(file)` → `IProcessorModel`。
- JAXB 映射约定：**字段上 `@XmlElement(name="Xxx")` 定义 XML 元素名；接口 getter 上 `@XmlTransient`**（避免接口声明被重复映射），setter 不注解。
- 实际 XML 骨架：`<TraceCompare><Version>…</Version><DetectorType>…</DetectorType>…<ReferenceModels>…</ReferenceModels></TraceCompare>`

### 4.3 版本兼容策略（⚠️ 推断 + ✅ 部分）

- `IProcessorModel.getVersion()/setVersion()` 提供版本字段；包名 `model/v1000/` 表示「1000 版 schema」。
- ⚠️ 本机**只有 v1000 一个版本**，未见到多版本分支/升级迁移代码（`ProcessorModelReader` 直接 unmarshal 成 v1000 类，没有按 `<Version>` 分派）。结论：**版本升级策略只是「包名+版本字段」约定，迁移逻辑待出现第二版才能确认**。
- ✅ 佐证该模式在该仓库内重复使用：massshiftdetector 也有 `model/v1000/`（`ProcessorModel_v1000`、`ProcessorSettings_v1000`、`MassShift_v1000`、`ScanMarker_v1000`）与 `io/ProcessorModelReader+Writer`。

### 4.4 另一条持久化路径：模板 settings 的「单字符串行」格式（✅ 本机源码，§6 详述）

与 JAXB 处理器文件并存，方法步骤参数（templates）走 **Jackson `@JsonProperty` + 字符串列表**，见 §5.3/§6。

## 5. ★ templates：ProcessSupplier 模式（✅ 本机源码）

`net.openchrom.xxd.process.supplier.templates` 同时示范了 **两套 ProcessSupplier 用法**，是本任务的核心范本。

### 5.1 机制 A：经典扩展点算法步骤（plugin.xml 注册）

以 `PeakDetector` 为例（`peaks/PeakDetector.java`，✅）：
- `public class PeakDetector extends AbstractPeakDetector implements IPeakDetectorMSD, IPeakDetectorCSD, IPeakDetectorWSD`
- **方法签名**：`detect(IChromatogramSelectionMSD/CSD/WSD, IPeakDetectorSettingsMSD/CSD/WSD, IProgressMonitor) → IProcessingInfo<?>`
- 内部流程：`super.validate(selection, settings, monitor)`（无错误才继续）→ 断言 `settings instanceof PeakDetectorSettings` → 对每条 `DetectorSetting` 行：由 RT 区间算 `startScan/stopScan`（`PeakSupport.getStartScan`），`deltaScan > 2`（至少 3 scan）才建峰 → `peakSupport.extractPeakByScanRange(...)`（支持 PeakType.CB 用基线模型的背景值当峰底）→ 可附加 `IIdentificationTarget` 与 classifier → `PeakSupport.addPeak(chromatogram, peak)`。
- 顺序优化：无 referenceIdentifier 的 DetectorSetting 先执行，有 ID 的后执行（可能引用前面检出的峰）。
- 同一模式：`PeakIdentifierMSD/CSD/WSD`（`AbstractPeakIdentifier.applyIdentifier`，按 RT 区间 + traces 匹配 + `LimitSupport.doIdentify` 门控，把 `LibraryInformation`+`ComparisonResult` 目标加入 `peak.getTargets()`）、`TemplateIntegrator`、`StandardsAssigner/StandardsReferencer/StandardsExtractor/CompensationQuantifier`、`ChromatogramFilterRetentionIndexMapper`、`ChromatogramReport`。

> 接口侧说明：ChemClipse 算法接口源码已在本机（`IPeakDetectorMSD extends IPeakDetector`，`detect(IChromatogramSelectionMSD, IPeakDetectorSettingsMSD, IProgressMonitor)→IProcessingInfo<?>`，见 chromatogram.msd.peak.detector/core/IPeakDetectorMSD.java ✅）。检测/积分/标识接口均在 `org.eclipse.chemclipse.chromatogram.*` 插件（MODULE_04 有接口源码 ✅）。

### 5.2 机制 B：OSGi DS `IProcessTypeSupplier`（面向 ProcessMethod 引擎）

**系统设置步骤**（`system/*.java`，✅ 完整读）——`DetectorExportProcessSupplier / IdentifierExportProcessSupplier / ReviewExportProcessSupplier`：
- `@Component(service = {IProcessTypeSupplier.class})`，类继承 `AbstractSystemProcessSettings`
- `getProcessorSuppliers()` 返回 `Collections.singleton(new ProcessSupplier(this))`
- 内部 `ProcessSupplier extends AbstractSystemProcessSupplier<DetectorExportProcessSettings>`：构造 `super(ID, NAME, DESCRIPTION, SettingsClass, parent)`
- **执行入口**：`executeUserSettings(ISystemProcessSettings settings, ProcessExecutionContext context)`——把 settings 的各字段**写入 `PreferenceSupplier`**（例如 `PreferenceSupplier.setExportNumberTracesDetector(...)`）。即这类步骤不直接处理数据，而是「设置注入」。

**真正改数据的步骤**（`chromatogram/NameSynonymReplacer.java`，✅ 完整读）：
- `@Component(service = {IProcessTypeSupplier.class})`，直接 `implements IProcessTypeSupplier`
- `getCategory()` → `ICategories.CHROMATOGRAM_FILTER`
- 内部 `ProcessSupplier extends AbstractProcessSupplier<NameSynonymReplacerSettings> implements IChromatogramSelectionProcessSupplier<NameSynonymReplacerSettings>`
- 构造 `super(ID, NAME, DESCRIPTION, SettingsClass, parent, DataCategory.CSD, DataCategory.MSD, DataCategory.WSD)` —— **DataCategory 决定该步骤适配哪种检测器数据**
- **执行入口（引擎调用）**：
  ```java
  @Override
  public IChromatogramSelection apply(IChromatogramSelection chromatogramSelection,
                                      NameSynonymReplacerSettings processSettings,
                                      ProcessExecutionContext context) throws InterruptedException {
      // 就地修改 selection 内色谱（scans/peaks/chromatogram 的 target 名替换），返回同一 selection
      return chromatogramSelection;
  }
  ```
- ✅ 已证实（本版，见 §7.3）：引擎 `AbstractProcessEntryContainer.applyProcessEntries` 按方法顺序把同一 `IChromatogramSelection` 传给各步骤的 `apply(...)`，步骤返回的 selection 作为下一步输入；settings 对象由 `ProcessEntryProcessorPreferences.getUserSettings()` 反序列化（`supplier.getSettingsParser().createDefaultInstance()` + Jackson `JSONSerialization.updateFromString`）后传入；`ProcessExecutionContext` 是跨步骤上下文容器（含 `IProgressMonitor` 进度/取消、`IMessageConsumer` 消息、按 Class 键的 `contextMap` 对象槽，见 §7.4）。

### 5.3 Settings 对象 ↔ 引擎传参（✅ 本机源码）

- 每个步骤的 settings 类（`settings/*.java`）都是**纯数据 POJO + Jackson 注解**：`@JsonProperty(value="字段名", defaultValue="…")`、`@JsonPropertyDescription`、`@IntSettingsProperty(minValue/maxValue)`、`@DoubleSettingsProperty`、`@StringSettingsProperty(regExp=…, isMultiLine=true)`。
- **模板型步骤把整张配置表编码成一个字符串字段**（如 `PeakDetectorSettings.detectorSettings`、`PeakIdentifierSettings.identifierSettings`、`PeakIntegrationSettings.integratorSettings`、`StandardsAssignerSettings.assignerSettings`、`CompensationQuantifierSettings.compensationSettings`），提供两个出入口：
  - `setXxx(String)` / `getXxx()`——Jackson 序列化的字符串载体
  - `setXxx(List<…>)` / `getXxxList()`——`@JsonIgnore`，经 `XxxListUtil + XxxValidator` 解析字符串为对象列表
- **行格式**（`util/AbstractTemplateListUtil.java`，✅）：字段分隔 `|`（`SEPARATOR_ENTRY`）、条目分隔 `;`（`SEPARATOR_TOKEN`）或换行、traces 内 `,` 分项、`-` 表范围。
  - Detector 行示例：`start|stop|peakType|traces|optimizeRange|referenceIdentifier|name|positionDirective|classifier|autoAdjustScanRange|autoAdjustDetectorRange`
  - Standards Assigner 行：`start|stop|name|concentration|unit|responseFactor|tracesIdentification`
  - Integrator 行：`start|stop|name|Trapezoid|Max`
  - Compensation 行：`name|istd|concentration|unit|adjustQuantitationFlag`
- **模板文件（.pdt/.pit/.ist/.irt/.prt/.txt）**：纯文本，每行一条设置（`XxxSettings.exportItems(File)`），导出用 `DetectorExport/IdentifierExport/…`（把色谱峰反向转成设置行，参数来自 PreferenceSupplier 的 export 系列）。导入用 `importItems(File)`。
- **序列化器钩子**（`serializer/*.java` + `service/*.java`，✅）：`NameReplacementsSerializer/ReportSettingsSerializer/…` 继承 Jackson `JsonSerializer<T>`，把集合对象 `writeString(obj.save())`；注册为 OSGi `ISerializationService` 组件 —— 方法模型序列化 settings 时经这些服务把字符串列表字段转成单串。

## 6. ★ templates：plugin.xml 扩展点全集（✅ 本机源码完整读）

一个插件挂接 7 类扩展点（含重复的检测器类型变体）：

### 6.1 扩展点 id 列表与步骤全集（id → 实现类 → settings 类）

| 扩展点 | 步骤 | id | 实现类 | settings 类 |
|---|---|---|---|---|
| `org.eclipse.chemclipse.chromatogram.{msd,csd,wsd}.peak.detector.peakDetectorSupplier` | 峰检测 [模板] | `…templates.peaks.detector.{msd,csd,wsd}` | peaks/PeakDetector | settings/PeakDetectorSettings |
| 同上 | 峰转移（Peak Transfer，1:1 复制到引用色谱） | `…templates.peaks.transfer.{msd,csd}` | peaks/PeakTransfer | settings/PeakTransferSettings |
| `org.eclipse.chemclipse.chromatogram.{msd,csd,wsd}.identifier.peakIdentifier` | 峰标识 [模板] | `…templates.peaks.identifier.{msd,csd,wsd}` | peaks/PeakIdentifier{MSD,CSD,WSD} | settings/PeakIdentifierSettings |
| `org.eclipse.chemclipse.chromatogram.xxd.quantifier.peakQuantifierSupplier` | ISTD 指定器 | `…templates.peaks.standards.assigner` | peaks/StandardsAssigner | settings/StandardsAssignerSettings |
| 同上 | ISTD 引用器 | `…templates.peaks.standards.referencer` | peaks/StandardsReferencer | settings/StandardsReferencerSettings |
| 同上 | 补偿定量器 | `…templates.peaks.compensation.quantifier` | peaks/CompensationQuantifier | settings/CompensationQuantifierSettings |
| 同上 | ISTD 抽取器 | `…templates.peaks.standards.extractor` | peaks/StandardsExtractor | settings/StandardsExtractorSettings |
| `org.eclipse.chemclipse.chromatogram.xxd.integrator.peakIntegratorSupplier` | 峰积分 [模板] | `net.openchrom.xxd.process.supplier.templates.peakIntegrator` | peaks/TemplateIntegrator | settings/PeakIntegrationSettings |
| `org.eclipse.chemclipse.chromatogram.filter.chromatogramFilterSupplier` | 保留指数映射滤波器 | `…templates.chromatogram.retentionIndexMapper` | chromatogram/ChromatogramFilterRetentionIndexMapper | settings/FilterSettingsRetentionIndexMapper |
| `org.eclipse.chemclipse.chromatogram.xxd.report.chromatogramReportSupplier` | 峰报告（.tsv） | `org.eclipse.chemclipse.chromatogram.xxd.report.supplier.openchrom.templateChromatogramReport` | chromatogram/ChromatogramReport | settings/ChromatogramReportSettings |
| `org.eclipse.chemclipse.{msd,csd}.converter.chromatogramSupplier` | 模板文件导出（.pdt/.pit/.ist/.irt/.txt/.prt） | `…templates.export.{detectorTemplate,identifierTemplate,standardsAssignerTemplate,standardsReferencerTemplate,reportTemplate,reviewTemplate}{MSD,CSD}` | io/{DetectorExport,IdentifierExport,StandardsExport,ReferencerExport,ReportExport,ReviewExport} + io/MagicNumberMatcher | （无，用 PreferenceSupplier 导出参数） |
| `org.eclipse.chemclipse.{msd,csd,wsd}.converter.chromatogramSupplier` | 命名 trace（.ntr）/ 时间区间（.tra）导出 | `…templates.export.chromatogram.{namedtraces,timeranges…}` | chromatogram/ChromatogramExportNamedTraces、ChromatogramExportTimeRanges | — |

> 注意细节：`PeakIdentifierWSD` 在 `csd.identifier` 与 `wsd.identifier` 两个扩展点各注册了一次（id 相同 `…peaks.identifier.wsd`）——plugin.xml 原文如此，可能是历史遗留。模板导出均 `isExportable="true" isImportable="false"`（只导出）。

### 6.2 步骤↔settings↔model 对应关系（✅ 本机源码）

| 步骤 settings | 行模型（model/*.java） | 校验器（util/*.java） | 说明 |
|---|---|---|---|
| PeakDetectorSettings | DetectorSetting / DetectorSettings（`.pdt`） | PeakDetectorListUtil + PeakDetectorValidator | 峰类型枚举 PeakType(VV/BB/MM/CB…) |
| PeakIdentifierSettings | IdentifierSetting / IdentifierSettings（`.pit`） | PeakIdentifierListUtil + PeakIdentifierValidator | CAS、comment、contributor、referenceId、traces |
| PeakIntegrationSettings | IntegratorSetting / IntegratorSettings | PeakIntegratorListUtil + PeakIntegratorValidator | 积分类型 Trapezoid\|Max |
| StandardsAssignerSettings | AssignerStandard / AssignerStandards（`.ist`） | StandardsAssignerListUtil + StandardsAssignerValidator | 浓度/单位/响应因子 |
| StandardsReferencerSettings | AssignerReference / AssignerReferences（`.irt`） | StandardsReferencerListUtil + StandardsReferencerValidator | 引用目标 |
| CompensationQuantifierSettings | CompensationSetting / CompensationSettings | CompensationQuantListUtil + CompensationQuantValidator | 补偿浓度与 ISTD |
| ReviewSettings | ReviewSetting / ReviewSettings（`.prt`） | ReviewListUtil + ReviewValidator | 人工复核范围 |
| ReportSettings / ChromatogramReportSettings | ReportSetting / ReportSettings / ReportColumns（`.txt`/`.tsv`） | ReportListUtil + ReportValidator + ReportSettingsValidator + ReportColumnsValidator | 报告列与策略 |
| NameSynonymReplacerSettings | NameReplacement / NameReplacements | NameReplacementValidator | 名称同义词替换（DS 服务机制 B） |

> 顶层 settings 类继承 ChemClipse 抽象基类：`AbstractPeakDetectorSettingsMSD`（chromatogram.msd.peak.detector/settings/）、`AbstractIdentifierSettings`（org.eclipse.chemclipse.model/identifier/）、`AbstractPeakIntegrationSettings`（chromatogram.xxd.integrator/core/settings/peaks/）、`AbstractPeakQuantifierSettings`（chromatogram.xxd.quantitation/settings/）——均已在本机 ✅。

## 7. ★ 引擎层：`org.eclipse.chemclipse.processing`（✅ 本机源码全链）

引擎全部源码在本机，本节为「方法模型 → 执行调度 → 参数绑定 → 旧式算法包装 → 方法文件」的完整证据链。所有类路径均以 `.fetch/chemclipse-src/plugins/` 为根。

### 7.1 类全景（✅ 全部读源码）

| 包 | 类 | 角色 |
|---|---|---|
| `processing.methods` | `IProcessMethod` / `ProcessMethod` / `ListProcessEntryContainer` / `AbstractProcessEntryContainer` / `IProcessEntryContainer` | 方法模型（§7.2）+ **核心执行循环 `applyProcessEntries`**（§7.5） |
| `processing.methods` | `IProcessEntry` / `ProcessEntry` | 方法步骤（§7.3） |
| `processing.methods` | `ProcessEntryProcessorPreferences` / `SubProcessExecutionConsumer` | 步骤 settings 桥（§7.3）/ 嵌套方法消费者（§7.6） |
| `processing.supplier` | `IProcessTypeSupplier` / `IProcessSupplier` / `IProcessSupplierContext` / `AbstractProcessSupplier` | supplier 三接口 + 抽象基类（§7.1.1）+ 静态执行入口 `applyProcessor`（§7.5） |
| `processing.supplier` | `ProcessExecutionContext` | 跨步骤上下文容器（§7.4） |
| `processing.supplier` | `IProcessExecutor` / `IProcessExecutionConsumer` / `ExecutionResultTransformer` | 执行三契约（§7.1.2） |
| `processing.supplier` | `IProcessorPreferences` / `NodeProcessorPreferences` / `ProcessSupplierFactory`(注解) | settings 偏好接口 + 工作区偏好实现 + 工厂方法注解 |
| `processing.system` | `SystemExecutor` / `AbstractSystemProcessSupplier` / `AbstractSystemProcessSettings` / `ISystemProcessSettings` / `DynamicProcessSettings` / `DynamicProcessSupplier` / `ProcessSettingsSupport` | 系统设置步骤（§7.6） |
| `processing.internal` | `OSGiProcessSupplierContext` / `ProcedureProcessTypeSupplier` / `OSGiFilterFactory` | DS 全局上下文（§7.5）/ 流程型步骤 / 过滤器工厂 |
| `processing` | `DataCategory` / `DataCategoryGroup` / `Processor` / `ProcessorCategory` / `ProcessorFactory` | 数据类型枚举、新式 Processor 框架 |
| `processing.filter` / `processing.detector` | `Filter` / `FilterContext` / `Filtered` / `FilterList` / `Detector` / `DetectorCategory` | 新式 Filter/Detector 基接口（UI 批处理用，非 ProcessMethod 主链路） |
| `org.eclipse.chemclipse.model` 的 `model/supplier` | `IChromatogramSelectionProcessSupplier` / `ChromatogramSelectionProcessSupplier` / `IMeasurementProcessSupplier` / `IScanProcessSupplier` / `IMeasurementFilterProcessTypeSupplier` / `PeakFilterProcessTypeSupplier` | **数据承载步骤契约 + 旧式算法包装基类**（§7.6/§7.7） |
| `org.eclipse.chemclipse.converter` 的 `methods` | `MethodProcessTypeSupplier` / `UserMethodProcessSupplier` / `MetaProcessorProcessSupplier` / `MethodConverter` / `MethodConverterSupport` | 用户/系统/捆绑方法 → supplier（§7.6） |
| `xxd.converter.supplier.ocx` 的 `internal/methods` + `methods` | `MethodExportConverter` / `MethodImportConverter` / `MethodReaderWriter_{1003,1004,1401,1402}` / `MethodReader_{1000,1001}` / `MethodWriter_{1000,1001}` / `GenericStreamMethodFormat` / `ObjectStreamMethodFormat` / `AbstractMethodReader` / `AbstractMethodWriter` | `.ocm` 方法文件版本化读写（§7.8） |
| `chromatogram.method.model` | `core/GCMethod` | **接口 `float getTemperature(int retentionTime)`——GC 柱温程序，与处理方法无关**（§7.8 修正） |

> ✅ **重要修正（相对旧文档 §7.2）**：方法文件模型**不是 `GCMethod`**。`GCMethod` 只是「按保留时间查 GC 炉温」的温程序接口（唯一方法 `getTemperature`）；处理方法的模型是 `ProcessMethod`/`ProcessEntry`（`processing.methods` 包），`.ocm` 文件存的也是 `ProcessMethod`。

#### 7.1.1 supplier 三接口（✅ IProcessSupplier.java / IProcessTypeSupplier.java / IProcessSupplierContext.java）

- `IProcessSupplier<SettingType>`：**纯描述符**（id/name/category/description/literature/settingsClass/DataCategory 集合/settingsParser/typeSupplier），自带 `matchesId(id)`（向后兼容旧 id）、`getContext()`（自身若是 context 则返回自身，否则返回 typeSupplier）、`SupplierType`（DEFAULT/INTERACTIVE/STRUCTURAL）、`validate()/validate(settings)`→`IStatus`。**本身没有执行方法**。
- `IProcessTypeSupplier extends IProcessSupplierContext`：`getCategory()` + `getProcessorSuppliers()`；default `getSupplier(id)` 线性扫描 `matchesId`；default `visitSupplier(consumer)`。被 OSGi DS `@Component(service=IProcessTypeSupplier.class)` 大量实现（§7.7）。
- `IProcessSupplierContext`：`<T> IProcessSupplier<T> getSupplier(String id)` + `void visitSupplier(Consumer)` + default `getSupplier(Predicate)`（按 id 排序 TreeSet）+ static `forDataTypes(...)` / `createDataCategoryPredicate(...)`（按 DataCategory 过滤）。

#### 7.1.2 执行三契约（✅ IProcessExecutor / IProcessExecutionConsumer / ExecutionResultTransformer）

- `IProcessExecutor`：`<X> void execute(IProcessorPreferences<X> preferences, ProcessExecutionContext context) throws Exception`。实现者：`AbstractSystemProcessSupplier`（系统步骤）、`UserMethodProcessSupplier`/`MetaProcessorProcessSupplier`（方法作为步骤，§7.6）、`IProcessExecutionConsumer`。
- `IProcessExecutionConsumer<T> extends IProcessExecutor`：`T getResult()`（默认 null）/ `canExecute(preferences)`（默认 true，不可执行则跳过）/ `withResult(initialResult)`。**数据消费者**——持有中间结果并把 `apply` 绑定到具体数据类型（§7.5 的 `createConsumer`）。
- `ExecutionResultTransformer<SettingType> extends IProcessSupplier<SettingType>`：`transform(consumer, preferences, context)` 可改写下游消费者（Procedures 用）。

### 7.2 方法模型：ProcessMethod / ListProcessEntryContainer（✅）

**方法 = 有向有序步骤列表，`IProcessEntry` 列表即顺序**。

- `ListProcessEntryContainer`（继承 `AbstractProcessEntryContainer`）：`List<IProcessEntry> entries`；**增删排序全部是 List 操作**：
  - `createEntry()` / `createEntry(IProcessSupplier, settings)`——新步骤（supplier 非空时自动填 processorId/name/description，settings 经 `entry.getPreferences(supplier).getSerialization().toString(settings)` 序列化成 JSON 字符串存入 entry）；`getEntries().add(entry)` 追加到末尾。
  - `addProcessEntry(IProcessEntry)`——**拷贝 + 改父**（`new ProcessEntry(other, this)`）；`removeProcessEntry` / `removeAllProcessEntries`。
  - 排序：无专用 API，直接操作 `getEntries()`（List）顺序 = 执行顺序。
  - `readOnly`（final 方法锁定）、`name`（setter 自动剥 `.ocm` 后缀）、`description`、`profiles` + `activeProfile`（`setActiveProfile` 级联所有 entry；`deleteProfile` 级联）。
- `ProcessMethod extends ListProcessEntryContainer implements IProcessMethod`：`uuid`（构造时 `UUID.randomUUID()`；拷贝构造**生成新 UUID**）、`operator`、`category`（`ResourceBundle` 本地化）、`profileColumnsMap`（profile→分离柱类型）、`metadata`（LinkedHashMap）、`sourceFile`（transient）、`catgories`（`Set<DataCategory>`，构造时固定）。`isFinal()` == `isReadOnly()`。构造：`new ProcessMethod(Set<DataCategory>)` 或拷贝 `new ProcessMethod(other)`。便捷常量 `ProcessMethod.CHROMATOGRAPHY`（CSD/MSD/VSD/WSD/FSD）、`ProcessMethod.NMR`。
- `IProcessMethod` 另有 `contentEquals(other, includeMetadata)`（比较 name/category/description/operator/isFinal/entries 内容）。

### 7.3 步骤模型：ProcessEntry（✅）

- `ProcessEntry extends ListProcessEntryContainer implements IProcessEntry`，字段：`parent`（所属方法）、`EnumSet<DataCategory> categories`、`Map<String,String> jsonSettingsMap`（**profile → JSON 设置字符串**）、`processorId`、`activeProfile=DEFAULT_PROFILE`（`"Default Profile"`）、`skipValidation`。
- `setSettings(String json)` → `jsonSettingsMap.put(activeProfile, json)`：**settings 按 profile 存单条 JSON 字符串**（不是对象、不是 XML）。`getSettings()` 取当前 profile；`getSettingsMap()` 全量只读；`copySettings(profile)` 把指定 profile 的 settings 拷到当前 profile。
- `getPreferences(context/supplier)` → `new ProcessEntryProcessorPreferences<>(supplier, this)`：运行时把 entry 的 JSON 字符串桥接成 `IProcessorPreferences`（`getUserSettingsAsString()`→`entry.getSettings()`；`isUseSystemDefaults()`→settings 为空或 `"{}"`）。
- **entry 自身也是容器**（`extends ListProcessEntryContainer`）→ 可内嵌子步骤，形成**组合方法**（§7.6 `SubProcessExecutionConsumer`）。
- 数据类别：`addDataCategory/removeDataCategory/getDataCategories`（继承自 supplier 或手动指定）。

### 7.4 ProcessExecutionContext（✅ 全字段）

`ProcessExecutionContext implements IProcessSupplierContext, IMessageConsumer`：

| 字段/能力 | 说明 | Source |
|---|---|---|
| `IProgressMonitor monitor` | **进度 + 取消**：`getProgressMonitor()`；取消不是 context 里的 boolean 标记，而是 `monitor.isCanceled()` + `apply` 抛 `InterruptedException` → 引擎转 `OperationCanceledException` | ProcessExecutionContext.java |
| `IProcessSupplierContext context` | supplier 查找委托（`getSupplier(id)` 先查自身 context，再沿 `parent` 链上溯） | 同上 |
| `IMessageConsumer consumer` | `addMessage(description,message,details,solution,type)` 转发；含 `addErrorMessage/addWarnMessage/addMessages` | 同上 |
| `ProcessExecutionContext parent` | `split()` / `split(childContext)` 创建子上下文（嵌套方法用），`getParent()` 可上溯 | 同上 |
| `Map<Class<?>,Object> contextMap`（IdentityHashMap） | **跨步骤对象槽**：`setContextObject(Class,Object)` / `getContextObject(Class)`——引擎在此存 `IProcessEntry`、`IProcessSupplier`、`IProcessorPreferences`、`IProcessExecutionConsumer`；取时 `type.isInstance` 校验并沿 parent 上溯 | 同上 |

> 步骤间数据传递方式：**数据本体（`IChromatogramSelection`）走 consumer 的 `AtomicReference` 结果槽**（§7.5），**元数据（当前 entry/supplier/preferences/consumer）走 contextMap**，**消息/进度/取消走 consumer/monitor**。无回滚/事务。

### 7.5 ★ 引擎调度全链（✅ 全链读源码）

**核心循环 = `AbstractProcessEntryContainer.applyProcessEntries(container, context, consumer)`**（methods/AbstractProcessEntryContainer.java:64）：

```
for(IProcessEntry entry : container) {                    // 顺序 = 方法顺序
    if(index < resumeIndex) continue;                     // 支持 resume：跳过前 resumeIndex 个
    IProcessSupplier<X> processor = context.getSupplier(entry.getProcessorId());  // ① 按 id 查 supplier
    if(processor == null) { addWarnMessage(...); continue; }   // 找不到 → 警告并跳过
    IProcessorPreferences<X> prefs = entry.getPreferences(processor);              // ② 参数绑定（JSON 字符串 ↔ 设置对象）
    context.setContextObject(IProcessEntry/IProcessSupplier/IProcessorPreferences/IProcessExecutionConsumer ...);
    ProcessExecutionContext entryContext = context.split(processor.getContext());  // ③ 子上下文（继承 supplier 自身 context）
    if(entry.getNumberOfEntries() > 0)                    // ④ 组合方法：递归执行子步骤
        AbstractProcessSupplier.applyProcessor(prefs, new SubProcessExecutionConsumer<>(consumer, sub → applyProcessEntries(entry, ...)), entryContext);
    else                                                  // ⑤ 简单方法：单步分发
        AbstractProcessSupplier.applyProcessor(prefs, consumer, entryContext);
    // finally 清空 contextMap 对象槽
}
return consumer.getResult();                              // ⑥ 聚合结果
```

**单步分发 = `AbstractProcessSupplier.applyProcessor(preferences, consumer, context)`**（supplier/AbstractProcessSupplier.java:165，静态）：

```
supplier = preferences.getSupplier();
canDirectExecute = consumer.canExecute(preferences);
supplierExecutionConsumer = (supplier instanceof IProcessExecutor) ? supplier : null;
transformer = (supplier instanceof ExecutionResultTransformer) ? supplier : null;
mustSplit = (调用数 > 1);
context.setContextObject(IProcessSupplier.class, supplier);
context.setContextObject(IProcessorPreferences.class, preferences);
if(transformer != null) consumer = transformer.transform(consumer, preferences, split-ctx);  // 可改写消费者
context.setContextObject(IProcessExecutionConsumer.class, consumer);
if(canDirectExecute)       consumer.execute(preferences, split-ctx);   // 数据消费者：最终调 apply()
if(supplierExecutionConsumer != null) supplierExecutionConsumer.execute(preferences, split-ctx);  // IProcessExecutor 步骤
// InterruptedException → Thread.interrupt + OperationCanceledException
// 其他异常 → context.addErrorMessage(name, "execution throws an error, processor is skipped") → **跳过该步继续**
return consumer.getResult();
```

**数据消费者把 `apply` 绑定到色谱数据**（model/supplier/IChromatogramSelectionProcessSupplier.java:39 `createConsumer`）：

```
AtomicReference<IChromatogramSelection> result = new AtomicReference<>(chromatogramSelection);
execute(prefs, ctx): supplier = prefs.getSupplier();
    if(supplier instanceof IChromatogramSelectionProcessSupplier<X> cs)
        updateResult(cs.apply(getResult(), prefs.getSettings(), ctx));     // 就地改 selection，返回下一 selection
    else if(supplier instanceof IMeasurementProcessSupplier<X> ms)
        ms.applyProcessor(Collections.singleton(getResult().getChromatogram()), prefs.getSettings(), ctx);
getResult() → result.get()   // 下一步的输入
```

**global supplier 查找表 = `OSGiProcessSupplierContext`**（internal/OSGiProcessSupplierContext.java，DS `@Component(service=IProcessSupplierContext.class)`）：
- `ConcurrentMap<String, IProcessSupplier<?>> supplierMap`（id→supplier；`putIfAbsent`，**重复 id 抛 `IllegalArgumentException`**）；
- `Set<IProcessTypeSupplier> typeSupplierSet`（`@Reference(cardinality=MULTIPLE, policy=DYNAMIC)` 动态增删 `IProcessTypeSupplier` 服务，每注册一个就把其全部 suppliers 入表）；
- `getSupplier(id)`：先查表，未命中再逐个问 typeSupplier（兼容 `matchesId` 别名）。

**UI 侧实际调用**（ux.extension.xxd.ui/swt/editors/ExtendedChromatogramUI.java:749-752）：`AbstractProcessSupplier.applyProcessor(settings, IChromatogramSelectionProcessSupplier.createConsumer(chromatogramSelection), new ProcessExecutionContext(monitor, processingInfo, processSupplierContext))`——这是单个 supplier 的应用入口；整方法执行走 `UserMethodProcessSupplier.execute`（§7.6）。

### 7.6 方法作为步骤：USER_METHODS / 系统 / Meta（✅）

- `MethodProcessTypeSupplier`（converter/methods/MethodProcessTypeSupplier.java，DS `@Component(service=IProcessTypeSupplier.class)`，category=`ICategories.USER_METHODS`）：聚合三类方法为 `IProcessSupplier`：
  1. **用户方法**（`parseUserMethods`）：从 `MethodConverter.getUserMethodDirectory()`（`PreferenceSupplier.P_METHOD_EXPLORER_PATH_ROOT_FOLDER`）读 `*.ocm` → `Adapters.adapt(file, IProcessMethod.class)` → `UserMethodProcessSupplier`；
  2. **捆绑方法**（`parseBundleMethods`）：用 `BundleTracker` 扫描各 bundle 的 `/OSGI-INF/processors/*.ocm` → `MetaProcessorProcessSupplier`（id=`ProcessMethod.{uuid}:bundle:{symbolicName}:{path}`）；
  3. **系统方法**（`parseSystemMethods`）：`Settings.getSystemMethodDirectory()` 目录下 `*.ocm` → `MetaProcessorProcessSupplier`（id=...`system:{filename}`）。
  - `getSupplier(id)` 额外支持按 `baseId:` 前缀匹配（避免精确匹配失败）。
- `UserMethodProcessSupplier extends AbstractProcessSupplier<Void> implements IProcessEntryContainer, IProcessExecutor`：`execute()` 从 context 取 `IProcessExecutionConsumer<?>`，转调 `AbstractProcessEntryContainer.applyProcessEntries(processMethod, context, consumer)`——**整方法作为一步执行**。
- `MetaProcessorProcessSupplier extends AbstractProcessSupplier<MetaProcessorSettings> implements IProcessExecutor`：`execute()` 用 `processorSettings.getProcessorPreferences(entry, entry.getPreferences(supplier))` 作为偏好函数递归 `applyProcessEntries`（MetaProcessorSettings 可整体覆盖/改写子步 settings）。
- **系统设置步骤**：`AbstractSystemProcessSupplier<S extends ISystemProcessSettings> extends AbstractProcessSupplier<S> implements IProcessExecutor, SystemExecutor`；`SystemExecutor` 接口 = `void executeUserSettings(ISystemProcessSettings settings, ProcessExecutionContext context)`；`execute()` 仅当 `!preferences.isUseSystemDefaults()` 时取 `getUserSettings()` 调 `executeUserSettings`（templates 的 DetectorExportProcessSupplier 等即此模式，见 §5.2）。`AbstractSystemProcessSettings implements ISystemProcessSettings, IProcessTypeSupplier`（category=`SYSTEM`）。
- `NodeProcessorPreferences`（supplier 包）：把 `IProcessorPreferences` 落到 Eclipse `Preferences` 节点（键 `useSystemDefaults` / `userSettings` / `askForSettings`）；`ProcessSettingsSupport.getWorkspacePreferences(supplier)` / `getPreferences(context)` 提供系统默认设置与「动态设置」（无对应 supplier 的偏好节点包成 `DynamicProcessSupplier`）。`ProcessSupplierFactory` 是方法注解，`E4ProcessSupplierContext`（processing.ui）用 DI 工厂实例化 supplier。

### 7.7 ★ 旧式算法 → IProcessSupplier 包装模式（✅ 本机源码）

**关键事实**：包装不在算法 supplier 插件（firstderivative/trapezoid/savitzkygolay 等**都没有** `*ProcessTypeSupplier`），而在**各「接口插件」里集中完成**——`org.eclipse.chemclipse.chromatogram.{msd,csd,wsd,vsd}.peak.detector`、`chromatogram.xxd.{integrator,baseline.detector,calculator,identifier,quantitation}`、`chromatogram.filter`、`converter`、`msd.identifier` 等。这些插件声明 `@Component(service=IProcessTypeSupplier.class)`，用**扩展点注册表（Support）动态枚举**全部已注册算法并逐一包装。

**包装基类链（实测，修正旧文档假设的 AbstractProcessTypeSupplier→AbstractProcessorSupplier→AbstractPeakDetectorSupplier 链条）：**

```
IProcessSupplier<SettingType>                        (processing/supplier，描述符)
└─ AbstractProcessSupplier<SettingsClass>             (processing/supplier，静态 applyProcessor 执行入口)
   └─ ChromatogramSelectionProcessSupplier<SettingsClass>   (model/supplier，implements IChromatogramSelectionProcessSupplier)
      └─ 各 wrapper 的内部类（如 PeakDetectorProcessorSupplier）
   └─ AbstractProcessSupplier（不同类，同名的 model/supplier 内类，用于 IMeasurementProcessSupplier）
```

- `IChromatogramSelectionProcessSupplier<SettingType> extends IProcessSupplier<SettingType>`（model/supplier）：**数据步骤契约** `IChromatogramSelection apply(IChromatogramSelection, SettingType, ProcessExecutionContext) throws InterruptedException` + static `createConsumer`。
- `ChromatogramSelectionProcessSupplier`（model/supplier）：把契约降级为 `apply(selection, settings, IMessageConsumer, IProgressMonitor)`（public apply 用 `context.getProgressMonitor()` 转发）——**旧式算法签名 `apply(selection, settings, consumer, monitor)` 的统一落点**。

**包装示例（✅ msd.peak.detector/core/PeakDetectorMSDProcessTypeSupplier.java）：**

```java
@Component(service = IProcessTypeSupplier.class)
public class PeakDetectorMSDProcessTypeSupplier implements IProcessTypeSupplier {
    getCategory() → ICategories.PEAK_DETECTOR
    getProcessorSuppliers():
        IPeakDetectorSupport support = PeakDetectorMSD.getPeakDetectorSupport();   // 扩展点注册表
        for(id : support.getAvailablePeakDetectorIds())
            list.add(new PeakDetectorProcessorSupplier(support.getPeakDetectorSupplier(id), this));
    private static final class PeakDetectorProcessorSupplier
            extends ChromatogramSelectionProcessSupplier<IPeakDetectorSettingsMSD> {
        ctor: super("PeakDetectorMSD."+supplier.getId(), supplier.getPeakDetectorName(),
                    supplier.getDescription(), supplier.getSettingsClass(), parent, DataType.MSD);
        apply(sel, settings, consumer, monitor):
            if(sel instanceof IChromatogramSelectionMSD && settings instanceof IPeakDetectorSettingsMSD)
                consumer.addMessages(PeakDetectorMSD.detect(sel, settings, supplier.getId(), monitor));
            return chromatogramSelection;           // 就地处理，返回原 selection
        matchesId(id) → super.matchesId(id) || supplier.getId().equals(id)   // 兼容原扩展点 id
    }
}
```

**同类 wrapper 全集（✅ 均已读）：**

| 包装类（`implements IProcessTypeSupplier`） | 所在插件 | category | 内部类 extends | 执行时转调 |
|---|---|---|---|---|
| `PeakDetectorMSDProcessTypeSupplier` | chromatogram.msd.peak.detector | `PEAK_DETECTOR` | ChromatogramSelectionProcessSupplier | `PeakDetectorMSD.detect(selMSD, settings, supplierId, monitor)` |
| `PeakDetectorCSDProcessTypeSupplier` / `PeakDetectorVSDProcessTypeSupplier` / `PeakDetectorWSDProcessTypeSupplier` | csd/vsd/wsd.peak.detector | 同上 | 同上 | 各 `*Detector.detect(...)` |
| `ChromatogramIntegratorProcessTypeSupplier` / `PeakIntegratorProcessTypeSupplier` / `CombinedIntegratorProcessTypeSupplier` | chromatogram.xxd.integrator（core/chromatogram、core/peaks、core/combined） | `PEAK_INTEGRATOR` / `CHROMATOGRAM_INTEGRATOR` / `COMBINED_...` | 同上 | `PeakIntegrator.integrate(sel, settings, supplierId, monitor)` / `ChromatogramIntegrator.integrate` |
| `ChromatogramFilterProcessSupplier` | chromatogram.filter（core/chromatogram） | `CHROMATOGRAM_FILTER` | 同上 | `ChromatogramFilter.applyFilter(sel, settings, supplierId, monitor)`——**包住 savitzkygolay 等全部 xxd.filter 扩展点算法** |
| `BaselineDetectorProcessTypeSupplier` | chromatogram.xxd.baseline.detector | `BASELINE_DETECTOR` | 同上 | `BaselineDetector.calculateBaseline(...)` |
| `ChromatogramCalculatorProcessTypeSupplier` | chromatogram.xxd.calculator | `CHROMATOGRAM_CALCULATOR` | 同上 | `ChromatogramCalculator.calculate(...)` |
| `PeakIdentifierMSDProcessTypeSupplier` 等 | chromatogram.msd/csd/wsd.identifier、xxd.identifier | `PEAK_IDENTIFIER` / `CHROMATOGRAM_IDENTIFIER` | 同上 | `PeakIdentifierMSD.identify(...)` / `ChromatogramIdentifier.identify(...)` |
| `PeakQuantifierProcessTypeSupplier` | chromatogram.xxd.quantitation | `PEAK_QUANTIFIER` | 同上 | `PeakQuantifier.quantify(...)` |
| `ChromatogramConverter{MSD,CSD,WSD,FSD}ProcessTypeSupplier` / `PeakConverterMSDProcessTypeSupplier` | 各 converter 插件 | `CHROMATOGRAM_EXPORT`/`PEAK_EXPORT` | 同上 | converter 读/写 |
| `MassSpectrumIdentifierProcessTypeSupplier` / `StandaloneMassSpectrumIdentifierProcessTypeSupplier` | msd.identifier | `MASS_SPECTRUM_IDENTIFIER` | AbstractProcessSupplier + IScanProcessSupplier | 谱标识 |
| `ScanMassSpectrumFilterProcessTypeSupplier` / `PeakMassSpectrumFilterProcessTypeSupplier` / `AbstractChromatogramSelectionMassSpectrumFilterProcessTypeSupplier` | chromatogram.msd.filter（core/massspectrum、core/peak） | `SCAN_MASS_SPECTRUM_FILTER` / `PEAK_MASS_SPECTRUM_FILTER` | 同上 | `ScanMassSpectrumFilter.applyFilter(...)` 等 |
| `MethodProcessTypeSupplier` | converter/methods | `USER_METHODS` | AbstractProcessSupplier + IProcessExecutor | 递归 applyProcessEntries（§7.6） |
| `IMeasurementFilterProcessTypeSupplier` | model/supplier | `FILTER` | AbstractProcessSupplier implements IMeasurementProcessSupplier | `IMeasurementFilter.filterIMeasurements(...)`（新式 Processor 框架，非扩展点） |
| `EditorProcessTypeSupplier` | ux.extension.xxd.ui/editors | `USER_INTERFACE` | — | 编辑器动作 |

**结论（P8 ✅）**：旧式扩展点算法（detect/integrate/identify/applyFilter...）由**接口插件**枚举扩展点注册表并包成 `IProcessSupplier`；包装层统一签名 `apply(selection, settings, messageConsumer, monitor)`（`ChromatogramSelectionProcessSupplier`），返回原 `IChromatogramSelection`，消息经 `IMessageConsumer.addMessages(IProcessingInfo)` 上报。

### 7.8 方法文件 `.ocm`（✅ 全链读源码）

**扩展点** `org.eclipse.chemclipse.converter.processMethodSupplier`；默认转换器 `org.eclipse.chemclipse.xxd.converter.supplier.chemclipse.processMethodSupplier`（`MethodConverter.DEFAULT_METHOD_CONVERTER_ID`）。入口 `MethodConverter.convert(file, monitor)` / `load(stream, nameHint, monitor)` / `store(...)`。

**版本（settings/Format.java）：** `0.0.0.1`(0001)→`0.0.0.2`(0002)→`0.0.0.3`(0003)→`1.4.0.0`(1400)→`1.4.0.1`(1401)→`1.4.0.2`(1402=LATEST)。

**两代格式（✅ MethodReader_1000 / MethodReaderWriter_1402）：**
- **旧代（0001-0002，MethodReader/Writer_1000/1001）**：**ZIP 容器**，内含两 entry——`Format.FILE_VERSION`（值 `"VERSION"`，版本字符串校验用）与 `Format.FILE_PROCESS_METHOD`（值 `"PROCESS_METHOD"`，`DataInputStream` 自定义二进制：operator、description、条目数、逐条 processorId/name/description/settings字符串/dataTypes/2 个废弃字符串）。仅支持文件（File），不支持流。
- **新代（0003=1003、1400=1004、1401、1402）**：**魔数头 `"MTH."+version`（如 `MTH.1.4.0.2`）+ GZIP 压缩 + Java `ObjectOutputStream` 自定义原始类型流**（非 XML、非标准 Java 序列化）。`GenericStreamMethodFormat.convert` 先校验魔数再解压。字段顺序（1402）：dataCategories（枚举名，legacy `ISD`→`VSD`）→ uuid → name → description → category → operator → supportResume（boolean）→ profiles（count+名字+activeProfile）→ profileColumnsMap（count+profile→柱类型名）→ metadata（count+键值）→ **entries（逐条：processorId/name/description → settingsMap(count + profile→JSON字符串 + activeProfile) → dataCategories → 子 entries 递归 → isReadOnly）** → isFinal。
- **settings 内嵌格式 = 每条 entry 的 profile→JSON 字符串**（§7.3），**用 Jackson ObjectMapper 序列化/反序列化**（`JSONSerialization`，支持 `ISettingsMigrator` 版本迁移；`SettingsClassParser` 反射 `@IntSettingsProperty` 等注解生成设置模型）。

**读取分派（✅ MethodImportConverter）：** 按 `1402→1401→1004→1003→1001→1000` 依次尝试，魔数/ZIP entry 校验失败则试下一个；首个成功即返回。写固定 `METHOD_VERSION_LATEST=1402`。

**`.ocm` 到底存了什么（结论 ✅）**：`ProcessMethod`（uuid、元数据、profiles、有序 ProcessEntry 列表 + 每步 settings JSON + DataCategory + 嵌套子步骤）。这与 tracecompare 的 `.otc`（JAXB XML 处理器专属模型）是**两套独立体系**：`.ocm` 是通用的 ProcessMethod 引擎方法文件，`.otc` 是 tracecompare 专属配置。

**GCMethod 修正**：`chromatogram.method.model/core/GCMethod.java` 仅 `float getTemperature(int retentionTime)` 一个方法（GC 炉温程序模型），与处理方法无关，从「方法文件模型」候选剔除。

## 8. ★ 设置序列化框架（Settings Serialization Framework，✅ 本机源码全链）

贯穿「算法 settings 类 → Jackson 注解 → JSON 字符串 → `.ocm` 方法文件/工作区偏好 → 引擎反序列化 → 设置对话框 UI」的底层机制。核心三件套：`SettingsClassParser`（注解→元数据）、`JSONSerialization`（对象↔JSON）、`ISerializationService`（自定义序列化器注册）。所有类路径均以 `.fetch/chemclipse-src/plugins/` 为根，templates 系列以 `openchrom/plugins/` 为根。

### 8.1 全景数据流（✅）

```
settings 类（纯数据 POJO + Jackson/自定义注解）
  ├─ SettingsClassParser.getInputValues()  ── 注解→ List<InputValue>（名字/类型/默认值/校验器）→ UI 生成控件 + 校验（§8.2）
  ├─ JSONSerialization.toString(obj)        ── 对象 → 紧凑单行 JSON（createEntry / 模板列表编辑器保存路径，裸 mapper）
  ├─ JSONSerialization.toString(Map)        ── InputValue 值表 → JSON（设置对话框保存路径，createMapper 含自定义序列化器）
  └─ JSONSerialization.updateFromString(obj, json) ── JSON → 原地更新对象（createMapper；含 ISettingsMigrator 迁移回调）
      ↕ entry.getSettings()/setSettings(json) —— ProcessEntry.jsonSettingsMap<profile, JSON 字符串>（§7.3）
```

### 8.2 SettingsClassParser：注解 → InputValue 元数据（✅）

Source: `support/settings/parser/SettingsClassParser.java` / `SettingsParser.java` / `InputValue.java`

- 用 Jackson 反射内省：`new ObjectMapper()`（disable `FAIL_ON_UNKNOWN_PROPERTIES`）→ `getSerializationConfig().introspect(javaType)` → `beanDescription.findProperties()`；对每个 `BeanPropertyDefinition`，**仅当 `property.getField() != null`（有真实字段）才生成 InputValue**——纯 getter 计算属性不参与。
- InputValue 字段来源：`name` = Jackson 属性名（`@JsonProperty("…")` 若标注则用之，即 **JSON key**）；`description` = `PropertyMetadata.getDescription()`（← `@JsonPropertyDescription`）；`defaultValue` = `PropertyMetadata.getDefaultValue()`（← `@JsonProperty(defaultValue="…")`；未声明则经 `createDefaultInstance()` 建默认实例后用 getter 取值）；`contributorURI` = `platform:/plugin/<bundle symbolicName>`（i18n 翻译用）。
- 注解分派（字段的 `getAllAnnotations()`；ChemClipse 侧 settings 注解都标了 `@JacksonAnnotation` 元注解，故被 Jackson 收集）：

| 注解（support/settings/*.java，每注解一文件） | 生成的 InputValue 元数据 | 适用字段类型 |
|---|---|---|
| `@LabelProperty(value, tooltip)` | label + tooltip | 任意（显示层） |
| `@IntSettingsProperty(step, minValue, maxValue, validation)` | MinMaxValidator + EvenOddValidatorInteger | int/Integer |
| `@LongSettingsProperty`（同构） | MinMaxValidator + EvenOddValidatorLong | long/Long |
| `@ShortSettingsProperty` | MinMaxValidator + EvenOddValidatorShort | short/Short |
| `@ByteSettingsProperty` | MinMaxValidator + EvenOddValidatorByte | byte/Byte |
| `@FloatSettingsProperty(step, minValue, maxValue)` | MinMaxValidator | float/Float |
| `@DoubleSettingsProperty(step, minValue, maxValue)` | MinMaxValidator | double/Double |
| `@StringSettingsProperty(regExp, description, isMultiLine, allowEmpty, proposals)` | RegularExpressionValidator（regExp 非空时；isMultiLine 时逐行校验）+ isMultiLine + proposals | String |
| `@FileSettingProperty(validExtensions, extensionNames, dialogType=OPEN/SAVE, onlyDirectory, allowEmpty)` | fileSettingProperty（文件对话框参数） | File |
| `@ComboSettingsProperty(value=Class<? extends ComboSupplier>, edit)` | 反射实例化 ComboSupplier + comboEdit | 任意（下拉/可编辑下拉） |
| `@ValidatorSettingsProperty(validator=Class<? extends IValidator>)` | 反射实例化自定义 IValidator | 任意（如 ReportSettings/ReportColumns） |
| `@MultiFileSettingProperty(validExtensions)` | 支持类存在；settings 对话框路径未用 | File 集合 |
| `@StringSelectionSettingProperty(ids, labels)` / `@StringSelectionRadioButtonsSettingProperty` | Eclipse 偏好页 field editor 用（ComboFieldEditorExtended / RadioGroupFieldEditorExtended） | String 枚举 |
| `@PreferenceProperty(key, qualifier)` | 偏好映射 | 任意 |
| Jackson 标准 `@JsonProperty(value, defaultValue)` / `@JsonPropertyDescription` / `@JsonIgnore` / `@JsonDeserialize(using=…)` | 名字/默认值/描述/排除/自定义反序列化（如 `WindowSizeDeserializer`） | 全部 |

- `createDefaultInstance()`：先尝试带 `defaultConstructorArgument` 的单参构造——`AbstractProcessSupplier.getSettingsParser()` 传 `this`（supplier），故 settings 类可声明「单参构造接收 supplier」以拿上下文（`AbstractProcessSupplier.java:157-163`）；否则用无参构造。

### 8.3 JSONSerialization：对象 ↔ JSON（✅ 双 ObjectMapper 路径）

Source: `support/settings/serialization/JSONSerialization.java` / `SettingsSerialization.java`（接口四方法：`toString(Map)` / `fromObject` / `updateFromString` / `toString(Object)`）

- **`createMapper()`（带服务，核心）**：新 ObjectMapper + `SimpleModule("ChemClipse")`，含：
  - `SimpleAbstractTypeResolver`（RESOLVER）：查静态 `MAPPINGS`（ConcurrentHashMap，由 `JSONSerialization.addMapping(super, sub)` 注册）做抽象类型→具体类解析；
  - **动态注册所有 OSGi `ISerializationService`**：`Activator.getDefault().getSerializationServices()`（ServiceTracker）→ `simpleModule.addSerializer/addDeserializer(clazz, ...)`；
  - disable `FAIL_ON_UNKNOWN_PROPERTIES`。
  - 用于：`fromObject()`（对象→值表）、`toString(Map<InputValue,Object>)`（值表→JSON）、`updateFromString()`（JSON→对象）。
- **`toString(Object)`（裸 mapper）**：`new ObjectMapper()`，无模块/无服务/不关 unknown。用于 `ListProcessEntryContainer.createEntry(supplier, settings)`（`processing/methods/ListProcessEntryContainer.java:127`：`entry.setSettings(entry.getPreferences(supplier).getSerialization().toString(settings))`）与模板列表编辑器保存（如 `TemplatePeakListEditor.getSettings()` → `preferences.getSerialization().toString(settingz)`）——这些 settings 只含 String 字段，无需自定义序列化器。
- `updateFromString(object, content)`：`createMapper().readerForUpdating(object).readValue(content)` —— **原地更新对象**（JSON 缺的字段保持默认）；若对象 `instanceof ISettingsMigrator` 再调 `transferToLatestVersion(content)`。`AbstractSettingsMigrator` 依次试 `getSettingsMigrationHandler()`，命中即停；handler 的 `getObjectMapper()` 用 `FAIL_ON_UNKNOWN_PROPERTIES` **严格反序列化**以拒绝错误版本（`ISettingsMigrationHandler.java:20-29`）。
- 输出为**紧凑单行 JSON**（`writeValueAsString`，无 pretty printer）；`.ocm` 里每步 settings 即此单行字符串。**不存在「多行 JSON」**；「单字符串行编码」的另一层含义是**字段值内部**的 `|`/`;` 编码（§5.3），由自定义序列化器把集合 `save()` 成单串。
- 多态 settings（接口字段→具体类）**不用 `@JsonSubTypes`**（chemclipse + openchrom 全仓 grep 为 0），用 `JSONSerialization.addMapping(IProcessMethod, ProcessMethod)` / `addMapping(IProcessEntry, ProcessEntry)`（`model/Activator.java:57-58`）。

### 8.4 IProcessorPreferences / IProcessEntry：读写在途（✅）

Source: `processing/supplier/IProcessorPreferences.java`、`processing/methods/ProcessEntryProcessorPreferences.java`、`processing/methods/ProcessEntry.java`、`processing/methods/ListProcessEntryContainer.java`、`processing/supplier/NodeProcessorPreferences.java`、`processing/system/ProcessSettingsSupport.java`

- `IProcessorPreferences<SettingType>`：`DEFAULT_SETTINGS_SERIALIZATION = new JSONSerialization()`；核心默认方法 `getUserSettings()`：
  ```
  serializedString = getUserSettingsAsString();
  if (serializedString == null || settingsClass == null) return null;      // 解析失败返回 null → UI 重新弹设置对话框
  defaultInstance = supplier.getSettingsParser().createDefaultInstance();   // 反射建默认对象
  getSerialization().updateFromString(defaultInstance, serializedString);   // JSON 原地更新
  // IOException → return null
  ```
- 两种实现：
  - **`ProcessEntryProcessorPreferences`（方法内步骤）**：`setUserSettings(s) → processEntry.setSettings(s)`（写 activeProfile）；`getUserSettingsAsString() → processEntry.getSettings()`；`isUseSystemDefaults()` → settingsClass==null 或字符串为 null/空/`"{}"`；`getDialogBehaviour()` 恒 `NONE`。即「方法步骤的偏好 = 挂在 entry 上的 profile→JSON 字符串」。
  - **`NodeProcessorPreferences`（工作区默认）**：Eclipse Preferences 节点（supplier id 命名），键 `useSystemDefaults` / `userSettings` / `askForSettings`；`ProcessSettingsSupport.getWorkspacePreferences(supplier)` → `InstanceScope` 节点 `org.eclipse.chemclipse.processing.supplier.IProcessSupplier` 下的 `node(supplier.getId())`。
- **Profile 语义**（Q3 答案）：一个方法可持有**多组命名设置**（如「仪器 A/仪器 B」），默认名 `"Default Profile"`（`IProcessEntryContainer.DEFAULT_PROFILE`；注释称旧版兼容用 `""`，但常量值已是 `"Default Profile"`）：
  - `ListProcessEntryContainer`：`Set<String> profiles` + `String activeProfile`；`setActiveProfile` **级联所有 entry**，`addProfile/deleteProfile` 级联。
  - `ProcessEntry`：`Map<String,String> jsonSettingsMap`（profile→JSON 字符串）；`setSettings(json)` 写 activeProfile；`getSettings()` 读 activeProfile；`getSettings(profile)` 按名读；`copySettings(profile)` 把另一 profile 的 JSON 拷到当前；`deleteProfile` 删除并回退 DEFAULT_PROFILE。
  - `.ocm` 字段序：profiles（count+名字）+ activeProfile → 每 entry 的 settingsMap（count + profile→JSON + activeProfile）（§7.8 已证）。

### 8.5 ISerializationService：自定义序列化器注册与回调（✅）

Source: `support/settings/serialization/ISerializationService.java`、`support/Activator.java`、`templates/{serializer,service}/*.java`、各 ChemClipse 插件 `service/*SerializationService.java`

- 接口：`getSupportedClass()` / `getSerializer()` / `getDeserializer()`。
- 注册：OSGi DS `@Component(service={ISerializationService.class}, configurationPolicy=ConfigurationPolicy.OPTIONAL)`。实例：templates 的 `NameReplacementsSerializationService` / `ReportColumnsSerializationService` / `ReportSettingsSerializationService`；ChemClipse 侧还有 `TargetTracesSerializationService`（msd.classifier.supplier.wnc）、`WavenumberSignalsSerializationService`（vsd.filter）、`RetentionIndexAssigner/MarkerSerializationService`（amdiscalri）、`ColumnMappingSerializationService` / `FileHeaderDataSerializationService` / `TimeRangesSerializationService`（model）、`ScanSerializationServiceMSD`（msd.model）、`TraceRangesSerializationService{1D,2D}`（tsd.model）、`ClassificationDictionarySerializationService`（xxd.classification）等。
- 回调时机：support 插件 `Activator.start` 里 `new ServiceTracker<>(context, ISerializationService.class, null).open()`；`JSONSerialization.createMapper()` **每次创建 mapper 都重查 ServiceTracker** 并把每个服务的 serializer/deserializer 注册进 SimpleModule——故 `fromObject` / `toString(Map)` / `updateFromString` 三个入口自动带上全部自定义序列化器。
- 典型实现：`ReportColumnsSerializer.serialize()` → `jsonGenerator.writeString(reportColumns.save())`（把集合编码成 `|`/`;` 单串）；`NameReplacementsDeserializer.deserialize()` → `new NameReplacements().load(parser.getText())`。底层 `IStringSerialization<DataType>`（`support/util`）约定 `serialize(List)/deserialize(String)`。
- 注意：`IAnnotationWidgetService extends ISerializationService`（`support.ui/services`），自定义 bean 类型可**同时**提供序列化与专属控件（`createWidget(parent, description, currentSelection)` + `getValue(...)`）。

### 8.6 与 UI 的衔接（简述，控件细节留 MODULE_07）（✅）

Source: `ux.extension.ui/methods/{SettingsUI, SettingsUIProvider, WidgetItem}.java`、`templates.ui/adapter/*AdapterFactory.java`、`support.ui/services/IAnnotationWidgetService.java`

- `SettingsUI.createControl(preferences)`：`preferences.getUserSettings()`（null 则 `createDefaultInstance()`）→ `Adapters.adapt(settings, SettingsUIProvider.class)`；命中适配器用**自定义编辑器**，否则 `DefaultSettingsUIProvider` → `SettingsUIControlImplementation`（通用注解驱动表单）。
- 自定义编辑器注册：settings 类配 Eclipse `IAdapterFactory`（扩展点 `org.eclipse.core.runtime.adapters`），`getAdapter(settings, SettingsUIProvider.class)` 返回 λ 构造编辑器——如 `PeakDetectorSettingsAdapterFactory` → `TemplatePeakListEditor`（表格式列表编辑，`getSettings()` 返回 JSON 字符串）。
- 通用表单：`SettingsClassParser.getInputValues()` + `getSerialization().fromObject(inputValues, preferences.getSettings())` → 每字段一个 `WidgetItem`：
  - **rawType 分派控件**（`WidgetItem.createControl`）：int/long/float/double/short/byte→Text（保存时 parse）；boolean→Checkbox；enum→Combo（`ILabel` 标签提供者）；String→Text（isMultiLine→多行；proposals→ContentProposalAdapter 联想）；File→Text+浏览按钮（FileDialog/DirectoryDialog，参数按 `FileSettingProperty`）；ComboSupplier→Combo；**其它类型→遍历 `IAnnotationWidgetService` 匹配 rawType 调 `createWidget`**。
  - 校验：`WidgetItem.validate()` 依次跑 InputValue 的 validators（MinMax/EvenOdd/RegExp/自定义）+ InputValidator，失败以 ControlDecoration 红标。
  - 保存：`getSettings()` → `preferences.getSerialization().toString(values)`（**Map 路径 = createMapper，自定义序列化器生效**）→ `preferences.setUserSettings(json)`。

## 9. 待回填清单（✅ 本版全部解决）

| # | 问题 | 本版状态 |
|---|---|---|
| P1 | IProcessMethod/ProcessMethod/ProcessEntry 的类结构与方法顺序定义 | **已解决 ✅**（§7.2/§7.3：List 顺序即执行顺序，增删排序=List 操作） |
| P2 | 引擎执行：步骤迭代、参数绑定、中断、进度 | **已解决 ✅**（§7.5：applyProcessEntries + applyProcessor 全链） |
| P3 | 滤波器接口签名（IChromatogramFilter?） | **已解决 ✅**：`IChromatogramFilter.applyFilter(selection, settings, monitor)→IProcessingInfo<IChromatogramFilterResult>`；检测器变体 `IChromatogramFilterMSD` 等；wrapper `ChromatogramFilterProcessSupplier`（§7.7） |
| P4 | 处理器如何通过扩展点注册进「处理方法」可选项 | **已解决 ✅**：经典扩展点 + OSGi DS `IProcessTypeSupplier` 双机制（§5/§6 + §7.7 接口插件集中包装） |
| P5 | 方法文件（*.ocm?）的序列化格式 | **已解决 ✅**：`MTH.<版本>` 魔数 + GZIP + ObjectStream 原始类型流；settings 按 profile 存 JSON 字符串；6 个版本 reader 分派（§7.8） |
| P6 | 处理器在 UI 的偏好持久化（PreferenceSupplier 模式） | **已解决 ✅**：`AbstractPreferenceSupplier` + `getPreferenceNode()=bundle symbolicName` + `initializeDefaults()`；另有 `NodeProcessorPreferences` + Eclipse Preferences 节点（§7.6） |
| P7 | ProcessExecutionContext 字段语义 | **已解决 ✅**（§7.4：monitor/consumer/context/parent/contextMap 全字段 + 取消机制） |
| P8 | 引擎如何把旧式扩展点算法步骤（IPeakDetectorMSD 等）包装成 IProcessSupplier | **已解决 ✅**（§7.7：接口插件 `@Component(IProcessTypeSupplier)` + 扩展点 Support 枚举 + `ChromatogramSelectionProcessSupplier` 桥；`apply(selection, settings, consumer, monitor)` 签名） |

## 10. Qt/C++ 移植要点（core_processing 模块，✅ 基于已确认引擎 + ⚠️ 设计笔记）

基于已确认的引擎事实（§7）提炼：

- **管线 = 可配置的有向步骤列表**，每步统一接口（对应 Java 的 `IProcessSupplier`）：
  ```cpp
  struct ProcessContext { std::shared_ptr<Chromatogram> chromatogram; bool cancelled; };
  class IProcessStep {
  public:
      virtual ~IProcessStep() = default;
      virtual QJsonObject defaultConfig() const = 0;      // = Java @JsonProperty defaultValue
      virtual QJsonObject validateConfig(const QJsonObject&) const = 0; // = Java Validator
      virtual ProcessResult execute(ProcessContext&, const QJsonObject&) = 0; // = apply(...)
  };
  ```
- **执行引擎 = 双循环（已确认 Java 原貌，Qt 直接对照实现）**：
  1. 方法循环 `applyProcessEntries`：for(entry){ 查 step；找不到→警告跳过；绑参数；entry 有子步骤则递归 }——对应 Qt `for(const StepConfig& s : method.steps)`。
  2. 单步分发 `applyProcessor`：`consumer.execute()`（数据消费者调 step 的 `apply(selection,settings,ctx)`）+ 可选 `IProcessExecutor`（方法/系统步骤自身再递归）+ 异常捕获→跳过继续。
  - Java 的「supplier 描述符（id/name/settingsClass/DataCategory）」与「执行逻辑（apply/execute）」分离；Qt 可合一为 `IProcessStep`（id+configSchema+execute）。
- **两套步骤机制合并为一**：OpenChrom 区分「扩展点算法（detect/integrate/identify/applyFilter(sel,settings,monitor)）」与「DS 步骤（apply(sel,settings,context)）」，Qt 无需保留 OSGi/扩展点，统一收敛到 `IProcessStep`；把算法包装成步骤即可（对应 §7.7 接口插件的包装层）。
- **管线阶段枚举**（固化）：
  `FILTER → BASELINE → DETECT → INTEGRATE → IDENTIFY → QUANTIFY → REPORT`
  OpenChrom 没有硬编码阶段序，阶段由步骤 `getCategory()`（如 `ICategories.CHROMATOGRAM_FILTER`）隐含——Qt 用枚举显式排序，参照 templates 插件 xml 的步骤全集（PeakDetector/PeakIdentifier/Integrator/StandardsAssigner/StandardsReferencer/CompensationQuantifier/Report/Filter）一一映射。
- **步骤配置 JSON Schema 持久化**（对应 Java 已确认格式）：
  - 方法文件：Java `.ocm` = 魔数 `MTH.<ver>` + GZIP + **二进制流**，**每步 settings 是「profile→JSON 字符串」**（`ProcessEntry.jsonSettingsMap`），settings 内容由 Jackson `ObjectMapper` 序列化（`JSONSerialization`）。Qt 对应：方法文件 = 版本头 + 压缩的 `[{ "id": stepId, "config": {...} }]`（JSON 或 Qt 自带序列化均可），每步 config 的 schema 由 `defaultConfig()` + 字段元数据生成（Java 是 Jackson 注解 + `@StringSettingsProperty` 正则）。
  - 模板列表（templates 的「一行一条配置」）：Qt 直接用 JSON 数组存储 `DetectorSetting[]` 等价结构，**不需要**复刻 `;`/`|` 字符串编码。
- **版本化模型**：仿 `.ocm` 的 6 版本 reader 分派（`MethodReaderWriter_1003/1004/1401/1402`，写固定最新版、读按新→旧尝试 + 魔数校验）——Qt 每步 config schema 带 `version` 字段 + 迁移函数表 `v1000→v1001…`（OpenChrom 已实践此模式：读时逐个版本尝试 + legacy `ISD→VSD` 类别映射）。
- **偏好参数**：Java `PreferenceSupplier`（`AbstractPreferenceSupplier` + bundle 名命名空间）→ Qt `QSettings`（org=自研、app=模块名，键如 `templates/export/optimizeRange` 对应 `P_EXPORT_OPTIMIZE_RANGE_DETECTOR`）。
- **步骤间通信**：直接传 `QSharedPointer<Chromatogram>` + 记录型通知（`setDirty` 等价物）；不需要 OSGi 服务总线。
- **进度/取消**：Java 无 context 内布尔标记——`ProcessExecutionContext` 持 `IProgressMonitor`（`monitor.isCanceled()`），步骤 `apply` 抛 `InterruptedException` → 引擎转 `OperationCanceledException`（UI 层捕获显示「已取消」）；异常非中断 → `addErrorMessage` 并**跳过该步继续**。Qt 建议 `std::function<bool()> isCancelled` + 进度回调注入 `ProcessContext`，取消经异常或返回值传播；**引擎默认「失败跳过继续」语义，Qt 需决定是否保留或改为中止**。

**设置序列化框架专项（对应 §8）：**
- **设置 = 纯数据对象 + 元数据驱动 UI/校验**（对应 `SettingsClassParser` → `InputValue`）。Qt 5.14 的 `QJsonObject` 无 schema 反射，**推荐每个 settings 类自持字段元数据表**（手写，胜过 Q_PROPERTY 反射，简单直观）：
  ```cpp
  struct FieldMeta {
      QString key;                 // = @JsonProperty("…") 的 JSON key
      QJsonValue def;              // = @JsonProperty defaultValue
      enum Type { Int, Double, Bool, String, Enum, File, Custom, Combo } type;
      bool multiLine;              // = @StringSettingsProperty isMultiLine
      std::optional<QPair<QVariant, QVariant>> minMax;   // = @Int/@Float SettingsProperty
      QRegularExpression re;       // = @StringSettingsProperty regExp
      QStringList proposals;       // = proposals[]（QCompleter 联想）
      QStringList comboItems;      // = ComboSupplier.items() / Enum 常量
      std::function<QString(const QVariant&)> custom;    // = @ValidatorSettingsProperty + 自定义序列化
      QString label, tooltip;      // = @LabelProperty（可作 i18n key）
  };
  struct SettingsSchema { QList<FieldMeta> fields; QJsonObject defaults() const; };  // = SettingsClassParser.getInputValues()
  ```
- **Jackson 注解 ↔ Qt 映射思路**：`@JsonProperty(value, defaultValue)` → `key` + `def`；`@IntSettingsProperty(min/max)` / `@FloatSettingsProperty` → `minMax`；`@StringSettingsProperty(regExp, isMultiLine, allowEmpty, proposals)` → `re` + `multiLine` + `proposals`（空字符串/allowEmpty 语义在 validator 里处理）；`@FileSettingProperty` → `QFileDialog` 参数（`validExtensions/extensionNames`→`setNameFilters`，`onlyDirectory`→`getExistingDirectory`，`dialogType`→`getOpenFileName/getSaveFileName`）；`@ComboSettingsProperty` → `comboItems` + `ComboSupplier.items/fromString/asString` 三方法对应的转换函数（Qt 直接用字符串列表即可）；`@ValidatorSettingsProperty` → `custom` 校验函数；`@LabelProperty` → 显示名/tooltip。多态字段：Java 用 `JSONSerialization.addMapping(接口, 实现)`（非 @JsonSubTypes）→ Qt 用 `QVariant::type()` + type tag 判定。
- **settings 存 JSON 字符串 ↔ QJsonObject 持久化**：Java 把**紧凑单行 JSON 字符串**存进 `.ocm` 二进制流（每步 profile→字符串）。Qt 对应：方法文件 `[{ "id": stepId, "profiles": { "Default Profile": {...} }, "activeProfile": "..." }]`，或直接存 `QByteArray(QJsonDocument::toJson(Compact))`；**同样按 profile 建 `QHash<QString, QJsonObject>`** 存多组设置，activeProfile 切换即换组。
- **单行编码的意义**：settings 作为「一条 String 字段」内嵌于二进制流/偏好节点，原子、无需多行转义、与 legacy 格式（0001-0002 ZIP、ObjectStream）天然兼容；Qt 中 JSON 字符串本身即 String，天然单行，无需额外编码。区分两件事：**settings JSON 整体单行** vs **复杂字段值内部**的 `|`/`;` 单串编码（模板列表，见下条）。
- **自定义字段序列化器**：Java `ISerializationService`（OSGi DS 自动注册 + createMapper 每次重建收集）→ Qt 做注册表 `QHash<int /*QMetaType*/, std::function<QString(const QVariant&)>> serializerMap` + 对应 `deserializerMap`，`toJson` 时字段 metaType 命中则用其编码（对应 ReportColumns/NameReplacements 的 `save()/load()` 单串），不引入 OSGi；若某类型还要专属编辑器，注册表可再挂 `std::function<QWidget*(const QVariant&)>` 工厂（对应 `IAnnotationWidgetService`）。
- **版本迁移**：Java `ISettingsMigrator`/`AbstractSettingsMigrator`（handler 列表逐个试 + FAIL_ON_UNKNOWN 严格反序列化拒绝错版本；`updateFromString` 后回调 `transferToLatestVersion(content)`）→ Qt 每个 settings 类给 `version` 字段 + 迁移函数表 `v1→v2→v3`，读入时先升版本再绑 UI。
- **偏好持久化双轨**：Java 方法内 `ProcessEntryProcessorPreferences`（JSON 挂 entry，`isUseSystemDefaults` = 字符串空/`{}`）+ 工作区 `NodeProcessorPreferences`（Preferences 节点键 `useSystemDefaults/userSettings/askForSettings`）→ Qt 统一：方法内 settings 随方法文件；默认设置存 `QSettings`（org=自研，key=`<stepId>/userSettings`），保留 `useSystemDefaults` 布尔（true 时忽略 userSettings 用内置默认）。

## 11. 证据登记表

| # | 结论 | Source | 状态 |
|---|---|---|---|
| P-A | 滤波器/计算器/处理器插件清单 | .fetch/chemclipse_tree.json | ✅ 存在性 |
| P-B | 处理器自带 versioned 模型 + Reader/Writer | openchrom/plugins/net.openchrom.xxd.processor.supplier.tracecompare/src/.../model/v1000/ + io/ | ✅ |
| P-C | 算法统一 IProcessingInfo 契约 | .fetch/sources/（baseline/peakdetector/integrator 接口）| ✅ |
| P-D | **引擎实现细节** | chemclipse/plugins/org.eclipse.chemclipse.processing（methods/supplier/system/internal 全包读源码） | ✅ 已深挖 |
| P-E | **方法文件格式** | **修正：方法模型不是 GCMethod**；`.ocm` = `MTH.<ver>`魔数 + GZIP + ObjectStream 原始类型流；settings 按 profile 存 JSON 字符串 | xxd.converter.supplier.ocx/internal/methods/*（MethodReaderWriter_1402、ObjectStreamMethodFormat、GenericStreamMethodFormat、MethodReader_1000、MethodImportConverter、MethodExportConverter） | ✅ |
| P-F | **tracecompare 无 execute；入口为 `getTrackStatistics(IProcessorModel)`；数据装载在 UI 插件 DataProcessorUI；逐 track 视觉比对 + 人工标记，无自动评分** | tracecompare/core/DataProcessor.java; tracecompare.ui/.../DataProcessorUI.java; MeasurementModelData.java; TrackStatistics.java | ✅ |
| P-G | **TrackStatistics 聚合规则 + matchProbability 公式 + DESC 排序** | tracecompare/model/TrackStatistics.java; core/TrackStatisticComparator.java | ✅ |
| P-H | **v1000 模型字段全表 + JAXB 注解约定（@XmlElement/@XmlRootElement/"TraceCompare" + @XmlTransient 于接口 getter）** | tracecompare/model/v1000/*.java | ✅ |
| P-I | **序列化 = JAXB XML 格式化输出；无版本分派（仅 v1000）** | tracecompare/io/ProcessorModelReader+Writer.java | ✅ |
| P-J | **versioned 模型模式在 massshiftdetector 复用（.mdp / v1000 / io Reader+Writer）** | massshiftdetector/.../core/MassShiftDetector.java; model/v1000/; io/ | ✅ |
| P-K | **templates plugin.xml 扩展点全集（7 类）与步骤↔id↔settings↔model 对应表** | templates/plugin.xml（完整读） | ✅ |
| P-L | **经典算法步骤签名 `detect(selection, settings, monitor)→IProcessingInfo<?>`（PeakDetector 三检测器接口）** | templates/peaks/PeakDetector.java | ✅ |
| P-M | **DS 步骤签名 `apply(IChromatogramSelection, Settings, ProcessExecutionContext)→IChromatogramSelection`；AbstractProcessSupplier + DataCategory + ICategories.CHROMATOGRAM_FILTER** | templates/chromatogram/NameSynonymReplacer.java | ✅ |
| P-N | **system ProcessSupplier：`executeUserSettings(ISystemProcessSettings, ProcessExecutionContext)` 写 PreferenceSupplier** | templates/system/DetectorExportProcessSupplier.java 等 | ✅ |
| P-O | **settings 单字符串行编码（`\|`分字段/`;`分条）、ListUtil+Validator 解析、模板文件逐行导出** | templates/settings/*.java; model/DetectorSettings.java; util/AbstractTemplateListUtil.java; util/PeakDetectorValidator.java; io/DetectorExport.java | ✅ |
| P-P | **Jackson settings 注解（@JsonProperty/@IntSettingsProperty/@StringSettingsProperty）+ ISerializationService 序列化钩子** | templates/settings/*.java; serializer/*.java; service/*.java | ✅ |
| P-Q | **PreferenceSupplier 模式（AbstractPreferenceSupplier + bundle 名 + initializeDefaults）** | tracecompare/preferences/PreferenceSupplier.java; templates/preferences/PreferenceSupplier.java | ✅ |
| P-R | **cms/massshiftdetector 为纯库无 plugin.xml，被 .ui 插件直接调用；decompose/caclulateIonCertainties 签名** | cms/core/MassSpectraDecomposition.java; massshiftdetector/core/MassShiftDetector.java | ✅ |
| P-S | **引擎插件类图全景：supplier 三接口 + IProcessExecutor/IProcessExecutionConsumer/ExecutionResultTransformer + methods 包 + system 包 + internal 包（含 OSGiProcessSupplierContext）** | processing/methods/{AbstractProcessEntryContainer,ListProcessEntryContainer,ProcessEntry,ProcessEntryProcessorPreferences,SubProcessExecutionConsumer}.java; processing/supplier/*.java; processing/system/*.java; processing/internal/OSGiProcessSupplierContext.java | ✅ 全部读源码 |
| P-T | **IChromatogramSelectionProcessSupplier 契约 + ChromatogramSelectionProcessSupplier 桥（apply 转发 IMessageConsumer+IProgressMonitor）+ createConsumer（AtomicReference 结果槽）** | model/supplier/{IChromatogramSelectionProcessSupplier,ChromatogramSelectionProcessSupplier,IMeasurementProcessSupplier}.java | ✅ |
| P-U | 引擎调度/方法文件序列化/旧式步骤包装 | — | ✅ 已解决（见 P-V~P-X） |
| P-V | **执行循环 = `AbstractProcessEntryContainer.applyProcessEntries`（resumeIndex 跳过 / 按 id 查 supplier / 找不到警告跳过 / split 子上下文 / 组合方法递归）；单步分发 = `AbstractProcessSupplier.applyProcessor`（consumer.execute + IProcessExecutor + transformer，异常→跳过继续，InterruptedException→OperationCanceledException）** | processing/methods/AbstractProcessEntryContainer.java:64-135; processing/supplier/AbstractProcessSupplier.java:165-218 | ✅ |
| P-W | **ProcessExecutionContext 全字段：monitor(进度/取消)/consumer(消息)/context(supplier 查找)/parent(split 链)/contextMap(按 Class 键跨步骤对象槽)** | processing/supplier/ProcessExecutionContext.java | ✅ |
| P-X | **`.ocm` 格式：ZIP(0001-0002) 与 魔数+GZIP+ObjectStream(0003/1400-1402) 两代；6 版本 reader 按新→旧尝试；写固定 1402；`MTH.` 魔数；settings=profile→JSON(Jackson JSONSerialization+SettingsClassParser)** | xxd.converter.supplier.ocx/.../{MethodReaderWriter_1402,ObjectStreamMethodFormat,GenericStreamMethodFormat,MethodReader_1000,MethodWriter_1000,MethodExportConverter,MethodImportConverter}.java; settings/Format.java; support/.../settings/serialization/JSONSerialization.java | ✅ |
| P-Y | **用户/捆绑/系统方法 → IProcessSupplier：MethodProcessTypeSupplier(BundleTracker 扫 /OSGI-INF/processors/*.ocm) + UserMethodProcessSupplier + MetaProcessorProcessSupplier（IProcessExecutor 递归 applyProcessEntries）；id=`ProcessMethod.{uuid}:user|bundle|system:...`** | converter/methods/{MethodProcessTypeSupplier,UserMethodProcessSupplier,MetaProcessorProcessSupplier,MethodProcessSupport,MethodConverter}.java | ✅ |
| P-Z | **旧式算法包装模式：接口插件 `@Component(IProcessTypeSupplier)` 枚举扩展点 Support → 内部类 extends ChromatogramSelectionProcessSupplier → apply 转调 `Xxx.detect/integrate/identify/applyFilter(sel,settings,supplierId,monitor)`，返回原 selection；`matchesId` 兼容原扩展点 id（包装器全表见 §7.7）** | chromatogram.msd.peak.detector/core/PeakDetectorMSDProcessTypeSupplier.java; chromatogram.xxd.integrator/core/peaks/PeakIntegratorProcessTypeSupplier.java; chromatogram.filter/core/chromatogram/ChromatogramFilterProcessSupplier.java; model/supplier/IMeasurementFilterProcessTypeSupplier.java | ✅ |
| P-AA | **GCMethod 修正：仅 `float getTemperature(int)`（GC 柱温程序接口），与处理方法无关；方法模型 = ProcessMethod/ProcessEntry** | chromatogram.method.model/core/GCMethod.java | ✅ |
| P-AB | **SettingsClassParser：Jackson 反射内省 BeanPropertyDefinition（仅 `property.getField()!=null` 者）→ InputValue（name=@JsonProperty、default=@JsonProperty(defaultValue)/默认实例 getter、desc=@JsonPropertyDescription、contributorURI=bundle）；注解分派全表（数值→MinMax+EvenOdd、String→RegExp+multiLine+proposals、File/Combo/Validator/Label…）；createDefaultInstance 支持单参构造（参数=supplier）** | support/settings/parser/{SettingsClassParser,SettingsParser,InputValue}.java; processing/supplier/AbstractProcessSupplier.java:157-163 | ✅ |
| P-AC | **JSONSerialization 双 mapper：createMapper（SimpleModule + SimpleAbstractTypeResolver + 动态注册全部 ISerializationService + 关 FAIL_ON_UNKNOWN）用于 fromObject/toString(Map)/updateFromString；toString(Object) 用裸 ObjectMapper（无服务）；updateFromString 用 readerForUpdating 原地更新 + ISettingsMigrator 迁移回调（handler 逐个试 + FAIL_ON_UNKNOWN 严格反序列化）；输出紧凑单行 JSON，无 pretty printer** | support/settings/serialization/{JSONSerialization,SettingsSerialization,ISerializationService,WindowSizeDeserializer}.java; support/settings/ISettingsMigrationHandler.java | ✅ |
| P-AD | **注解族全表（ChemClipse 侧全标 @JacksonAnnotation、@Target FIELD、@Retention RUNTIME）：Int/Long/Short/Byte/Float/Double/StringSettingsProperty、FileSettingProperty、MultiFileSettingProperty、ComboSettingsProperty(+ComboSupplier 三方法)、ValidatorSettingsProperty、LabelProperty、StringSelectionSettingProperty、StringSelectionRadioButtonsSettingProperty、PreferenceProperty；@JsonSubTypes 全仓 grep 为 0（多态用 JSONSerialization.addMapping）** | support/settings/*.java（逐文件读）; model/Activator.java:57-58 | ✅ |
| P-AE | **IProcessorPreferences 读写链：getUserSettings = settingsParser.createDefaultInstance + updateFromString（IO 异常→null 让 UI 重弹）；ProcessEntryProcessorPreferences 桥 entry JSON（setUserSettings→entry.setSettings 写 activeProfile；isUseSystemDefaults=字符串空/`{}`；DialogBehavior 恒 NONE）；NodeProcessorPreferences 键 useSystemDefaults/userSettings/askForSettings + Preferences 节点（supplier id 命名）** | processing/supplier/IProcessorPreferences.java; methods/ProcessEntryProcessorPreferences.java; supplier/NodeProcessorPreferences.java; system/ProcessSettingsSupport.java | ✅ |
| P-AF | **Profile 机制：DEFAULT_PROFILE="Default Profile"；ListProcessEntryContainer 级联 setActiveProfile/addProfile/deleteProfile；ProcessEntry.jsonSettingsMap<profile,JSON> + setSettings/getSettings(profile)/copySettings/deleteProfile；createEntry(supplier,settings) 经 getSerialization().toString(settings) 序列化** | processing/methods/{IProcessEntryContainer,ListProcessEntryContainer,ProcessEntry,IProcessEntry}.java | ✅ |
| P-AG | **ISerializationService：@Component(service={ISerializationService.class}, OPTIONAL) DS 声明；support Activator ServiceTracker 收集（getSerializationServices）；createMapper 每次重建注册 serializer/deserializer；典型=ReportColumns/NameReplacements save()/load() 单串；IAnnotationWidgetService extends ISerializationService 兼作控件工厂** | support/Activator.java:45-46,64-67; templates/{serializer,service}/*.java; support.ui/services/IAnnotationWidgetService.java | ✅ |
| P-AH | **UI 桥：SettingsUI Adapters.adapt(settings,SettingsUIProvider) → 自定义编辑器（IAdapterFactory，如 PeakDetectorSettingsAdapterFactory→TemplatePeakListEditor）或 DefaultSettingsUIProvider→SettingsUIControlImplementation；WidgetItem 按 rawType 建控件（Text/Checkbox/Combo(ILabel)/File(FileDialog 按 FileSettingProperty)/IAnnotationWidgetService）；保存走 toString(Map)=createMapper 路径；校验=InputValue validators+InputValidator+ControlDecoration** | ux.extension.ui/methods/{SettingsUI,SettingsUIProvider,WidgetItem}.java; templates.ui/adapter/PeakDetectorSettingsAdapterFactory.java | ✅ |
| P-AI | **验证器实现：MinMaxValidator（数值域）/RegularExpressionValidator（multiline 逐行校验）/EvenOddValidator{Integer,Long,Short,Byte}/InputValidator** | support/settings/validation/*.java | ✅ |
