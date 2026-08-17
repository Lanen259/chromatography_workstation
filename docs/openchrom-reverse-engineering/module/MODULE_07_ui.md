# MODULE_07 — UI 架构层（UI Architecture Layer）

> **状态：🟢 已解析（壳 / menu.icon / preferencePages / SettingsUIProvider 插槽 ✅；ChemClipse UI 消费侧 ✅ 源码确认 2026-08-17）**
> 回答「OpenChrom 的界面是怎么搭出来的」。关键现实：**社区版 UI 层很薄**——本仓库只提供「产品/启动/关于/安装器/设置页/方法链菜单图标」这些壳与插槽；真正的 chromatogram view / 方法编辑器 / 透视图在 ChemClipse `org.eclipse.chemclipse.ux.extension.*`（现源码已就位于 `.fetch/chemclipse-src`，本次已确认核心链路，见 §10–§16）。

---

## 0. 分层总览（✅ 源码确认 + ❓ 外部）

```text
┌─ 社区版 RCP 壳（本仓库 openchrom/plugins，✅ 有源码）
│   net.openchrom.rcp.compilation.community.ui   ← product / splash / about
│   net.openchrom.installer(.ui)                 ← pluginDiscovery 扩展点 + 安装向导
│   net.openchrom.feature.branding               ← 品牌样式 / PGP 信任 / e4 fragment
│   net.openchrom.xxd.base.ui                    ← 交互服务接口（OSGi Service API）
│   各 *.ui 插件                                 ← menu.icon 图标 + preferencePages 设置页
│
├─ ChemClipse RCP 应用层（org.eclipse.chemclipse.rcp.app(.ui)，❓）
│   Application.java / Application.e4xmi / 工作台窗口 / 透视图 addon
│
├─ ChemClipse UI 扩展层（org.eclipse.chemclipse.ux.extension.*，❓）
│   ux.extension.ui        ← 方法编辑器 MethodEditorSupport/ProcessingWizard/SettingsUIProvider
│   ux.extension.xxd.ui    ← 色谱图视图（ChromatogramChart）等
│   ux.extension.{csd,msd,wsd,pcr}.ui ← 各类视图部分
│
└─ 第三方 UI 库（L4 外部）
    SWTChart（org.eclipse.swtchart + extensions）→ Qt 用 QCustomPlot/Qt Charts 对应

> 本次（2026-08-17）已将 ChemClipse 三层中 ✅ 源码确认的链路回填到 §10–§16：方法编辑器（§10）、设置表单生成（§11）、方法执行链（§12）、工作台 e4 模型（§13）、色谱视图数据绑定（§14）、support.ui 定位（§15）、Qt 移植对比（§16）。
```

---

## 1. 启动链（✅ 产品定义 + ⚠️/❓ 应用本体）

### 1.1 产品（product）定义 — `net.openchrom.rcp.compilation.community.ui/plugin.xml` ✅

```xml
<extension id="product" point="org.eclipse.core.runtime.products">
  <product application="org.eclipse.chemclipse.rcp.app.ui.org.eclipse.chemclipse.rcp.application"
           name="OpenChrom (Hillenkamp)">
     <property name="windowImages" value="icons/logo_16x16..128x128.png"/>
     <property name="aboutText"   value="%aboutPlaceholder"/>
     <property name="aboutImage"  value="icons/about_250x330.png"/>
     <property name="appName"     value="OpenChrom"/>
     <property name="applicationXMI"
               value="platform:/plugin/org.eclipse.chemclipse.rcp.app.ui/Application.e4xmi"/>
  </product>
</extension>
```

**要点：**
- 社区版 RCP 壳贡献的**就是产品定义本身**（id 的 extension id 为 `product`，绑定的 splash 用 `net.openchrom.rcp.compilation.community.ui.product`）。
- **application id 属于 ChemClipse**：`org.eclipse.chemclipse.rcp.app.ui.org.eclipse.chemclipse.rcp.application` ❓——在 ChemClipse 树中对应 `chemclipse/plugins/org.eclipse.chemclipse.rcp.app.ui/src/org/eclipse/chemclipse/rcp/app/ui/Application.java`（❓ 实现细节本机无源码）。
- **工作台模型**指向 `platform:/plugin/org.eclipse.chemclipse.rcp.app.ui/Application.e4xmi` ✅（该文件存在于 ChemClipse 树：`chemclipse/plugins/org.eclipse.chemclipse.rcp.app.ui/Application.e4xmi`，内容 ❓）。
- 品牌名 "OpenChrom (Hillenkamp)" 与窗口图标、about 图片/文本由本仓库贡献。

### 1.2 启动画面（Splash）— ✅ 源码已读

`src/.../splash/EnhancedSplashHandler.java`：

```text
class EnhancedSplashHandler extends BasicSplashHandler
  ├─ init(Shell splash)
  │    ├─ super.init(splash)
  │    ├─ product.getProperty(STARTUP_PROGRESS_RECT)  // 默认 (5,275,445,15)
  │    └─ product.getProperty(STARTUP_MESSAGE_RECT)   // 默认 (7,252,445,20)
  │    → setProgressRect / setMessageRect              // 定位进度条与消息文字
```

- 绑定：`splashHandlerProductBinding` → product `net.openchrom.rcp.compilation.community.ui.product` → splash `...ui.splash.enhancedSplashHandler` ✅
- 配套资源：`splash.bmp` / `splash.svg`（启动画面图）、`icons/about_250x330.png`、`about.mappings`（`0=${buildId} 1=${releaseName}`）✅
- `plugin_customization.ini`：`SHOW_PROGRESS_ON_STARTUP=true`、`SHOW_TRADITIONAL_STYLE_TABS=false`、`SHOW_MEMORY_MONITOR=true` ✅

### 1.3 社区版壳还贡献了什么

| 插件 | 贡献 | 证据 |
|---|---|---|
| net.openchrom.rcp.compilation.community.ui | product、splash、about、窗口图标、plugin_customization | ✅ plugin.xml + src |
| net.openchrom.feature.branding | e4 fragment（fragment.e4xmi，❓ 内容）+ PGP 可信密钥（9 个 .asc）+ 亮/暗主题 stylesheet | ✅ plugin.xml |
| net.openchrom.installer.ui | e4 fragment（Install Add-ons / Check for Updates 命令+handler）+ 2 个 IStartup | ✅ plugin.xml + fragment.e4xmi |
| net.openchrom.xxd.base.ui | OSGi 服务接口集合（见 §6） | ✅ 源码 |

> ❓ 透视图（Perspective）定义、菜单/工具栏布局、状态栏都在 ChemClipse `org.eclipse.chemclipse.ux.extension.*` 的 fragment.e4xmi 里（树中确认 `ux.extension.{ui,xxd,csd,msd,wsd,pcr}.ui/fragment.e4xmi` 存在）。

---

## 2. menu.icon 扩展点：方法链菜单（★「处理方法 UI」的插槽）

- 扩展点属主：`org.eclipse.chemclipse.xxd.process.ui.menu.icon`（ChemClipse `org.eclipse.chemclipse.xxd.process.ui`，❓ 消费方实现）。
- 元素：`<icon class="..." id="..."/>`。`class` 实现 `org.eclipse.chemclipse.xxd.process.ui.menu.IMenuIcon`（`Image getImage()`，✅ 社区类 implements 已证）；`id` = 该处理方法在核心插件里的 **supplier id**（菜单项用它把「图标 ↔ 处理步骤」关联起来）。
- 本仓库 **15 个 `<extension>` 块、共 20 个 icon 声明**（✅ 逐文件读取）：

| 插件 | icon 数 | id（= 方法链步骤） | icon 类 |
|---|---|---|---|
| xxd.process.supplier.templates.ui | 9 | templates.peaks.detector / peaks.identifier / peakIntegrator / openchrom.templateChromatogramReport（TemplateMenuIcon）；peaks.transfer.msd / .csd（TransferMenuIcon）；ui.msd.direct（PeakReviewMenuIcon）；ui.msd（TemplateMenuIcon）；processors.nameReplacer（ReplacerMenuIcon） | 4 个类 ✅ |
| xxd.converter.supplier.animl.ui | 4 | msd.converter.supplier.animl.chromatogram / csd.converter.supplier.animl / wsd.converter.supplier.animl.chromatogram / msd.converter.supplier.animl.spectrum | MenuIcon |
| chromatogram.msd.peak.detector.supplier.amdis.ui | 3 | ...supplier.amdis.elu（FileMenuIcon）/ ...filter.AmbiguousPeakRemoverFilter（AmbiguousPeakRemoverFilterMenuIcon）/ ...supplier.amdis（DeconvolutionMenuIcon） | 3 个类 ✅ |
| chromatogram.xxd.report.supplier.csv.ui | 1 | ...openchrom.chromatogramReportCSV | MenuIcon |
| chromatogram.xxd.report.supplier.excel.template.ui | 1 | ...supplier.excel.template | MenuIcon |
| chromatogram.xxd.report.supplier.pdf.ui | 1 | ...supplier.openchrom.pdf.ui | PortableDocumentFormatMenuIcon |
| xxd.classifier.supplier.ratios.ui | 1 | net.openchrom.xxd.classifier.supplier.ratios.quant | MenuIcon |

