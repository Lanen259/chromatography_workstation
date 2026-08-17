# MODULE_06 — Report Model（报告模型层）

> **状态：🟢 全链路源码确认（Writer 数据绑定 ✅、序列化 ✅、Excel 占位符 ✅、PDF 壳 ✅、ChemClipse 核心框架类 ✅、UI 消费链 ✅）**
> 回答「最后如何生成报告」。OpenChrom 社区版自带 CSV / Excel 模板 / PDF 三个报告供应商，外加 `net.openchrom.xxd.process.supplier.templates`（Template Report，基于化合物设定的汇总报告）。ChemClipse 完整源码已抓取到 `.fetch/chemclipse-src/plugins/`，本模块核心类与 UI 消费链已全部读源确认。

---

## 1. 报告生成机制（✅ 源码确认）

### 1.1 报告扩展点

文件：`openchrom/plugins/net.openchrom.chromatogram.xxd.report.supplier.csv/plugin.xml` ✅

```xml
<extension point="org.eclipse.chemclipse.chromatogram.xxd.report.chromatogramReportSupplier">
  <ChromatogramReportSupplier
        fileExtension=".csv"
        fileName="CSV Chromatogram Peak Report"
        reportGenerator="...csv.core.ConfigurableReport"
        reportName="Chromatogram Peak Report (*.csv)"
        reportSettings="...csv.settings.ChromatogramReportSettings">
  </ChromatogramReportSupplier>
</extension>
```

**机制要点：**
- 每个报告格式 = 一个 `chromatogramReportSupplier` 扩展
- 声明：报告生成器类（`reportGenerator`）+ 设置类（`reportSettings`）+ 文件名/扩展名
- 扩展点所属插件：`org.eclipse.chemclipse.chromatogram.xxd.report`（ChemClipse）✅
- 三个供应商的声明均确认（plugin.xml 逐字读取）：
  - CSV：`...report.supplier.csv.core.ConfigurableReport` → `...csv.settings.ChromatogramReportSettings`，`.csv` ✅
  - Excel 模板：`...report.supplier.excel.template.core.ExcelTemplateReport` → `...excel.template.settings.ChromatogramReportSettings`，`.xlsx` ✅
  - PDF：`...report.supplier.pdf.ui.core.ChromatogramReportGenerator` → `...pdf.ui.settings.ChromatogramReportSettings`，`.pdf` ✅

### 1.2 报告生成器基类与实现（三个供应商同构）

三个生成器 `ConfigurableReport` / `ExcelTemplateReport` / `ChromatogramReportGenerator` 结构完全一致 ✅
（Source: 各自 `core/*.java`）。**基类只有 `validate()`，`generate()` 重载族与 `getChromatogramList()` 声明在接口、实现在每个供应商子类**：

```text
IChromatogramReportGenerator (接口)                     // ChemClipse: chromatogram/IChromatogramReportGenerator.java ✅
  ├─ generate(File, boolean append, IChromatogram, monitor)          // 4 个重载
  ├─ generate(File, append, List<IChromatogram>, monitor)
  ├─ generate(File, append, IChromatogram, settings, monitor)
  ├─ generate(File, append, List<IChromatogram>, settings, monitor)
  └─ validate(File)

AbstractChromatogramReportGenerator implements IChromatogramReportGenerator   // 基类只实现 validate() ✅
  └─ validate(file)                       // 见 §1.3

xxxReport extends AbstractChromatogramReportGenerator     // 每个供应商子类，实现 4 个 generate ✅
  ├─ generate(File, append, IChromatogram, monitor)      // getChromatogramList() + 默认设置
  ├─ generate(File, append, List<IChromatogram>, monitor)
  ├─ generate(File, append, IChromatogram, settings, monitor)
  ├─ generate(File, append, List<IChromatogram>, settings, monitor)
  └─ report(file, append, chromatograms, settings)
       ├─ super.validate(file)                       // 基类校验（null→ERROR、建目录/文件、canWrite）
       ├─ settings instanceof ChromatogramReportSettings   // 非本插件设置 → warn 并跳过
       ├─ new XxxReportWriter().generate(file, append, chromatograms, reportSettings)
       └─ processingInfo.setProcessingResult(file)
```

- 无 settings 参数的两个 `generate(...)` 从 `PreferenceSupplier.getReportSettings()` 取默认设置 ✅
- `getChromatogramList()` 把单色谱包成 List（`protected`，**在具体子类里**，如 ConfigurableReport.java:83-88）✅
- 基类 `AbstractChromatogramReportGenerator` 的 `validate()` 行为已在 ChemClipse 源码确认，见 §1.3 ✅

### 1.3 报告核心框架（ChemClipse `org.eclipse.chemclipse.chromatogram.xxd.report`，✅ 全源码确认）

插件目录：`plugins/org.eclipse.chemclipse.chromatogram.xxd.report/`（唯一职责：**声明扩展点** + 核心 API/注册表；`plugin.xml` 只含一行 `<extension-point id="...chromatogramReportSupplier">`）

**① `AbstractChromatogramReportGenerator`（chromatogram/AbstractChromatogramReportGenerator.java）——基类只含 `validate()`**
- `validate(File file)`（L27-44）：
  1. `file == null` → `getProcessingInfo("The file couldn't be found.")`（ERROR 消息）
  2. `createDirectoriesAndFiles(file)`（私有，L54-82）：
     - 若 file 是目录：不存在则 `mkdirs()`，失败 → ERROR「The given directory couldn't be created」
     - 否则：对父目录 `mkdirs()`（不存在时），文件不存在则 `createNewFile()`（捕获 IOException）→ ERROR「The given file couldn't be created」
  3. `!file.canWrite()` → ERROR「The file is not writeable: <path>」
  4. 全部通过 → 返回空 `ProcessingInfo<>`（无消息 = 校验通过）
- `getProcessingInfo(message)`（L84-90）：`new ProcessingInfo<>()` + `ProcessingMessage(MessageType.ERROR, "Chromatogram Report", message)`
- **注意：基类没有 `generate()`、没有 `getChromatogramList()`、没有 processingInfo 处理**——4 个 `generate()` 由 `IChromatogramReportGenerator` 接口声明、每个供应商子类实现（§1.2 的树已示出）。processingInfo 的成功路径（`setProcessingResult(file)`）在子类 `report()` 里 ✅

**② `IChromatogramReportGenerator`（接口）**：4 个 `generate(...)` 重载（File + append + IChromatogram/List + IChromatogramReportSettings/无 + IProgressMonitor）+ `validate(File)`，均返回 `IProcessingInfo<?>`

**③ `IChromatogramReportSupplier`（接口）+ 扩展点字段映射**
- 接口方法：`getId()` / `getDescription()` / `getReportName()` / `getFileExtension()` / `getFileName()` / `getSettingsClass()`
- 扩展点元素属性 → 字段（`ChromatogramReports` 常量 L39-45 + schema exsd）：
  - `id` → getId；`description` → getDescription；**`reportName` → getReportName**（无 `name` 属性）；`fileExtension` → getFileExtension（`setFileExtension` 自动补 `.` 前缀）；`fileName` → getFileName；`reportSettings` → getSettingsClass（可省略）
  - **`reportGenerator` 不进供应商 bean**：在生成时由注册表 `element.createExecutableExtension("reportGenerator")` 直接实例化（§1.4）✅
