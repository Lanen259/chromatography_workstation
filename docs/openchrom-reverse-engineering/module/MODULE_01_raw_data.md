# MODULE_01 — Raw Data（原始数据层 / 导入导出）

> **状态：🟡 分析中（导入/导出链路 ✅，ChemClipse 核心类内容 ✅ 已抓取）**
> 回答「数据从哪里来 / 数据如何保存」。
> 结论分级：✅ 源码确认 ｜ ⚠️ 背景假设（待验证）｜ ❓ 待验证
> 本次重写：深挖 CDF(netCDF) 读取/写入、TSD 多色谱、匹配器、GAML/AnIML/mz5/mzmlb/mzdb/mgf/cms/rdx3 各格式入口；**新增原生工程格式 `.ocb`（Open Chromatography Binary）完整解析（§7）**。

---

## 1. 本层职责

- 把磁盘上的各种色谱/质谱文件格式解析成内存模型（`IChromatogram`）
- 把内存模型写回文件（保存/导出）
- 统一所有供应商格式差异，让上层只面对一个模型接口

**关键设计事实（✅ 源码确认）**：Vendor 模型不是"中转 DTO"，而是**直接继承 ChemClipse 核心模型**（`VendorChromatogram extends AbstractChromatogramMSD`、`VendorScan extends AbstractScanMSD/AbstractScanCSD`、`VendorIon extends AbstractIon`）。Reader 解析出来的对象本身就是 `IChromatogram`/`IScan`，没有第二层转换代码。因此 "VendorScan 字段如何映射成 IScan" 的答案是：**继承即映射**（RT 一律以毫秒 int 存储；MSD 的质谱 = `addIon(IIon)` 列表；总信号由 ion 列表求和，CSD 则是单独字段）。

## 2. 核心机制：供应商扩展点（✅ 源码确认）

文件：`net.openchrom.msd.converter.supplier.cdf/plugin.xml` ✅

```xml
<extension point="org.eclipse.chemclipse.msd.converter.chromatogramSupplier">
  <ChromatogramSupplier
        description="Reads an writes ANDI/AIA CDF Chromatograms."
        exportConverter="...converter.ChromatogramExportConverter"
        fileExtension=".CDF"
        filterName="ANDI/AIA CDF Chromatogram (*.CDF)"
        id="net.openchrom.msd.converter.supplier.cdf"
        importContentMatcher="...FileContentMatcher"
        importConverter="...ChromatogramImportConverter"
        importMagicNumberMatcher="...MagicNumberMatcher"
        isExportable="true" isImportable="true">
  </ChromatogramSupplier>
</extension>
```

**机制要点（全部 ✅，从全部供应商插件 plugin.xml 逐一核对）：**
- 每种文件格式 = 一个插件 + 一个 `chromatogramSupplier` 扩展
- 供应商声明：文件扩展名 / 过滤名 / 导入转换器 / 导出转换器 / 两个匹配器（magic number + content）
- **扩展点家族按检测器分开（✅ 命名已逐一确认）**：
  - `org.eclipse.chemclipse.{msd,csd,wsd}.converter.chromatogramSupplier`
  - `org.eclipse.chemclipse.msd.converter.massSpectrumSupplier`（AnIML 质谱、库）
  - `org.eclipse.chemclipse.msd.converter.databaseSupplier`（mgf、cms、btmsp、microbenet — 库/谱库格式）
  - `org.eclipse.chemclipse.wsd.converter.scanSupplier`（WSD 光谱扫描）
  - **TSD（GCxGC）不走 plugin.xml，而是 OSGi 组件 `IConverterServiceTSD`**（`@Component` 注解，CSD/MSD CDF 各注册一个）✅
- **`net.openchrom.xxd.converter.supplier.gaml` 与 `.animl` 没有 plugin.xml**，它们是**共享基础库**（JAXB 模型 + io 工具），真正注册供应商的是各检测器插件（msd/csd/wsd/fsd/nmr/vsd）✅
- UI 的「打开文件」对话框通过 ChemClipse 核心 `org.eclipse.chemclipse.converter` 的 `ChromatogramConverterSupport`/`IConverterSupport` + `org.eclipse.chemclipse.processing.converter.ISupplierFileIdentifier` 枚举供应商 → 匹配器判断文件类型（这些核心类源码不在本机，**存在性 ✅ / 内容 ❓**）

## 3. 导入调用链（✅ 源码确认）

文件：`net.openchrom.msd.converter.supplier.cdf/src/.../converter/ChromatogramImportConverter.java` ✅

```text
ChromatogramImportConverter.convert(File, monitor)      // 入口
  ├─ super.validate(file) → IProcessingInfo           // 基类：null/空文件校验
  ├─ SpecificationValidator.validateSpecification(file) // 文件名规范化（见下）
  ├─ IChromatogramMSDReader reader = new ChromatogramReaderMSD()
  ├─ reader.read(file, monitor)   // ★ 产出 VendorChromatogram（直接是 IChromatogramMSD）
  └─ processingInfo.setProcessingResult(chromatogram)
```

**`SpecificationValidator`（✅，`internal/converter/SpecificationValidator.java`）**：
- `file.isDirectory()` → 追加 `\CHROMATOGRAM.CDF`
- 路径大写后非 `.CDF` 结尾 → 自动补 `.CDF`；以 `.` 结尾 → 补 `CDF`
- 仅修正文件名，不做内容校验

**Reader 内部格式嗅探（✅ `ChromatogramReaderMSD.isValidFileFormat`）**：读文件**前 3 字节** `new String(data).trim().equals("CDF")`——这正是 netCDF 魔数（"CDF" + 版本字节）。CSD 版相同（`ChromatogramReaderCSD.isValidFileFormat`）。**读不到 CDF 头 → 返回 null（无异常）**。

**接口分工（✅）**：
- 读取接口：`org.eclipse.chemclipse.msd.converter.io.IChromatogramMSDReader`（含 `read()` 与 `readOverview()`）；CSD 对应 `IChromatogramCSDReader`
- 基类：`org.eclipse.chemclipse.converter.chromatogram.AbstractChromatogramImportConverter`（含 `validate()`、`convertOverview()`）
- 结果封装：`IProcessingInfo<T>`（错误/消息/结果集）
- 每个供应商插件实现自己的 Reader

> UI 侧分发链（文件选择器 → `ChromatogramConverterSupport` → 匹配器 → ImportConverter）的**具体 UI 类**仍不在本仓库 → ❓（核心类在 ChemClipse `org.eclipse.chemclipse.converter`，见 R1）。

## 4. 保存调用链（✅ 源码确认）

模型侧证据（`.fetch/sources/model/IChromatogram.java`）✅：
- `getConverterId()` / `setConverterId(String)`：**转换器打开文件时记住自己的 id**，保存时据此回调对应的导出转换器
- `isFinalized()` / `setFinalized(boolean)`：finalized 的色谱不可被覆盖保存

**converterId 的实际写入点（✅ 源码确认）**：
- CDF CSD：`ChromatogramReaderCSD.setChromatogramEntries` → `chromatogram.setConverterId("net.openchrom.csd.converter.supplier.cdf")`
- CDF MSD：`ChromatogramReaderMSD.setChromatogramEntries` → `setConverterId(CONVERTER_ID)`（常量 = 插件 id）
- GAML：`chromatogram.setConverterId("")`（**空 id = 只读格式，不可导出**）✅
- TSD：`chromatogramTSD.setConverterId("")` ✅

**导出链（✅ 源码确认）**：

```text
ChromatogramExportConverter.convert(File, IChromatogram, monitor)
  ├─ SpecificationValidator.validateSpecification(file)   // MSD 版也补 .CDF
  ├─ super.validate(file) → IProcessingInfo
  ├─ chromatogram instanceof IChromatogramMSD ?            // 类型守卫
  ├─ IChromatogramMSDWriter writer = new ChromatogramWriter()
  ├─ writer.writeChromatogram(file, chromatogram, monitor) // 抛 IOException / FileIsNotWriteableException
  └─ processingInfo.setProcessingResult(file)
```

**原生保存格式（✅ 已深挖，见 §7）**：
- ChemClipse 有 `org.eclipse.chemclipse.xxd.converter.supplier.ocx` 插件，**注册的文件扩展名是 `.ocb`（Open Chromatography Binary），不是 `.ocx`**（plugin.xml + VersionConstants 确认）
- 内部是**版本化 Reader/Writer 家族**：`internal/io/ChromatogramReader_0701 ~ _1502`（CSD/WSD/MSD 各一套，共 20+ 版本）
- 完整结构、字段布局、版本演进见 §7（✅ 全部源码确认）

## 5. ★ CDF = netCDF 深挖（✅ 源码确认）

### 5.1 用到的 netCDF 概念