> 图标类模式：`TemplateMenuIcon.getImage()` → `Activator.getDefault().getImage(Icon.TEMPLATE)`；`FileMenuIcon` → `ApplicationImageFactory.getInstance().getImage(IApplicationImage.IMAGE_FILE, 16x16)`。全部是**取图标**，不含执行逻辑 ✅。

### 2.1 ★ 真正的方法编辑器在哪（✅ 已回填）

社区版只给每个步骤挂图标。**步骤列表 + 参数表单 + 执行**由 ChemClipse `org.eclipse.chemclipse.ux.extension.ui`（methods/editors/swt 包）+ `org.eclipse.chemclipse.ux.extension.xxd.ui`（色谱图编辑器内的 MethodSupportUI / ProcessorToolbarUI）提供。**注意**：源码树中没有 `org.eclipse.chemclipse.ux.extension.ui.methods` 独立插件——该方法包属于 `org.eclipse.chemclipse.ux.extension.ui` 插件。完整链路见 **§10（方法编辑器）**、**§11（设置表单）**、**§12（执行链）**。

---

## 3. ★ SettingsUIProvider 适配器：方法步骤「参数表单」插槽（✅ 机制确认）

这是社区插件声明**每步参数表单**的方式——通过 Eclipse `org.eclipse.core.runtime.adapters` 扩展，把「settings 对象」适配成 `SettingsUIProvider`：

```xml
<extension point="org.eclipse.core.runtime.adapters">
  <factory adaptableType="...templates.settings.PeakDetectorSettings"
           class="...templates.ui.adapter.PeakDetectorSettingsAdapterFactory">
    <adapter type="org.eclipse.chemclipse.ux.extension.ui.methods.SettingsUIProvider"/>
  </factory>
</extension>
```

适配器工厂（✅ 源码已读 `PeakDetectorSettingsAdapterFactory.java`）：

```text
getAdapter(adaptableObject, adapterType)
  ├─ adaptableObject instanceof PeakDetectorSettings
  └─ adapterType == SettingsUIProvider → (parent, preferences, showProfileToolbar) ->
        preferences.getUserSettings()  → new TemplatePeakListEditor(parent, preferences, settings)
```

**即：方法编辑器拿到一个 settings 对象 → 通过 Eclipse adapter 机制得到其 UI Provider → 调用得到 SWT 编辑器控件。** 社区插件负责把每个 settings 类映射到自己的 SWT 表单：

| 插件 | 适配工厂数 | 覆盖的 settings（= 方法步骤） |
|---|---|---|
| xxd.process.supplier.templates.ui | 7 | PeakDetectorSettings → TemplatePeakListEditor；PeakIdentifierSettings；PeakReviewSettings；PeakIntegrationSettings；StandardsAssignerSettings；StandardsReferencerSettings；CompensationQuantifierSettings |
| xxd.classifier.supplier.ratios.ui | 3 | TraceRatioSettings / TimeRatioSettings / QuantRatioSettings |

对应 SWT 编辑器清单（✅ 目录确认）：`swt/TemplatePeakListEditor.java`、`TemplatePeakIdentifierEditor.java`、`TemplatePeakIntegrationEditor.java`、`TemplateReportEditor.java`、`TemplateReviewEditor.java`、`StandardsAssignerEditor.java`、`StandardsReferencerEditor.java`、`CompensationQuantifierEditor.java`、`ReportColumnEditor.java`、`NameReplacementsEditor.java`；`swt/peaks/` 下另有 `TemplatePeakDetectorUI` / `TemplatePeakReviewUI`（Composite，内部是 `PeakDetectorControl`）等**交互式峰检测/评审控件**（用 SWTChart）。

---

## 4. preferencePages 设置页（✅ 全量统计）

`org.eclipse.ui.preferencePages`：**18 个插件、共 38 个设置页**（templates.ui 19 页 + ratios.ui 4 页 + 其余单页）。按父分类（ChemClipse 的分类页）归类：

| 分类（ChemClipse 父页 category） | 本仓库子页（id ← class） |
|---|---|
| **转换器** `*converterPreferencePage`（csd/msd/converter/wsd 4 类） | csd.cdf.ui（"Common Data Format (*.cdf)"）、msd.cdf.ui（"NetCDF MS Converter"）、cms.converter.ui、animl.ui、wsd.arw.ui |
| **峰检测** `...peak.detector.ui.preferences.peakDetectorPreferencePage` | amdis.ui（%amdisExternal）、templates.ui（PagePeakDetector、PagePeakTransfer） |
| **标识** `...msd.identifier.ui.preferences.identifierPreferencePage` | jmol.ui、massbank.ui（"MassBank"）、xxd.identifier.supplier.cdk.ui（"CDK Identifier Support"）、templates.ui（PagePeakReview ×2、PagePeakReviewMSD、PagePeakReviewCSD） |
| **处理** `...xxd.process.ui.preferences.processPreferencePage` | massshiftdetector.ui、tracecompare.ui、cms.process.ui、templates.ui（"Template Processor"） |
| **报告** `...xxd.report.ui.preferences.reportPreferencePage` | csv.ui（"CSV Chromatogram Report"）、excel.template.ui（"Excel Template Chromatogram Report"）、templates.ui（PageChromatogramReport） |
| **定量** `...xxd.quantitation.ui.preferences.preferencePage` | templates.ui（PageStandardsAssigner/Referencer/CompensationQuantifier/StandardsExtractor） |
| **积分** `...xxd.integrator.ui.preferences.integratorPreferencePage` | templates.ui（PagePeakIntegrator） |
| **分类器** `...msd.classifier.ui.preferences.classifierPreferencePage` | ratios.ui（PreferencePage "Ratio Classifier" + 子页 Trace/Time/Quant Ratios） |
| **常规设置** `...rcp.app.ui.preferences.settingsPreferencePage` | installer.ui（"Plugin Installer"）、msd.extensions.cdk.ui（"CDK MS Tools"） |

> 设置页类统一命名 `preferences/PreferencePage.java`（或 `Page*`），各自通过 `PreferenceSupplier`（`AbstractPreferenceSupplier`，Eclipse preferences API）读写。Qt 对应：`QSettings` + `QTabWidget`（见 §7）。

---

## 5. 报告 UI（✅ plugin.xml + 关键源码）

| 插件 | 贡献的 UI | 说明 |
|---|---|---|
| csv.ui | preferencePage + 1 个 menu.icon；另有 `swt/ReportColumnEditor`、`services/ReportColumnsAnnotationService` | **薄壳**：报告供应商本身在核心插件 `net.openchrom.chromatogram.xxd.report.supplier.csv`（见 MODULE_06） |
| excel.template.ui | preferencePage + 1 个 menu.icon（MenuIcon→IMAGE_EXCEL） | 同样薄壳，供应商在 `...excel.template` |
| pdf.ui | **自含供应商**：`chromatogramReportSupplier` 扩展（fileExtension=.pdf、reportGenerator=`core.ChromatogramReportGenerator`、reportSettings=`settings.ChromatogramReportSettings`）+ preferenceSupplier + 1 个 menu.icon | 亮点：`swt/ChartExportRunnable.java` ✅——用 **SWTChart**（`org.eclipse.chemclipse.ux.extension.xxd.ui.charts.ChromatogramChart` + `org.eclipse.swtchart.extensions.core.BaseChart`）离屏绘制色谱/峰/扫描标注，再经 `org.eclipse.swtchart.export.vector.PDFExportHandler` 分页导出 PDF（A4 横向） |
| tracecompare.ui | **完整 UI 插件**：`org.eclipse.ui.editors`（EditorProcessor，MultiPageEditorPart 两页：TraceComparison/Results）+ `org.eclipse.ui.newWizards`（WizardProcessor "TraceCompare Report"）+ preferencePage + e4 fragment | 社区版少见的「真编辑器」；`.otc` 文件编辑器，JAXB 读写 ProcessorModel（io 在核心插件） |