- `AbstractChromatogramReportSupplier`：getter/setter 实现 + `equals/hashCode`（按 id/description/reportName/fileExtension/fileName）；`setSettingsClass` 是 `protected`
- `ChromatogramReportSupplier extends AbstractChromatogramReportSupplier`（空壳，用于注册表实例化）

**④ 设置基类（settings/ 包）**
- `IChromatogramReportSettings extends IProcessSettings`：`getExportFolder()` / `isAppend()` / `getFileNamePattern()`
- `AbstractChromatogramReportSettings`（Jackson 注解驱动）：
  - `exportFolder`（File，`@JsonProperty("Export Folder")` + `@FileSettingProperty(onlyDirectory=true, dialogType=SAVE_DIALOG)`）——为 null 时退回 `getDefaultFolder()`（抽象）
  - `append`（boolean，`@JsonProperty("Append")` 默认 false）
  - `filenamePattern`（String，`@JsonProperty("File Name")` 默认 `{chromatogram_name}{extension}`，支持 `IProcessSettings` 的 8 个变量：`{chromatogram_name}`、`{chromatogram_dataname}`、`{chromatogram_samplegroup}`、`{chromatogram_shortinfo}`、`{chromatogram_samplename}`、`{chromatogram_operator}`、`{chromatogram_instrument}`、`{chromatogram_tags}`、`{extension}`、`{current_directory}`）
- `DefaultChromatogramReportSettings extends AbstractChromatogramReportSettings`：只实现 `getDefaultFolder()` = `PreferenceSupplier.getReportExportFolder()`（preference `reportExportFolder`，默认 ""）
- CSV 插件的 `ChromatogramReportSettings extends DefaultChromatogramReportSettings` 再叠加插件自有字段（delimiter/printResultsHeader 等，见 §8）✅

**⑤ `Delimiter` 枚举（org.eclipse.chemclipse.model/src/.../model/settings/Delimiter.java）✅**
- 值：`COMMA(',', "Comma")` / `SEMICOLON(';', "Semicolon")` / `TAB('\t', "Tab")`
- `getCharacter()` 返回对应 char；实现 `ILabel`（`label()`）；`getOptions()` 返回 `[value,label]` 二维数组供 SWT Combo/偏好页填充

**⑥ `IChromatogramOverview` 时间因子（org.eclipse.chemclipse.model/src/.../model/core/IChromatogramOverview.java L25-27）✅**
- `SECOND_CORRELATION_FACTOR = 1000.0d`（1ms×1000=1s）
- `MINUTE_CORRELATION_FACTOR = 60000.0d`（ms→min）✅ 与文档 §3.2 用法一致
- `HOUR_CORRELATION_FACTOR = 3600000.0d`
- 注意字段是 `double`；保留时间本身以 ms 为单位的 int（`getStartRetentionTime()` 注释「milliseconds」）

### 1.4 扩展注册表机制（core/ChromatogramReports.java，✅ 全源码确认）★ 供应商发现与 id 检索

`ChromatogramReports` = **静态工具类**（无实例），一切从 Eclipse 扩展注册表 `Platform.getExtensionRegistry().getConfigurationElementsFor(EXTENSION_POINT)` 现读现建，**不缓存**：

- `EXTENSION_POINT = "org.eclipse.chemclipse.chromatogram.xxd.report.chromatogramReportSupplier"`；属性名常量：`ID`、`DESCRIPTION`、`FILTER_NAME="reportName"`、`FILE_EXTENSION`、`FILE_NAME`、`REPORT_GENERATOR`、`REPORT_SETTINGS`（L39-45）
- **枚举供应商** `getChromatogramReportSupplierSupport()`（L109-150）→ 返回 `ChromatogramReportSupport`：
  1. 遍历扩展元素；先设 `fileExtension`/`fileName`，用 `isValid()`（正则 `[\\/:*?"<>|]`，拒绝含非法文件名字符的供应商，L205-222）
  2. 合法才继续设 `id`/`description`/`filterName`（reportName）
  3. `reportSettings` 属性存在 → `element.createExecutableExtension("reportSettings")` 实例化设置类 → `supplier.setSettingsClass(instance.getClass())`（异常 → null，设置类可选）
- **按 id 取生成器** `getChromatogramReportGenerator(id)`（L156-169）：`getConfigurationElement(id)`（L175-188 遍历匹配 `id` 属性）→ `element.createExecutableExtension("reportGenerator")` 反射实例化 → `IChromatogramReportGenerator`
- **静态便捷入口** 4 个 `generate(File, append, chromatogram/List, settings/无, supplierId, monitor)`（L54-100）：按 id 找生成器 → 调用对应 `generate()`；找不到 → `getNoChromatogramReportAvailableProcessingInfo`（ERROR 消息）
- `ChromatogramReportSupport extends AbstractChromatogramReportSupport`（内存列表容器，L20-169）：
  - `add(supplier)`；`getFilterNames()`（报告名数组，供 UI Combo/FileDialog）；`getReportExtensions()`；`getReportSupplierId(int index)`；`getReportSupplierId(String name)`（**按报告名 → id**，首匹配）；`getReportSupplier(String id)`（**按 id 检索**）；`getAvailableProcessorIds()`；`getReportSupplier()`（全列表）
  - 列表为空 → 抛 `NoReportSupplierAvailableException`
- 扩展点 schema：`<ChromatogramReportSupplier>` 元素，`reportGenerator` 的 `basedOn` = `AbstractChromatogramReportGenerator`、`reportSettings` 的 `basedOn` = `IChromatogramReportSettings`（PDE 校验类型）✅

---

## 2. 报告字段模型（✅ 源码确认）★ 报告模型 = 峰字段全集

文件：`openchrom/plugins/net.openchrom.chromatogram.xxd.report.supplier.csv/src/.../model/ReportColumns.java` ✅

`ReportColumns extends ArrayList<String>`，60 个常量定义报告列名（= 从峰/色谱/定量/标识可导出的**全部字段清单**）：

| 分组 | 列名（常量值） |
|---|---|
| 色谱 | Chromatogram Name, File Path, Number Peaks |
| 峰几何/积分 | Peak Number, Retention Time, Retention Index, Scans, Start, Stop, Peak Area, Peak Area [%], Peak Height, Peak Resolution |
| 峰宽系列 | Peak Width Baseline from Inflection Points, Peak Width Baseline Total, Peak Width at Half Peak Height, Peak Width at 0% / 5% / 10% / 15% / 50% / 85% Height |
| 峰质量 | Purity, Components, S/N, Leading, Tailing, Integrator, Model, Detector |
| 定性 | Target, Formula, CAS, SMILES, InChI, InChIKey, Match Factor, Reverse Match Factor, Probability, Identifier, Database |
| 定量 | Quantifier, Classifier, Quantitation Area, Quantitation Calibration Method, Quantitation Chemical Class, Quantitation Concentration, Quantitation Concentration Unit, Quantitation Area Description, Quantitation Name, Quantitation Flag, Quantitation Signal, Quantitation Cross 0, Quantitation Reference |
| 内标 | Internal Standard Chemical Class / Concentration / Concentration Unit / Name / Compensation Factor |
| 动态扩展 | 当前色谱 `getHeaderDataMap()` 的任意 key（见 §5.3，运行时加入可用列） |