CDF（ANDI/AIA NetCDF）读写走 **UCAR netCDF Java 库**（`ucar.nc2.NetcdfFile` / `NetcdfFormatWriter`）✅。文件由三部分组成：**Dimension（维度）→ Variable（变量，带维度+类型）→ 全局 Attribute（属性）**。常量集中在 `io/support/CDFConstants.java`。

### 5.2 CSD 读取（单通道检测器，如 FID）

文件：`io/ChromatogramReaderCSD.java` + `io/support/AbstractCDFChromatogramArrayReader.java` + `CDFChromtogramArrayReader.java`（CSD）✅

**读取的关键变量/维度**：
| 类别 | netCDF 名称 | 作用 |
|---|---|---|
| 维度 | `point_number` | 扫描数（CSD 用这个；<2 个扫描抛 `NotEnoughScanDataStored`）|
| 变量 | `actual_delay_time`（标量 float）| 扫描延时 |
| 变量 | `actual_sampling_interval`（标量 float）| 扫描间隔 |
| 变量 | `actual_run_time_length`（标量 float，**可选**）| DataApex 变体：有它时 interval = (runTime-delay)/(scans-1) |
| 变量 | `ordinate_values`（float[point_number]）| 每扫描的信号强度 |
| 属性 | `retention_unit`（全局）| `seconds`/`Seconds`/`s` → ×1000；`minutes`→×60000；否则 ×1（ms）|
| 属性 | `operator_name` / `dataset_date_time_stamp` | 操作员 / 文件日期 |

**RT 重建（✅）**：`retentionTime = scanDelay + i * scanInterval`（ms），逐扫描累加；`scanInterval==0` 时兜底 200ms。每个扫描 = `new VendorScan(intensity)`，`setRetentionTime(rt)`，`addScan`。

**峰表（✅ `readPeakTable`）**：可选变量 `peak_name`(char[][]) + `peak_retention_time`(float[]) → 通过 `chromatogram.getScanNumber(rt)` 找到扫描号，给该扫描加 `IdentificationTarget`（`LibraryInformation` + `COMPARISON_RESULT_BEST_MATCH`）。

**概览模式（✅ 关键发现）**：CSD 的 `readOverview()` **直接调用 `readFile()`**——CSD CDF **没有概览裁剪**，与 MSD 不同。

### 5.3 MSD 读取（全扫描质谱）★ 重点

文件：`io/ChromatogramReaderMSD.java` + `io/support/AbstractCDFChromatogramArrayReader.java` + `CDFChromtogramArrayReader.java`（MSD）+ `CDFChromatogramOverviewArrayReader.java` ✅

**MSD CDF 用 "稀疏扁平数组 + 索引" 存储所有质谱**（每扫描峰数可变）：
| 变量 | 维度 | 含义 |
|---|---|---|
| `scan_acquisition_time` | double[scan_number] | 每扫描采集时间（秒） |
| `total_intensity` | double[scan_number] | 每扫描 TIC |
| `mass_values` | double/flt[point_number] | 所有扫描的 m/z 拼成的长数组 |
| `intensity_values` | float[point_number] | 对应强度 |
| `point_count` | int[scan_number] | 每扫描峰数 |
| `scan_index` | int[scan_number] | 每扫描在长数组里的起始偏移 |
| `scan_number`（维度） | — | MSD 用 `scan_number`（CSD 用 `point_number`）|

**质谱重建（✅ `CDFChromtogramArrayReader.getMassSpectrum(scan)`）**：
```text
peaks  = point_count[scan-1]
offset = scan_index[scan-1]
for i in 0..peaks-1:
    mz = mass_values[offset+i]; inten = intensity_values[offset+i]
    if inten > 0: massSpectrum.addIon(new VendorIon(mz, inten), false)
rt = scan_acquisition_time[scan-1] * 1000   // 秒→毫秒（硬编码×1000，不读 retention_unit）
```

**厂商兼容（✅ 注释原文）**：Shimadzu 写 `Ion→Double / Abundance→Int / PointCount→Int`；Agilent 写 `Ion→Float / Abundance→Float`。读取统一用 `get1DJavaArray(DOUBLE/FLOAT/INT)` 强制转换，兼容两者。

**scanDelay/scanInterval（✅ `AbstractCDFChromatogramArrayReader`）**：
- `getScanDelay()` = `scan_acquisition_time[0] * 1000`
- `getScanInterval()` = 前 `max(scans/(10*4),2)` 个间隔的平均 ×1000

**概览模式（✅ `readChromatogramOverview`）**：只读 `scan_acquisition_time` + `total_intensity`，每个扫描只放**一个 `VendorIon(IIon.TIC_ION, tic)`**（`IIon.TIC_ION` 是 ChemClipse 约定的 TIC 哨兵值）。**省掉了 mass_values 长数组的读取与逐峰重建**——这是 MSD CDF 的概览裁剪策略。

### 5.4 写入（导出）— 写出哪些变量 ✅

文件：`io/ChromatogramWriter.java`（MSD）+ `ChromatogramWriterCSD.java`（CSD）+ `io/support/{DimensionSupport, ScanSupport, AttributeSupport, VariableSupport, DataEntry}.java` ✅

- 用 `NetcdfFormatWriter.createNewNetcdf3(path)` 写 netCDF-3
- **MSD 写出的变量清单**（`ChromatogramWriter.writeCDFChromatogram`）：`error_log`(char)、`a_d_sampling_rate`(double，填 -9999)、`a_d_coaddition_factor`(short，-9999)、`scan_acquisition_time`(=RT/1000 秒)、`scan_duration`(-9999)、`inter_scan_time`(-9999)、`resolution`(-9999)、`actual_scan_number`(=i)、`total_intensity`(=getTotalSignal)、`mass_range_min/max`(=扫描最低/最高离子)、`time_range_min/max`(-9999)、`scan_index`、`point_count`、`flag_count`(0)、`mass_values`/`time_values`/`intensity_values`、instrument_* 系列(char，空)
- **`ScanSupport`（✅）**：预扫描一遍计算 `scan_index[i]`（前缀和累计离子数）与 `point_count[i]`、每扫描 min/max m/z
- **`addVariableScanValues()`（✅ 细节）**：把每扫描的 ion 列表顺序铺进 `mass_values`/`intensity_values`；`time_values` 填 `NULL_VALUE_TIME=9.96921E36f`（**未实现**，源码注释承认 "F-Search could not show ion values correctly"）
- **CSD 写出**：`ordinate_values`（=各扫描 `getTotalSignal()`）+ 标量 `actual_delay_time`/`actual_sampling_interval`（=RT/1000 秒）+ 全局属性（`AttributeSupport`：`dataset_completeness=C1+C2`、`aia_template_revision=1.0.1`、`netcdf_revision=2.3.2`、`retention_unit=Seconds` 等）
- 日期格式：`DateSupport` 用 `SimpleDateFormat("yyyyMMddHHmmssZ", Locale.ENGLISH)`，例 `20080630140216+0200`（✅）

## 6. ★ TSD 多色谱机制（GCxGC）✅

**注册方式不同**：不走 plugin.xml，而是 OSGi 组件 `IConverterServiceTSD`（`@Component(service = {IConverterServiceTSD.class})`）：
- CSD：`ChromatogramImportConverterTSD` → `getId=net.openchrom.csd.converter.supplier.cdf`，`getFileExtension()=".cdfx"`，描述 "GCxGC-FID (CDF)"
- MSD：`ChromatogramImportConverterTSD` → `getFileExtension()=".cdfy"`，描述 "GCxGC-MS (CDF)"
- 两者 `getImportConverter()` 返回各自的 `ChromatogramReaderTSD`；`getExportConverter()` 返回 null（**TSD 只读不可导出**）；FileContentMatcher 恒返回 true

**读取算法（✅ `ChromatogramReaderTSD.readChromatogram`，CSD 与 MSD 同构）**：
```text
1. 用普通 1D Reader（CSD/MSD）把 .cdfx/.cdfy 读成 1D 色谱
2. modulationTime = PreferenceSupplier.getModulationTime2D()   // 默认 10000 ms (10 s)
3. 遍历 1D 扫描：delta = scan.RT - offset
   - delta < modulationTime → 信号追加进当前 signals 列表（视为同一次 2D 调制）
   - 否则 → 把 signals 作为一帧：new ScanTSD(rt=该扫描RT, signals=float[])，addScan；offset=该RT
4. 结果：每个 ScanTSD 的 signals[] = 第二维色谱（column 2）的扫描信号
```