> 报告 UI 的通用结构 = **一个菜单图标（进入方法链/报告菜单）+ 一个设置页（选列/参数）**；数据导出在核心插件。❓ UI 侧「选列 → 调用 reportGenerator」的完整调用链待回填。

---

## 6. 安装器与插件发现（✅ 机制源码确认）

### 6.1 扩展点定义 — `net.openchrom.installer/plugin.xml`

```xml
<extension-point id="pluginDiscovery" name="Plugin Discovery" schema="schema/pluginDiscovery.exsd"/>
```

元素（由 `PluginDiscoveryExtensionReader` 解析，✅ 源码）：`pluginCategory`（id/name/description/relevance/icon）、`pluginDescriptor`（id=Installable Unit、name、provider、license、description、categoryId、platformFilter、groupId、icon、url、summary、kind=converter|extension|dynamic）。

### 6.2 声明端 — `net.openchrom.installer.ui/plugin.xml` ✅

- 2 个 `pluginCategory`：Proprietary OpenChrom File Format Converters（relevance 100）、OpenChrom Extensions 商业扩展（relevance 50）。
- **80+ 个 `pluginDescriptor`**（约 77 个 converter feature + 12 个商业 extension），全部 provider=Lablicate；converter 的 id 即各家仪器格式 feature（EZChrom、Agilent、Thermo/Finnigan、Waters、Bruker、Shimadzu、ABSciex…），kind=converter。
- 另有一设置页 "Plugin Installer"。

### 6.3 发现与安装机制（✅ 源码）

```text
PluginDiscovery.performDiscovery()            // net.openchrom.installer.model
  ├─ BundleDiscoveryStrategy.performDiscovery() // 读本机扩展注册表：pluginCategory/pluginDescriptor
  │     → PluginDiscoveryExtensionReader 解析 → PluginDescriptor / DiscoveryCategory
  ├─ DSDiscoveryStrategy.performDiscovery()     // 读 OSGi DS 服务 PluginsDS（动态私有扩展，
  │                                             //   插件以 IPluginDescriptor DS 组件注册）
  ├─ filterDescriptors()                        // platformFilter OSGi Filter 匹配
  └─ connectCategoriesToDescriptors()           // categoryId ↔ category

UI 入口：
  FeatureCheck (IStartup, earlyStartup)         // 若一个 converter feature 都没装 + 首选项允许
    → MessageDialogWithToggle 询问
    → PluginDiscoveryWizard + PrepareInstallProfileJob(IPluginInstallJob, p2) 安装
  AddonsInstallHandler (@Execute, "Install Add-ons" 命令)  // e4 fragment 注入应用模型
  CheckForUpdatesHander                          // "Check for Updates"
  P2Cleanup (IStartup)                           // p2 垃圾回收
```

- 首选项：`preferences/PreferenceSupplier.java`（`P_PROPRIETARY_CONVERTERS` 默认 ALWAYS、`P_FILTER_PATH_IMPORT/EXPORT`）。
- 安装底层是 **Eclipse p2**（ProvisioningUI / PrepareInstallProfileJob / IProfileRegistry，✅ import 已证），走 p2 仓库装 feature。

---

## 7. Qt/C++ 移植要点（ui 模块）⚠️ 设计笔记

对应你的 Qt 工程模块：`ui`。

1. **方法编辑器（★ 核心）**：OpenChrom = 方法链（menu.icon 的 20 个 id 即处理步骤清单：检测/标识/积分/定量/报告/分类器）+ 每步参数表单（SettingsUIProvider 适配器）。
   - Qt：`QListWidget`/`QTreeView` 作**步骤列表**（顺序 = 方法链），`QStackedWidget` 作**参数表单容器**；每个步骤 = 一个 `QWidget` 表单类（对应 templates.ui 的 `*Editor`/`TemplatePeakListEditor`）。
   - 步骤注册表：仿扩展点，用 `QPluginLoader` 或注册表 map（stepId → {菜单图标 QIcon, 表单工厂, settings 类型}）替代 menu.icon + adapters 两套机制。
   - 设置页 ↔ 参数表单**共用同一份 settings 结构**：OpenChrom 里 preferencePage 与 SettingsUIProvider 都指向同一 settings 类——Qt 中一个 `struct StepSettings` + 一个表单工厂，设置对话框（QTabWidget）与方法编辑器复用同一工厂。
2. **菜单注册 → Qt QMenu**：menu.icon 每个 id 是一个 QAction（icon + 触发执行该步骤），挂到方法编辑器的"添加步骤"菜单 / 色谱视图右键菜单。
3. **设置页 → QSettings + QTabWidget**：把 18 个插件的 38 个设置页收敛为 QSettings 组织（org=you, app=cds）+ 分组 tab；`PreferenceSupplier` 单例 → `SettingsManager`。
4. **色谱图表视图 → QCustomPlot / Qt Charts**：OpenChrom 的 SWTChart（L4 外部）承载色谱图、峰标注（P1、S1）、多页导出。Qt 用 QCustomPlot 实现 TIC 曲线 + 峰标记 + 缩放平移 + 离屏导出 PNG/PDF；`ChartExportRunnable` 的「离屏 chart → 分页导出」思路可直接照搬（QCustomPlot 的 `toPixmap`/`savePdf`）。
5. **报告 UI**：报告 = 菜单图标 + 设置页 + 生成器，Qt 沿用 MODULE_06 的 `IReportGenerator::generate(...)` 插件接口，UI 仅负责选供应商 + 配置列。
6. **安装器/插件发现**：Qt 无 OSGi/p2。用 `QPluginLoader` 扫描插件目录 + 各插件暴露的 `pluginDescriptor`（Qt 侧 = JSON manifest）替代 pluginDiscovery；「私有动态插件」用 DS 服务 → 运行时插件目录热扫。
7. **启动**：社区版壳只是 product 壳——Qt 对应 `QApplication` + `QSplashScreen`（进度条 + 消息 = EnhancedSplashHandler 的 progress/message rect）+ about 对话框（logo + 版本）。

---

## 10. ChemClipse UI 消费侧：方法编辑器（✅ 源码确认）

源码根：`.fetch/chemclipse-src/plugins/`（下同）。**方法包不在独立插件里，而是 `org.eclipse.chemclipse.ux.extension.ui` 下的 `methods/`、`editors/`、`swt/` 包。**

### 10.1 编辑器装配（ProcessMethodEditor → ExtendedMethodUI）

- `.fetch/chemclipse-src/plugins/org.eclipse.chemclipse.ux.extension.ui/editors/ProcessMethodEditor.java`：e4 `MPart`（ID `...part.processMethodEditor`），`@PostConstruct initialize(parent)` → `Adapters.adapt(processMethodFile, IProcessMethod.class)` 读出方法对象 → 构造 `ExtendedMethodUI(parent, SWT.NONE, processSupplierContext, categories)`，setModificationHandler(this)、setProcessMethod。
- `MethodEditorSupport.java`（`org.eclipse.chemclipse.converter.methods.MethodConverter` 供应商）：`openEditor(file,...)` → 抛给父类 `AbstractSupplierFileEditorSupport.openEditor(...)` 按 `ProcessMethodEditor.ID` 打开编辑器（SWT 文件双击→编辑器）。
- `ExtendedMethodUI.java`：编辑器主 Composite（FillLayout + GridLayout），**自上而下 5 块**：`createToolbarMain`（3 个折叠按钮 + 设置按钮）、`ProcessMethodHeader`（方法头：名称/分类/描述）、`ProcessMethodProfiles`（方法 profile 下拉）、`MethodTreeViewer`（**步骤树**）、`ProcessMethodToolbar`（**底部步骤工具栏**）。每块回调 `updateProcessMethod()`（刷新树 + 更新按钮态）并 `setMethodDirty(true)` 标记 dirty。

### 10.2 步骤列表（读 ProcessMethod → IProcessEntry → 树）

- `swt/MethodTreeViewer.java`：JFace `TreeViewer`（SWT.BORDER|MULTI|FULL_SELECTION）。
  - 列定义在 `internal/provider/MethodListLabelProvider.java`：`TITLES = {""图标, Name, Description, Type, Settings, Id}`（BOUNDS 50/250/250/160/300/110）。
  - **ContentProvider**：`getElements` = `IProcessEntryContainer.getNumberOfEntries()` 遍历 `getEntries()`；`hasChildren/getChildren` 若 `processingSupport.getSupplier(entry.getProcessorId())` 返回的 supplier 实现了 `IProcessEntryContainer`（组合方法），则递归展开子条目——方法可嵌套。
  - 第 0 列图标 = 校验状态（OK/WARN/ERROR/INFO，`MethodListLabelProvider.validate` 会用 `preferencesSupplier.apply(entry, context)` 反序列化 user settings 验证能否解析）；Settings 列直接显示 `preferences.getUserSettingsAsString()` 的 JSON。