> 这条字段清单同时是「峰模型 + 定量模型 + 标识结果」的完整快照——对自研 CDS 的导出/报告设计极具参考价值。✅
> `getDefault()` 按固定顺序返回全部 60 列（ReportColumns.java:88-154）✅；`load()/save()` 用逗号分隔串持久化（见 §4）。

---

## 3. ConfigurableReportWriter：列名 → 取值的数据绑定（★ 最关键机制）✅ 源码确认

文件：`openchrom/plugins/net.openchrom.chromatogram.xxd.report.supplier.csv/src/.../io/ConfigurableReportWriter.java`

### 3.1 绑定方式：巨型 if 链（非反射 / 非 Map / 非 switch）

`printChromatogramData()`（第 110–460 行）对每个选中列名跑一遍 **`reportColumn.equals(ReportColumns.XXX)` 的 if 链**，命中就 `records.add(值)`。**不是**反射，**不是** Map 查表，**不是** switch。✅
- 列顺序 = `records` 添加顺序 = ReportColumns 列表顺序，与表头一致 ✅
- 若某列在所有分支都未命中（例如动态 header key 在当前色谱 header 里不存在），该行**少一个值** → 与表头错位。这是该实现的结构性边界（源码可见）✅

### 3.2 逐字段取值来源（全部 ✅，Source: ConfigurableReportWriter.java:110-460）

**色谱级（每条数据行都从 chromatogram 取）：**
- `Chromatogram Name` → `chromatogram.getName()`（L113）
- `File Path` → `chromatogram.getFile()`（L116，File 对象由 CSVPrinter 转串）
- `Number Peaks` → `chromatogram.getPeaks().size()`（L125）
- **动态 header 列** → `chromatogram.getHeaderDataMap()` 逐个 entry：列名等于 header key 时取 `header.getValue()`（L118-123）

**峰级（`IPeak peak`，先按 `PeakRetentionTimeComparator(SortOrder.ASC)` 排序，L107-108）：**
- `Peak Number` → 每色谱自 1 递增的 `peakNumber`（L127-129，排序后重编号）
- `Components` → `peak.getSuggestedNumberOfComponents()`（L145）
- `Peak Area` → `peak.getIntegratedArea()`（L150）；`Peak Area [%]` → `100.0d / totalPeakArea * peakArea`，totalPeakArea 为 0 时返回 0（L152-154, L470-476，totalPeakArea = `chromatogram.getPeakIntegratedArea()` L105）
- `Integrator / Model / Detector / Quantifier` → `getIntegratorDescription() / getModelDescription() / getDetectorDescription() / getQuantifierDescription()`（L156-167）
- `Classifier` → `peak.getClassifiers()`（L168-169）

**色谱峰子类型（`peak instanceof IChromatogramPeak` 才取值，否则空串 L131-143）：**
- `Purity` → `chromatogramPeak.getPurity()`；`S/N` → `chromatogramPeak.getSignalToNoiseRatio()`

**峰模型（`IPeakModel peakModel = peak.getPeakModel()`，L171）：**
- `Retention Time` → `peakModel.getRetentionTimeAtPeakMaximum() / IChromatogramOverview.MINUTE_CORRELATION_FACTOR`（毫秒→分钟，L172-175；常量值在 ChemClipse 核心 ❓，按用法为 60000）
- `Peak Height` → `peakModel.getPeakAbundanceByInflectionPoints()`（L177）
- `Peak Width Baseline from Inflection Points` → `getWidthBaselineByInflectionPoints() / FACTOR`（L179-181）
- `Peak Width Baseline Total` → `getWidthBaselineTotal() / FACTOR`（L182-184）
- `Peak Width at Half Peak Height` → `getWidthByInflectionPoints() / FACTOR`（L185-187）
- `Peak Width at 0%/5%/10%/15%/50%/85% Height` → `getWidthByInflectionPoints(0.0f / 0.05f / 0.10f / 0.15f / 0.50f / 0.85f) / FACTOR`（L188-205）
- `Leading / Tailing` → `peakModel.getLeading() / getTailing()`（L206-211）
- `Start / Stop` → `getStartRetentionTime() / getStopRetentionTime()`（原始毫秒，未除 FACTOR，L212-217）
- `Retention Index` → `peakModel.getPeakMaximum().getRetentionIndex()`（L218-220）
- `Scans` → `peakModel.getNumberOfScans()`（L221-223）
- `Peak Resolution` → 与相邻峰算 `PeakResolution(peak, nextPeak).calculate()`：按**未排序的** `chromatogram.getPeaks()` 求 indexOf，前峰取 (prev, peak)、末峰取 (peak, next)、单峰为空串（L224-241）✅

**标识结果（`peak.getTargets()` 按 `IdentificationTargetComparator(SortOrder.DESC)` 排序取**第 1 个**目标，L242-252；无目标时整组为空串）：**
- `Target / Formula / CAS / SMILES / InChI / InChIKey / Mol Weight / Identifier / Database` → `libraryInformation.getXxx()`（L253-336）
- `Match Factor / Reverse Match Factor / Probability` → `comparisonResult.getMatchFactor() / getReverseMatchFactor() / getProbability()`（L295-315）

**内标（`peak.getInternalStandards().get(0)`，L337-341）：**
- `Internal Standard Chemical Class / Concentration / Concentration Unit / Name / Compensation Factor` → `internalStandard.getXxx()`（L342-376）

**定量（`peak.getQuantitationEntries().get(0)`，L377-381）：**
- `Quantitation Area / Calibration Method / Chemical Class / Concentration / Concentration Unit / Area Description / Name / Flag / Signal / Cross 0` → `quantitationEntry.getXxx()`（`getQuantitationFlag().label()` L433）（L382-451）
- `Quantitation Reference` → `peak.getQuantitationReferences().get(0)`（L452-459）

### 3.3 CSV 输出格式（✅ 源码确认）

- **库**：Apache Commons CSV `CSVPrinter` + `CSVFormat.RFC4180`（引号规则按 RFC4180，分隔符由设置注入），`FileWriter(file, append)`（L52）✅
- **表头**：首行 = 选中列名（ReportColumns 顺序）；`isPrintResultsHeader()` 决定是否打印；**追加到已有非空文件时**，仅当 `isAppendResultsHeader()` 才重复打印表头（`printHeader()` L82-99）✅
- **数据行**：每峰一行（`csvPrinter.printRecord(records)`），值按列顺序 ✅
- **空值**：可选数据缺失（无目标/内标/定量）补 `""`（空串）✅
- **多色谱/追加**：外层 for 遍历 `chromatograms` 列表；`reportReferencedChromatograms()` 为真时再遍历 `chromatogram.getReferencedChromatograms()`（L63-70）；每色谱数据块后按 `printSectionSeparator` 打一个空行（L465-467）✅
- **空列清单** → `getValidatedReportColumns()` 退回 `ReportColumns.getDefault()`（全列，L77-80）✅

