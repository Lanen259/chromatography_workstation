# MODULE_08 — Plugin & Extension Point Architecture（插件与扩展点架构层）

> **状态：🟡 分析中（扩展点全集 ✅ 已采集；注册表运行时机制 ⚠️/❓）**
> 回答「插件机制是什么 / 供应商如何被加载与调用」。核心产出 = **37 个扩展点 × 136 个挂接块 × 供应商注册全清单**，这是自研 Qt 插件系统（QPluginLoader）的直接规格参考。
>
> **溯源边界**：本仓库只含 68 个 `net.openchrom.*` 社区插件（L2 层）。扩展点**定义方** = ChemClipse（`org.eclipse.chemclipse.*`，外部二进制，注册表消费逻辑本机无源码）；本仓库 = 扩展点**挂接方**。因此「注册表如何运行时枚举/实例化」标 ⚠️/❓，「插件声明了什么、实现类做什么」为 ✅。
> **数据采集**：`python` 遍历全部 `openchrom/plugins/*/plugin.xml`（ET 解析），2026-08-16 复跑核对。

---

## 1. 一句话结论

OpenChrom 插件层 = **OSGi bundle + plugin.xml 声明 + 极简 Activator**。供应商模式的本质 = 「**在 plugin.xml 里写死一个 id 和几个实现类全限定名，ChemClipse 扩展注册表按 id 查出来 new 出来再调 execute/detect/convert/generate**」。算法插件（无 UI）几乎零启动钩子（`Bundle-ActivationPolicy: lazy`，无 Activator）；UI 插件才带 Activator，且只干一件事——初始化偏好存储。

- 全仓库 **68 个插件**、其中 **58 个有 plugin.xml**、**10 个无 plugin.xml**（纯工具库/文档/扩展，见 §6.4）。
- 共 **37 个不同扩展点**、**136 个 `<extension>` 挂接块**、**~200 个注册元素**。
- 仅 **1 个自建扩展点**：`net.openchrom.installer.pluginDiscovery`（由 `net.openchrom.installer` 定义）。其余 36 个全部消费 ChemClipse/Eclipse 既有点。

---

## 2. 完整扩展点注册表（★ 自研插件系统的规格清单）

### 2.1 统计口径
- **挂接块数** = `<extension point="...">` 块数（一个块可含多个注册元素）
- **注册元素数** = 块内直接子元素数（一个元素 = 一个注册条目，如一个 `<ChromatogramSupplier>` = 一个供应商）
- **挂接插件数** = 去重后的插件数（同一插件可多次挂同一扩展点）
- 数据源：全部 `plugin.xml` 实际解析 ✅

### 2.2 总表（37 个扩展点 × 挂接数 × 代表插件）

**A. ChemClipse 数据/算法域扩展点（24 个）— 自研需复刻的核心插槽**