**结论（✅）**：
- **「一个文件含多条色谱」的 TSD 语义 = 把 1D 连续扫描按调制周期切帧成 2D 矩阵**，不是"多条独立色谱"
- TSD 色谱只保留**总信号**（`scan.getTotalSignal()`），**丢掉质谱信息**（MSD TSD 也如此）
- `VendorChromatogramTSD extends AbstractChromatogramTSD`，`getTypeTSD()` 返回 `TypeTSD.GCxGC_MS`，轴标签 "Retention Time Column 1 [min]" / "Column 2 [scans]"（CSD 版）
- `IChromatogram.getReferencedChromatograms()` 的"多色谱"另有实例：**GAML** 一个文件可含多个 `<Experiment>`，`ChromatogramReaderVersion120.read()` 把第 2+ 个色谱 `addReferencedChromatogram` 挂到第 1 个上（✅）

## 7. ★ 原生工程格式 `.ocb`（Open Chromatography Binary）✅ 全量源码确认

> 插件：`chemclipse-src/plugins/org.eclipse.chemclipse.xxd.converter.supplier.ocx/`
> **关键澄清：插件名叫 "ocx"，但文件扩展名是 `.ocb`，不存在 `.ocx` 文件**（plugin.xml 三处 `fileExtension=".ocb"` + `VersionConstants.FILE_EXTENSION_CHROMATOGRAM=".ocb"`）。`.ocm`（处理方法文件）由同一插件注册（`processMethodSupplier` 扩展点）。本节的 `.ocb` 同时支持 MSD / CSD / WSD 三种检测器。

### 7.1 物理结构：ZIP 容器 + 逐 entry 二进制流（✅）

- **不是纯二进制，不是 XML**：整体是一个标准 **ZIP 文件**（`java.util.zip.ZipFile` / `ZipOutputStream`），每个 Zip entry = 一个"逻辑文件"，entry 内容用 `DataOutputStream` 按**大端**写原始类型（`Format.CHROMATOGRAM_COMPRESSION_TYPE = ZipOutputStream.DEFLATED`）。
- **压缩级别可配**：`PreferenceSupplier.P_CHROMATOGRAM_COMPRESSION_LEVEL`，默认 1（0-9，折中体积/速度）；`.ocm` 默认 0（`METHOD_COMPRESSION_LEVEL`，不压缩）。
- **无文件头魔数**：格式校验 = 打开 ZIP 后找名为 `VERSION` 的 entry，读字符串（见 7.3）比对精确版本号。`isValidFileFormat()`（各版本化 Reader）只认完全相等的版本字符串。`FileContentMatcher{MSD,CSD,WSD}` 则扫描 ZIP 目录条目存在性：MSD 认 `MSD/CHROMATOGRAM/`（旧版 `CHROMATOGRAM/`），CSD 认 `CSD/CHROMATOGRAM/` 或 `FID/CHROMATOGRAM/`，WSD 认 `WSD/CHROMATOGRAM/`。
- **`SpecificationValidator`**（✅）：目录 → 追加 `\CHROMATOGRAM.ocb`；不以 `.ocb` 结尾 → 自动补 `.ocb`；以 `.` 结尾 → 补 `ocb`。与 CDF 的 `SpecificationValidator` 同模式。

### 7.2 ZIP entry 命名清单（✅ `settings/Format.java`）

| 常量 | entry 名 | 内容 | 版本 |
|---|---|---|---|
| `FILE_VERSION` | `VERSION` | 版本字符串（int 长度 + UTF-16BE） | 全部 |
| `FILE_TIC_MSD` | `MSD/OVERVIEW/TIC` | 概览（RT+totalSignal） | ≥1.0.0.1；旧版 `OVERVIEW/TIC` |
| `FILE_TIC_WSD` | `WSD/OVERVIEW/TIC` | 概览（totalSignal+RT） | ≥1.0.0.1 |
| `FILE_SYSTEM_SETTINGS_MSD` | `MSD/CHROMATOGRAM/SYSTEM_SETTINGS` | IMethod 方法字段 | ≥1.0.0.5 |
| `FILE_SCANS_MSD` | `MSD/CHROMATOGRAM/SCANS` | 全部扫描质谱 | ≥1.0.0.1（旧 `CHROMATOGRAM/SCANS`） |
| `FILE_SCANPROXIES_MSD` | `MSD/CHROMATOGRAM/SCANPROXIES` | 扫描代理索引 | ≥1.0.0.3 |
| `FILE_BASELINE_MSD` | `MSD/CHROMATOGRAM/BASELINE` | 基线 | ≥0.8.0.3 |
| `FILE_PEAKS_MSD` | `MSD/CHROMATOGRAM/PEAKS` | 峰表 | 全部 |
| `FILE_AREA_MSD` | `MSD/CHROMATOGRAM/AREA` | 积分条目 | 全部 |
| `FILE_IDENTIFICATION_MSD` | `MSD/CHROMATOGRAM/IDENTIFICATION` | 色谱级识别目标 | 全部 |
| `FILE_HISTORY_MSD` | `MSD/CHROMATOGRAM/HISTORY` | 编辑历史 | 全部 |
| `FILE_MISC_MSD` | `MSD/CHROMATOGRAM/MISC` | header map + 显示设置 + finalized | 全部 |
| `FILE_SEPARATION_COLUMN_MSD` | `MSD/CHROMATOGRAM/SEPARATION_COLUMN` | RI 表 + 色谱柱 | ≥1.3.0.0 |
| `FILE_REFERENCE_INFO` | `REFERENCE_INFO` | 引用色谱数量（int） | ≥1.3.0.0 |
| `CHROMATOGRAM_REFERENCE_{i}/CHROMATOGRAM_TYPE` | 引用色谱类型 + 嵌套条目 | ≥1.3.0.0 |
| CSD/WSD | `CSD/...`、`WSD/...` | 同上换前缀 | — |

> 目录前缀演进：`0.9.0.3` 及之前直接 `CHROMATOGRAM/...`、`OVERVIEW/TIC`（还出现过 legacy `FID/CHROMATOGRAM/...`）；`1.0.0.1` 起改成检测器前缀 `MSD/CHROMATOGRAM/...`、`MSD/OVERVIEW/TIC`。

### 7.3 基础序列化原语（✅ `IFileHelper` + `AbstractIO_1502` + `ReaderHelper`）

- **字符串**：`int length` + `length × char`（`DataOutputStream.writeChars` = **UTF-16BE**，每字符 2 字节）；`null` → `writeInt(-1)`。读端同构（`readInt` <0 返回 null）。
- **字符串集合**：`int size`（null → -1）+ 每个字符串。`IFileHelper.writeStringCollection/readStringCollection`。
- **标量**：全部 Java 原始类型（`DataOutputStream` 大端）：`boolean=1B`、`short=2B`、`int=4B`、`long=8B`、`float=4B`、`double=8B`。
- **枚举**：按枚举 `name()` 的字符串写（如 PeakType、MassSpectrumType(short 0/1)、SeparationColumnType）。读端 `valueOf` 失败有默认值兜底。

### 7.4 MSD 读取流程与字段布局（✅ `ChromatogramReader_1502` + `ReaderProxy_1502` + `ReaderIO_1502`）

**读取入口链**：`ChromatogramImportConverter.convert → validate → SpecificationValidator → new ChromatogramReaderMSD().read`；`ChromatogramReaderMSD` 用 `ReaderHelper.getVersion(file)`（ZipInputStream 扫到 `VERSION` entry 读字符串）→ `getChromatogramReader(version)` 分派到 `ChromatogramReader_0701~1502` 等（21 个分支）。

**`ChromatogramReader_1502.readZipData` 读取顺序**（每个 entry 独立 DataInputStream，缺失即抛 IOException；`REFERENCE_INFO` 读取包 try-catch，缺了只记日志）：
1. `SYSTEM_SETTINGS` → `IMethod`：instrumentName(str)、ionSource(str)、samplingRate(double)、solventDelay(int)、sourceHeater(double)、stopMode(str)、stopTime(int)、timeFilterPeakWidth(int)
2. `SCANS` → `int 扫描数` + 每扫描一个质谱（见下）
3. `BASELINE` → `int 扫描数` + `int 模型数` + 每模型：`baselineId(str)` + 每扫描 `(RT int, 背景丰度 float)` → 逐段 `addBaseline(startRT,stopRT,startBg,stopBg,false)`
4. `PEAKS` → `int 峰数` + 每峰（见下）
5. `AREA` → 色谱积分：`integratorDescription(str)` + 积分条目列表 + 背景 `integratorDescription(str)` + 背景积分条目
6. `IDENTIFICATION` → 色谱级识别目标列表（`ReaderIO_1502.readIdentificationTargets`，布局见下）
7. `HISTORY` → `int 条数` + 每条 `(long 时间戳, str 描述)`；描述以 `ProcessSupplierEntry_` 开头者用 `ProcessSupplierSupport` 恢复成 `ProcessSupplierEntry`（Base64 载荷：id/name/description/userSettings 四段）
8. `MISC` → `int 条数` + 每对 `(key,value)` 进 headerData；然后 `TargetDisplaySettings`（见下）+ `finalized(boolean)`
9. `SEPARATION_COLUMN` → `int RI 条目数` + 每 `(name(str), RT int, RI float)` + `SeparationColumn`（name/type/packaging/calculationType/length/diameter/phase/thickness 全字符串）
10. `REFERENCE_INFO` + 嵌套引用色谱（见 7.6）
最后 `setAdditionalInformation`：`setConverterId("org.eclipse.chemclipse.xxd.converter.supplier.chemclipse")`、`setFile(file)`、`ChromatogramSupport.calculateScanIntervalAndDelay`。