---

## 4. ReportColumns 序列化与持久化（✅ 源码确认）

文件：`serializer/ReportColumnsSerializer.java`、`serializer/ReportColumnsDeserializer.java`、`service/ReportColumnsSerializationService.java`

- **格式**：Jackson `JsonSerializer<ReportColumns>` —— 序列化结果为**单个 JSON 字符串**，内容是逗号分隔的列名：`writeString(reportColumns.save())` → `"Chromatogram Name,File Path,..."` ✅（ReportColumnsSerializer.java:29）
- **反序列化**：`new ReportColumns().load(jsonParser.getText())` —— 按 `","` split、去单双引号、trim、跳空（ReportColumns.java:171-182）✅
- **注册**：`ReportColumnsSerializationService implements ISerializationService`，OSGi DS 组件（`OSGI-INF/...ReportColumnsSerializationService.xml`），声明 `getSupportedClass()=ReportColumns.class` ✅
- **用途**：ChemClipse 设置框架（`org.eclipse.chemclipse.support.settings.serialization`，❓ 核心）在序列化含 `ReportColumns` 字段的设置类时回调该 Service。即**用户勾选的列以逗号串内嵌在 `ChromatogramReportSettings` 的 JSON 里**随方法/过程设置持久化 ✅（推断链路：csv.settings 标注 `@ValidatorSettingsProperty(ReportColumnsValidator.class)` + 该 Service 存在；核心如何落盘 ❓）
- **Eclipse preferences 里只存**：CSV 插件的 `delimiter`、`reportReferencedChromatograms`（`PreferenceSupplier.P_DELIMITER/P_REPORT_REFERENCED_CHROMATOGRAMS`，L25-33）——**列勾选本身不进 preferences**，在设置对象 JSON 中 ✅
- `ReportColumnsValidator`：仅类型校验（必须 instanceof ReportColumns），不做内容检查（validators/ReportColumnsValidator.java）✅

---

## 5. 报告 UI 触发链（✅ 全链路源码确认）

### 5.1 三个 UI 插件（csv.ui / excel.template.ui / pdf.ui）声明的入口

| UI 插件 | plugin.xml 扩展 | 内容 |
|---|---|---|
| csv.ui | `org.eclipse.ui.preferencePages`（category=`org.eclipse.chemclipse.chromatogram.xxd.report.ui.preferences.reportPreferencePage`）| PreferencePage：列分隔符（Combo）+ Report Referenced Chromatograms（布尔）✅ |
| csv.ui | `org.eclipse.chemclipse.xxd.process.ui.menu.icon` | `MenuIcon`（IMenuIcon → IMAGE_CSV 图标）✅ |
| excel.template.ui | `org.eclipse.ui.preferencePages`（同 category）| PreferencePage：模板文件选择（FileFieldEditor）+「Create Excel Template」按钮（生成含全部占位符的 .xltx，见 §6.3）✅ |
| excel.template.ui | `org.eclipse.chemclipse.xxd.process.ui.menu.icon` | `MenuIcon` ✅ |
| pdf.ui | `org.eclipse.chemclipse.xxd.process.ui.menu.icon` | `PortableDocumentFormatMenuIcon` ✅ |

- **菜单/对话框本体已确认**：报告导出向导在 ChemClipse `org.eclipse.chemclipse.chromatogram.xxd.report.ui`（§5.2），settings 编辑与图标走现代 Process 框架（`xxd.process.ui`，§5.3）✅
- **csv.ui 的列编辑器接入**：`ReportColumnsAnnotationService extends ReportColumnsSerializationService implements IAnnotationWidgetService`（OSGi 组件）——把 SWT 的 `ReportColumnEditor` 控件挂到 ChemClipse 通用设置注解系统，使「Columns」设置项在方法编辑器里显示为双列表 ✅
- `ReportColumnEditor`（csv.ui/swt，511 行）：左「Available」右「Reported」双 Table + 工具条（Add/Remove/Clear/Move Up/Down）+ 搜索过滤；可用列 = `ReportColumns.getDefault()` + 当前色谱 `getHeaderDataMap().keySet()`（**运行时动态列**，L454-459、L66-79）；`load(String)/save()` 复用逗号串 ✅
- excel.template.ui 的 PreferencePage 里「Create Excel Template」：`ExcelTemplateReportWriter.generateTemplate(file)` 导出两行模板（表头=占位符 key、第 2 行=`{key}`），询问是否设为默认模板，并用系统编辑器打开（PreferencePage.java:67-104）✅

### 5.2 报告导出向导（ChemClipse `xxd.report.ui/export/wizards`，✅ 全源码确认）

**入口**：`xxd.report.ui/plugin.xml` 注册 `org.eclipse.ui.exportWizards` 扩展（category "Reporting" + wizard `ChromatogramReportExportWizard`，id `...report.ui.chromatogramReportExportWizard`，name "Chromatogram Report"）→ 出现在 **File ▸ Export ▸ Reporting ▸ Chromatogram Report**（Eclipse 标准导出向导机制，无 Java 代码直接引用该类）✅

**向导链**（`ChromatogramReportExportWizard extends Wizard implements IExportWizard`，L40-166）：
1. **第 1 页 `ChromatogramSelectionWizardPage`**（来自 `ux.extension.msd.ui`）：多选 `*.ocb` 色谱文件（文件表）
2. **第 2 页 `ReportSupplierSelectionWizardPage`**（L37-253）：
   - SWT Table 三列：**Report Name / Report Folder/Report File / Report Id**（每行 = 一个报告任务：供应商 id + 输出目录或追加文件）
   - 「Add」按钮 → 打开 `ChromatogramReportEntriesWizard`（包在 `ProcessWizardDialog extends WizardDialog` 里）
   - 「Remove / Remove All」删行；`checkReportSupplier()` 至少一行才可完成
3. **`ChromatogramReportEntriesWizardPage`**（L42-231）——**选择格式 + 输出**：
   - `reportSupport = ChromatogramReports.getChromatogramReportSupplierSupport()`（L107，**读扩展注册表**）
   - Combo `setItems(reportSupport.getFilterNames())`（L121-123）＝ 全部已注册报告名（"Chromatogram Peak Report (*.csv)" 等）
   - 「Append the chromatogram reports to a distinct file」复选框 → 决定选**报告文件**（FileDialog，默认名 `reportSupplier.getFileName()`、过滤 `*`+`getFileExtension()`、名称 `getReportName()`）还是**报告目录**（DirectoryDialog）
   - `getChromatogramReportEntry()`（L62-80）：`getReportSupplier()`（Combo index → `reportSupport.getReportSupplierId(描述串)` **报告名→id** → `getReportSupplier(id)`）→ 组装 `ChromatogramReportSupplierEntry(reportFolderOrFile, reportSupplierId)`