- `ProcessMethodToolbar.java`（extends `ToolBar`）持有 `ProcessMethod processMethod`；`updateTableButtons()` 按 `processMethod.isReadOnly()/isFinal()` + 选中项所属容器是否可写（`MethodSupport.getContainer`，要求 `ListProcessEntryContainer` 且非 readOnly）来启停增删改按钮。

### 10.3 添加 / 删除 / 排序 / 复制 / 编辑

- **添加**：`ProcessMethodToolbar.createAddButton`（SWT.DROP_DOWN，点箭头 = 列出用户方法文件/加载文件；点本体 = `ProcessingWizard.open(shell, contextList, dataCategories)`）。
  - `methods/ProcessingWizard.java`：`WizardDialog` + 单页 `ProcessingWizardPage`，返回 `Map<IProcessSupplierContext, IProcessEntry>`。`ProcessingWizardPage` 按 `dataCategories` 过滤出可用的 `IProcessSupplier` 列表供选择。
  - 拿到新 `IProcessEntry` 后：`modifyProcessEntry(...)`（若该 supplier `getSettingsParser().getInputValues()` 非空则弹 `SettingsWizard` 编辑参数）→ `processMethod.addProcessEntry(editedEntry)`（或选中组合条目时 `selectedEntry.addProcessEntry(...)` 加入子列表）。
- **删除**：`deleteSelectedProcessEntries` → 收集 `MethodSupport.getContainer(object)`（获得父 `ListProcessEntryContainer`）→ `container.removeProcessEntry(entry)`；`deleteAllProcessEntries` → `processMethod.removeAllProcessEntries()`。均先 `MessageDialog.openQuestion`。
- **排序**：`moveProcessEntriesUp/Down` → 对 `container.getEntries()` 做 `Collections.swap`（带 offset 防止多选错位）。
- **复制**：`createCopyButton` → `new ProcessEntry(entry, container)` 插入同位置。
- **编辑单步参数**：树双击 → `modifyProcessEntry(shell, entry, IProcessEntry.getContext(entry, processingSupport), true)` → `SettingsWizard.openEditPreferencesWizard`（见 §11）；另有关键 `createModifySettingsButton` 工具栏按钮 + `createModifyDescriptionButton`（InputDialog 改描述）。
- 键盘：DEL 删除、Ctrl+D 全删、Ctrl+C 复制到剪贴板（TSV 文本，`copyToClipboard`）。

### 10.4 保存方法

- `ProcessMethodEditor.save()`（`@Persist`，Ctrl+S）：`new ProcessMethod(extendedMethodUI.getProcessMethod())` → `MethodConverter.convert(file, methodNew, DEFAULT_METHOD_CONVERTER_ID, monitor)`（序列化为 .omth JSON）→ 成功则 `dirtyable.setDirty(false)`、`notifications.updated(...)`、`UpdateNotifierUI.update(TOPIC_METHOD_UPDATE, method)`。
- `saveAs()` → `MethodFileSupport.saveProccessMethod(shell, newMethod)`：`FileDialog`（SAVE）+ `MethodConverterSupport.getSupplier()` 选转换器 → `MethodConverter.convert` 写盘；`dirtyable.setDirty(!saveSuccessful)`、`notifications.created(newMethod)`。
- 打开新方法：`CreateProcessMethodHandler`（xxd.ui fragment 命令 `...command.createProcessMethod`，File 菜单 "New Process Method" + Process 菜单项）→ 建空 `ProcessMethod` 存盘 → `openEditor`。
- dirty 判定：`setDirty(boolean)` → `dirtyable.setDirty(!extendedMethodUI.getProcessMethod().equals(currentProcessMethod))`（深度 equals 比对）。

---

## 11. 设置表单：注解 POJO → SWT 控件（✅ 源码确认）

### 11.1 注解 → InputValue（SettingsClassParser）

- `org.eclipse.chemclipse.support/settings/parser/SettingsClassParser.java`：
  - `getInputValues()` 用 **Jackson** 反射 settings 类：`new ObjectMapper().getSerializationConfig().introspect(javaType).findProperties()` → 每个 `BeanPropertyDefinition` 的 `AnnotatedField` 生成一个 `InputValue`（rawType=字段类型、name=属性名、description=JsonPropertyDescription、defaultValue=JsonProperty defaultValue 或 getter 从默认实例取、contributorURI=`platform:/plugin/<bundle>`）。
  - 逐注解追加行为：`@IntSettingsProperty` → `MinMaxValidator(min,max)` + `EvenOddValidatorInteger`；`@Long/Float/Double/Short/ByteSettingsProperty` 同理；`@StringSettingsProperty` → `RegularExpressionValidator(regExp,...)` + `isMultiLine` + `proposals[]`（内容联想）；`@FileSettingProperty` → 存 `FileSettingProperty`（dialogType=OPEN/SAVE、onlyDirectory、validExtensions）；`@ComboSettingsProperty` → 反射实例化 `ComboSupplier`（`comboSettingsProperty.value()`）；`@LabelProperty` → label/tooltip；`@ValidatorSettingsProperty` → 实例化自定义 `IValidator`。
  - `createDefaultInstance()`：单参数构造器传 `defaultConstructorArgument`（即 supplier 自身），否则无参构造。
- 设置类实例（对照）`org.eclipse.chemclipse.chromatogram.vsd.filter/settings/WavenumberRemoverSettings.java`：

  ```java
  @JsonProperty(value = "Wavenumbers", defaultValue = "200 202")
  @JsonPropertyDescription(value = "List the wavenumbers, separated by a white space.")
  @StringSettingsProperty(regExp = "(\\d+[;|\\s]?)+", description = "must be space separated digits.")
  private String wavenumbers = "200 202";
  @JsonProperty(value = "Mode", defaultValue = "INCLUDE")
  private MarkedTraceModus markMode = MarkedTraceModus.INCLUDE;   // enum → Combo
  ```

### 11.2 SettingsUI 装配（SettingsUIProvider 消费 + 默认生成器）

- `methods/SettingsUIProvider.java`：接口 `createUI(parent, IProcessorPreferences, showProfileToolbar)` 返回 `SettingsUIControl`（`setEnabled/validate/getSettings()/addChangeListener/restoreDefaults/getControl()`）。
- `methods/SettingsUI.java#createSettingsUIProvider`：先 `preferences.getUserSettings()`（无则 `getSettingsParser().createDefaultInstance()`），再 `Adapters.adapt(settings, SettingsUIProvider.class)`——**这就是 §3 社区版 7 个 adapter factory 的消费点**；适配失败则 `DefaultSettingsUIProvider` → `SettingsUIControlImplementation`（注解驱动的默认表单生成器）。
- `SettingsUIControlImplementation`：`preferences.getSerialization().fromObject(inputValues, preferences.getSettings())` → `Map<InputValue, Object>`（JSON→值）；逐项建 `WidgetItem`；2 列 GridLayout（Label + 控件）。
- `methods/WidgetItem.java#createControl` **按 rawType 建控件**：
  - `int/long/float/double/short/byte/String` → `Text`（数字文本 / 多行 `SWT.MULTI` / 带 `ContentProposalAdapter` 联想）；
  - `boolean/Boolean` → `Button(SWT.CHECK)`；
  - `Enum` → `ComboViewer(READ_ONLY)`，标签优先 `ILabel.label()`；
  - `File` → Text + "…" 按钮 → `FileDialog`/`DirectoryDialog`（读 `@FileSettingProperty` 的 dialogType/validExtensions，OPEN 或 SAVE 模式）；
  - 其它类型 → `Activator.getDefault().getAnnotationWidgetServices()`（OSGi DS 跟踪 `IAnnotationWidgetService`，见 `ux.extension.ui/Activator.java` 第 59 行 ServiceTracker）按 `getSupportedClass()==rawType` 匹配自定义控件 `createWidget(parent, description, value)`。