| 扩展点 ID | 块 | 注册元素 | 插件数 | 代表插件（元素名） |
|---|---|---|---|---|
| `org.eclipse.chemclipse.msd.converter.chromatogramSupplier` | 12 | 19 | 10 | cdf/animl/gaml/mgf/mz5/mzdb/mzmlb/rdx3/ratios/templates `<ChromatogramSupplier>` |
| `org.eclipse.chemclipse.csd.converter.chromatogramSupplier` | 7 | 13 | 6 | animl/arw/cdf/gaml/ratios/templates `<ChromatogramSupplier>` |
| `org.eclipse.chemclipse.wsd.converter.chromatogramSupplier` | 7 | 7 | 7 | abif/animl/arw/axr/cdf/gaml/templates `<ChromatogramSupplier>` |
| `org.eclipse.chemclipse.msd.converter.databaseSupplier` | 5 | 5 | 5 | btmsp/cms/mgf/microbems.pkf/microbenet `<DatabaseSupplier>` |
| `org.eclipse.chemclipse.msd.converter.massSpectrumSupplier` | 2 | 2 | 2 | animl / microbems.muf `<MassSpectrumSupplier>` |
| `org.eclipse.chemclipse.{fsd,nmr,vsd,wsd}.converter.scanSupplier` | 各1 | 各1 | 各1 | 各 `.gaml` `<ScanSupplier>`（fsd=Fluorescence, vsd=Vibrational, wsd=UV/Vis） |
| `org.eclipse.chemclipse.chromatogram.msd.peak.detector.peakDetectorSupplier` | 5 | 6 | 3 | **amdis**（2：AMDIS/ELU）、**templates**（3：detector/transfer）`<PeakDetector>` |
| `org.eclipse.chemclipse.chromatogram.csd.peak.detector.peakDetectorSupplier` | 3 | 4 | 2 | templates（detector/transfer）+ templates.ui（template/direct）`<PeakDetector>` |
| `org.eclipse.chemclipse.chromatogram.wsd.peak.detector.peakDetectorSupplier` | 2 | 3 | 2 | templates + templates.ui `<PeakDetector>` |
| `org.eclipse.chemclipse.chromatogram.msd.identifier.peakIdentifier` | 2 | 3 | 2 | templates + templates.ui `<PeakIdentificationSupplier>` |
| `org.eclipse.chemclipse.chromatogram.csd.identifier.peakIdentifier` | 3 | 4 | 2 | templates + templates.ui `<PeakIdentificationSupplier>` |
| `org.eclipse.chemclipse.chromatogram.wsd.identifier.peakIdentifier` | 2 | 3 | 2 | templates + templates.ui `<PeakIdentificationSupplier>` |
| `org.eclipse.chemclipse.chromatogram.xxd.identifier.targetIdentifier` | 4 | 4 | 4 | massbank/cas/foodb/tgsc `<TargetIdentificationSupplier>` |
| `org.eclipse.chemclipse.chromatogram.xxd.identifier.scanIdentifier` | 2 | 2 | 2 | massbank / foodb `<ScanIdentificationSupplier>` |
| `org.eclipse.chemclipse.chromatogram.xxd.identifier.chromatogramIdentifier` | 1 | 3 | 1 | **cdk**（chromatogramIdentifier/molweight/cleaner）`<ChromatogramIdentificationSupplier>` |
| `org.eclipse.chemclipse.chromatogram.msd.classifier.chromatogramClassifierSupplier` | 4 | 4 | 1 | ratios（trace/time/quant/qual）`<ChromatogramClassifierSupplier>` |
| `org.eclipse.chemclipse.chromatogram.xxd.report.chromatogramReportSupplier` | 4 | 4 | 4 | csv / excel.template / pdf.ui / templates `<ChromatogramReportSupplier>` |
| `org.eclipse.chemclipse.chromatogram.xxd.quantifier.peakQuantifierSupplier` | 1 | **4** | 1 | **templates**（4 个定量器）`<PeakQuantifierSupplier>` |
| `org.eclipse.chemclipse.chromatogram.xxd.integrator.peakIntegratorSupplier` | 1 | 1 | 1 | templates `<PeakIntegratorSupplier>` |
| `org.eclipse.chemclipse.chromatogram.filter.chromatogramFilterSupplier` | 1 | 1 | 1 | templates `<ChromatogramFilterSupplier>` |
| `org.eclipse.chemclipse.xxd.process.ui.menu.icon` | 15 | 20 | 7 | 各 `.ui` 插件 `<icon>`（处理流程菜单挂点） |

**B. OpenChrom 自建扩展点（1 个）**

| 扩展点 ID | 块 | 注册元素 | 插件数 | 定义方 | 元素 |
|---|---|---|---|---|---|
| `net.openchrom.installer.pluginDiscovery` | 2 | **103** | 1 | `net.openchrom.installer`（plugin.xml `<extension-point>` + schema/pluginDiscovery.exsd）✅ | `installer.ui` 挂 2 `pluginCategory` + **101 个 `pluginDescriptor`**（商业/厂商格式转换器目录，见 §2.4） |

**C. Eclipse 标准 RCP 扩展点（12 个，消费 Eclipse 框架，非自研重点）**