**MSD 扫描（每扫描一个质谱，v1502 布局）**：
```text
short  massSpectrometer          // MS 级（枚举 MassSpectrometer{MS1,MS2,MS3} 序号，序值待验证 ⚠️）
short  massSpectrumType          // 0=centroid, 1=profile
double precursorIon              // MS/MS 前体，MS1 为 0
int    retentionTime             // 毫秒
int    relativeRetentionTime
int    retentionTimeColumn1      // GCxGC
int    retentionTimeColumn2
float  retentionIndex
bool   hasAdditionalRetentionIndices
  (true) int size; 每项: str columnType(旧名 POLAR/APOLAR/SEMIPOLAR) + float RI
int    timeSegmentId
int    cycleNumber
int    numberOfIons
  每离子:
    double mz; float abundance
    int transition                // 0=普通离子，非 0=MSn
    (非 0) str compoundName; double q1Start,q1Stop,q3Start,q3Stop;
           double collisionEnergy; double q1Resolution,q3Resolution;
           int transitionGroup; int dwell
  识别目标列表（见下）            // v1502 扫描级 targets
```
随后（SCANS 内每个扫描之后）写 `bool hasOptimizedMassSpectrum`，true 则再写一个上面的"普通质谱"（无 massSpectrometer 头）。**注意：v1004 起才加入 timeSegmentId/cycleNumber 与 optimizedMassSpectrum。**

**识别目标（`WriterIO_1502.writeIdentificationTarget` / `ReaderIO_1502`，v1502）**：
```text
int count; 每个:
  str identifier; bool verified
  int retentionTime; float retentionIndex
  int casNumbers.size + 每 str casNumber
  str comments; str referenceIdentifier; str miscellaneous; str database; int databaseIndex
  str contributor; str name
  int synonyms.size + 每 str
  str formula; str smiles; str inChI; str inChIKey
  double molWeight; double exactMass; str moleculeStructure
  int columnIndexMarkers.size + 每: float retentionIndex + SeparationColumn
  int flavorMarkers.size + 每: bool verified + str literatureReference/matrix/odor/samplePreparation/solvent + odorThresholds
  float matchFactor; float matchFactorDirect; float reverseMatchFactor;
  float reverseMatchFactorDirect; float probability; bool isMatch; float inLibFactor
```

**峰（v1502 布局，`readPeak`）**：
```text
str detectorDescription; str quantifierDescription; bool activeForAnalysis
str integratorDescription; str modelDescription; str peakType; int suggestedNumberOfComponents
str 旧 classifier 占位(兼容 2020/09/11);  str 集合 classifiers
bool strictModel                              // v1502 起可写 false；v1501 及以前恒 true
float startBackgroundAbundance; float stopBackgroundAbundance
质谱（=峰最大点，布局同 7.4 扫描，含 massSpectrometer 头）
int numberOfRetentionTimes; 每 (int RT, float relativeIntensity)   // PeakIntensityValues
int 积分条目数; 每 (double m/z, double integratedArea)
识别目标列表（峰级）
int 定量条目数; 每个:
  str name; str chemicalClass; double concentration; str concentrationUnit;
  double area; str calibrationMethod; bool usedCrossZero; str description;
  str quantitationFlag(name); str group; int signals.size + 每 double   // v1500 起多信号
bool hasOptimizedMassSpectrum（true → 再写一个普通质谱）
int internalStandards.size + 每: str name; double concentration; str unit; double compensationFactor; str chemicalClass
int quantitationReferences.size + 每 str
```
**TargetDisplaySettings（MISC 内）**：bool showPeakLabels、bool showScanLabels、int collisionDetectionDepth、int rotation、str libraryField、str displayOption、int map.size + 每 (str key, bool value)。

**峰/色谱积分条目（`readIntegrationEntries`）**：MSD 每条目 = `(double ion=m/z, double integratedArea)`；**CSD/WSD 每条目只有 `double integratedArea`（无 m/z）**。

### 7.5 CSD 与 WSD 差异（✅ `ChromatogramReader_1502` CSD/WSD 版 + Writer）

- **CSD 扫描**：`RT int, relativeRT int, totalSignal float, RT1 int, RT2 int, RI float, [typedRIs], timeSegmentId int, cycleNumber int` + 识别目标列表。**无离子、无 SCANPROXIES、无 optimizedMassSpectrum**。CSD 峰最大点 = 同 CSD 扫描布局。
- **WSD 扫描**：先写 `int scanSignals.size` + 每 `(double 波长, float 吸光度)`，然后 RT 字段组（RT/relativeRT/RT1/RT2/RI/[typedRIs]）+ `float totalSignal` + `timeSegmentId/cycleNumber` + 识别目标。**波长以 double 写（TODO 注释：下版本要改 float）**。WSD 峰最大点 = RT 字段组（无信号数组，只有 totalSignal）。
- **概览**：MSD/WSD 有 `OVERVIEW/TIC`；WSD 概览每扫描顺序为 `float totalSignal` 然后 `int RT`（**与 MSD 的 `int RT` 然后 `float` 顺序相反**）。CSD **没有** OVERVIEW/TIC entry，CSD `readOverview` = **全量 `readFromZipFile`**（与 CDF CSD 行为一致，无概览裁剪）。
- **WSD 读取 SEPARATION_COLUMN 的 `copyFrom` 方向与 MSD/CSD 相反**（`separationColumnSource.copyFrom(sink)`），疑似上游 bug（不影响字段存在性，⚠️ 仅在 WSD 版）。

### 7.6 概览模式（readOverview）与懒加载（✅）

- **MSD/WSD `readOverview`**：只读 `OVERVIEW/TIC` entry——`int 扫描数` + 每扫描（RT + totalSignal），每个扫描只放一个 `VendorIon(IIon.TIC_ION, totalSignal)`。**不读 SCANS/峰/基线等**。CSD 则全量读。
- **懒加载扫描代理（MSD ≥1.0.0.3，默认关闭 `useScanProxies=false`）**：
  - 写入时（`ChromatogramWriter_1502.writeChromatogramScans`）先写 SCANS 完整质谱，同时收集 `ScanProxy{offset(写入该扫描前的字节偏移), RT, numberOfIons, totalSignal, RI, timeSegmentId, cycleNumber}` 写进 `SCANPROXIES` entry。
  - 读取时若 `useScanProxies=true`：只读 SCANPROXIES，构造 `VendorScanProxy extends AbstractRegularMassSpectrumProxy`（不读离子）。首次访问离子 → `importIons()` → `ProxyReaderMSD` 按版本选 `ReaderProxy_1502` → `ZipFile.getInputStream(SCANS entry).skipBytes(offset)` 随机定位，读该扫描的完整质谱（含 optimizedMassSpectrum）。
  - `ChromatogramReaderMSD.read` 末尾：若启用 `LOAD_SCAN_PROXIES_IN_BACKGROUND` 且文件 >2MB，起后台线程 `enforceLoadScanProxies`（源码注释警告并发删除扫描会 `ConcurrentModificationException`，故默认关闭）。
- **写入前导出**：`ChromatogramWriterMSD.writeChromatogram` 先 `chromatogram.enforceLoadScanProxies(monitor)` 确保代理全部物化再写。

### 7.7 版本演进（✅ `versions/` + `settings/Format.java` + 逐版本 diff）

版本号格式 `主.次.修订.发布`（如 `1.5.0.2`）。命名惯例：每个主版本一个科学家代号。**`Format` 中共 21 个版本常量，当前最新 `1.5.0.2`（McLafferty v3）**，`CHROMATOGRAM_VERSION_LATEST` 即写入版本；`ChromatogramVersion` 枚举只收录 11 个命名发布版（V_0701/V_0803/V_0903/V_1004/V_1100/V_1300/V_1301/V_1400/V_1500/V_1501/V_1502），供导出设置选择。