- **读回**：`WidgetItem.getValue()` 按 rawType 解析（`Integer.parseInt`、`Boolean.getBoolean`、enum 名、comboSupplier.asString、File…）→ `SettingsUIControlImplementation.getSettings()` = `Map<InputValue,Object>` → `preferences.getSerialization().toString(values)` → **JSON 字符串**。
- **校验**：`WidgetItem.validate()` 跑 `inputValue.getValidators()` + `InputValidator` → `IStatus`；失败 `ControlDecoration.showHoverText(status.getMessage())`。
- **向导页** `methods/SettingsPreferencesPage.java`：`ScrolledComposite` + 文献区（literatureReferences Combo + DOI 按钮）+ `new SettingsUI<>(parent, preferences, showProfileToolbar)` + "记住决定"复选框 + "恢复默认"。监听 `addChangeListener` → 实时 `validate()` 设置 `setPageComplete` + 取 `getSettings()` 存 `jsonSettings`。
- **写回 settings**：`methods/SettingsWizard.openEditPreferencesWizard`（Finish 时）：`preferences.setAskForSettings(!dontAskAgain)`、`preferences.setUseSystemDefaults(false)`、**`preferences.setUserSettings(settingsPage.getSettingsEdited())`**——JSON 字符串写回 `IProcessorPreferences`（`ProcessSettingsSupport`/`ProcessEntry` 持有）。执行时 `preferences.getUserSettings()` 反序列化回 settings 对象。
- `SettingsWizard.getSettings(shell, preferences, ...)`：按 `DialogBehavior.SHOW` 决定执行前是否强制弹向导（`getSettingsParser().getInputValues()` 非空才弹）。

---

## 12. 运行方法：菜单/工具栏 → 执行链（✅ 源码确认）

### 12.1 menu.icon 的消费方（Processor 图标）

- `org.eclipse.chemclipse.xxd.process.ui/menu/IMenuIcon.java`：`EXTENSION_POINT_ID = "org.eclipse.chemclipse.xxd.process.ui.menu.icon"`，`Image getImage()`。
- `xxd.process.ui/support/ProcessorSupport.java#getMenuIcon(supplier)`：`IExtensionRegistry` 取全部 `menu.icon` 元素，**`if(!processSupplier.getId().contains(id)) continue;`**（id 子串匹配）→ `createExecutableExtension("class")` → `menuIcon.getImage()`。匹配失败回退 `getDefaultIcon`（按 category 映射内置图标）。
- `xxd.process.ui/toolbar/Processor.java`：`getMenuIcon()` 优先用户覆盖的 imageFileName，否则 `ProcessorSupport.getMenuIcon` → `getDefaultIcon`。`ProcessorSupport.getActiveProcessors(suppliers, settings)` 解析 `%`/`§` 分隔的持久化串（id§imageFile§active§index）。

### 12.2 快速访问工具栏（ProcessorToolbarUI）

- `ux.extension.xxd.ui/swt/ProcessorToolbarUI.java`（位于 `ExtendedChromatogramUI` 主工具栏第一格，`createProcessorToolbarUI`）：`PreferencesProcessSupport.getActiveProcessors()`（从首选项取激活 processor 列表）→ 每个 `Processor` 一个 `Button`，`button.setImage(processor.getMenuIcon())`，点击 → `executionListener.accept(supplier, context)`；`executionListener` = `ExtendedChromatogramUI::executeSupplier`。

### 12.3 executeSupplier（单击处理器执行）

`ux.extension.xxd.ui/editors/ExtendedChromatogramUI.java#executeSupplier`：
1. `SettingsWizard.getSettings(shell, ProcessSettingsSupport.getWorkspacePreferences(supplier), true)` —— 执行前按 DialogBehavior 收集/确认参数；CANCEL 则 return。
2. `MetaProcessorProcessSupplier` / `UserMethodProcessSupplier` → `ResumeMethodSupport.selectMethodParameters`（见 §12.5）。
3. **宏录制**：若 `toolbarMethodControl.getProcessMethodMacroRecorder()!=null`，把当前 supplier 记成一个 `ProcessEntry` 追加进录制方法。
4. `processChromatogram(monitor -> ...)` 包在 `ProgressMonitorDialog` 内：`AbstractProcessSupplier.applyProcessor(settings, IChromatogramSelectionProcessSupplier.createConsumer(selection), new ProcessExecutionContext(monitor, processingInfo, context))` → `IChromatogramSelectionProcessSupplier.apply(...)` 执行实际算法 → `updateResult`（`ProcessingInfoPartSupport.getInstance().update` 显示到 Feedback 部件）+ `AuditTrailSupport` + `NoiseFactorSupport`。

### 12.4 运行整份方法（MethodSupportUI → applyProcessEntries）

- `ux.extension.ui/methods/MethodSupportUI.java`：色谱图编辑器内「方法工具栏」`createToolbarMethod` → `MethodSupportUI`（方法下拉 Combo + 新建/编辑/复制/录制/删除/目录/执行 7 钮）。执行按钮 → `MethodSupport.runMethod(methodListener, processMethod, shell)`，然后 `UpdateNotifierUI.update(TOPIC_EDITOR_CHROMATOGRAM_UPDATE, ...)`。
- `methods/MethodSupport.java#runMethod`：
  - `ResumeMethodSupport.selectMethodParameters(shell, processMethod)` → 设 `activeProfile` + `resumeIndex`；
  - `new ProgressMonitorDialog(shell).run(true, false, monitor -> methodListener.execute(processMethod, monitor))`；`InvocationTargetException`/`MethodCancelException` → `ProcessingInfoPartSupport.getInstance().update(processingInfo)`（Feedback 视图显示失败/取消）。
  - `methodListener` = `ExtendedChromatogramUI` 构造时传入的 lambda → `AbstractProcessEntryContainer.applyProcessEntries(processMethod, new ProcessExecutionContext(monitor, processingInfo, processTypeSupport), IChromatogramSelectionProcessSupplier.createConsumer(selection))` + AuditTrail + NoiseFactor + `UpdateNotifierUI.update(getDisplay(), selection.getSelectedScan())`。
- `org.eclipse.chemclipse.processing/methods/AbstractProcessEntryContainer.java#applyProcessEntries`：
  - `resumeIndex = container.isSupportResume() ? getResumeIndex() : DEFAULT`；遍历 `for(IProcessEntry processEntry : container)`，`index < resumeIndex` 跳过；
  - `context.getSupplier(processEntry.getProcessorId())` 解析处理器（找不到 → warn 跳过）；
  - `preferenceSupplier.apply(processEntry, processor)`（默认 `processEntry.getPreferences(supplier)` → `IProcessorPreferences`）→ `context.setContextObject(...)` 注入当前 entry/supplier/preferences/consumer → `context.split(processor.getContext())` 建子上下文；
  - 条目有子条目（组合方法）→ 递归 `applyProcessEntries`；否则 `AbstractProcessSupplier.applyProcessor(preferences, consumer, entryContext)` → `consumer.execute(preferences, context)`（可取消：`InterruptedException` → `OperationCanceledException`，由 `AbstractProcessSupplier` finally 清理 context）。
- **进度/取消接入**：`ProcessExecutionContext` 持有 Eclipse `IProgressMonitor`；`ProgressMonitorDialog.run(true, true, ...)`（runMethod 用 `(true,false)` 不可取消，编辑器 processChromatogram 用 `(true,true)` 可取消）；处理器内部 `monitor.isCanceled()` / 抛 `InterruptedException` 即中断链；结果一律走 `IProcessingInfo` → `ProcessingInfoPartSupport`（底部右侧 Feedback 部件，见 §13）。

### 12.5 断点续跑

- `methods/ResumeMethodSupport.java#selectMethodParameters`：`processMethod.isSupportResume()` 且 `PreferenceSupplierMethods.isShowResumeMethodDialog()` → 弹 `ResumeMethodDialog`（profile + resumeIndex 选择）；用户取消抛 `MethodCancelException`。默认 resumeIndex=0（全跑）。
- `MethodParameters.java`：`{profile, resumeIndex}` 传输对象。

### 12.6 处理器注册（OSGi DS）

- `xxd.process/Activator.java`：`ServiceTracker<IProcessTypeSupplier, IProcessTypeSupplier>` 跟踪所有注册的 `IProcessTypeSupplier`。
- `xxd.process/support/ProcessTypeSupport.java`：`IProcessSupplierContext` 实现，`getSupplier(id)` 遍历本地 + `Activator.getProcessTypeSuppliers()`。
- 注册方式（对照 `org.eclipse.chemclipse.chromatogram.vsd.filter/processors/WavenumberRemover.java`）：`@Component(service = {IProcessTypeSupplier.class})`；内部类 `ProcessSupplier extends AbstractProcessSupplier<Settings>`（构造传 id/name/description/settingsClass/parent/DataCategory）+ 实现 `IChromatogramSelectionProcessSupplier`（`apply(selection, settings, context)` = 实际算法）；`getProcessorSuppliers()` 返回集合。

---

## 13. 工作台模型（✅ 源码确认）

### 13.1 启动（3.x 兼容工作台 + e4 模型）