4. **`performFinish()`**（L68-136）：
   - `getInputFiles()` 第 1 页文件表；`getReportSupplier()` 第 2 页表 → `Map<supplierId, folderOrFile>`
   - 对每个供应商、每个输入文件：`ChromatogramConverterMSD.convert(file, CONVERTER_ID_CHROMATOGRAM, monitor)` 加载色谱；若输出是**目录** → `appendReport=false`、文件名 = `chromatogram.getName()`；否则 → `appendReport=true`、用所选文件
   - 执行 `ChromatogramReports.generate(file, append, chromatogram, reportSupplierId, monitor)`（**无 settings 变体 → 生成器从 PreferenceSupplier 取默认设置**，见 §1.2）✅
   - 注意：**这条旧向导不弹设置编辑对话框**；settings 对象编辑走 §5.3 的现代 Process 路径

### 5.3 现代 Process 路径（设置对象如何传给 generate + menu.icon，✅ 全源码确认）

**进程供应商**（`xxd.report/core/ChromatogramReportsProcessSupplier.java`，OSGi DS `@Component(service=IProcessTypeSupplier.class)`）：
- `getProcessorSuppliers()`（L49-62）：`ChromatogramReports.getChromatogramReportSupplierSupport()` 枚举 → 每个供应商包装成 `ChromatogramReportProcessorSupplier extends ChromatogramSelectionProcessSupplier<IChromatogramReportSettings>`（L64-116），构造函数传入 supplier.getId()/getReportName()/getDescription()/getSettingsClass() 与 DataType(MSD/CSD/WSD/VSD)
- `apply(selection, processSettings, messageConsumer, monitor)`（L76-115）——**这是设置对象进入 generate 的路径**：
  1. `processSettings instanceof IChromatogramReportSettings` 直接用，否则 new `DefaultChromatogramReportSettings`
  2. 校验 `getExportFolder()`（null → ERROR「No output folder specified...」）
  3. exportFolder 含 `{current_directory}` → 替换为 `chromatogram.getFile().getParent()`
  4. `fileName = AbstractProcessSettings.validateFileName(chromatogram, settings.getFileNamePattern(), supplier.getFileExtension())` → `new File(exportFolder, fileName)`
  5. `ChromatogramReports.generate(file, settings.isAppend(), chromatogram, settings, getId(), monitor)`（L107）——**settings 对象从方法编辑器的通用设置 UI（Jackson 注解 + IAnnotationWidgetService）流入**
  6. `messageConsumer.addMessages(info)` + `addInfoMessage("Report written to " + file)`；返回原 selection
- 该 DS 组件使**每个报告供应商自动成为一个「处理方法」**，进入 Process Methods 编辑器/快捷工具栏（消费方 = `xxd.process.ui`，处理框架属 process 模块，不在本模块深挖）✅

**menu.icon 扩展点消费链**（`xxd.process.ui`，✅）：
- 扩展点定义：`xxd.process.ui/plugin.xml` `<extension-point id="org.eclipse.chemclipse.xxd.process.ui.menu.icon">`；schema 元素 `<icon class=... id=...>`；接口 `IMenuIcon`（`getImage()`）
- 供应商 UI 插件声明：如 pdf.ui/plugin.xml `<icon class="...pdf.ui.icon.MenuIcon" id="org.eclipse.chemclipse.chromatogram.xxd.report.supplier.pdf"/>`；CSV 的 id = `net.openchrom.chromatogram.xxd.report.supplier.openchrom.chromatogramReportCSV`（= 该供应商的 `chromatogramReportSupplier` 扩展 id，逐字一致）
- 消费方 `ProcessorSupport.getMenuIcon(IProcessSupplier)`（L153-172）：读扩展注册表 → `processSupplier.getId().contains(element.id)` **子串匹配** → `createExecutableExtension("class")` → `IMenuIcon.getImage()`；`Processor.getMenuIcon()`（toolbar/Processor.java L79-91）优先用户覆盖图 → `getMenuIcon()` → 类别兜底图（报告类别 → `IMAGE_CHROMATOGRAM_REPORT`，`getDefaultIcon` L177-218）
- 即：**报告方法在 Process 工具栏的图标由报告插件经 menu.icon 贡献，按供应商 id 子串匹配** ✅

---

## 6. Excel 模板报告引擎（✅ 源码确认）

文件：`excel.template/.../io/{ExcelTemplateReportWriter, PlaceholderProcessor, CellData, SheetCopySupport}.java`

### 6.1 占位符语法：`{key}`（花括号，非 `${...}`）

- `PlaceholderProcessor.PLACEHOLDER_START = "{"`，`PLACEHOLDER_STOP = "}"` ✅
- 构造 `new PlaceholderProcessor(key, Function<CellData,String>)` → placeholder = `"{key}"` ✅
- `populate(CellData)`：若当前单元格文本包含 `{key}`，则 `replace("{key}", function.apply(cellData))`——**替换该单元格内所有出现处** ✅

### 6.2 占位符全集（~55 个，`createPlaceholderProcessors()` L54-139）✅

| 来源 | 占位符 key |
|---|---|
| 色谱 | chromatogram_name, chromatogram_area（getPeakIntegratedArea）, number_peaks |
| 峰 | peak_number（**1 起始**：`getPeakNumber()+1`）, components, peak_area, integrator, peak_model, peak_detector, quantifier |
| 色谱峰/噪声 | noise_factor（先 `chromatogram.getSignalToNoiseRatio(100)` 触发噪声计算，再取 `getNoiseCalculator().getNoiseFactor()`）, purity, s/n |
| 峰模型 | retention_time_start, retention_time, retention_time_stop, peak_height, peak_width_baseline_from_inflection_point, peak_width_baseline_total, peak_width_by_inflection_points, leading, tailing, retention_index, scans, peak_total_signal（getPeakMaximum().getTotalSignal()） |
| 库信息 | best_target, formula, cas, smiles, inchi, inchi_key, mol_weight, reference_identifier, database |
| 比对结果 | match_factor, reverse_match_factor, probability |
| 内标 | internal_standard_chemical_class, internal_standard_concentration, internal_standard_concentration_unit, internal_standard_name, internal_standard_compensation_factor |
| 定量 | quantitation_entry_area, quantitation_calibration_method, quantitation_chemical_class, quantitation_concentration, quantitation_concentration_unit, quantitation_description, quantitation_name, quantitation_flag, quantitation_signal, quantitation_cross_zero |
| 定量引用 | quantitation_reference（`cellData.getQuantitationReference()`，即 `peak.getQuantitationReferences().get(0)`） |

- 数值全部转 `String`（如 `Double.toString(...)`）✅

### 6.3 CellData：单峰上下文快照（CellData.java）✅

- 字段：cellValue + chromatogram + peakNumber + 派生缓存（peak, peakModel, libraryInformation, comparisonResult, internalStandard, quantitationEntry, quantitationReference）
- `updatePeak()`（L115-166）：按 peakNumber 取 `chromatogram.getPeaks().get(peakNumber)`；`peakModel`；最佳标识目标 `IIdentificationTarget.getIdentificationTarget(peak)` → libraryInformation/comparisonResult；内标、定量、定量引用各取**第 0 个**；越界/缺失→null/空串。占位符函数对 null 一律返回 `""`

### 6.4 Writer 主流程（ExcelTemplateReportWriter.generate() L166-228）✅