| 版本常量 | 值 | 代号 | 相对上一版本的变化（均从 Reader diff 确认） |
|---|---|---|---|
| 0701 | 0.7.0.1 | Nernst | 最初版：无基线/定量/扫描目标；质谱头为字符串 |
| 0801 | 0.8.0.1 | — | +峰定量条目（legacy 单 signal：bool + double） |
| 0802 | 0.8.0.2 | — | +MS/MS 离子跃迁（m/z+abundance 后 int transition） |
| 0803 | 0.8.0.3 | Dempster | +基线（BASELINE entry，单模型） |
| 0901 | 0.9.0.1 | — | +识别目标 formula/molWeight |
| 0902 | 0.9.0.2 | — | +synonyms 集合 |
| 0903 | 0.9.0.3 | Mattauch | +suggestedNumberOfComponents |
| 1001 | 1.0.0.1 | — | **目录重构：`MSD/` 前缀**（MSD/CHROMATOGRAM、MSD/OVERVIEW/TIC） |
| 1002 | 1.0.0.2 | — | +扫描级识别目标（readMassSpectrumIdentificationTargets） |
| 1003 | 1.0.0.3 | — | +SCANPROXIES 懒加载索引；共享 ReaderProxy |
| 1004 | 1.0.0.4 | Aston | +timeSegmentId/cycleNumber；+optimizedMassSpectrum（bool+质谱） |
| 1005 | 1.0.0.5 | — | +SYSTEM_SETTINGS（IMethod）；+离子跃迁 compoundName/dwell |
| 1006 | 1.0.0.6 | — | +峰 quantifierDescription/activeForAnalysis；识别扩展 referenceIdentifier/database/contributor/manualVerify/forwardMatchFactor |
| 1007 | 1.0.0.7 | — | +SMILES/InChI；comparisonResult 改为 matchFactorDirect/reverseMatchFactorDirect/isMatch |
| 1100 | 1.1.0.0 | Diels | 内容同 1007，仅进度 SubMonitor 化 |
| 1300 | 1.3.0.0 | Dalton v1 | +SEPARATION_COLUMN（RI 表+柱）；+REFERENCE_INFO/引用色谱嵌套 |
| 1301 | 1.3.0.1 | Dalton v2 | classifier 字符串→字符串集合（保留旧占位读） |
| 1400 | 1.4.0.0 | Lawrence | 基线单模型→**多模型**（+模型数+baselineId 前缀） |
| 1500 | 1.5.0.0 | McLafferty v1 | 定量条目单 signal→**多信号列表**；+QuantitationFlag |
| 1501 | 1.5.0.1 | McLafferty v2 | 识别目标重构到共享 `ReaderIO_1501`（+columnIndexMarkers/flavorMarkers/inLibFactor/InChIKey/exactMass/moleculeStructure/databaseIndex） |
| 1502 | 1.5.0.2 | McLafferty v3 | 峰 strictModel 布尔化（不再恒 true） |

> **检测器版本覆盖范围（目录清单确认）**：MSD 最全（0701~1502 全套）；CSD 从 1001 起步；WSD 从 1005 起步——0701~0903 仅 MSD 有。

### 7.8 `.ocb` 附带机制（✅）

- **引用色谱**（≥1.3.0.0）：`REFERENCE_INFO` = `int 数量`；每个引用色谱一个目录 `CHROMATOGRAM_REFERENCE_{i}/`，内含 `CHROMATOGRAM_TYPE`（字符串 `MSD`/`CSD`/`WSD`）和嵌套的一套完整色谱 entry（写入时递归调用对应 Writer 的 `writeChromatogram(zip, prefix, ...)`；读取时按类型递归 `ChromatogramReader{MSD,CSD,WSD}.read(zipFile, directory, ...)`）。**ZipInputStream 流式读取不支持嵌套引用（源码注释承认），只能 ZipFile 随机读。**
- **可选"引用色谱单独导出"**：`PreferenceSupplier.isChromatogramExportReferencesSeparately()`（默认关）→ `ChromatogramReferencesSupport.exportReferences` 把每个引用色谱另存为 `原名_{MSD|CSD|WSD}_{header值}.ocb`（header 字段默认 DATA_NAME）。
- **峰 supplier**：`plugin.xml` 另注册 `msd.converter.peakSupplier`（`PeakImportConverter`，`isExportable=false`）→ `PeakReaderMSD` 只读 `MSD/CHROMATOGRAM/PEAKS` entry 建 `IPeaksMSD`（不读扫描）。`PeakReader_1502` 用独立 `IonTransitionSettings`。
- **converterId 归属**：`CONVERTER_ID_CHROMATOGRAM = "org.eclipse.chemclipse.xxd.converter.supplier.chemclipse"`、`...processMethodSupplier`（.ocm）、`...peaks`。读取后 `setConverterId` → 上层"另存为"自动回到 .ocb。
- **深拷贝过程（OSGi `Procedure` 组件 `ChromatogramProcedure`）**：把色谱先导出成临时 `.ocb` 再导入，实现"Copy Chromatogram Selection"（供子流程处理）。
- **压缩/设置入口**：`ConverterProcessSupplier`（OSGi `IProcessTypeSupplier`）把压缩级别等偏好接到系统设置 UI。

### 7.9 `.ocm`（Process Method 处理方法文件，同一插件）✅

`plugin.xml` 注册 `converter.processMethodSupplier`（fileName=`ProcessMethod`，filter "Process Method (*.ocm)"）。`MethodImportConverter` 按 `1402 → 1401 → 1004 → 1003 → 1001 → 1000` 顺序逐个尝试，返回 null 就试下一个。

- **旧版（v0.0.0.1 / v0.0.0.2，`MethodWriter_1000/1001` → `AbstractMethodWriter`）**：**ZIP 容器**，含 `VERSION`（int+UTF-16BE）与 `PROCESS_METHOD` 两个 entry。`PROCESS_METHOD` 用 DataInputStream 序列化：`operator(str)`、`description(str)`、`int 条目数`，每个条目 `processorId/name/description/settings`（各 str）+ `int 类别数` + 每类别 str + 2 个占位 str（旧兼容）。
- **新版（v0.0.0.3 / 1.4.0.0 / 1.4.0.1 / 1.4.0.2，`MethodReaderWriter_1003/1004/1401/1402` → `ObjectStreamMethodFormat`）**：**不是 ZIP**——明文流以魔数 `"MTH." + 版本字符串`（UTF-8）开头，随后 `GZIP` 压缩的 **Java 对象流（`ObjectInputStream`）**载荷。字符串为 `writeObject(String)`（Java 序列化 TC_STRING 格式），枚举同。载荷字段：dataCategories、UUID、name、description、category、operator、supportResume、profiles、metaData map、processEntry 树、final。**Qt 移植若要读新版 .ocm 需要实现一个 Java 序列化（ObjectInputStream 头部 `AC ED 00 05` + TC_* 标记）解码器。**
- `MethodVersion` 枚举只列 1400(Lawrence v4)/1401(v5)/1402(v6)，0001~0003 已被注释掉。
- 方法匹配器：`MagicNumberMatcher` 查 `.ocm` 扩展名；`FileContentMatcher` 恒返回 true（不做内容嗅探）。



## 8. 匹配器（✅ 源码确认）

**`MagicNumberMatcher`（魔数/扩展名匹配，extends `AbstractMagicNumberMatcher`）**：
- CDF MSD：`checkFileExtension(file, ".cdf")` 或 `.cdfy`（GCxGC-MS）→ true
- CDF CSD：`.cdf` 或 `.cdfx`（GCxGC-FID）→ true
- GAML（xxd 版）：`.gaml`；MGF：`.mgf`
- **只查扩展名，不读文件内容**（基类 `checkFileExtension` 的行为；本机只见到子类实现）

**`FileContentMatcher`（内容嗅探，extends `AbstractFileContentMatcher`）**：
- **MSD CDF（✅）**：用 `NetcdfFiles.open()` 真正打开文件，`findVariable("mass_values") != null` → 是 MSD 文件（"If mass values are stored, assume that it is a MSD file"）
- **CSD CDF（✅）**：更精细——全局属性 `separation_experiment_type`：若含 "Liquid Chromatography" → false（排除 LC）；若 = "Gas Chromatography"/"gas_chromatography" 且无 `mass_values` → true；全局属性 `detector_name` = "flame ionization" → true；兜底：无 `mass_values` 就按 FID 处理返回 true
- 逻辑顺序体现**MSD 与 CSD 的互斥嗅探**：MSD 靠"有质谱"，CSD 靠"无质谱 + 实验类型/检测器名"

**两个匹配器的分工（✅ 推断，注释原文佐证）**：魔数匹配用于文件对话框的粗过滤（快），内容匹配用于打开时精确认证（慢，真读文件头）。

## 9. 其他格式族入口（✅ 源码确认，抽样）