- `rcp.app.ui/Application.java`（`IApplication.start`）→ `internal/support/ApplicationSupportDefault.java#start`：`PlatformUI.createDisplay()` → `PlatformUI.createAndRunWorkbench(display, new ApplicationWorkbenchAdvisor())`（返回 `RETURN_RESTART` 则 `EXIT_RESTART`）。——这是 **3.x Workbench API 兼容层**跑在 e4 模型上（产品 `applicationXMI` 指向 Application.e4xmi）。
- `rcp.app.ui/ApplicationWorkbenchAdvisor.java`：`getInitialWindowPerspectiveId() = "org.eclipse.chemclipse.ux.extension.xxd.ui.perspective.main"`；`getDefaultPageInput()` = IWorkspace root。
- `rcp.app.ui/ApplicationWorkbenchWindowAdvisor.java`：编辑区拖放 `FileTransfer`、`setShowProgressIndicator(true)`、禁用 LogView 抢焦点。

### 13.2 Application.e4xmi 主窗口结构

- `TrimmedWindow`（800×600, tags:shellMaximized）：
  - `children`：`PerspectiveStack`（`...perspectivestack.main`）——实际透视图由 fragment 注入；
  - `mainMenu`：File 菜单（New 分隔符 / OpenChromatogram 分隔符 / Save / SaveAll 等 `HandledMenuItem`）；
  - `trimBars top` 4 个 ToolBar：main（Save、SaveAll）、perspectives（Perspective Switcher、Select View、Reset Perspective）、window（Quick Access、Preferences）、help（About、Help）、plugins（Updates 隐藏）；
  - `trimBars bottom`（`org.eclipse.ui.trim.status`）：`StatusLine`、`ProgressBar`、`HeapStatus`（toBeRendered=false）；
  - `sharedElements`：`ProcessingInfoPart`（Feedback）、`EditHistoryPart`；
  - `handlers`：Quit/About/Save/SaveAll/Preferences/Updates/PerspectiveSwitcher/SelectView/Import/Export/Help/QuickAccess…（`bundleclass://...handlers.*`）；`bindingTables`：M1+Q quit、M1+A about。
- **addons**：CommandService/Contexts/Bindings 处理、Cleanup/DnD/MinMax、`PerspectiveApplicationAddon`（rcp.app.ui）、`ContextAddon`（support.ui）、`PerspectiveSupport`（xxd.ui）、`ProgressAddon`（progress.ui）。
- `rcp.app.ui/addons/PerspectiveApplicationAddon.java`：启动选默认透视图（系统属性 `application.perspective` 优先，否则 `IPerspectiveAndViewIds.PERSPECTIVE_WELCOME = "...ux.extension.ui.perspective.welcome"`）→ `modelService.find(id, application)` → `perspectiveStack.setSelectedElement(...)`；`APP_STARTUP_COMPLETE` 后把各透视图 `cloneElement` 成 **snippet**（供 Reset Perspective 恢复）。

### 13.3 透视图布局（ux.extension.xxd.ui/fragment.e4xmi）

`fragment:ModelFragments` 向 `perspectivestack.main` 注入 3 个 Perspective：

**`perspective.main`（"Data Analysis (Main)"）**，`PartSashContainer` 嵌套：
- 左列（containerData=2500）：`partstack.left.top`（DataExplorer、SequenceExplorer、PeakScanList）、`partstack.left.center`（隐藏）；
- 中列（7500, horizontal）：`org.eclipse.ui.editorss`（**编辑器区**）+ `partstack.right.top`（隐藏）；
- 底栏（4180）：`partstack.bottom.left`（EditHistoryPart）、`partstack.bottom.center`（LogView "Console"）、`partstack.bottom.right`（ProcessingInfoPart）。

其余：`perspective.maldi`（"MALDI-TOF MS"）、`perspective.wsd`（"HPLC-DAD"）。

`descriptors`（可开关的 `PartDescriptor`，~50 个，均 `bundleclass://...ux.extension.xxd.ui.parts.*`）：ChromatogramOverlay、HeaderData、ChromatogramOverview、ScanChart、ScanTable、Targets、PeakChart、PeakDetails、Quantitation、SubtractScan、CombinedScan、ComparisonScan、IntegrationArea、InternalStandards、PeakDetector、MeasurementResults、NMROverlay、VSDOverlay、MassSpectrumOverlay、ChromatogramHeatmap、ChromatogramScanInfo、PeakQuantitationList、Baseline、PeakScanList（含 LinkWithEditorHandler 同步勾）、QuantitationReferences、QuantResponseChart/List、QuantPeaksChart/List、QuantSignalsList、PeakTraces、ScanBrowse、Synonyms、Molecule、PenaltyCalculation、ColumnIndices、FlavorMarker、CasNumbers、ChromatogramStatistics、Literature、ChromatogramIndices、ChromatogramSignalNoise、RegularExpression。

- `sharedElements` 注入 11 个常驻 Part：DataExplorerPart、PeakScanListPart、TargetsPart、ComparisonScanChartPart、ScanChartPart、SequenceExplorerPart、ChromatogramOverlayPart、PeakDetectorPart、EditHistoryPart、ProcessingInfoPart、ChromatogramHeatmapPart、MeasurementResultsPart。
- 工具栏 fragment：`toolbar.dataanalysis`（隐藏，GroupHandlerOverview/Overlay/Scans/Peaks/Targets/Chromatogram/ISTD/Miscellaneous 九宫格——每个按钮弹动态 Parts 菜单，命令 `...command.partHandler` → `toolbar/CommandPartHandler`）；main toolbar 加 Undo/Redo；File 菜单加 "New Process Method"（`...handlers.CreateProcessMethodHandler`）。

---

## 14. 色谱视图数据绑定（✅ 源码确认）

### 14.1 事件总线（UpdateNotifier → IEventBroker → DataUpdateSupport）

- `org.eclipse.chemclipse.model/notifier/UpdateNotifier.java`：静态 `update(IChromatogramSelection)` → `eventBroker.send(IChemClipseEvents.TOPIC_CHROMATOGRAM_XXD_UPDATE_SELECTION, selection)`。另对 IPeak/IScan/ITargetSupplier/IEditHistory 分别 `send` 对应 topic（`IChemClipseEvents` 定义全部 topic 字符串）。
- `swt.ui/notifier/UpdateNotifierUI.java`：`update(Display, topic, object)` → `display.asyncExec(() -> UpdateNotifier.update(...))`——**把事件搬回 SWT UI 线程**。
- `ux.extension.ui/support/DataUpdateSupport.java`：构造注入 `IEventBroker`；`subscribe(topic, properties)` 为每个 topic 注册一个 `EventHandler` → 收到事件存进 `objectMap`（每 topic 只保留最新对象）+ `fireUpdate(topic, objects)` 遍历 `IDataUpdateListener`。
- `ux.extension.ui/parts/AbstractUpdater.java`（**所有视图 Part 与编辑器的基类**）：构造时 `dataUpdateSupport.add(updateListener)`；`updateSelection(objects, topic)`：`DataUpdateSupport.isVisible(control) || topic.matches(EDITOR_CLOSE_REGEX)` 且 `isUpdateTopic(topic)` → `updateData(objects, topic)`；`@Focus setFocus` 首次从 `dataUpdateSupport.getUpdates(topic)` 补拉当前态；`@PreDestroy` 反注册并 `UpdateNotifier.update(TOPIC_PART_CLOSED,...)`；`subscribeAdditionalTopics()` 可扩展订阅（scan/peak/editor update/editor close…）。
- `ux.extension.ui/parts/AbstractPart.java`：`AbstractUpdater` 子类，`createControl(parent)` 建主控件。

### 14.2 编辑器侧刷新