1. 打开**模板文件**（`reportSettings.getTemplate()`，XSSFWorkbook），取**第 0 个 sheet** 为模板 sheet
2. `append` 且目标文件已有内容 → 复用目标 workbook；否则先写一个空 workbook
3. 在目标 workbook `createSheet()` + `SheetCopySupport.copy(sheetTemplate, sheetTarget)`：逐列拷列宽、逐行逐格拷值（STRING/NUMERIC/BOOLEAN/FORMULA/BLANK）+ `createCellStyle().cloneStyleFrom()` 复制样式（SheetCopySupport.java）✅
4. `getPlaceholderRow()`：找第一个含「以 `{` 开头且以 `}` 结尾」字符串单元格的行 = **占位符行（数据模板行）** ✅
5. 每色谱：从 `placeholderRow + 1` 开始逐峰 `createRow`；每列复制模板单元格样式；按类型处理：STRING → 跑 `populatePlaceholders()`（逐个 PlaceholderProcessor 替换）；NUMERIC/BOOLEAN → 原值；FORMULA → 在公式字符串上替换占位符后 `setCellFormula`；BLANK → blank（L409-461）✅
6. 填完删占位符行（`shiftRows(+1, last, -1)`，L472-478）✅
7. **多色谱**：第一个色谱用第一张拷贝 sheet，后续每个色谱新建 sheet 再拷贝模板（L377-395）✅
8. `recalculate()`（L480-503）：把纯数字字符串单元格转数值、以 `=` 开头的字符串转公式，再 `FormulaEvaluator.evaluateAll()` 全量求值 ✅

### 6.5 模板文件来源与生成

- 设置字段：`ChromatogramReportSettings.template`（File，`@JsonProperty(defaultValue=".xltx")`）✅
- 偏好：`PreferenceSupplier.P_TEMPLATE = "excelReportTemplateFile"`，默认值字符串 `".xltx"`（即默认指向当前工作目录相对路径 .xltx；用户通过设置页选择真实 .xltx/.xlsx 文件）✅
- 模板制作：PreferencePage「Create Excel Template」按钮 → `generateTemplate()` 生成含全部占位符的两行模板（第 1 行 header key、第 2 行 `{key}`），可设为默认 ✅

---

## 7. PDF 报告（✅ 壳源码确认 / 细节库行为 ⚠️）

文件：`pdf.ui/.../{core/ChromatogramReportGenerator, generator/ChromatogramReportWriter, generator/MethodSettings, swt/ChartExportRunnable}.java`

### 7.1 声明（pdf.ui/plugin.xml）✅
- `ChromatogramReportSupplier`：`.pdf`，`fileName="Results"`，`reportName="Chromatogram Results (*.pdf)"`，generator=pdf.ui.core.ChromatogramReportGenerator，settings=pdf.ui.settings.ChromatogramReportSettings
- 附加 `org.eclipse.chemclipse.xxd.process.ui.menu.icon` 图标扩展

### 7.2 生成流程（ChromatogramReportWriter.generate() L98-111）✅
- 库：**PDFBox**（PDDocument + PDPageContentStream + 自定义 `org.eclipse.chemclipse.pdfbox.extensions` 表格/文本封装）
- 每个色谱（及引用色谱）打印一组页面；`document.save(file)` —— **`append` 参数被接收但未真正利用**（始终重建文档）✅
- 页面构成（`print()` L139-176）：
  1. **Header Table**：`chromatogram.getHeaderDataMap()` 的 Name/Value 对；值为 JSON 时用 Jackson 解析进 `MethodSettings`（`@JsonAnySetter` 收集键值）展开成多行（L393-430）✅
  2. **图表页**：`ChartExportRunnable` 在 SWT 事件线程 `syncExec` 离屏构建 `ChromatogramChart`（1080×190px 比例），把 RT 范围按 `numberImagesPerPage`（默认 5）切成 N 段，逐段 `chromatogramChart.setRange()` + `PDFExportHandler.execute(..., A4_LANDSCAPE)` 导出为**临时 PDF 文件**，再把每页 `addPage` 并入主文档（L178-197, L190-197）——即**图表由 SWTChart 矢量导出，不是位图** ✅
  3. **Peak Table**：ID/RT/Area%/Identification 四列，best identification 按保留指数选（`IIdentificationTarget.getLibraryInformation(targets, retentionIndex)`）✅
  4. **Scan Table**：已识别扫描的 ID/RT/Scan#/Identification ✅
  5. **Quantitation Table**：每峰每定量条目 #/Identification/Substance/RT/Area/Conc./Unit，横排 landscape ✅
- 分页：表按 `MAX_ROWS_PORTRAIT=36` / `MAX_ROWS_LANDSCAPE=20` 分页，页脚「Page x/y」，每页打品牌页眉（`icons/logo.pdf` 合入 + OPENChrom 字标，L118-137）✅
- 文本规范化：`normalizeText()` 用 NFD 归一化、非 Basic Latin 字符替换为 `?`（L334-337）✅

### 7.3 PDF 设置
- `ChromatogramReportSettings`：仅 `numberImagesPerPage`（int，默认 5，`@JsonPropertyDescription` 说明 0 关闭分图）✅
- `PreferenceSupplier.P_NUMBER_IMAGE_PAGES = "numberImagePages"`，默认 5 ✅
- `PreferenceSupplier.getReportSettings()` 有 `// TODO`，未填字段（pdf.ui/preferences/PreferenceSupplier.java:51-56）✅

---

## 8. 设置对象字段清单（ChromatogramReportSettings，✅ 各插件字段）

三个插件设置类都 `extends DefaultChromatogramReportSettings`（ChemClipse 核心类，源码已确认，见 §1.3④）；`DefaultChromatogramReportSettings extends AbstractChromatogramReportSettings` 自带三个公共字段：**exportFolder（File）/ append（boolean）/ filenamePattern（String）**，即所有报告插件共享的「输出目录 + 追加 + 文件名模板」基座 ✅

**CSV（csv/settings/ChromatogramReportSettings.java）✅**
| 字段 | 默认 | 说明 |
|---|---|---|
| printResultsHeader | true | 打印表头 |
| appendResultsHeader | true | 追加时是否重复表头 |
| printSectionSeparator | true | 每色谱块后空行 |
| delimiter | Delimiter.COMMA | 分隔符枚举（**COMMA/SEMICOLON/TAB，`model/settings/Delimiter.java`：`getCharacter()` 返回 `','`/`';'`/`'\t'`**，源码确认）|
| reportColumns | null（空→全列） | `ReportColumns`（列勾选）|
| reportReferencedChromatograms | false | 是否报告引用色谱 |

- `getFormat()` = `CSVFormat.RFC4180.builder().setDelimiter(delimiter.getCharacter())` ✅

**Excel 模板（excel.template/settings）✅**：仅 `template`（File，默认 `".xltx"`）

**PDF（pdf.ui/settings）✅**：仅 `numberImagesPerPage`（默认 5）