| 扩展点 ID | 块 | 元素 | 插件数 | 代表用途 |
|---|---|---|---|---|
| `org.eclipse.ui.preferencePages` | 18 | 18 | 18 | 各插件「设置页」（方法/插件参数，`<page>`） |
| `org.eclipse.e4.workbench.model` | 5 | 5 | 5 | e4 工作台 `<fragment>`（菜单/工具栏） |
| `org.eclipse.equinox.p2.engine.pgp` | 9 | 9 | 1 | feature.branding 的 `<trustedKeys>`（p2 签名密钥） |
| `org.eclipse.core.runtime.adapters` | 4 | 12 | 2 | ratios.ui / templates.ui `<factory>`（适配器） |
| `org.eclipse.ui.newWizards` | 2 | 4 | 2 | massshiftdetector.ui / tracecompare.ui `<wizard>` |
| `org.eclipse.ui.editors` | 2 | 2 | 2 | massshiftdetector.ui / tracecompare.ui `<editor>` |
| `org.eclipse.help.toc` / `help.contexts` | 2+1 | 2+1 | 2+1 | installer.ui / templates.ui 帮助 |
| `org.eclipse.ui.splashHandlers` | 1 | 2 | 1 | rcp.compilation.community.ui（splashHandler + productBinding） |
| `org.eclipse.ui.startup` | 1 | 2 | 1 | installer.ui（FeatureCheck / P2Cleanup） |
| `org.eclipse.core.runtime.products` | 1 | 1 | 1 | rcp.compilation.community.ui（产品定义） |
| `org.eclipse.e4.ui.css.swt.theme` | 1 | 2 | 1 | feature.branding（`<stylesheet>`） |

### 2.3 各类扩展点元素属性 Schema（✅ 全部从实际 plugin.xml 读出）

| 扩展点 | 元素 | 属性全集 |
|---|---|---|
| 色谱转换器（msd/csd/wsd） | `<ChromatogramSupplier>` | `id, filterName, fileExtension, fileName, description, importConverter, exportConverter, importMagicNumberMatcher, importContentMatcher, isImportable, isExportable` |
| 数据库/质谱/扫描转换器 | `<DatabaseSupplier>` / `<MassSpectrumSupplier>` / `<ScanSupplier>` | 同上（DatabaseSupplier 无 importContentMatcher） |
| 峰检测器 | `<PeakDetector>` | `id, peakDetectorName, peakDetector, peakDetectorSettings, description` |
| 峰标识器 | `<PeakIdentificationSupplier>` | `id, identifierName, identifier, identifierSettings, description` |
| 目标/扫描/色谱标识器 | `<TargetIdentificationSupplier>` / `<ScanIdentificationSupplier>` / `<ChromatogramIdentificationSupplier>` | `id, identifierName, targetURL/identifier, identifierSettings, description` |
| 分类器 | `<ChromatogramClassifierSupplier>` | `id, classifierName, classifier, classifierSettings, description` |
| 报告器 | `<ChromatogramReportSupplier>` | `id, reportName, fileName, fileExtension, reportGenerator, reportSettings, description` |
| 定量器 | `<PeakQuantifierSupplier>` | `id, peakQuantifierName, peakQuantifier, peakQuantifierSettings, description` |
| 积分器 | `<PeakIntegratorSupplier>` | `id, integratorName, integrator, integratorSettings, description` |
| 滤波器 | `<ChromatogramFilterSupplier>` | `id, filterName, filter, filterSettings, description` |
| 流程菜单 | `<icon>` | `id, class`（id 关联供应商 id，class=UI 图标菜单类） |

> 统一规律（✅）：**每类供应商 = id + 一个「实现类」属性 + 一个「设置类」属性 + 展示名**。命名属性规则 = `xxx`（实现类）+ `xxxSettings`（设置类）+ `xxxName`（显示名）。

### 2.4 pluginDiscovery 插件目录（⚠️ 附注）
`net.openchrom.installer.ui/plugin.xml` 的 `pluginDiscovery` 挂接携带 **101 个 pluginDescriptor**，即 OpenChrom 的「可安装扩展目录」——绝大多数是 **Lablicate 商业/厂商格式转换器 feature**（EZChrom、Agilent、Thermo、Bruker、Shimadzu、Varian、Waters、ABSciex 等），含 `categoryId/groupId/kind/license/provider/url/icon`。这解释了「社区版缺的仪器格式在哪买」。**说明插件生态 = 社区 68 个 + 商业目录 101 个**。✅ 数据源：installer.ui/plugin.xml。

---

## 3. 转换器供应商解剖（★ 打开文件匹配链路）

### 3.1 声明（cdf 例，✅ 源码确认）
文件：`net.openchrom.msd.converter.supplier.cdf/plugin.xml`