- `ux.extension.xxd.ui/editors/AbstractChromatogramEditor.java`：`extends AbstractUpdater<ExtendedChromatogramUI>`，主 topic `TOPIC_CHROMATOGRAM_XXD_UPDATE_SELECTION`；`updateData`：IChromatogramSelection（且 `extendedChromatogramUI.isActiveChromatogramSelection`）→ `extendedChromatogramUI.update()` + dirty 同步；IScan → `updateSelectedScan()`；IPeak → `updateSelectedPeak()`；`TOPIC_EDITOR_*` 特化刷新。`processChromatogram`：加载时按首选项自动套用某方法文件。
- `ExtendedChromatogramUI.java`（编辑器内主 Composite）：
  - 装配：`createChromatogramSection` = 主工具栏（ProcessorToolbarUI + 折叠按钮 + 分离柱下拉 + 网格/图例/重置/帮助/设置）、Info 条、References 条、Edit 条（基线 UI）、Alignment 条、Method 条（MethodSupportUI）、**ChromatogramChart**；
  - `updateChromatogram()`：`chromatogramChart.deleteSeries()` → `addChromatogramSeriesData()`（色谱线 + 6 类峰 + 已识别 scan + 选中峰/scan + 基线；峰/scan 用 `TargetReferenceLabelMarker` 画标签）→ `addLineSeriesData`（按首选项压缩类型 `getCompressionLength` 后 `chart.addSeriesData(list, compressionToLength)`）；
  - 选中峰/scan 局部更新：`updateSelectedPeak()` 删 `SERIES_ID_SELECTED_PEAK_MARKER`/`SHAPE*` 重建、`updateSelectedScan()` 删 `SELECTED_SCAN` 重建；
  - `adjustChromatogramSelectionRange()`：`chart.setRange(X, startRT, stopRT)` / `setRange(Y, startAbundance, stopAbundance)`——**视图范围由 selection 驱动**。

### 14.3 用户在图上框选 → 反向写回 selection

- `createChromatogramChart`：`baseChart.addCustomRangeSelectionHandler(new ChromatogramSelectionHandler(this))`（自定义 SWTChart 范围选择器）；并注册 8 个 `HandledEventProcessor`（Scan/Peak 点击、方向键移动选择、整谱移动）。
- `swt/editors/ChromatogramSelectionHandler.java#handleUserSelection`：读 `baseChart.getAxisSet().getXAxis(ID_PRIMARY_X_AXIS).getRange()` / Y → `extendedChromatogramUI.setChromatogramSelectionRange(startRT, stopRT, startAbundance, stopAbundance)` → `chromatogramSelection.setRanges(..., false)` + `chromatogramSelection.update(true)` → **再次走 §14.1 总线广播**，所有视图联动。

### 14.4 多视图联动（消费侧实例）

- `parts/ChromatogramOverviewPart.java`：`extends AbstractPart<OverviewChartUI>`，topic=`TOPIC_CHROMATOGRAM_XXD_UPDATE_SELECTION`；`updateData` → `overviewSupport.process` → `updateChart`：`TotalScanSignalExtractor.getTotalScanSignals(false)` 抽 TIC 点 → `SeriesData(x=retentionTime, y=totalSignal)` → `OverviewChartUI.addSeriesData`（**概览/缩略图**）。
- `parts/PeakScanListPart.java`：topic 同；`updatePeakSelection(@UIEventTopic(TOPIC_PEAK_XXD_UPDATE_SELECTION) IPeak peak)` → `getControl().updateSelection()`（表↔图双向，LinkWithEditorHandler 同步勾）；订阅编辑关闭 topic 清空。
- 选中峰/scan 变化 → `UpdateNotifier.update(IPeak)` / `update(IScan)` → 订阅方（PeakChart、ScanChart、TargetsPart、PeakDetailsPart、ComparisonScan 等）局部刷新，不重绘整谱。

### 14.5 触发时机汇总

- 编辑器 `onFocus` / `@Persist` 后 / 执行完方法后：`extendedChromatogramUI.fireUpdate(display)` → `fireUpdateChromatogram/fireUpdatePeak/fireUpdateScan` → `UpdateNotifierUI.update(display, ...)`（重发当前 selection/peak/scan）。
- selection 对象自身：`setSelectedPeak/setSelectedScan/setRanges` 默认 `fireUpdateChange(true)` → `UpdateNotifier.update(this)`。

---

## 15. support.ui 定位（✅ 目录/源码确认）

`org.eclipse.chemclipse.support.ui` **不承载 SettingsUIProvider**（设置表单框架在 ux.extension.ui）。它提供通用 SWT 支撑：`swt/ControlBuilder`（`createColumn`/`createContainer`，被 MethodTreeViewer/SettingsUI 使用）、`services/IAnnotationWidgetService`（自定义设置控件插槽，OSGi DS）、`provider/*`（AbstractChemClipseLabelProvider/ListContentProvider 等）、`workbench/EditorSupport`（编辑器 MAP_FILE 常量）、`workbench/WorkbenchAdvisorSupport.declareProjectExplorerImages`、`parts/EditHistoryPart`、`activator/ContextAddon`（e4 context 支持）。

---

## 16. 对比 Qt：可移植与 Java 特有（⚠️ 设计笔记，延续 §7）

**可直接移植（照抄架构）**
1. **菜单→工具栏→执行链**：menu.icon 的 20 个 id = 方法步骤注册表；Qt 用 `QAction`（icon + id）+ `QToolBar`/`QMenu`。点击 → `StepSettingsDialog`（复用表单生成器）→ `QtConcurrent::run` 跑管线 + `QProgressDialog`（取消 → 抛 OperationCanceledException 等价物，中断后续步骤）。对应 OpenChrom：`Processor.getMenuIcon → ProcessorToolbarUI → executeSupplier → applyProcessEntries`。
2. **选择事件联动**：`UpdateNotifier → IEventBroker → DataUpdateSupport → IDataUpdateListener` 是纯订阅/发布。Qt 等价：进程内 `QObject` 事件总线（信号 `update(QString topic, QVariant data)`）+ 各视图 `connect` 订阅；`DataUpdateSupport.isVisible` 对应 `isVisible()` 跳过隐藏视图；`display.asyncExec` 对应 Qt 队列连接（自动回主线程）。topic 常量表照抄 `IChemClipseEvents`。
3. **设置表单生成**：`SettingsClassParser`（Jackson 反射字段 + 注解）→ Qt 用 `QMetaObject`/`QVariant` 描述字段描述符表 `{key, type, label, tooltip, default, validator(min/max/regex), comboItems, fileExt}`，写一个 `SettingsFormGenerator`（`QFormLayout` + QLineEdit/QSpinBox/QComboBox/QCheckBox/文件选择）。编辑后 `toJson(QVariantMap)` ↔ 执行时 `fromJson`。校验用 `QValidator` 对应 `InputValidator/RegularExpressionValidator/MinMaxValidator`。`IAnnotationWidgetService` 对应“自定义控件工厂”注册表。
4. **方法编辑器**：`QTreeView` + `QStandardItemModel` 复刻 `MethodTreeViewer`（列：状态/名称/描述/类型/设置/ID）；增删改排序直接操作 `QList<StepEntry>`；`ListProcessEntryContainer` 对应容器 `add/remove/swap`。保存 = JSON 序列化（对应 `MethodConverter.convert`）。
5. **概览/主图联动**：主图 `ChromatogramChart` 与 `ChromatogramOverviewPart` 各自订阅 selection topic、各自从 `IChromatogramSelection` 取范围重建 series——Qt 两个 `QCustomPlot` 都监听同一信号即可。用户框选 → 写回 selection → 广播，与 `ChromatogramSelectionHandler` 一致。

**Java/Eclipse 特有（Qt 需替换）**
1. **e4 工作台模型**（Application.e4xmi / fragment / PerspectiveStack / TrimBar / PartDescriptor / snippet / ResetPerspective）→ Qt 用 `QMainWindow` + `QDockWidget` + `QTabWidget`；「透视图」= 保存的 `QMainWindow::saveState()/restoreState()` 布局；PartDescriptor 开关 = `QDockWidget::toggleViewAction()`。
2. **Eclipse adapter 机制**（`Adapters.adapt(settings, SettingsUIProvider.class)` 动态适配）→ Qt 用注册表 `QHash<QString /*typeName*/, FormFactory>` 或 `qobject_cast`。
3. **OSGi DS**（`@Component(service=IProcessTypeSupplier)` + ServiceTracker）→ Qt `QPluginLoader` + 每插件暴露 `IProcessTypeSupplier`（接口 + 工厂）。
4. **SWT/JFace viewer**（TreeViewer/TableViewer/ContentProvider/LabelProvider、ComboViewer、ControlDecoration）→ Qt Model/View（delegate）+ `QValidator`。
5. **SWTChart + extensions**（L4 外部，`BaseChart`/`IChartSettings`/`RangeRestriction`/`ICustomSelectionHandler`/`HandledEventProcessor`）→ QCustomPlot/Qt Charts（压缩显示可照 `getCompressionLength` 抽稀逻辑）。
6. **ProgressMonitorDialog / IProgressMonitor**（Eclipse 作业框架）→ Qt `QProgressDialog` + `QFutureWatcher`（取消协作）。

---

## 8. 待回填清单（❓）