**对比 — templates 插件（抽样，`net.openchrom.xxd.process.supplier.templates`）✅**
- 有**独立**的 `model/ReportColumns`（不同字段集：Peak Name、CAS#、Start/Stop Time Setting、**跨色谱统计列** Min/Mean/Median/Max RT·RI·S/N·面积、Sum/Min/Max/Mean/Median/StDev Area、Area [%]、Concentrations、按 trace 动态生成 `Min Area Trace (32)` 等）
- 设置类增加：printHeader、traces（TraceValidator 校验）、headerField、printSummary、`reportSettings`（`ReportSettings<ReportSetting>`：name+CAS+position+strategy）、formatConcentration、openReportAfterProcessing
- 其 `core/ReportWriter` 用 **PrintWriter + TAB 分隔**（`DELIMITER = "\t"`），按 ReportSetting 将峰分组、跨色谱聚合、可打印 Summary 区块 → 属于「按化合物设定出汇总报告」模型，与 CSV 逐峰明细报告互补
- 结论：**ReportColumns 是「每个报告模型自定」的列清单模式，各插件字段集不同**；templates 报告器本仓库无完整读取（抽样）⚠️

---

## 9. 数据流位置

```text
IChromatogram + 峰列表（含积分/定量/标识结果）
   ↓
用户选择报告供应商 + 设置（CSV 列勾选 / Excel 模板文件 / PDF 分图数）
   ↓
reportGenerator.generate(file, append, chromatograms, settings)
   ↓
Writer：
  CSV    → if 链逐列取值 → CSVPrinter(RFC4180, Delimiter) → FileWriter(append)
  Excel  → 拷贝模板 sheet → {占位符} 替换 → XSSFWorkbook（每色谱一张 sheet）
  PDF    → PDFBox 表 + SWTChart 离屏导出 PDF 页合入
```

---

## 10. Qt/C++ 移植要点（⚠️ 设计笔记，对应 Qt 工程 `report` 模块）

- **报告 = 数据视图，不是硬编码**：`enum class ReportColumn` 直接由 ReportColumns 的 60 常量 + Excel 的 ~55 占位符 key 合并定义，作为峰表/报告/导出的公共枚举。
- **QAbstractTableModel 承载峰表**：报告 = 把 model 导出到 `QTextStream`（CSV，RFC4180 引号规则自实现：含分隔符/引号/换行时加双引号并转义）/ `QXlsx`（Excel）/ `QtPDF`。
- **绑定方式取舍**：Java 用 if 链逐列取值（可维护性差）；Qt 建议用**列枚举 → std::function 的映射表**（`QHash<ReportColumn, std::function<QVariant(const Peak&)>>`），或 `switch` 集中分派——保持列顺序由用户选择决定。
- **动态列支持**：headerDataMap 的任意 key 可作为列（OpenChrom 特性），Qt 报告模块应支持「自定义列名 → 值回调」注入。
- **空值处理**：可选字段缺失输出空串（不占位），Excel 占位符缺失输出 `""`；注意空串导致的 CSV 行对齐风险。
- **Excel 占位符引擎**：`{key}` 语法（非 `${}`），单元格内全局替换；模板行 = 首行含占位符的行；复制模板 sheet 时克隆列宽与单元格样式；数值字符串与公式回填（`=...`→公式）后全量求值；多色谱 = 每色谱复制一张 sheet；追加写（打开已有文件续写）。
- **追加写 + 多色谱批量报告**：批量分析一次出报告很实用，建议保留 append 语义与「引用色谱」可选。
- **供应商模式**：报告器插件化 → Qt `QPluginLoader`，每插件实现 `IReportGenerator::generate(QIODevice*, const QList<ChromatogramPtr>&, const ReportSettings&)`；菜单/对话框由主程序统一提供（对应 ChemClipse 核心 UI 的向导 + Process 双路径，§5.2/§5.3）。
- **PDF 参考价值有限**：PDF 报告强依赖 SWT 离屏图表（Java 特有事件线程模型）；Qt 端建议用 `QCustomPlot`/`QtCharts` 直接渲染到 `QPainter`/`QPdfWriter`，规避离屏 shell 的复杂性。
- **供应商注册/发现模式**（对应 §1.4）：Java 用 Eclipse 扩展注册表现读现建、不缓存；Qt 用 `QPluginLoader` 扫插件目录 → 每插件 `Q_DECLARE_INTERFACE(IReportGenerator)` 提供 `supplier()` 元数据（id/报告名/扩展名/设置类），注册表对象持有 `QList<IReportSupplier*>` 并提供 `supplierById()` / `supplierIdByName()` 检索；设置类实例化走 `QMetaObject` 工厂（按类型名）。
- **基类/接口拆分**（对应 §1.3①②）：Qt 端 `IReportGenerator` 接口只声明 `generate(...)` 重载 + `validate(QFileInfo)`；基类只实现 `validate()`（null→失败、建目录/空文件、可写检查），`generate()` 族放各插件实现——保持「公共校验下沉、业务上浮」的分层。
- **设置基座**（对应 §1.3④）：Qt 共享基类 `ReportSettingsBase` 含 `exportFolder / append / fileNamePattern`，`fileNamePattern` 支持 `{chromatogram_name}`、`{extension}`、`{current_directory}` 等占位符 → 用 `QDir::fromNativeSeparators` + 正则替换实现；每插件设置类继承之再叠加自有字段。
- **时间因子常量**（对应 §1.3⑥）：直接照抄 `enum TimeFactor { Second=1000, Minute=60000, Hour=3600000 }`（double）；RT 内部一律 ms。
- **Delimiter**（对应 §1.3⑤）：Qt `enum class Delimiter { Comma=',', Semicolon=';', Tab='\t' }`，携带 `toChar()` 与 `label()`，Combo 用 `label` 填充、写文件用 `toChar`。
- **UI 双路径取舍**（对应 §5.2/§5.3）：旧向导（选文件→选供应商→出报告）与新型 Process 方法（settings 对象进入 generate）在 Qt 合并为：报告对话框 = `QWizard`（色谱选择页 + 供应商页），「设置」按钮弹出各插件的 `QWidget` 设置面板（按 settings 类名查插件元数据），点完成时把面板对象作为 `QVariant` 传给 `generate()`——同时满足旧式快捷导出与新式可持久化方法。
- **图标贡献**（对应 §5.3 menu.icon）：Qt 用插件清单声明「图标 + 供应商 id 前缀」，主程序遍历 `QIcon` 缓存时按 `supplierId.startsWith(iconPrefix)` 匹配（复制 Java 的子串匹配语义）。

---