```xml
<extension point="org.eclipse.chemclipse.msd.converter.chromatogramSupplier">
  <ChromatogramSupplier
    id="net.openchrom.msd.converter.supplier.cdf"
    filterName="ANDI/AIA CDF Chromatogram (*.CDF)"
    fileExtension=".CDF"
    importConverter="...converter.ChromatogramImportConverter"          ← 导入类
    exportConverter="...converter.ChromatogramExportConverter"          ← 导出类
    importMagicNumberMatcher="...converter.MagicNumberMatcher"          ← 魔数匹配
    importContentMatcher="...converter.FileContentMatcher"              ← 内容嗅探
    isImportable="true" isExportable="true">
```

**一个转换器 = 4 类实现类**：Import Converter + Export Converter + MagicNumberMatcher + FileContentMatcher。

### 3.2 两阶段匹配（★ 打开文件的「魔数先、内容后」）
两个 matcher 都实现同一接口形态 `checkFileFormat(File) -> boolean`，但成本分级（✅ 源码确认实现，⚠️ 注册表调用顺序为推断）：

| 阶段 | 类 | 检测内容 | 成本 | Source |
|---|---|---|---|---|
| ① MagicNumberMatcher | `cdf/.../MagicNumberMatcher extends AbstractMagicNumberMatcher` | **文件扩展名**（`.cdf` / `.cdfy`，后者=GCxGC-MS） | 极低 | MagicNumberMatcher.java L22-35 |
| ② FileContentMatcher | `{msd,csd,wsd}.converter.supplier.cdf/.../converter/FileContentMatcher extends AbstractFileContentMatcher` | **打开文件嗅探 netCDF 内容判定域**（三域各一个，互补判定，见下） | 高（读文件） | 三插件 FileContentMatcher.java |

> ⚠️ 推断依据：a) plugin.xml 属性声明顺序 `importMagicNumberMatcher` 在 `importContentMatcher` 之前；b) 语义上扩展名判断必须先于昂贵的文件打开。**注册表内是否按此顺序短路（magic 命中即跳过 content）本机无 ChemClipse 源码，标 ⚠️/❓**。
> ✅ 可观测事实（逐 plugin.xml 核实）：**msd / csd / wsd 三个域的 CDF 供应商都同时声明 MagicNumberMatcher + FileContentMatcher**——`.cdf` 扩展名被三域共享，正是靠内容嗅探区分域。三个 FileContentMatcher 是互补判定：
> - **MSD**：netCDF 中存在变量 `mass_values` → 是质谱文件（Source: msd.../FileContentMatcher.java L39-41）✅
> - **CSD**：`separation_experiment_type`=Gas Chromatography 且无 `mass_values` → true；或 `detector_name`="flame ionization" → true；兜底无 `mass_values` 即假定 FID → true（Source: csd.../FileContentMatcher.java L36-58）✅
> - **WSD**：`separation_experiment_type` 含 Liquid Chromatography 且无 `mass_values` → true；或 `detector_name` 含 PDA/DAD/UV-Vis → true；或 `detector_unit` ∈ {mAU, AU, A.U.} → true（Source: wsd.../FileContentMatcher.java L36-60）✅
> 即三域用「质谱变量有无 + 检测器类型/单位」做互斥判定——这是打开文件时区分共享扩展名的真实机制。

### 3.3 导入执行（✅ 源码确认）
`ChromatogramImportConverter extends AbstractChromatogramImportConverter<IChromatogramMSD>`：
`convert(File, monitor)` → `super.validate(file)`（基类校验）→ `SpecificationValidator.validateSpecification`（大小写/规范）→ `new ChromatogramReaderMSD().read(file, monitor)` → 得 `IChromatogramMSD` → `processingInfo.setProcessingResult(chromatogram)`。另有 `convertOverview()` 只读概览（编辑器缩略图用）。✅ Source: ChromatogramImportConverter.java L37-73。

### 3.4 同格式的多域复用 + DS 混合机制
- `cdf` 插件同时被 **msd / csd / wsd** 三个域各自独立注册（各自 plugin.xml），且 amdis 插件直接 `Require-Bundle: net.openchrom.msd.converter.supplier.cdf`（复用其读 CDF 能力）。✅
- cdf 里 **TSD 域** 不走扩展点而走 **OSGi Declarative Service**：`OSGI-INF/...ChromatogramImportConverterTSD.xml` 声明 `<provide interface="IConverterServiceTSD"/>`。✅ —— **同一插件混合使用「扩展点 + DS」两种注册机制**。