| # | 问题 | 归属 |
|---|---|---|
| UI1 | ChemClipse 方法编辑器如何消费 menu.icon id 与 SettingsUIProvider（步骤列表 ↔ 参数表单绑定） | ✅ §10/§11（2026-08-17 源码确认） |
| UI2 | `Application.e4xmi` 工作台模型（菜单栏/工具栏/视图/透视图树）内容 | ✅ §13（2026-08-17 源码确认） |
| UI3 | 色谱图视图/峰表/结果视图的实现与数据绑定 | ✅ §14（2026-08-17 源码确认） |
| UI4 | UI 触发报告的调用链：菜单 → supplier 枚举 → reportGenerator.generate | ⚠️ 部分：§12 已确认「菜单→executeSupplier→applyProcessor→consumer.execute」通用链；报告供应商具体实现仍归 MODULE_06 |
| UI5 | 参数修改 → 保存到方法对象 → 重新执行管线的完整事件链（MethodParameters / ResumeMethodSupport） | ✅ §11.2 写回 + §12.5 断点续跑 + §12.4 执行链（2026-08-17） |
| UI6 | tracecompare.ui EditorProcessor 的完整读写与 dirty 机制细节（JAXB 模型） | ⚠️ 部分待读 |
| UI7 | SWTChart 渲染性能策略（抽稀/降采样） | ✅ 部分：`addLineSeriesData` 用 `chromatogramChartSupport.getCompressionLength(compressionType, ...)` 抽稀（§14.2）；SWTChart 库本身仍 ❓ L4 |

---

## 9. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| UI-A | 产品定义（application id / applicationXMI / about / 窗口图标） | openchrom/plugins/net.openchrom.rcp.compilation.community.ui/plugin.xml | ✅ |
| UI-B | 启动画面：EnhancedSplashHandler 定位进度条/消息矩形 | .../community/ui/splash/EnhancedSplashHandler.java | ✅ |
| UI-C | 品牌：e4 fragment + PGP 信任 + 主题样式 | net.openchrom.feature.branding/plugin.xml | ✅ |
| UI-D | menu.icon 扩展点 15 块/20 图标，class 实现 IMenuIcon.getImage() | 7 个插件 plugin.xml + icon/*.java | ✅ |
| UI-E | SettingsUIProvider 适配器机制（settings→表单） | templates.ui / ratios.ui plugin.xml + adapter/*AdapterFactory.java | ✅ |
| UI-F | 设置页 18 插件/38 页，按 9 类父分类挂接 | 全量 Grep org.eclipse.ui.preferencePages | ✅ |
| UI-G | PDF 报告用 SWTChart 离屏绘制 + PDFExportHandler 导出 | .../pdf.ui/swt/ChartExportRunnable.java | ✅ |
| UI-H | tracecompare 真编辑器（MultiPageEditorPart + 向导） | .../tracecompare.ui/plugin.xml + editors/EditorProcessor.java | ✅ |
| UI-I | pluginDiscovery 扩展点 + Bundle/DS 双发现策略 + FeatureCheck 安装向导 | net.openchrom.installer(.ui) plugin.xml + model/*.java | ✅ |
| UI-J | 安装底层 p2 | PrepareInstallProfileJob / ProvisioningUI / IProfileRegistry import | ✅ |
| UI-K | 方法编辑器/色谱视图/工作台模型本体 | chemclipse_tree.json：org.eclipse.chemclipse.ux.extension.ui / ux.extension.xxd.ui / rcp.app.ui | ❓ |
| UI-L | SWTChart 库 | 树中无源码（仅 rcp.ui.icons 引用 swtchart.gif） | ❓ L4 |
| UI-M | 方法编辑器装配：ProcessMethodEditor(e4 Part,@PostConstruct)→ExtendedMethodUI(头/Profile/树/工具栏)→MethodEditorSupport.openEditor | ux.extension.ui/editors/ProcessMethodEditor.java + editors/ExtendedMethodUI.java + methods/MethodEditorSupport.java | ✅ |
| UI-N | 步骤树：MethodTreeViewer(TreeViewer 6 列, IProcessEntryContainer 递归展开) + MethodListLabelProvider(校验图标+JSON settings 列) | ux.extension.ui/swt/MethodTreeViewer.java + internal/provider/MethodListLabelProvider.java | ✅ |
| UI-O | 步骤增删排序复制：ProcessMethodToolbar(ProcessingWizard→addProcessEntry / container.removeProcessEntry / Collections.swap / copy 到剪贴板) | ux.extension.ui/swt/ProcessMethodToolbar.java + methods/ProcessingWizard.java | ✅ |
| UI-P | 保存方法：ProcessMethodEditor.save(@Persist)→MethodConverter.convert；saveAs→MethodFileSupport.saveProccessMethod | ux.extension.ui/editors/ProcessMethodEditor.java + methods/MethodFileSupport.java | ✅ |
| UI-Q | 注解→InputValue：SettingsClassParser 用 Jackson 反射字段+@JsonProperty/@IntSettingsProperty/@StringSettingsProperty/@FileSettingProperty/@ComboSettingsProperty/@LabelProperty | org.eclipse.chemclipse.support/settings/parser/SettingsClassParser.java + InputValue.java | ✅ |
| UI-R | 表单生成：SettingsUI(Adapters.adapt→SettingsUIProvider, 兜底 DefaultSettingsUIProvider)→WidgetItem 按 rawType 建 SWT 控件+读回 JSON+校验 | ux.extension.ui/methods/SettingsUI.java + methods/WidgetItem.java | ✅ |
| UI-S | 向导页与写回：SettingsPreferencesPage(validate/getSettings)+SettingsWizard.openEditPreferencesWizard→preferences.setUserSettings(JSON) | ux.extension.ui/methods/SettingsPreferencesPage.java + methods/SettingsWizard.java | ✅ |
| UI-T | 执行链：MethodSupport.runMethod→ProgressMonitorDialog→applyProcessEntries→AbstractProcessSupplier.applyProcessor→consumer.execute；断点续跑 ResumeMethodSupport/MethodParameters；处理器注册 @Component(IProcessTypeSupplier)+ServiceTracker | ux.extension.ui/methods/MethodSupport.java + processing/methods/AbstractProcessEntryContainer.java + processing/supplier/{AbstractProcessSupplier,ProcessExecutionContext}.java + xxd.process/Activator.java | ✅ |
| UI-U | 处理器工具栏：ProcessorToolbarUI(Processor.getMenuIcon→ProcessorSupport.getMenuIcon 按 id 子串匹配 menu.icon) + ExtendedChromatogramUI.executeSupplier | ux.extension.xxd.ui/swt/ProcessorToolbarUI.java + xxd.process.ui/{support/ProcessorSupport, toolbar/Processor, menu/IMenuIcon}.java | ✅ |
| UI-V | 工作台启动：Application→ApplicationSupportDefault→PlatformUI.createAndRunWorkbench；Advisor 初始透视图 xxd.ui.perspective.main | rcp.app.ui/Application.java + internal/support/ApplicationSupportDefault.java + ApplicationWorkbenchAdvisor.java | ✅ |
| UI-W | e4 模型：Application.e4xmi(TrimmedWindow/PerspectiveStack/mainMenu/trimBars/handlers/addons) + PerspectiveApplicationAddon(默认透视图+snippet) | rcp.app.ui/Application.e4xmi + addons/PerspectiveApplicationAddon.java | ✅ |
| UI-X | 透视图布局：ux.extension.xxd.ui/fragment.e4xmi(perspective.main 左/中/底 布局、11 sharedElements、~50 PartDescriptor、dataanalysis 工具栏、CreateProcessMethod 命令) | ux.extension.xxd.ui/fragment.e4xmi | ✅ |
| UI-Y | 事件总线：UpdateNotifier(IEventBroker.send)→UpdateNotifierUI(display.asyncExec)→DataUpdateSupport(subscribe+fireUpdate)→AbstractUpdater/AbstractPart(updateData) | org.eclipse.chemclipse.model/notifier/UpdateNotifier.java + swt.ui/notifier/UpdateNotifierUI.java + ux.extension.ui/{support/DataUpdateSupport, parts/AbstractUpdater, parts/AbstractPart}.java | ✅ |
| UI-Z | 色谱图绑定：AbstractChromatogramEditor(updateData)→ExtendedChromatogramUI.updateChromatogram/addChromatogramSeriesData/addLineSeriesData(压缩)；图框选 ChromatogramSelectionHandler→setRanges→update(true) 广播；多视图 Overview/PeakScanList 联动 | ux.extension.xxd.ui/{editors/AbstractChromatogramEditor, swt/editors/ExtendedChromatogramUI, swt/editors/ChromatogramSelectionHandler, parts/ChromatogramOverviewPart, parts/PeakScanListPart}.java | ✅ |
