# core_processing 线程记忆

## 2026-08-18 core_processing 线程（审查整改轮，M2 最终态：待合并）
- **提交记录（做到哪一步）**：
  - `b3c5f1e` feat：接口 + 5 算法 + Registry + Pipeline + 金标准测试（18 槽）
  - `259e0c1` fix：审查整改（见下）
  - `0fb209e` doc：PROJECT.md §1/§2 状态行 + 首版记忆文件
- **做了什么（审查整改）**：requesting-code-review 子代理审查 `b3c5f1e`（实测编译运行 + 探针实证）。结论「Ready to merge: With fixes」。
  - 修 Critical：`PeakDetectorFirstDerivative::configure` 未校验 `windowSize`，负数触发 `diff=w/2` 负下标越界写（审查者编译探针实证段错误，exit 139）。改为钳位 [0,45] 且偶数强制奇数（OpenChrom IntSettings 校验口径）。
  - 修 Important：`lastScan` 由 S-3 对齐为 S-4（Java `limit=size-CONSECUTIVE_SCAN_STEPS` 的 0 基映射），注释与实现一致。
  - 文档化偏差：梯形积分与 OpenChrom `stopRT=rts[i+1]-1` 的刻意差异（离散 qint64 点无整毫秒重叠歧义，直接积满间隔）；定量近零斜率视为无曲线；peakHeight 注明未扣背景。
  - 补 3 测试：`peakDetectorWindowSizeValidation`（负/偶数不崩溃）、`pipelineUnknownStepSkipped`（未知步跳过继续）、`quantifierDegenerateCalibration`（法方程分母 0 不崩溃）。
- **最终验证**：全量构建 0 error；`ctest` 2/2 全绿（core_model + core_processing）；测试槽 21 个全 PASS（init/cleanup 在内共 23 PASS）；D4 无 QtWidgets、只 include core_model + QtCore。工作树干净。
- **下一步**：等主控线程按治理规范 §3 合回 main（软件审查 ctest 全绿 + 人工代码审查放行）。
- **卡点/依赖**：无代码卡点。协调事项沿用下方首版条目 ①②③（结果访问器增补 / 校准表参数 schema / OpenChrom 忠实行为）。⚠️ 主控注意：中途 shell 目录漂移，有一次 `cmake -S/-B build` 误指向主 checkout 的 gitignored `build/` 目录并重新配置成 MinGW 工程——无 git 影响，可删除重建；实施计划存于 worktree `build/m2-core-processing-plan.md`（gitignored，未入库）。

---

## 2026-08-18 core_processing 线程（M2 完成，待合并）
- **做了什么**：契约 §4.2 冻结接口逐字实现（`include/core_processing/interfaces.h`：IFilter/IBaselineDetector/IRawPeak/IPeakDetector/IIntegrator/IQuantifier/QuantEntry/CalibrationTable/Registry/ProcessingPipeline）+ 5 内置算法（`src/`：FilterSavitzkyGolay / BaselineLinear / PeakDetectorFirstDerivative / IntegratorTrapezoid / QuantifierCalibration）+ Registry 单例内置注册 + ProcessingPipeline 管线执行器 + 金标准测试 `tests/t_core_processing.cpp`（18 槽，全绿）。先 `git merge main` 同步（M1 已合入）。
- **为什么这么设计**：
  - 冻结接口逐字照抄，只做三类编译必要增补（任务明示许可）：① ProcessingPipeline 补 `: public QObject` + Q_OBJECT（signals 必需）；② 补 QtCore include + QuantEntry/CalibrationTable 前向声明（契约把 IQuantifier 签名放在结构体定义之前，不声明不编译）；③ Registry 补「其余访问器」、Pipeline 补结果只读访问器（`peaks()/baseline()/quantEntries()`，契约缺之管线结果不可见，M6 取数据用）。
  - **参数注入**：冻结接口的 apply/detect/quantitate 无参数形参，算法参数（Method.step.parameters）经内部辅助接口 `IConfigurable::configure(QVariantMap)` 注入（src/ 私有，不进冻结头）。管线按 id 经 Registry 取实例 → dynamic_cast 配置 → 执行，全程不耦合具体算法类。
  - **工作信号约定**（src/WorkingSignal.h）：所有算法读 processedPoints（非空）否则 signalPoints；滤波器写回 processedPoints；管线 execute() 开头重置 processed 为空 → 首滤波器从原始开始、多滤波器链式、重跑覆盖（契约「改参数→重跑管线→覆盖本副本」）。
  - **一阶导数峰检测**（核心）：逐条对照逆向 MODULE_04 §2.2 PeakDetectorCSD.java —— 归一化(NORMALIZATION_BASE=100000)、相邻斜率、可选居中滑动平均、峰起=连续 3 斜率>阈值且严格递增、峰顶=斜率首次<0（过零）、峰止=峰顶后斜率首次回正（默认 lastScan）、宽度≥3。阈值 OFF=0.0005/LOW=0.005/MEDIUM=0.05/HIGH=0.5。
  - **SG 平滑**：法方程最小二乘运行时求卷积核 `W = X·(X^T X)^-1·X^T`，三区处理（首尾边界核 + 中间对称核）。**踩坑**：首版权重公式漏了 `X^T` 收缩（写成 `X·(X^T X)^-1`），二次曲线保真测试抓住后修正。
  - **梯形积分**：VV 语义（峰两端背景直线），逐段梯形积 max(0,y−bg)，总面积 ÷100（OpenChrom CORRECTION_FACTOR_TRAPEZOID，ChemStation 因子）。
  - **校准定量**：校准表由管线从 quant 步 parameters 解析（`componentName` + `points[{concentration,area}]`）；最小二乘拟合 area=a·conc+b，≥2 点最小二乘 / 1 点过原点 / 空表浓度 0。
- **验证**：`cmake --build build` 全量 0 error；`ctest --test-dir build --output-on-failure` → 2/2 全绿（core_model + core_processing）；18 槽金标准测试全 PASS（两峰高斯 apex≈600/1200、阈值过滤 LOW/MEDIUM/HIGH、SG 二次曲线保真+尖峰核值+面积守恒、三角面积 50.0、校准 2.5/10.0、管线全链 QSignalSpy）。D4 接口纪律自查：无 QtWidgets/QtGui、只 include core_model + QtCore。
- **下一步**：主控线程按治理规范 §3 合回 main（先软件审查 ctest 全绿 + 人工代码审查放行）。M6(ui) 取峰/基线/定量结果经 Pipeline 新增访问器。
- **卡点/依赖**：无代码卡点。**协调事项（主控需知）**：① ProcessingPipeline 结果访问器（`peaks()/baseline()/quantEntries()`）是契约未冻结的增补，若契约 §4.2 后续想纳入正式签名可据此登记；② 校准表从步骤参数编码（componentName/points）是本次定的管线惯例，M6 方法编辑器写 quant 步参数时需对齐此 schema；③ 积分面积带 ÷100 校正因子、峰检测"末峰 stop 延至数据尾"是 OpenChrom 忠实行为，定量相对比较不受影响。