---

## 4. 聚合型插件：templates（★ 一个插件挂 8 类扩展点）

文件：`net.openchrom.xxd.process.supplier.templates/plugin.xml`（唯一一个算法层聚合插件）✅ 全文已读

### 4.1 注册映射表（base 插件：8 类扩展点 / 32 个注册元素）

| 扩展点 | 注册 id | 实现类 | 方法入口 |
|---|---|---|---|
| msd/csd/wsd `peakDetector` | `peaks.detector.{msd,csd,wsd}` + `peaks.transfer.{msd,csd}` | `peaks.PeakDetector`（**一个类实现 IPeakDetectorMSD+CSD+WSD 三接口**）/ `peaks.PeakTransfer` | `detect(selection, settings, monitor)` ✅ |
| msd/csd/wsd `peakIdentifier` | `peaks.identifier.{msd,csd,wsd}` | `peaks.PeakIdentifier{MSD,CSD,WSD}` | ✅ |
| xxd `peakQuantifier` | `peaks.standards.assigner` / `.referencer` / `peaks.compensation.quantifier` / `peaks.standards.extractor`（**1 个块 4 元素**） | `StandardsAssigner` / `StandardsReferencer` / `CompensationQuantifier` / `StandardsExtractor` | `quantify(List<IPeak>, settings, monitor)` ✅ |
| xxd `peakIntegrator` | `templates.peakIntegrator` | `peaks.TemplateIntegrator` | ✅ |
| msd/csd `chromatogramSupplier` | `export.{detector,identifier,standardsAssigner,standardsReferencer,report,review}Template{MSD,CSD}`（模板文件导出，各 6 个）+ `export.chromatogram.namedtraces`(msd) / `.timeranges`(msd/csd/wsd) | `io.DetectorExport`/`IdentifierExport`/`StandardsExport`/`ReferencerExport`/`ReportExport`/`ReviewExport`、`chromatogram.ChromatogramExportNamedTraces`、`ChromatogramExportTimeRanges` | 全 `isExportable=true, isImportable=false`（**纯导出模板**） |
| xxd `chromatogramReportSupplier` | `...templateChromatogramReport` | `chromatogram.ChromatogramReport` | ✅ |
| filter `chromatogramFilterSupplier` | `chromatogram.retentionIndexMapper` | `chromatogram.ChromatogramFilterRetentionIndexMapper` | ✅ |

### 4.2 关键设计
1. **同一实现类多域复用**：`PeakDetector` 一个类实现了 MSD/CSD/WSD 三个域的 `IPeakDetector*` 接口，用三个 id 注册三次——**域差异只体现在扩展点 id，不体现在实现类**。✅
2. **转换器扩展点不止用于数据文件**：templates/ratios 用 `chromatogramSupplier` 扩展点注册「**模板/方法文件导出器**」（.pdt/.pit/.ist/.irt/.prt/.ntr/.tra/.qar/.tir/.trr）——扩展点被复用为通用文件 IO 插槽。
3. **templates.ui 是对外 UI 壳**：另注册 5 个 detector + 6 个 identifier 的 UI 变体（`*.ui.{msd,csd,wsd}.{template,direct}`）+ 9 个 `menu.icon` + preferencePages + toc。✅ templates.ui/plugin.xml
4. ⚠️ **观察到上游 copy-paste 缺陷**：csd `peakIdentifier` 块里第二个元素 id 为 `...identifier.wsd`、实现 `PeakIdentifierWSD`，却挂在 csd 扩展点下（plugin.xml L54-63）。标注仅供参考——说明扩展点机制本身对此无防御。

---

## 5. 供应商生命周期（声明 → 加载 → 检索 → 执行）

```text
① 声明：plugin.xml <extension point="EP"> <XxxSupplier id=... xxx=实现类 xxxSettings=设置类/>
        （OSGi 启动时 Equinox 解析 plugin.xml，注册到 IExtensionRegistry）   ⚠️ 本机无 ChemClipse 消费方源码
② 加载：Bundle-ActivationPolicy: lazy → 首次被引时才激活类加载器         ✅ MANIFEST 确认
③ 检索：UI/引擎按 id 从注册表查 IConfigurationElement → 读属性字符串       ⚠️/❓ 注册表机制待验证
④ 实例化：Class.forName(实现类全限定名).newInstance()                     ⚠️ 推断（OSGi 惯例）
⑤ 执行：调 detect()/convert()/quantify()/generate()/filter()             ✅ 实现类侧源码确认
```