## 11. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| RP-A | 报告扩展点机制 | openchrom/plugins/net.openchrom.chromatogram.xxd.report.supplier.csv/plugin.xml（+ excel.template / pdf.ui plugin.xml）| ✅ |
| RP-B | 报告生成器基类链（单/多色谱、追加、默认设置）| csv/core/ConfigurableReport.java、excel.template/core/ExcelTemplateReport.java、pdf.ui/core/ChromatogramReportGenerator.java（均 extends AbstractChromatogramReportGenerator）| ✅ |
| RP-C | 报告字段全集（60 列）| csv/model/ReportColumns.java | ✅ |
| RP-D | 设置持久化模式（preferences + serializer）| csv/preferences/PreferenceSupplier.java + serializer/* + service/ReportColumnsSerializationService.java | ✅ |
| RP-E | Excel/PDF 报告内部 | net.openchrom...report.supplier.excel.template / pdf.ui | ✅（本次深挖）|
| RP-F | Writer 数据绑定 = if 链逐列取值（非反射/Map/switch）| csv/io/ConfigurableReportWriter.java:110-460 printChromatogramData() | ✅ |
| RP-G | CSV 输出格式（RFC4180+Delimiter、表头/追加、空串、多色谱+引用色谱、空列→全列）| csv/io/ConfigurableReportWriter.java generate()/printHeader()/printChromatogramData()、csv/settings/ChromatogramReportSettings.getFormat() | ✅ |
| RP-H | ReportColumns 序列化 = Jackson 写逗号分隔字符串 | csv/serializer/ReportColumnsSerializer.java + Deserializer + ReportColumns.java load/save | ✅ |
| RP-I | Excel 占位符 = `{key}` 语法，~55 个占位符 | excel.template/io/PlaceholderProcessor.java + ExcelTemplateReportWriter.createPlaceholderProcessors() | ✅ |
| RP-J | Sheet 复制/占位符行/删行/重算机制 | excel.template/io/SheetCopySupport.java、ExcelTemplateReportWriter.getPlaceholderRow()/printPeaks()/deletePlaceholderRow()/recalculate() | ✅ |
| RP-K | PDF = PDFBox 表格 + SWTChart 离屏矢量导出 PDF 页 | pdf.ui/generator/ChromatogramReportWriter.java + swt/ChartExportRunnable.java | ✅ |
| RP-L | 设置字段清单（CSV/Excel/PDF）| 三个 settings/ChromatogramReportSettings.java + PreferenceSupplier.java | ✅ |
| RP-M | UI 触发链：preference page + menu.icon + IAnnotationWidgetService | csv.ui / excel.template.ui / pdf.ui 的 plugin.xml、ReportColumnEditor、ReportColumnsAnnotationService | ✅ 本插件部分 |
| RP-N | templates 插件为独立报告模型（自定列+汇总统计+TAB 输出）| xxd.process.supplier.templates/{model/ReportColumns, model/ReportSetting, core/ReportWriter, settings/ChromatogramReportSettings} | ✅ 抽样 |
| RP-O | 报告导出向导（exportWizards 扩展 → 选色谱 → 选供应商+输出 → 调 generate 无 settings 变体）| xxd.report.ui/plugin.xml + export/wizards/ChromatogramReportExportWizard.java + ReportSupplierSelectionWizardPage.java + internal/wizards/ChromatogramReportEntriesWizard(Page).java | ✅ |
| RP-P | AbstractChromatogramReportGenerator.validate 逻辑 / DefaultChromatogramReportSettings 字段 / Delimiter 枚举 / IChromatogramOverview 时间因子 | .fetch/chemclipse-src/plugins/org.eclipse.chemclipse.chromatogram.xxd.report/src/.../chromatogram/AbstractChromatogramReportGenerator.java、settings/{AbstractChromatogramReportSettings, DefaultChromatogramReportSettings}.java、org.eclipse.chemclipse.model/src/.../{settings/Delimiter.java, core/IChromatogramOverview.java} | ✅ |
| RP-Q | 基类只有 validate()；generate() 重载族 + getChromatogramList() 声明在 IChromatogramReportGenerator、实现在各供应商子类 | xxd.report/.../chromatogram/{IChromatogramReportGenerator, AbstractChromatogramReportGenerator}.java + openchrom csv/core/ConfigurableReport.java | ✅ |
| RP-R | 扩展点字段映射：id/description/reportName/fileExtension/fileName/reportSettings→IChromatogramReportSupplier；reportGenerator 不进 bean、由注册表 createExecutableExtension 即时实例化 | xxd.report/.../core/{IChromatogramReportSupplier, AbstractChromatogramReportSupplier, ChromatogramReports}.java + schema/*.exsd | ✅ |
| RP-S | 扩展注册表机制：ChromatogramReports 静态读注册表（isValid 过滤非法文件名 → add）；按 id/报告名检索；ChromatogramReportSupport 内存容器 | xxd.report/.../core/ChromatogramReports.java + AbstractChromatogramReportSupport.java + IChromatogramReportSupport.java | ✅ |
| RP-T | 设置基类 AbstractChromatogramReportSettings：exportFolder/append/filenamePattern（默认 {chromatogram_name}{extension}，支持 8+ 变量）；DefaultChromatogramReportSettings.getDefaultFolder=preference reportExportFolder | xxd.report/.../settings/*.java + model/settings/{IProcessSettings, AbstractProcessSettings}.java + report/preferences/PreferenceSupplier.java | ✅ |
| RP-U | Delimiter 枚举：COMMA=','/SEMICOLON=';'/TAB='\t'，getCharacter()/label()/getOptions() | org.eclipse.chemclipse.model/src/.../model/settings/Delimiter.java | ✅ |
| RP-V | IChromatogramOverview 时间因子精确值：SECOND=1000.0d、MINUTE=60000.0d、HOUR=3600000.0d | org.eclipse.chemclipse.model/src/.../model/core/IChromatogramOverview.java:25-27 | ✅ |
| RP-W | 现代 Process 路径：ChromatogramReportsProcessSupplier（OSGi IProcessTypeSupplier）把每个供应商包装为 ChromatogramSelectionProcessSupplier，apply() 校验导出目录/变量替换/validateFileName 后把 settings 对象传入 generate | xxd.report/.../core/ChromatogramReportsProcessSupplier.java + OSGI-INF/*.xml | ✅ |
| RP-X | menu.icon 扩展点消费链：xxd.process.ui 定义扩展点 + IMenuIcon；ProcessorSupport.getMenuIcon 按供应商 id 子串匹配、createExecutableExtension("class")；Processor 兜底类别图标 | xxd.process.ui/plugin.xml + menu/IMenuIcon.java + support/ProcessorSupport.java + toolbar/Processor.java + 供应商 ui/plugin.xml | ✅ |
| RP-Y | 导出向导 performFinish：多色谱多供应商循环，目录→每色谱一名、文件→追加，ChromatogramConverterMSD 加载色谱 | xxd.report.ui/.../export/wizards/ChromatogramReportExportWizard.java:68-136 | ✅ |

### 遗留待验证（❓）
| # | 问题 |
|---|---|
| 1 | ~~报告菜单项/供应商对话框/设置编辑对话框的完整 UI 代码~~ → 已确认（§5.2/§5.3，RP-O/RP-W/RP-X）|
| 2 | ~~AbstractChromatogramReportGenerator.validate() 具体校验逻辑；DefaultChromatogramReportSettings 公共字段~~ → 已确认（§1.3，RP-P/RP-Q/RP-T）|
| 3 | ~~Delimiter 枚举定义、MINUTE_CORRELATION_FACTOR 精确值~~ → 已确认：COMMA/SEMICOLON/TAB；60000.0d（§1.3⑤⑥，RP-U/RP-V）|
| 4 | 设置对象 JSON 落盘位置与文件名（方法/过程设置存储机制）|——归 **process 模块**（`org.eclipse.chemclipse.processing` 的 ProcessMethod 持久化框架），本模块只确认了 `ReportColumnsSerializationService` 是序列化回调点（§4）|
