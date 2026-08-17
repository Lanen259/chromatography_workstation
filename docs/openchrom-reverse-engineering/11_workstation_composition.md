# OpenChrom 色谱工作站 · 组成分析（供后续开发 agent 使用）

> **用途**：把"色谱工作站 10 大功能模块"映射到 OpenChrom 的实际组件（插件/Feature/扩展点/外部库），
> 供另一名 agent 据此评估"直接改造 OpenChrom 还是自研"，以及确定每块功能的代码落点。
>
> **生成日期**：2026-08-15
> **数据来源**：本仓库源码（`openchrom/` 目录）+ 产品定义 + target platform 定义
> **溯源标记**：✅ 本仓库源码确认 ｜ ⚠️ 外部组件/合理推断 ｜ ❓ 待验证
> **重要前提**：本仓库只含 **OpenChrom 社区插件的源码**；核心引擎在 **ChemClipse 平台**（二进制，外部拉取）。下文第 2 节详细说明。

---

## 1. 一句话结论

OpenChrom = **Eclipse RCP 运行时** + **ChemClipse CDS 核心平台（外部二进制）** + **OpenChrom 社区插件层（本仓库 68 个插件源码）** + **SWTChart 绘图** + **第三方科学计算库**。

- 产品名：**OpenChrom (Hillenkamp)**，版本 1.6.27，社区版（Community Edition）。
- ✅ 依据：`openchrom/products/net.openchrom.rcp.compilation.community.product/openchrom.compilation.community.product`
- ✅ 应用入口：`application="org.eclipse.chemclipse.rcp.app.ui.org.eclipse.chemclipse.rcp.application"`（属于 ChemClipse，不在本仓库）
- ✅ 版本号：根 `openchrom/pom.xml` `artifactId=master` `version=1.6.27-SNAPSHOT`，`releaseName=1.6.27`

---

## 2. 四层架构总览