| 格式 | 形态 | 读取入口 | 模型构建要点 |
|---|---|---|---|
| **GAML** | XML（JAXB 模型 v100/110/120）+ 二进制值 | `ChromatogramReader.getReader` 读前 100 字符嗅探 `Reader1XX.VERSION` 版本号 → 分派版本化 Reader；`ChromatogramReaderVersion120` | Technique.CHROM 的 Xdata=RT 轴，Technique.MS 的 Xdata(m/z)+Ydata(强度)；`Reader120.parseValues` 按 `values.format`(FLOAT64/32)+`byteorder`(INTEL) 解码；`convertToMiliSeconds(rt, unit)`：秒×1000/分×60000；一文件多 `<Experiment>` → 第 1 个为主、其余 `addReferencedChromatogram`；`readOverview` 只读名字/日期/limsID |
| **AnIML** | XML（ASTM AnIML 模型）+ base64 二进制段 | `ChromatogramReader.read`（Common.getAnIML 解 XML）| `readSample`/`readMethod`→元数据；`readRetentionTime`（Separation Monitoring→Time 轴，autoIncrement 给 scanDelay/scanInterval）；`readTotalIonCurrent`（Mass Spectrum Time Trace→TIC）；`readMassSpectra` 把 m/z+Intensity 序列写回已有扫描（`getScan(spectra)`）；`readPeakTable`（Start/End Time→`PeakBuilderMSD.createPeak` 建峰+IdentificationTarget）；二进制解码在 `BinaryReader.decode{Int,Float,Double}Array`（小端）；`readOverview` 只读 sample 信息 |
| **mz5** | HDF5（单文件）| `ChromatogramReader.read`（HDF5Factory）| CV 本体（accession 码：scan start time=1000016、ms level=1000511 等）；TIC 从 `CHROMATOGRAM_{TIME,INTENSITY,INDEX}` 读；**全量扫描用 `VendorScanProxy` 懒加载**（见 §10）；m/z 存**增量/差分**，`ReaderProxy` 用 `mz += mzs[o]` 累加重建（✅ 注释原文 "first m/z value and then deltas"）|
| **mzMLb** | 单文件 HDF5，内含 `mzML` XML 数据集 | `ChromatogramReader.read`（读 HDF5 的 "mzML" 字节 → 解析 XML 元数据）| 二进制 m/z+intensity 数组 + `ScanMarker(offset,length)` → `VendorScanProxy`；`ReaderProxy` 首次访问才读 HDF5 数组（懒）|
| **mzDB** | SQLite 数据库 | `ChromatogramReader.read`（JDBC `org.sqlite.JDBC`）| SQL 查询 `mzdb`（creation_timestamp×1000→ms）、`sample`（名字）、`spectrum`、`scans` 表；版本 >0.7 警告不兼容；**readOverview 返回 null** |
| **MGF** | 文本（Mascot Generic Format）| `MassSpectraReader.read`（BufferedReader 逐行 + 正则）| `BEGIN IONS`/`END IONS` 分谱；`TITLE=` 标识、`RTINSECONDS=` → ×1000 ms、`PEPMASS=` 前体、`m/z 强度` 行建 `Ion`；库格式走 `msd.converter.databaseSupplier` |
| **CMS** | 文本（校准质谱库）| `MassSpectrumReader.read`（正则预编译）| `CONVERTER_ID="net.openchrom.msd.process.supplier.cms"`（注释原文：转换器 id 用于扩展点机制）；CAS/COMMENTS 正则解析 → `CalibratedVendorLibraryMassSpectrum`；数据库 supplier |
| **RDX3（导出）** | **R 语言 `.RData` 序列化**（GZIP + SEXP）| 仅导出：`ChromatogramExportConverterMSD`（注册到 msd.converter.chromatogramSupplier，isImportable=false）| `ChromatogramWriter.export`：魔数 "RDX3" + SEXP 头；数据列 = `RT`(ms/60000→min) + `RI` + 每个整数 m/z 一列（`IExtractedIonSignal.getAbundance(mz)`）；class 段标 `tbl_df/tbl/data.frame`——即导出成 **R data.frame 供下游 R 分析** |

> 本机插件目录里其余格式（btmsp/muf/pkf/microbenet/arw/abif/axr/wsd.cdf 等）本次未逐文件读源码，仅 plugin.xml 级确认（见 §10 清单），读取策略可推测与同类相似（⚠️）。

## 10. 支持格式清单（✅ plugin.xml 逐项核对）

### 10.1 本仓库（OpenChrom 社区版）— 各供应商注册的扩展点/格式

| 检测器/扩展点 | 插件 | 扩展名 | filterName | 导入 | 导出 |
|---|---|---|---|---|---|
| msd.chromatogramSupplier | cdf | .CDF | ANDI/AIA CDF Chromatogram | ✅ | ✅ |
| msd.chromatogramSupplier | gaml | .gaml | GAML MSD Chromatogram | ✅ | ❌ |
| msd.chromatogramSupplier | animl | .animl | AnIML MSD Chromatogram | ✅ | ✅ |
| msd.chromatogramSupplier | mz5 | .mz5 | mz5 Chromatogram | ✅ | ✅ |
| msd.chromatogramSupplier | mzdb | .mzDB | mzDB Chromatogram | ✅ | ✅ |
| msd.chromatogramSupplier | mzmlb | .mzMLb | mzMLb Chromatogram | ✅ | ✅ |
| msd.chromatogramSupplier | mgf | .mgf | MGF Chromatogram | ✅ | ✅ |
| msd.chromatogramSupplier | rdx3 | .RData | R Data | ❌ | ✅ |
| msd.massSpectrumSupplier | animl | .animl | AnIML Mass Spectrum | ✅ | ✅ |
| msd.databaseSupplier | mgf | .mgf | Mascot Generic Format | ✅ | ✅ |
| msd.databaseSupplier | cms | .CMS | Calibrated Mass Spectra | ✅ | ✅ |
| msd.databaseSupplier | btmsp | .btmsp | Bruker Biotyper Main Spectra Projections | ✅ | ❌ |
| msd.databaseSupplier | microbems.muf | .muf | MicrobeMS spectral multifile | ✅ | ❌ |
| msd.databaseSupplier | microbems.pkf | .pkf | MicrobeMS peak list files | ✅ | ❌ |
| msd.databaseSupplier | microbenet | .xml | MicrobeNet MALDI | ❌ | ✅ |
| csd.chromatogramSupplier | cdf | .CDF | ANDI/AIA CDF Chromatogram | ✅ | ✅ |
| csd.chromatogramSupplier | gaml | .gaml | GAML CSD Chromatogram | ✅ | ❌ |
| csd.chromatogramSupplier | animl | .animl | AnIML FID Chromatogram | ✅ | ✅ |
| csd.chromatogramSupplier | arw | .arw | ARW Chromatogram | ✅ | ❌ |
| wsd.chromatogramSupplier | cdf | .CDF | ANDI/AIA CDF Chromatogram | ✅ | ✅ |
| wsd.chromatogramSupplier | gaml | .gaml | GAML WSD Chromatogram | ✅ | ❌ |
| wsd.chromatogramSupplier | animl | .animl | AnIML UV-Vis Chromatogram | ✅ | ✅ |
| wsd.chromatogramSupplier | arw | .arw | ARW Chromatogram | ✅ | ❌ |
| wsd.chromatogramSupplier | abif | .ab1 | ABIF Sanger Sequencing Trace File | ✅ | ❌ |
| wsd.chromatogramSupplier | axr | .axr | AXR Chromatogram (JSON) | ✅ | ❌ |
| wsd.scanSupplier | gaml | .gaml | UV/Vis Spectroscopy | ✅ | ❌ |
| fsd.scanSupplier | gaml | .gaml | Fluorescence Spectroscopy | ✅ | ❌ |
| nmr.scanSupplier | gaml | .gaml | NMR Spectroscopy | ✅ | ❌ |
| vsd.scanSupplier | gaml | .gaml | Vibrational Spectroscopy | ✅ | ❌ |

> **备注**：csd/wsd 的 cdf 插件还有 TSD 组件（`IConverterServiceTSD`，`.cdfx`/`.cdfy`，只读，不在上表）。

### 10.2 ChemClipse（商业版/更多格式，目录树确认存在）
`xxd.converter.supplier.{mzml, jcampdx, cml, csv, ascii, ocx}`、`msd.converter.supplier.{amdis, cml, excel, matlab.parafac, mmass, mzdata, mzxml, sirius}`、`csd.converter.supplier.xy`、`wsd.converter.supplier.{scf, spectroml}`、`pcr/nmr/tsd/fsd` 等 ✅（模块级）。其中 **`ocx` 插件 = `.ocb`/`.ocm` 原生工程格式，已全量深挖 ✅（见 §7）**；其余格式（mzml/jcampdx/cml/csv/ascii/amdis/excel/matlab 等）源码未读（⚠️）。

## 11. 原始数据在内存中的形态（✅ 部分确认 + 新确认）