✅ **实现类侧已确认的执行入口**（这就是「按 id 查出来调 execute」的直接证据）：
- 峰检测：`PeakDetector.detect(IChromatogramSelection*, IPeakDetectorSettings*, IProgressMonitor)` — templates/peaks/PeakDetector.java
- AMDIS 检测：`PeakDetectorAMDIS.detect(...)` → `new AmdisIdentifier().calculateAndSetDeconvolutedPeaks(...)`（调外部 AMDIS）— amdis/core/PeakDetectorAMDIS.java L27-50
- 转换：`ChromatogramImportConverter.convert(File, monitor)` — cdf/converter/ChromatogramImportConverter.java
- 定量：`StandardsAssigner.quantify(List<IPeak>, IPeakQuantifierSettings, IProgressMonitor)` — templates/peaks/StandardsAssigner.java L41
- 报告：`ConfigurableReport.generate(...)`（extends AbstractChromatogramReportGenerator）— csv/core/ConfigurableReport.java（见 MODULE_06）

所有执行入口统一携带 **settings + IProgressMonitor**，返回 **IProcessingInfo**（含消息/结果/错误）——跨域一致的算法契约。✅

---

## 6. MANIFEST.MF 依赖网络（✅ 抽样 5 插件全文读取）

### 6.1 抽样清单
| 插件 | Require-Bundle 特征 | Bundle-Activator |
|---|---|---|
| msd.converter.supplier.cdf | 10 个 chemclipse（converter/model/tsd.converter/tsd.model…）+ 5 第三方（NetCDF wrapped edu.ucar / guava / re2j / protobuf / jdom2） | 无 |
| xxd.process.supplier.templates | **41 个 bundle**：csd/msd/wsd model + 8 个算法 API + **10 个 ChemClipse 算法供应商**（firstderivative、savitzkygolay、baselinesubtract、zeroset、scan、snip、trapezoid、xpass…）+ jackson + commons-math3 | 无 |
| chromatogram.xxd.report.supplier.csv | report API + jackson + commons-csv | 无 |
| msd.peak.detector.supplier.amdis | **依赖社区插件 cdf**（跨插件依赖）+ ChemClipse detector API | 无 |
| net.openchrom.installer | Eclipse UI 全家桶（ui/forms/intro/e4.workbench）+ chemclipse.rcp.ui.icons | 无 |

> ✅ 规律：**算法插件只依赖 ChemClipse API + 第三方，不依赖其他社区插件**（amdis→cdf 是唯一反例）。`Bundle-SymbolicName ...;singleton:=true`、`Bundle-Version: 1.6.27.qualifier`、`Bundle-RequiredExecutionEnvironment: JavaSE-21` 全插件一致。

### 6.2 Bundle-Activator（✅ 全仓库统计）
- **仅 20 个 `.ui` 插件**有 Activator（`Bundle-Activator: net.openchrom.xxx.ui.Activator`）；纯算法插件一律无。
- Activator 形态统一：`extends AbstractActivatorUI`，`start()` 里只做 `initializePreferenceStore(PreferenceSupplier.INSTANCE())`，即**把插件默认设置注册进 Eclipse preferences**。✅ Source: msd.converter.supplier.cdf.ui/Activator.java

### 6.3 Declarative Services（✅ 存在，但非主机制）
- **12 个插件**的 MANIFEST 含 `Service-Component: OSGI-INF/*.xml`（组件描述文件）。
- 用途两类：a) 把「服务型」算法注册为 OSGi 服务（cdf 的 `ChromatogramImportConverterTSD → IConverterServiceTSD`）；b) 序列化服务（templates/csv 的 `*SerializationService`）。
- 结论：**OpenChrom 以扩展点为主、DS 为辅**；两者都提供「按 id/接口查找实现」的能力。✅