```text
┌──────────────────────────────────────────────────────────────────────┐
│ L4 运行时层  Eclipse RCP / Equinox / e4 / SWT·JFace  (2026-06)        │  ← target platform 拉取
│    + SWTChart（色谱/光谱绘图）、Nebula、Draw2D                        │
├──────────────────────────────────────────────────────────────────────┤
│ L3 核心平台层  ChemClipse (org.eclipse.chemclipse.*)                  │  ← 二进制，外部拉取
│    数据模型 / 处理引擎 / 转换器框架 / 标识器框架 / 定量框架            │
│    报告框架 / UI(视图·编辑器·透视图) / 扩展点注册表                    │
│    (download.chemclipse.net/integration/repository)                  │
├──────────────────────────────────────────────────────────────────────┤
│ L2 社区插件层  OpenChrom (net.openchrom.*)  ★ 本仓库 68 个插件源码     │
│    转换器、峰检测器、标识器、分类器、处理器、报告器、模板、安装器      │
├──────────────────────────────────────────────────────────────────────┤
│ L1 第三方库    CDK、jHDF5、InChI、Beam、Apache POI、PDFBox、sqlite、    │
│                JAXB、Opsin、Jackson、Protobuf 等                      │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.1 各层来源与证据

| 层 | 组件 | 来自 | 依据 |
|---|---|---|---|
| L4 运行时 | Eclipse 2026-06 SDK、Equinox、SWT/JFace/e4、p2 | `mirror.lablicate.com/eclipse/releases/2026-06/` | ✅ `releng/.../net.openchrom.targetplatform.target` |
| L4 绘图 | `org.eclipse.swtchart.feature` + `swtchart.extensions` | `download.eclipse.org/swtchart/...` | ✅ 同上 + MANIFEST 依赖 |
| L3 核心 | `org.eclipse.chemclipse.rcp.compilation.community.feature` | `download.chemclipse.net/integration/repository/` | ✅ targetplatform + `net.openchrom.platform.feature/feature.xml` |
| L2 社区插件 | 68 个 `net.openchrom.*` 插件 | **本仓库** | ✅ `openchrom/plugins/` |
| L1 第三方 | CDK/jhdf5/inchi/beam；POI/PDFBox/csv；JAXB/sqlite/opsin | `bundles.openchrom.net/3rdparty`、orbit、maven | ✅ targetplatform |

> ⚠️ **关键约束**：L3 ChemClipse 在本仓库**只有依赖关系、没有源码**。要深度逆向核心算法（峰检测/积分/定量引擎本体），必须另行获取 ChemClipse 源码（`org.eclipse.chemclipse.*`，位于 download.chemclipse.net 或 lablicate 站点）。本仓库的 68 个插件是"供应商（supplier）"，负责把具体格式/算法以扩展方式挂到 ChemClipse 的扩展点上。

---

## 3. 检测器类型域（数据模型的组织方式）

OpenChrom/ChemClipse 的数据模型**按检测器类型划分域**，每个域有独立的 model / converter / processor 体系。这是理解全项目命名的钥匙。

| 域 | 含义 | 本仓库证据 | model 归属 |
|---|---|---|---|
| **CSD** | 单检测器色谱（FID 等一路模拟信号） | ✅ `net.openchrom.csd.converter.supplier.*` | `org.eclipse.chemclipse.csd.model`（外部） |
| **MSD** | 质谱检测器（每个保留时间点一簇质谱） | ✅ `net.openchrom.msd.*` 系列 | `org.eclipse.chemclipse.msd.model`（外部） |
| **WSD** | 波长光谱检测器（UV/Vis 二维） | ✅ `net.openchrom.wsd.converter.supplier.*` | `org.eclipse.chemclipse.wsd.model`（外部） |
| **FSD** | 荧光光谱检测器 | ✅ 转换器 filterName=`Fluorescence Spectroscopy (*.gaml)` | `org.eclipse.chemclipse.fsd.model`（外部） |
| **VSD** | 光谱型域（读取 `ISpectrumVSD`） | ✅ `net.openchrom.vsd.converter.supplier.gaml` | `org.eclipse.chemclipse.vsd.model`（外部） |
| **TSD** | 总信号域（`getTotalSignal()` 总离子/总信号色谱） | ✅ `ChromatogramReaderTSD.java` | `org.eclipse.chemclipse.tsd.model`（外部） |
| **NMR** | 核磁谱域 | ✅ `net.openchrom.nmr.converter.supplier.gaml` | `org.eclipse.chemclipse.nmr.model`（外部） |
| **XXD** | 通用域（跨检测器，如通用报告/标识/模板） | ✅ `net.openchrom.xxd.*` 系列 | `org.eclipse.chemclipse.model` 等 |

✅ 依据（依赖图谱，来自全部 68 个插件 MANIFEST.MF 的 `Require-Bundle` 统计）：
`msd.model`(26 次)、`wsd.model`(12)、`csd.model`(7)、`vsd.model`(2)、`tsd.model`(2)、`fsd.model`(1)、`nmr.model`(1)。

> 构建自研工作站时，如果只需"单检测器 FID/紫外 + 数据处理"，**CSD + WSD + XXD** 三条线即可覆盖，MSD 可后置。

---

## 4. 扩展点体系（ChemClipse 核心的"插槽"）

ChemClipse 把每个功能做成**扩展点（extension-point）**，OpenChrom 插件通过 `plugin.xml` 的 `<extension point="...">` 挂接。下列扩展点是从本仓库全部 `plugin.xml` 统计出来的真实"插槽"清单（✅ 已确认存在）：

### 4.1 转换器类
| 扩展点 | 用途 | 挂接数 |
|---|---|---|
| `org.eclipse.chemclipse.msd.converter.chromatogramSupplier` | MSD 色谱导入导出 | 12 |
| `org.eclipse.chemclipse.wsd.converter.chromatogramSupplier` | WSD 色谱导入导出 | 7 |
| `org.eclipse.chemclipse.csd.converter.chromatogramSupplier` | CSD 色谱导入导出 | 7 |
| `org.eclipse.chemclipse.msd.converter.databaseSupplier` | 谱库（数据库）读写 | 5 |
| `org.eclipse.chemclipse.msd.converter.massSpectrumSupplier` | 质谱文件读写 | 2 |
| `org.eclipse.chemclipse.{wsd,vsd,nmr,fsd}.converter.scanSupplier` | 各类谱扫描读写 | 各 1 |

✅ 供应商实现模式（例：`net.openchrom.msd.converter.supplier.cdf/plugin.xml`）：
`ChromatogramSupplier` 元素声明 `importConverter` / `exportConverter` / `fileExtension` / `importMagicNumberMatcher` / `importContentMatcher`——**一个转换器 = 实现这 4 类接口**。

### 4.2 峰检测类
| 扩展点 | 用途 | 挂接数 |
|---|---|---|
| `org.eclipse.chemclipse.chromatogram.msd.peak.detector.peakDetectorSupplier` | MSD 峰检测 | 5 |
| `org.eclipse.chemclipse.chromatogram.csd.peak.detector.peakDetectorSupplier` | CSD 峰检测 | 3 |
| `org.eclipse.chemclipse.chromatogram.wsd.peak.detector.peakDetectorSupplier` | WSD 峰检测 | 2 |

### 4.3 标识类（定性）
| 扩展点 | 用途 | 挂接数 |
|---|---|---|
| `org.eclipse.chemclipse.chromatogram.xxd.identifier.targetIdentifier` | 目标物标识 | 4 |
| `org.eclipse.chemclipse.chromatogram.csd.identifier.peakIdentifier` | CSD 峰标识 | 3 |
| `org.eclipse.chemclipse.chromatogram.wsd.identifier.peakIdentifier` | WSD 峰标识 | 2 |
| `org.eclipse.chemclipse.chromatogram.msd.identifier.peakIdentifier` | MSD 峰标识 | 2 |
| `org.eclipse.chemclipse.chromatogram.xxd.identifier.scanIdentifier` | 扫描标识 | 2 |
| `org.eclipse.chemclipse.chromatogram.xxd.identifier.chromatogramIdentifier` | 色谱标识 | 1 |

### 4.4 定量/积分/过滤/分类/报告/处理
| 扩展点 | 用途 | 挂接数 |
|---|---|---|
| `org.eclipse.chemclipse.chromatogram.msd.classifier.chromatogramClassifierSupplier` | 分类器 | 4 |
| `org.eclipse.chemclipse.chromatogram.xxd.report.chromatogramReportSupplier` | 报告 | 4 |
| `org.eclipse.chemclipse.chromatogram.xxd.quantifier.peakQuantifierSupplier` | **峰定量（内标/外标）** | 1 |
| `org.eclipse.chemclipse.chromatogram.xxd.integrator.peakIntegratorSupplier` | **峰积分** | 1 |
| `org.eclipse.chemclipse.chromatogram.filter.chromatogramFilterSupplier` | 色谱滤波/平滑 | 1 |

### 4.5 UI / 系统
| 扩展点 | 用途 | 挂接数 |
|---|---|---|
| `org.eclipse.ui.preferencePages` | 设置页（方法/插件参数） | 18 |
| `org.eclipse.chemclipse.xxd.process.ui.menu.icon` | **处理流程菜单（方法链 UI）** | 15 |
| `org.eclipse.e4.workbench.model` | e4 工作台扩展（菜单/工具栏/视图） | 5 |
| `org.eclipse.ui.newWizards` / `editors` / `help.toc` / `splashHandlers` | 标准 RCP 扩展 | 若干 |
| `net.openchrom.installer.pluginDiscovery` | 插件发现/安装 | 2 |

> ❓ **说明**：积分器/定量器的**实现**（如梯形积分 `trapezoid`、一阶导数峰检测 `firstderivative`、Savitzky-Golay 平滑、基线扣除 `baselinesubtract` 等）均在 ChemClipse 的 `*.supplier.*` 包中（见依赖统计，外部二进制），本仓库只有扩展点挂接与社区供应商。

---

## 5. 本仓库 68 个插件全清单（按功能分类）

> 路径：`openchrom/plugins/`。每个插件是 OSGi bundle，结构 = `META-INF/MANIFEST.MF` + `plugin.xml`（扩展点）+ `src/`（java 源码）+ 部分含 `OSGI-INF`。

### 5.1 数据格式转换器（Converter / Supplier）——共 33
**MSD 质谱转换器（12 + 3 辅助）**
- `msd.converter.supplier.animl`：AnIML 色谱读写
- `msd.converter.supplier.btmsp`：Bruker Biotyper 谱库（btmsp）
- `msd.converter.supplier.cdf`：ANDI/AIA CDF 色谱读写（+`cdf.ui`）
- `msd.converter.supplier.cms`：CMS 质谱读写（+`cms.ui` +`cms.documentation`）
- `msd.converter.supplier.gaml`：GAML 色谱读写
- `msd.converter.supplier.mgf`：Mascot 通用格式（mgf）质谱读写
- `msd.converter.supplier.microbems.muf` / `.pkf`：MicrobeMS 谱库/峰表
- `msd.converter.supplier.microbenet`：MicrobeNet MALDI XML 写出
- `msd.converter.supplier.mz5` / `mzdb` / `mzmlb`：mz5 / mzDB / mzMLb 色谱读写

**CSD 单检测器转换器（4 + 1 UI）**
- `csd.converter.supplier.animl`（AnIML）、`arw`（ARW FID）、`cdf`（ANDI/AIA CDF，+`cdf.ui`）、`gaml`

**WSD 波长转换器（6 + 1 UI）**
- `wsd.converter.supplier.abif`（ABIF Sanger 测序 trace）、`animl`、`arw`（+`arw.ui`）、`axr`（AXR JSON）、`cdf`、`gaml`

**XXD 通用转换器（3 + 1 UI）**
- `xxd.converter.supplier.animl`（+`animl.ui`）、`gaml`、`rdx3`（RData 色谱导出）

**其他检测器扫描转换器**
- `fsd.converter.supplier.gaml`、`vsd.converter.supplier.gaml`、`nmr.converter.supplier.gaml`（均读 GAML 扫描）

### 5.2 峰检测器 —— 共 2
- `chromatogram.msd.peak.detector.supplier.amdis`（+`amdis.ui`）：**AMDIS 峰解卷积**（NIST 算法）

### 5.3 处理器 / 分析工具 —— 共 6
- `chromatogram.msd.processor.supplier.massshiftdetector`（+`ui`）：**质量位移检测器**（`MassShiftDetector.java`）
- `xxd.processor.supplier.tracecompare`（+`ui`）：**谱图对比**（`DataProcessor` + `TrackStatisticComparator`）
- `msd.process.supplier.cms`（+`ui` +`documentation`）：**质谱相关性/分解**（`MassSpectraCorrelation`、`MassSpectraDecomposition`）

### 5.4 模板方法（方法管理/自动化核心）—— 共 2
- `xxd.process.supplier.templates`（+`ui`）：**基于模板的峰检测/峰标识/ISTD 定量分配**（`PeakDetector`、`PeakIdentifierMSD/CSD/WSD`、`StandardsAssigner`）——注册了 msd/csd/wsd 三域峰检测器 + 峰标识 + 定量器

### 5.5 标识器（定性）—— 共 6
- `xxd.identifier.supplier.cas`（CAS 数据库链接）、`cdk`（+`ui`，**CDK 计算 SMILES 分子式**）、`foodb`（FooDB）、`tgsc`（TGSC 香原料库）、`chromatogram.xxd.identifier.supplier.jmol.ui`（JMol 3D 显示设置页）
- `msd.identifier.supplier.massbank`（+`ui`）：**MassBank 谱库匹配链接**

### 5.6 分类器 —— 共 2
- `xxd.classifier.supplier.ratios`（+`ui`）：**峰比率分类器**（计算峰 trace 比率）

### 5.7 报告器 —— 共 5
- `chromatogram.xxd.report.supplier.csv`（+`ui`）：**CSV 报告**
- `chromatogram.xxd.report.supplier.excel.template`（+`ui`）：**基于 Excel 模板的报告**
- `chromatogram.xxd.report.supplier.pdf.ui`：**PDF 报告**

### 5.8 基础 / 框架 / 安装 —— 共 8
- `xxd.base` / `xxd.base.ui`：OpenChrom 基础 API（导出 `net.openchrom.xxd.base.model`）
- `feature.branding`：品牌
- `rcp.compilation.community.ui`：**社区版 RCP 壳**（产品定义、启动、about、splash）
- `installer` / `installer.ui`：**插件发现与安装器**（定义 `pluginDiscovery` 扩展点）
- `msd.extensions.cdk`（+`ui`）：**CDK 化学扩展**（第三方 CDK 库桥接）

---

## 6. Feature 与产品构成

本仓库 37 个 feature（`openchrom/features/`）把上面的插件组装成可安装单元。

- **平台 Feature**：`net.openchrom.platform.feature`（`feature.xml` ✅）——把所有社区插件 + ChemClipse 社区平台 + NMR 处理 base 组装在一起，并 **requires**：`org.eclipse.rcp`、`org.eclipse.help`、`org.eclipse.license`、`org.eclipse.swtchart.feature`、`org.eclipse.equinox.p2.user.ui`。
- **产品**：`net.openchrom.rcp.compilation.community.product` 只包含 2 个 feature：
  `net.openchrom.platform.feature` + JRE（`org.eclipse.justj.openjdk.hotspot.jre.full.stripped`）。
- 其余 36 个 feature 一一对应"单个插件/插件组"（如 `msd.converter.supplier.cdf.feature`），可单独作为 p2 更新/安装单元。
- ✅ 依赖图谱显示关键 ChemClipse 内部供应商（被引用的外部 bundle）：`filter.supplier.savitzkygolay`（SG 平滑）、`filter.supplier.baselinesubtract`（基线扣除）、`integrator.supplier.trapezoid`（梯形积分）、`peak.detector.supplier.firstderivative`（一阶导数峰检测）、`edit.supplier.snip`（SNIP 本底扣除）等。

---

## 7. 10 大功能模块 → 组件映射（核心章节）

> 图例：**本仓库**＝可在 `openchrom/plugins` 直接改源码；**ChemClipse（外部）**＝核心引擎，需获取其源码或仅使用其 API；❓＝社区版未确认具备。

### 模块 1 · 数据管理
| 子项 | 组件 | 归属 |
|---|---|---|
| 数据文件导入/导出 | 转换器扩展点 + 全部 `*.converter.supplier.*` | 本仓库（格式实现）+ ChemClipse（转换框架） |
| 方法文件 | `org.eclipse.chemclipse.processing`（处理方法模型）+ templates 模板 | ChemClipse + 本仓库 |
| 样品信息 / 序列 / 工程 | ❓ 社区版无独立"序列/工程"插件（需验证 ChemClipse 是否内置） | 待验证 |

### 模块 2 · 数据采集 ⚠️ 关键缺口
- **本仓库/社区版没有任何仪器驱动、串口/USB/网络通信、实时采集插件**（✅ grep 确认：全部 `*.java` 中无 data-acquisition/instrument-driver 代码，仅有文件读取器）。
- 实时数据 / 通道 / 时间轴 / 设备通信：**社区版不提供，需自研**。ChemClipse 是否有采集框架：❓ 待验证。
- ⚠️ 若目标只是"离线分析+自配采集前端"，可把采集做成独立模块，通过 CSV/CDF/GAML 转成 OpenChrom 数据模型接入。

### 模块 3 · 色谱图
| 子项 | 组件 | 归属 |
|---|---|---|
| 曲线绘制（实时/历史） | **SWTChart**（`org.eclipse.swtchart` + `org.eclipse.swtchart.extensions`） | L4 外部库 |
| 视图/缩放/平移/多通道/光标 | ChemClipse UX 视图（`org.eclipse.chemclipse.ux.extension.*.ui`）+ e4 工作台 | ChemClipse（外部） |
| 光谱 3D/JMmol | `chromatogram.xxd.identifier.supplier.jmol.ui` | 本仓库（UI 壳） |

### 模块 4 · 色谱数据处理（核心）
| 子项 | 组件 | 归属 |
|---|---|---|
| 基线（Baseline） | `chromatogram.xxd.baseline.detector`（基线检测 API）+ `edit.supplier.snip`（SNIP 扣除） | ChemClipse（外部） |
| 峰检测（Peak Detection） | `chromatogram.*.peak.detector` API + 供应商：**AMDIS**（本仓库）、**模板峰检测**（本仓库）、`firstderivative`（ChemClipse） | 混合 |
| 峰积分（Peak Integration） | `chromatogram.xxd.integrator` API + `integrator.supplier.trapezoid` | ChemClipse（外部） |
| 平滑（Smoothing） | `chromatogram.filter` API + `filter.supplier.savitzkygolay`（SG） | ChemClipse（外部） |
| 噪声 / 滤波（Filtering） | `chromatogram.filter` / `msd.filter.supplier.xpass` / `filter.supplier.zeroset` / `filter.supplier.scan` | ChemClipse（外部） |
| 数据校正 | `chromatogram.xxd.calculator`、`classifier.supplier.ratios`（峰比率） | ChemClipse + 本仓库 |

### 模块 5 · 定性分析
| 子项 | 组件 | 归属 |
|---|---|---|
| 保留时间 / 峰识别 | `peakIdentifier`（msd/csd/wsd）+ 模板峰标识 | ChemClipse API + 本仓库模板 |
| 组分表 / 目标物 | `targetIdentifier`（xxd 目标标识） | ChemClipse API |
| 标准品 / 谱库匹配 | **MassBank**、**CAS**、**FooDB**、**TGSC** 标识器；**AMDIS** 解卷积 | 本仓库 |

### 模块 6 · 定量分析
| 子项 | 组件 | 归属 |
|---|---|---|
| 内标 / 外标 / 校准 | `chromatogram.xxd.quantitation` + `peakQuantifierSupplier` 扩展点；templates 提供 **`StandardsAssigner`（ISTD 标准分配）** | ChemClipse API + 本仓库模板 |
| 浓度计算 / 校准曲线 | 定量引擎 | ChemClipse（外部）❓ 深度实现待获取源码 |
| 报告结果 | 见模块 8 | — |

### 模块 7 · 方法管理
- 处理方法（processing method）模型：`org.eclipse.chemclipse.processing`（✅ 被 44 个插件依赖，是方法链核心）。
- 方法编辑 UI：`org.eclipse.chemclipse.xxd.process.ui` + **`menu.icon` 扩展点（15 处挂接，方法链菜单）**。
- 方法参数设置页：18 个 `preferencePages`（本仓库各供应商提供自己的设置页，如"Template Processor"、"Mass Shift Detector Settings"、"Trace Compare Settings"、"CDK MS Tools"）。
- 模板化方法：`xxd.process.supplier.templates`（✅ 本仓库，模板即"方法"的载体）。

### 模块 8 · 报告
| 子项 | 组件 | 归属 |
|---|---|---|
| 色谱图 / 峰表 / 定量结果 | `chromatogram.xxd.report.chromatogramReportSupplier`（4 处挂接） | ChemClipse API + 本仓库 |
| CSV | `report.supplier.csv` | 本仓库 |
| Excel | `report.supplier.excel.template`（基于 Apache POI） | 本仓库 |
| PDF | `report.supplier.pdf.ui`（基于 PDFBox） | 本仓库 |

### 模块 9 · 自动化
- Sequence / Batch：❓ **本仓库无 sequence/batch 插件**；ChemClipse 是否提供批处理框架：待验证。
- 自动分析：处理流程（processing method chain）+ `menu.icon` 方法链可串接多步骤（峰检测→积分→定性→定量），可作为"自动分析"基础。
- 自动报告：报告器可在处理链中触发（模板 `StandardsAssigner` 展示了处理链内嵌定量的模式）。

### 模块 10 · 系统
| 子项 | 组件 | 归属 |
|---|---|---|
| 用户 / 权限 | ❓ 社区版未见用户管理插件（Eclipse 无内置）；需自研 | 缺口 |
| 配置 | 18 个 `preferencePages` + Eclipse preferences | 本仓库 + L4 |
| 日志 | `org.eclipse.chemclipse.logging`（✅ 51 个插件依赖）+ Eclipse `.metadata` 日志 | ChemClipse（外部） |
| 插件（扩展/更新） | p2（`equinox.p2.*`）+ **`net.openchrom.installer`（`pluginDiscovery` 插件发现安装）** | L4 + 本仓库 |

---

## 8. 工作站能力缺口（构建/改造时需补齐）

| # | 功能 | 状态 | 建议 |
|---|---|---|---|
| 1 | 数据采集（实时/设备通信） | ❌ 社区版没有 | 自研采集模块，输出 CDF/CSV 接入 |
| 2 | 实时曲线刷新 | ⚠️ SWTChart 具备绘图，实时数据推送需自研 | 自研 |
| 3 | 用户/权限管理 | ❌ 没有 | 自研或引入 RCP 安全框架 |
| 4 | Sequence/Batch 批处理 | ❓ 未确认 | 自研或用 processing method chain 模拟 |
| 5 | 核心算法源码 | ⚠️ 在 ChemClipse（外部） | 需要下载 ChemClipse 源码做深度逆向 |
| 6 | 谱库（NIST/AMDIS 完整版） | ⚠️ AMDIS 供应商存在，完整算法在 NIST | 仅调用接口 |

**已具备可直接用的能力**：多格式导入导出（CDF/GAML/AnIML/mz*）、峰检测（AMDIS/模板/一阶导数）、SG 平滑、基线扣除、梯形积分、峰标识（CAS/CDK/FooDB/TGSC/MassBank）、定量（内标分配）、报告（CSV/Excel/PDF）、插件式扩展框架。

---

## 9. 给后续 agent 的行动建议

1. **先决定路线**：A) 改造 OpenChrom（在 L2 层加插件、复用 ChemClipse 引擎）还是 B) 自研（参考架构与扩展点设计，复用 SWTChart + 数据模型思路）。
2. **若走 A**：核心工作 = 新增"数据采集插件"（挂 `chromatogramSupplier` 扩展点，实现 Import/Export Converter）+ 自定义"数据处理供应商"。参考现成模板 `net.openchrom.xxd.process.supplier.templates` 的完整写法。
3. **若走 B**：参照本文第 4 节扩展点体系设计解耦（转换器/峰检测/积分/标识/定量/报告全部插件化），参照第 3 节按检测器类型建域。
4. **必读源码锚点**（本仓库内）：
   - 转换器写法：`net.openchrom.msd.converter.supplier.cdf/plugin.xml` + `src/.../converter/ChromatogramImportConverter*.java`
   - 峰检测/标识/定量写法：`net.openchrom.xxd.process.supplier.templates/plugin.xml`
   - 产品/壳：`net.openchrom.rcp.compilation.community.ui`
   - 插件发现/安装：`net.openchrom.installer`
5. **补充资料**：获取 ChemClipse 源码（`org.eclipse.chemclipse.*`）用于核心算法；查看 target platform（`releng/net.openchrom.targetplatform/*.target`）确定可用外部库版本。

---

## 10. 证据与待验证清单

### 已确认（✅，本仓库内可复现）
- 68 个插件、37 个 feature、产品 = OpenChrom (Hillenkamp) 1.6.27 社区版
- 四层架构、ChemClipse 外部依赖、target platform 组成
- 全部扩展点清单（第 4 节）、插件分类清单（第 5 节）
- FSD = Fluorescence Spectroscopy（`filterName="Fluorescence Spectroscopy (*.gaml)"`）
- TSD 使用总信号（`getTotalSignal()`）；VSD 为光谱型域（`ISpectrumVSD`）
- 社区版无数据采集/仪器驱动代码

### 待验证（❓）
| # | 问题 |
|---|---|
| Q1 | VSD 全称（Voltammetric?）—— 需 ChemClipse 源码 |
| Q2 | TSD 全称（Total Signal?）—— 需 ChemClipse 源码 |
| Q3 | ChemClipse 是否内置 Sequence/Batch 批处理框架 |
| Q4 | ChemClipse 定量引擎（校准曲线/浓度计算）实现细节 |
| Q5 | ChemClipse 是否提供实时数据采集 API |
| Q6 | `xxd.converter.supplier.animl` 基础转换器与 `msd/csd/wsd` 各域 animl 的关系 |

---

*本文档由逆向工程主控文档（`docs/reverse-engineering/README.md`）的溯源纪律约束生成；与 00–10 系列文档互相补充。*