- `IChromatogram.addScan(IScan)` / `getScans()` / `getScan(number)` / `removeScans(from,to)`：色谱 = 有序 IScan 集合 ✅
- `recalculateRetentionTimes()`：增删扫描后可重算 RT ✅
- `IChromatogram.getReferencedChromatograms()`：**一个文件可能含多条色谱**（GAML 多 Experiment 实例）✅
- `IChromatogram.getNoiseCalculator()` / `getSignalToNoiseRatio(abundance)`：噪声模型挂在色谱上 ✅
- **RT 单位（✅ 新确认）**：内存模型一律**毫秒 int**；`IChromatogramOverview.SECOND_CORRELATION_FACTOR`（=1000）与 `MINUTE_CORRELATION_FACTOR`（=60000）用于换算（数值由源码运算反推：RT/1000 得秒、RT/60000 得分、秒×1000/分×60000 得 ms；常量本身在 chemclipse.model，⚠️ 未直接读到定义）
- **IScan（✅ 新确认）**：MSD 扫描 = `AbstractScanMSD` 派生，离子列表 `List<IIon>`（m/z double + abundance float），`VendorIon extends AbstractIon`；CSD 扫描 = `AbstractScanCSD` 派生，`totalSignal` 单字段；**无独立 VendorScan→IScan 转换层**
- **懒加载代理（✅ 新确认）**：mz5/mzmlb 用 `AbstractRegularMassSpectrumProxy` 派生（`VendorScanProxy`），扫描只持有 `(mzs[], intensity[], start, length)`，`importIons(monitor)` 被首次访问离子时才被调用 → 大文件低内存模式。**ocb 另有同族懒加载**：`VendorScanProxy`（ocx 包）持 `(file, offset, version)`，靠 `SCANPROXIES` entry 的字节偏移定位 `SCANS` entry 惰性读回（见 §7.6）✅
- `IScan` 更多成员（接口未抓取）→ 见 MODULE_02 ❓

## 12. 待回填清单（❓）

| # | 问题 | 位置 |
|---|---|---|
| R1 | UI「打开文件」→ 供应商枚举 → 匹配 → ImportConverter 的完整调用链类名 | ChemClipse `org.eclipse.chemclipse.converter` 的 `ChromatogramConverterSupport`/`IConverterSupport` + `processing.converter.ISupplierFileIdentifier`（存在性✅/内容❓）|
| R2 | IChromatogramXXXReader 基类/接口的完整方法集（read/readOverview/读取进度）；`AbstractChromatogramImportConverter.validate` 内部逻辑 | chemclipse.{msd,csd,wsd,xxd}.converter |
| R3 | 导出 Converter 在 UI 侧如何按 converterId 自动选型（保存对话框逻辑）| chemclipse.converter / ux |
| R4 | ~~.ocx 原生格式内部结构~~ → **✅ 已解决**：`.ocb` ZIP 容器 + entry 布局 + 版本演进 0701~1502 + 字段布局，见 §7 | chemclipse.xxd.converter.supplier.ocx（R-Y~R-AN）|
| R5 | ~~大文件读取策略~~ → **✅ 部分解决**：ocb 用 SCANPROXIES 懒加载代理 + 后台线程 `enforceLoadScanProxies`（默认关）+ SubMonitor 进度（§7.6）；**UI 调度层（文件对话框→converter 的路由）仍 ❓** | chemclipse.xxd.converter.supplier.ocx + chemclipse.converter |
| R6 | `AbstractScanMSD.addIon`/`getTotalSignal`/`IIon.TIC_ION` 精确实现（VendorScan 行为依赖它）| chemclipse.msd.model `AbstractScanMSD` |
| R7 | `IChromatogramOverview.SECOND/MINUTE_CORRELATION_FACTOR` 常量定义确认 | chemclipse.model `IChromatogramOverview` |
| R8 | WSD/FSD/NMR/VSD 的 GAML、WSD CDF、arw/abif/axr 读取器细节（本次仅 plugin.xml 级）| 对应 supplier 插件 |
| R9 | TSD 的 `IConverterServiceTSD` 在 UI 侧的分发（打开 .cdfx/.cdfy 如何路由到 TSD 而非 1D）| chemclipse.tsd.* |

## 13. Qt/C++ 移植要点（io 模块设计笔记）

自研 CDS 对应模块名 **`io`**，建议接口（对照 §3 已确认的 Java 链）：

```cpp
// 对应 IChromatogramXXXReader.read / readOverview
class IFormatReader {
public:
    enum class Error { Ok, FileNotFound, UnsupportedFormat, Corrupt, Aborted };
    // 全量读取：解析成 ChromQt::Chromatogram*
    virtual Result<std::shared_ptr<Chromatogram>, Error> read(const QString& path,
                                                              Progress* progress) = 0;
    // 概览：只读元数据 + TIC 轴（对照 CDF MSD 的 readOverview）
    virtual Result<std::shared_ptr<ChromatogramOverview>, Error> readOverview(
                                                              const QString& path) = 0;
    // 可选：按需读取单个扫描（对照 mz5/mzmlb 的 ScanProxy.importIons）
    virtual Result<std::shared_ptr<Scan>, Error> readScan(const QString& path, int scanNo) = 0;
    virtual ~IFormatReader() = default;
};
```

**移植要点（每条都有 Java 源码依据）**：
1. **格式注册表**（对应 §2 扩展点）：`QPluginLoader` 加载 `IFormatPlugin`，插件元数据声明 `fileExtension / filterName / magicNumber / importable / exportable`；探测分两级——扩展名快筛（对应 MagicNumberMatcher）+ 打开文件头嗅探（对应 FileContentMatcher）。
2. **VendorScan→Scan 映射策略**：直接照搬"继承即映射"成本高（Java 靠多继承）。Qt 建议：`Scan` 提供 `addIon(mz, abundance)` + `setRetentionTime(int ms)` 原生方法，Reader 解析时直接填；或保留 `VendorScan` 原始字段 + `Scan::fromVendor(...)` 显式转换。**RT 进模型一律毫秒**。
3. **CDF 读取算法可直接照搬**（§5.3 的 scan_index/point_count 稀疏数组重建 + §5.2 的 CSD 等间隔重建），netCDF 库用 `netcdf-cxx4` 或 QIODevice + 自写 CDF-3 头解析（CDF 头 3 字节魔数 `"CDF"` 校验，`isValidFileFormat` 行为一致）。
4. **按需读取（大数据预览）**：对照 `VendorScanProxy`，`Scan` 可持 `LazySlice{mzArr, intenArr, start, length}` + `materialize()` 回调，TIC 轴用 `readOverview`（只读 `total_intensity` 数组）。
5. **TSD（GCxGC）**：`ChromatogramReaderTSD` 的"调制周期切帧"逻辑（§6）可直接移植——需要一个 `modulationTime` 配置项（OpenChrom 默认 10s）。
6. **错误处理**：Java 用「返回 null + IProcessingInfo 收集错误」；Qt 建议返回 `Error` 枚举码 + 可选错误消息，避免异常抛出（对应基类 `validate()` 的语义）。
7. **`.ocb` 原生格式移植（§7 全量字段清单可直接落码）**：
   - 容器：`QZipReader`/`QZipWriter`（QtGui）或 minizip；entry 内二进制用 `QDataStream`（注意 `setByteOrder(BigEndian)`、`setVersion(QDataStream::Qt_1_0)` 最接近 Java `DataOutputStream`）。
   - 字符串原语：`qint32 长度 + UTF-16BE（QChar 按大端写 2 字节）+ null=-1`；字符串集合同理（§7.3）。
   - 解析分派：读 `VERSION` entry → 字符串 → 查版本分发表（0701~1502）；**读写只需实现最新 1.5.0.2 一套，读取兼容旧版可逐版本退化**（建议先 1502 + 再补 1300/1003/0701 等里程碑）。
   - 字段顺序严格按 §7.4/7.5 的 readPeak/readNormalMassSpectrum/readIdentificationTargets 布局；`DataOutputStream` 无对齐 padding，逐字节连续。
   - 懒加载：`ScanProxy` 记录的 `offset` 是 SCANS entry 内的字节偏移，Qt 可用 `QIODevice::seek` 实现 `readScan`（对应 `IFormatReader.readScan`）。
   - 概览：MSD/WSD 读 `OVERVIEW/TIC`（注意两者字段顺序相反）；CSD 无概览裁剪，直接全量。
   - `.ocm` 旧版=ZIP 好解；**新版 = "MTH." 魔数 + GZIP + Java ObjectInputStream 载荷，Qt 需自写 Java 序列化解码器（含 AC ED 00 05 头部与 TC_* 标记），成本高**（§7.9）。
   - 单元测试锚点：一个 `.ocb` 文件应能被自研 Reader 打开并得到与 OpenChrom 一致的扫描数/RT/峰数——建议先用 OpenChrom 导出一个样例文件做 golden 对照。⚠️ 本机暂无样例 .ocb 文件，验证需人工提供。

