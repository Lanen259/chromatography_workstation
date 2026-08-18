# io 线程记忆

## 2026-08-18 io 线程（M4 完成，待合并）
- **做了什么**：契约 §4.4 冻结接口逐字实现（`include/io/converters.h`：ImportResult / IChromatogramImporter / IChromatogramExporter / ConverterRegistry）+ `src/ImporterCsv`（CSV 导入）+ `src/ExporterCsv`（CSV 导出）+ `src/ConverterRegistry`（单例 + 内置注册）+ 回环测试 `tests/t_io.cpp`（7 槽，全绿）。提交前先 `git merge main` 同步。
- **为什么这么设计**：
  - 冻结接口逐字照抄，只做两类编译必要增补（任务明示许可）：① 补齐 QtCore include（QString/QStringList/QHash）；② ConverterRegistry 补私有工厂表存储 + 私有默认构造（单例必需）。
  - **自研 CSV 格式（锁定进测试，导出/导入严格互逆）**：默认两列 `retentionTimeMs,intensity`；首行可选表头（仅对首个非注释/非空行探测，含 "retentionTimeMs"/"retention" 关键词则跳过）、`#` 注释行与空行跳过；导出写表头 + 每点「整数毫秒,`QString::number(x,'f',6)` 定点强度」，行尾 LF（QFile 不带 `QIODevice::Text`）、UTF-8（QTextStream 默认）。
  - **注册表按扩展名多键登记**（不同于 report 按 formatName 单键）：registerImporter/registerExporter 用探针实例取 `supportedExtensions()`，把工厂按每个扩展名（小写化）插入 QHash；importerFor/exporterFor 从 filePath 纯字符串提取小写扩展名（含点）查表，未注册返回 nullptr。
  - **导入填模型**：`setSignalPoints` + RT 网格从首末点推断（scanDelayMs=首点 RT，scanIntervalMs=(末−首)/(n−1)，n<2 为 0）+ `setName(QFileInfo::completeBaseName)` + `setConverterId("io.csv")`。坏行/缺列/非数字返回 `ok=false` 并记录行号；空文件/无数据点 `ok=false`。
  - 失败路径不污染 `out`：解析全部进局部 `QVector<Signal>`，成功后一次性写入。
- **验证**：全量 `cmake --build build` 0 error（io + core_model + core_processing + acq + report + ui 全量编译）；`ctest --test-dir build --output-on-failure` → 4/4 全绿（core_model + core_processing + acq + io）；io_tests 7 槽全 PASS（roundTrip / emptyFileImport / badLine / headerVariants / exportFormatBytes / registry / missingFileImport）。D4 接口纪律自查：无 QtWidgets/QtGui，只 include core_model + QtCore。
- **下一步**：主控线程按治理规范 §3 合回 main（软件审查 ctest 全绿 + 人工代码审查放行）。
- **卡点/依赖**：无代码卡点。**协调事项（主控需知）**：① RT 网格推断取「等间隔假设」（scanInterval=(末−首)/(n−1)），与 core_model 网格语义 RT=scanDelay+i·scanInterval 一致；非等间隔 CSV 会取平均步长，属本次拍板口径；② 导入读文件带 `QIODevice::Text`（兼容 CRLF），导出不带（固定 LF），二者字节语义不同是刻意的；③ 后续加 CDF/GAML 等格式 = 新增实现类 + 注册一行，开闭原则，不动冻结签名。