### 6.4 无 plugin.xml 的 10 个插件（✅）
`xxd.base`、`xxd.base.ui`、`xxd.converter.supplier.animl`、`xxd.converter.supplier.gaml`、`msd.converter.supplier.cms.documentation`、`msd.process.supplier.cms.documentation`、`msd.extensions.cdk`、`msd.process.supplier.cms`、`chromatogram.msd.processor.supplier.massshiftdetector`、`xxd.processor.supplier.tracecompare` —— 纯库/文档插件（`xxd.converter.supplier.animl` 是 msd/csd/wsd animl 三插件的共享库）。**注意：massshiftdetector 与 tracecompare 的算法本体无扩展点，只由各自 .ui 插件以 editor/wizard 形式暴露**。

---

## 7. 任务书 5 问速答

| # | 问题 | 答案 | 状态 |
|---|---|---|---|
| 1 | 完整扩展点注册表 | 37 EP / 136 块 / ~200 元素；24 ChemClipse + 1 OpenChrom 自建（pluginDiscovery）+ 12 Eclipse 标准。见 §2.2 | ✅ |
| 2 | 转换器供应商解剖 | 元素含 id/importConverter/exportConverter/fileExtension/filterName/isImportable/isExportable/importMagicNumberMatcher/importContentMatcher；打开文件按「魔数(扩展名)→内容嗅探」两阶段匹配，**msd/csd/wsd 三域都各有一对 matcher**（`.cdf` 扩展名共享，靠 netCDF 内容互补判定域：MSD=有 mass_values；CSD=GC/火焰离子化；WSD=LC/PDA·DAD·UV-Vis/mAU） | ✅ 声明/实现，⚠️ 注册表顺序 |
| 3 | 聚合型插件 | templates 一个插件 8 类 EP/32 元素；核心是「一个实现类多域复用 + 转换器扩展点兼作模板导出器」 | ✅ |
| 4 | 供应商生命周期 | 声明→lazy 加载→注册表按 id 检索→实例化→调 detect/convert/quantify/generate；执行入口实现侧全部确认，注册表本体 ⚠️/❓ | 部分 ✅ |
| 5 | Qt 移植要点 | 见 §8 | — |

---

## 8. Qt/C++ 移植要点（⚠️ 设计笔记）

1. **扩展点 = JSON 元数据**：每个插件 .so 旁挂一个 `.json`（或嵌入 Q_PLUGIN_METADATA），用「扩展点 id → 注册元素数组」表达 plugin.xml：
   ```json
   { "extensionPoints": {
       "msd.converter.chromatogramSupplier": [
         { "id":"my.converter.cdf", "filterName":"ANDI/AIA CDF (*.CDF)",
           "fileExtension":".CDF",
           "importConverter":"my::converter::CdfImportConverter",
           "magicNumberMatcher":"my::converter::CdfMagicNumberMatcher",
           "contentMatcher":"my::converter::CdfContentMatcher" } ] } }
   ```
2. **插件注册表 = id → 工厂 Map**：`QPluginLoader` 加载插件 → 读元数据 → 把每个注册元素的 id 与工厂函数入 `QHash<QString, PluginFactory>`（按扩展点分层：`QHash<ExtensionPoint, QHash<Id, Factory>>`）。**一个插件可注册多类扩展点**（templates 模式 → 一个 .so 暴露多张表）。
3. **两段式匹配器链**：`IMatcher { bool match(file); int cost; }`；打开文件时先跑「魔数/扩展名」低成本匹配器集合，全不中再跑「内容嗅探」高成本集合——复刻 MSD/CSD/WSD 共享扩展名的区分需求（例如 CDF 文件需内容级嗅探判域）。
4. **设置类与算法类分离**：每个供应商 = `算法实现 + Settings 结构 + 工厂` 三件套；Settings 用 QVariantMap/序列化承载，对应 Java 的 `*Settings` 属性。
5. **域前缀命名**：扩展点 id 按 `msd/csd/wsd/xxd` 分域（同 Qt 命名空间 `Msd::IPeakDetector` / `Csd::IPeakDetector`），保留「一个实现多域注册」的能力（模板峰检测器在 3 域各注册一次）。
6. **算法插件 vs UI 插件分离**：算法插件（Qt core lib，无 UI 依赖）与 UI 插件（QWidget + menu 贡献）分两个模块，对应 OpenChrom 的 base/.ui 双插件。UI 贡献（菜单/设置页/报告图标）也走元数据声明。
7. **进程内 vs 进程外**：OpenChrom 是同 JVM 反射实例化；Qt 可用 `QPluginLoader` 静态/动态插件，复杂格式转换器（NetCDF 级依赖）建议独立进程或预编译库隔离。
8. **CMake 插件化**：主程序（`app`）+ 插件目录约定；每个插件一个 `add_library(MODULE)` + `install` 到插件目录；主程序启动时扫描目录 `QDir::entryList("*.so")`。与既有 Qt 工程 CMake 结构对应（`app`/插件目录）。