## 14. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| R-A | 供应商扩展点机制 | openchrom/plugins/net.openchrom.msd.converter.supplier.cdf/plugin.xml | ✅ |
| R-B | 导入链 入口→Reader→模型 | .../cdf/.../converter/ChromatogramImportConverter.convert() | ✅ |
| R-C | 模型记住 converterId | .fetch/sources/model/IChromatogram.java (getConverterId) + ChromotogramReaderCSD/MSD.setChromatogramEntries | ✅ |
| R-D | 格式清单 | openchrom/plugins/*/plugin.xml 逐项核对 | ✅ |
| R-E | ~~.ocx 原生格式~~ → **✅ .ocb/.ocm 格式（见 R-Y~R-AN）** | chemclipse.xxd.converter.supplier.ocx 全插件（plugin.xml + Format.java） | ✅ |
| R-F | Vendor 模型即 ChemClipse 模型（继承即映射）| cdf/model/VendorChromatogram.java, VendorScan.java, VendorIon.java | ✅ |
| R-G | SpecificationValidator 文件名规范化 | msd.cdf/internal/converter/SpecificationValidator.java | ✅ |
| R-H | CDF 头 3 字节魔数 "CDF" 校验 | ChromatogramReaderCSD/MSD.isValidFileFormat | ✅ |
| R-I | CDF CSD 读取（point_number + delay/interval + ordinate_values + retention_unit 换算）| csd.cdf/io/ChromatogramReaderCSD.readChromatogram + AbstractCDFChromatogramArrayReader.initializeVariables | ✅ |
| R-J | CDF MSD 稀疏数组重建质谱（scan_index/point_count/mass_values/intensity_values）| msd.cdf/io/support/CDFChromtogramArrayReader.getMassSpectrum | ✅ |
| R-K | MSD 概览模式裁剪（仅 TIC_ION，不读质谱）| msd.cdf/ChromatogramReaderMSD.readChromatogramOverview + CDFChromatogramOverviewArrayReader | ✅ |
| R-L | CSD 概览模式不裁剪（readOverview→readFile）| csd.cdf/ChromatogramReaderCSD.readOverview | ✅ |
| R-M | CDF 写入变量清单与 ScanSupport 索引计算 | msd.cdf/ChromatogramWriter + DimensionSupport + ScanSupport | ✅ |
| R-N | 峰表读取（peak_name/peak_retention_time→IdentificationTarget）| csd.cdf/AbstractCDFChromatogramArrayReader.readPeakTable | ✅ |
| R-O | 匹配器逻辑（扩展名魔数 + mass_values/分离类型内容嗅探）| cdf 各 FileContentMatcher/MagicNumberMatcher（CSD/MSD）| ✅ |
| R-P | TSD 多色谱 = 调制周期切帧；.cdfx/.cdfy；OSGi 组件注册 | csd.cdf/ChromatogramReaderTSD + ChromatogramImportConverterTSD + PreferenceSupplier | ✅ |
| R-Q | GAML 版本嗅探 + 多 Experiment 引用色谱 + 单位换算 | msd.gaml/ChromatogramReader.getReader + ChromatogramReaderVersion120 + xxd.gaml/Reader120 | ✅ |
| R-R | AnIML XML+二进制读取（RT/TIC/质谱/峰表四步回填）| msd.animl/io/ChromatogramReader + xxd.animl/converter/BinaryReader | ✅ |
| R-S | mz5/mzMLb 懒加载代理扫描 + m/z 增量重建 | mz5/ChromatogramReader + VendorScanProxy + ReaderProxy；mzmlb/ReaderProxy | ✅ |
| R-T | mzDB SQLite / MGF 文本 / CMS 库格式读取 | mzdb/ChromatogramReader；mgf/io/MassSpectraReader；cms/io/MassSpectrumReader | ✅ |
| R-U | RDX3 = R .RData 导出（data.frame：RT/RI/各 m/z 列）| xxd.rdx3/core/ChromatogramWriter | ✅ |
| R-V | 日期格式 yyyyMMddHHmmssZ | cdf/io/support/DateSupport | ✅ |
| R-W | 扩展点家族分布（chromatogram/massSpectrum/database/scanSupplier + TSD 组件）| 全部供应商 plugin.xml 的 point= 核对 | ✅ |
| R-X | GAML/AnIML 的 xxd 插件为共享库（无 plugin.xml）| net.openchrom.xxd.converter.supplier.{gaml,animl}/ 目录 | ✅ |
| R-Y | 扩展名为 `.ocb`/`.ocm`（非 .ocx）；converterId 三套（色谱/方法/峰）| ocx/plugin.xml + versions/VersionConstants.java | ✅ |
| R-Z | `.ocb` = ZIP 容器 + `VERSION` entry 版本校验 + entry 命名清单（MSD/CSD/WSD/FID 前缀演进）| settings/Format.java + ReaderHelper.getVersion + FileContentMatcher{MSD,CSD,WSD} | ✅ |
| R-AA | 序列化原语：int 长度+UTF-16BE 字符串、null=-1、字符串集合、Java 大端标量 | org.eclipse.chemclipse.converter/io/IFileHelper + xxd.ocx/internal/io/AbstractIO_1502 | ✅ |
| R-AB | MSD v1502 扫描/峰/识别目标/定量/基线/历史/MISC/方法/SeparationColumn 字段布局（§7.4）| msd.ocx/internal/io/ChromatogramReader_1502 + ReaderIO_1502 + WriterIO_1502 | ✅ |
| R-AC | CSD 布局差异（无离子/SCANPROXIES/optimized；积分仅面积；概览=全量读）| csd.ocx/internal/io/ChromatogramReader_1502 + io/ChromatogramReaderCSD.readOverview | ✅ |
| R-AD | WSD 布局差异（scanSignal 数组 double 波长；概览字段顺序 totalSignal 先；copyFrom 方向疑似 bug）| wsd.ocx/internal/io/ChromatogramReader_1502 + ChromatogramWriter_1502 | ✅ |
| R-AE | 版本演进 0701~1502 共 22 版 + 代号（Nernst→McLafferty）与逐版新增字段（§7.7）| versions/ChromatogramVersion + Format 版本常量 + 逐版本 diff | ✅ |
| R-AF | 概览模式：MSD/WSD 只读 OVERVIEW/TIC；CSD 无裁剪；读失败回退跨检测器 | msd/csd/wsd.ocx io 入口 + internal Reader readOverview + createChromatogram{MSDFromFID,FIDFromMSD} | ✅ |
| R-AG | 懒加载 SCANPROXIES：offset 定位 SCANS entry + VendorScanProxy/ReaderProxy 惰性读回；默认关 | msd.ocx/internal/io/ReaderProxy_1502 + ScanProxy + VendorScanProxy + PreferenceSupplier | ✅ |
| R-AH | 引用色谱：REFERENCE_INFO + CHROMATOGRAM_REFERENCE_{i} 嵌套（递归 Reader/Writer）；ZipInputStream 不支持 | msd.ocx/internal/io/ChromatogramReader_1502.readReferencedChromatograms + ChromatogramWriter_1502 | ✅ |
| R-AI | 峰 supplier 只读 PEAKS entry（isExportable=false）；ChromatogramReferencesSupport 单独导出引用 | ocx/plugin.xml peakSupplier + PeakReader_1502 + ChromatogramReferencesSupport | ✅ |
| R-AJ | `.ocm` 双格式：旧 ZIP(VERSION+PROCESS_METHOD) / 新 "MTH."+GZIP+Java 序列化；导入按 1402→…→1000 尝试 | xxd.ocx/internal/methods/{AbstractMethodWriter, GenericStreamMethodFormat, ObjectStreamMethodFormat, MethodReaderWriter_1402} + MethodImportConverter | ✅ |
| R-AK | 压缩级别偏好（.ocb 默认 1，.ocm 默认 0）+ 系统设置入口 | xxd.ocx/preferences/PreferenceSupplier + system/ConverterProcessSupplier | ✅ |
| R-AL | 深拷贝过程：导出→导入临时 .ocb（OSGi Procedure 组件）| xxd.ocx/ChromatogramProcedure + ChromatogramProcedureSettings | ✅ |
| R-AM | `SpecificationValidator` 补 `.ocb` 扩展名/`CHROMATOGRAM.ocb` 目录 | xxd.ocx/internal/support/SpecificationValidator | ✅ |
| R-AN | 历史/宏记录：HISTORY 内 `ProcessSupplierEntry_` Base64 载荷恢复 | xxd.ocx/internal/support/ProcessSupplierSupport | ✅ |