---

## 9. 待回填清单（❓）

| # | 问题 | 归属 |
|---|---|---|
| EP1 | ChemClipse 扩展注册表运行时：`getExtension` 枚举/实例化的确切代码（`ChromatogramConverterSupport` 等消费者类） | 需 ChemClipse 源码 |
| EP2 | 打开文件时 match 顺序是否 Magic→Content 短路（短路径遍历） | 同上 |
| EP3 | `menu.icon` 的 `<icon class>` 如何被流程链 UI 消费（`ITemplateMenuIcon` 接口） | 同上 |
| EP4 | DS 组件（12 个）在运行时的实际消费方（除 TSD 外） | 同上 |
| EP5 | `PluginDiscovery` 的 `pluginDescriptor.url` 指向（p2 repo？）与安装流程 | installer 源码可进一步追 |
| EP6 | 供应商设置类与 UI 设置页的绑定方式（`*Settings` ↔ preferencePages） | 部分可本仓库追 |
| EP7 | 各供应商 id 与 `menu.icon` id 的对应关系是否即「方法链菜单」映射依据 | 待追 process.ui |

---

## 10. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| EP-A | 37 EP / 136 块 / ~200 元素全量统计 | 全部 `openchrom/plugins/*/plugin.xml`（ET 解析复跑） | ✅ |
| EP-B | 唯一自建 EP `pluginDiscovery` + schema | net.openchrom.installer/plugin.xml + schema/pluginDiscovery.exsd | ✅ |
| EP-C | 转换器元素属性全集 + cdf 4 类实现类 | net.openchrom.msd.converter.supplier.cdf/plugin.xml L4-18 | ✅ |
| EP-D | 魔数/内容两阶段 matcher 实现（msd/csd/wsd 三域各一对） | cdf/converter/MagicNumberMatcher.java + {msd,csd,wsd}.cdf/.../FileContentMatcher.java | ✅ 实现；⚠️ 顺序 |
| EP-E | 导入转换执行链 | cdf/converter/ChromatogramImportConverter.java convert() L37-73 | ✅ |
| EP-F | templates 聚合 8 类 EP / 32 元素 | net.openchrom.xxd.process.supplier.templates/plugin.xml（全文） | ✅ |
| EP-G | 单类多域复用（PeakDetector 三接口） | templates/peaks/PeakDetector.java L49-69 | ✅ |
| EP-H | 定量执行入口 | templates/peaks/StandardsAssigner.java quantify() L41 | ✅ |
| EP-I | AMDIS 调外部算法 | amdis/core/PeakDetectorAMDIS.java L27-50 → AmdisIdentifier.calculateAndSetDeconvolutedPeaks() | ✅ |
| EP-J | Activator 仅 .ui 插件、只做偏好初始化 | 20 个 .ui MANIFEST.MF + cdf.ui/Activator.java | ✅ |
| EP-K | DS 组件 12 个；cdf TSD 走 DS | 各 MANIFEST Service-Component + cdf OSGI-INF/*.xml | ✅ |
| EP-L | 依赖网络抽样（cdf/templates/csv/amdis/installer） | 5 个 META-INF/MANIFEST.MF 全文 | ✅ |
| EP-M | 无 plugin.xml 的 10 个插件 | 目录遍历 | ✅ |
| EP-N | 注册表运行时枚举/实例化 | ChemClipse（外部） | ⚠️/❓ |
| EP-O | 匹配器短路调用顺序 | ChemClipse（外部） | ⚠️/❓ |

---

*本文档受 `docs/reverse-engineering/README.md` 溯源纪律约束；与 08_plugin_architecture.md（框架版）、11_workstation_composition.md §4（已回填，本文为其逐 plugin.xml 复核版）互链。*
