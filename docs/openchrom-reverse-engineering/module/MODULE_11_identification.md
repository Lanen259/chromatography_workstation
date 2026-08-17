# MODULE_11 — 峰/扫描定性鉴定（Identification）与质谱相似度匹配算法

> **状态：🟡 分析中（IIdentificationTarget 结构 ✅、IComparisonResult 全字段 ✅、LibraryInformation 全字段 ✅、三种相似度算法公式 ✅（Alfassi 几何距离 / 谱熵 / 距离家族）、标识扩展点 ✅、NIST 进程调用接口 ✅、MassBank 在线跳转 ✅、File/TimeRanges 标识器 ✅、ITarget 父链 ✅、DatabasesCache/ComparatorCache 缓存 ✅、IScan/IPeak 挂载语义 ✅；本模块遗留 ❓ 仅限外部实现层（ID-4/ID-5））**
> 服务于自研 CDS Qt 工程「定性分析」模块。严格遵守：✅ 源码确认 vs ⚠️ 推测，不许混。本模块旧文档交叉证据见 MODULE_02 §1.4（ITargetSupplier.getTargets() = `Set<IIdentificationTarget>` 可变 HashSet）与 §3（`IScan.getTargets()`、AbstractScan 字段 `identificationTargets`）。

---

## 1. 鉴定目标结构：IIdentificationTarget（★ ✅ 源码确认）

### 1.1 接口全貌

文件：`.fetch/chemclipse-src/plugins/org.eclipse.chemclipse.model/src/org/eclipse/chemclipse/model/identifier/IIdentificationTarget.java`（包 `org.eclipse.chemclipse.model.identifier`）

```java
public interface IIdentificationTarget extends ITarget {
    ILibraryInformation getLibraryInformation();   // 库信息（化合物元数据）
    IComparisonResult getComparisonResult();       // 匹配结果（MF/RMF/概率…）
    String getIdentifier();  void setIdentifier(String identifier); // 如 "NIST (extern)"
    boolean isVerified();    void setVerified(boolean verified);     // 人工核验标记
    IIdentificationTarget makeDeepCopy();
}
```

- **identifier 语义**（javadoc 明文）：如 `'NIST (extern)'`；**禁止翻译**——`ILibraryService` 靠它反查"该目标由哪个库提供参考谱"以在比对视图展示（MassBank `TargetIdentifier` 也靠 `getIdentifier()=="File Identifier"` 判定是否由文件库提供，见 §8）。
- **ITarget 父接口**（`model/targets/ITarget.java`，本任务已深读，见 §1.3）：`ITarget extends Serializable` 是**空标记接口**（无成员）；IIdentificationTarget extends ITarget，target 本体只挂"库信息 + 比较结果"。
- 静态工具方法（源码直证）：
  - `getIdentificationTarget(IScan)` / `getLibraryInformation(IScan)`：取 scan.targets 中**排序第一名**（§4 排序语义）；peak 版经 `peak.getPeakModel().getPeakMaximum()`（峰顶扫描）再取。
  - `getTargetsSorted(Collection, retentionIndex)`：`IdentificationTargetComparator(SortOrder.DESC, retentionIndex)` 排序 → List。
  - `createDefaultTarget(name, cas, identifier, matchFactor=100)`：建 `LibraryInformation`（name/cas/comments=""/contributor=""/referenceIdentifier=""）+ `ComparisonResult(mf, mf, mf, mf)` + `IdentificationTarget`。
- **实现类**：
  - `AbstractIdentificationTarget`（同目录）：字段 `libraryInformation`、`comparisonResult`、`identifier=""`、`verified=false`；构造器两个引用 **null 抛 ReferenceMustNotBeNullException**；**equals/hashCode 键** = `identifier + name + cas + matchFactor + reverseMatchFactor`（同一目标的去重依据，`getTargets()` 是 HashSet 因此去重靠它）。
  - `model/implementation/IdentificationTarget.java`：额外字段 `IScan libraryScan`（库参考谱背引用，`setLibraryScan`/`getAdapter` 返回），`makeDeepCopy()` 复制 (libraryInfo, comparisonResult, identifier)。

### 1.2 峰值变体与结果容器（✅）

| 变体 | 接口 | 说明 |
|---|---|---|
| Peak 版结果 | `IPeakComparisonResult extends IComparisonResult` | 仅多 `isMarkerPeak()/setMarkerPeak(boolean)`（标记峰标记） |
| Peak 版库信息 | `IPeakLibraryInformation extends ILibraryInformation` | 空接口（标记） |
| 单次鉴定结果 | `IIdentificationResult`（`AbstractIdentificationResult` 实现） | `add/remove/removeAll(IIdentificationTarget)`、`getBestHit()`（**= 遍历取 matchFactor 最大的 target**，`AbstractIdentificationResult.getBestHit()`）、`getIdentificationEntries()` → Collection |
| 结果集合 | `IIdentificationResults`（`add/remove/removeAll/getIdentificationResult(int)/getIdentificationResults()`） | 0-based |
| Peak 版集合 | `IPeakIdentificationResults extends IIdentificationResults` | 空接口 |
| 具体类 | `PeakIdentificationResult`（msd.model/implementation）、`IdentificationResult/IdentificationResults`（model/implementation） | 纯空子类 |

> NIST `Identifier.assignPeakCompounds` 显示用法：每个被鉴定峰建一个 `PeakIdentificationResult`，命中的 target 同时 `add` 进 result **和** `peak.getTargets()`（`IPeak.getTargets()` 来自 ITargetSupplier，可变 Set）——**结果容器（IIdentificationResults）与模型内嵌 targets（Set）双写**。

---

### 1.3 ITarget 父链与关联接口（★ ✅ 源码确认）

**继承链（目标本体，全在 `model` 插件）**：

```
ITarget                // 空标记接口，仅 `extends Serializable`（无任何成员）
 └─ AbstractTarget     // 空抽象类（仅 serialVersionUID 常量）
     └─ AbstractIdentificationTarget → IIdentificationTarget   // 鉴定目标，见 §1.1
另：model/statistics/Target.java = `extends AbstractVariable implements ITarget`（统计变量版，与鉴定无关）
```

`Source: targets/ITarget.java:31 + targets/AbstractTarget.java:15 + identifier/AbstractIdentificationTarget.java:18 + statistics/Target.java:19`

**ITargetReference（引用包装，注意它不继承 ITarget！）**：`targets/ITargetReference extends ITargetSupplier`——本质是"带定位信息的 ITargetSupplier 引用"，供 UI 标签/持久化使用：
- 成员：`getSignal()`（IScan 或 IPeak）、`getRetentionTimeMinutes()`、`getRetentionIndex()`、`getID()`（唯一 ID = `type.label() + "." + RT分钟`）、`getType()`（`TargetReferenceType`: NONE/SCAN/PEAK）。
- 默认方法：`getBestIdentificationTarget()` = `IIdentificationTarget.getIdentificationTarget(getTargets(), getRetentionIndex())`；`getTargetLabel(LibraryField)`。
- 实现 `TargetReference`：持 `ITargetSupplier supplier` 委托 `getTargets()`（空 supplier → 空集合）；静态 `getScanReferences/getPeakReferences` 批量建引用（空 targets 的 scan 跳过、peak 空 targets 但含 classifiers 也建）；`createVisibilityFilter` → PEAK 用 `isShowPeakLabels()`、SCAN 用 `isShowScanLabels()`。
`Source: targets/ITargetReference.java:20/47-60 + targets/TargetReference.java:33/74-81/107-162 + targets/TargetReferenceType.java:18-21`

**ITargetDisplaySettings（UI 显示设置，与 ITarget 继承链无任何关系）**：峰/扫描标签显示开关（`isShowPeakLabels/isShowScanLabels`）、`DisplayOption`（10 档枚举：STANDARD / NUMBERS / RETENTION_TIME / RETENTION_INDEX / RETENTION_INDEX_AREA_PERCENT / AREA_PERCENT 及各自的 "[Display Field]" 变体）、标签旋转角 `getRotation/setRotation`（度）、`getCollisionDetectionDepth/setCollisionDetectionDepth`、`LibraryField`（标签显示字段）、可见性映射 `getVisibilityMap(): Map<String,Boolean>`（key = TargetReference.getID()，不可修改）+ `isVisible/isMapped/putAll`。实现类：`model/core/AbstractMeasurementTarget extends AbstractMeasurement implements ITargetDisplaySettings`。
`Source: targets/ITargetDisplaySettings.java + targets/DisplayOption.java:17-28 + core/AbstractMeasurementTarget.java:25`

---

### 1.4 IScan/IPeak 目标挂载（★ ✅ 源码确认）

**接口层**：`IScan extends ISignal, IAdaptable, Serializable, ITargetSupplier`（IScan.java:28）；`IPeak extends ITargetSupplier, IClassifier, ISignal`（IPeak.java:24）；`ISpectrumPeak extends ITargetSupplier, IClassifier`（ISpectrumPeak.java:15）。`getTargets()` 统一返回 **`Set<IIdentificationTarget>`**（可变）。

**实现层（挂载点，重点）**：

| 类 | getTargets() 实现 | 含义 |
|---|---|---|
| `AbstractScan` | 直接返回自身字段 `identificationTargets = new HashSet<>()`（:52/:275-278） | **每个扫描持有自己的 targets 集**；所有 IScanMSD 继承之 |
| `AbstractPeak` | `getPeakModel().getPeakMaximum().getTargets()`（:337-340） | **峰没有独立 targets 集，委托给峰顶扫描（peak maximum IScanMSD）**——峰鉴定结果实际挂在峰顶扫描上 |

**添加语义**：`getTargets().add(IIdentificationTarget)` 走 **HashSet.add** → 去重键 = `AbstractIdentificationTarget.equals/hashCode`（identifier+name+cas+MF+RMF，§1.1）——同一化合物重复命中不会产生重复 target。

**Best Match 排序语义（读取时排序，模型不维护排序状态）**：
- `IIdentificationTarget.getIdentificationTarget(Set, retentionIndex)`（IIdentificationTarget.java:130-138）：构造 `IdentificationTargetComparator(SortOrder.DESC, retentionIndex)` → 排序 → **取 [0]**；`getIdentificationTarget(Set, Comparator)`（:147-163）size==1 直接返回。
- `TargetSupport.getBestIdentificationTarget(ITargetSupplier)`（TargetSupport.java:44-61）：`isUseRetentionIndexQC()` 时取峰顶/扫描的 RI 作排序基线 → 调上者。
- "**最佳命中**" = `verified → ΔRI(小) → MF → RMF → MFD → RMFD → prob` 排序后第一名（排序器细节见 §8）。
- `getTargetsSorted(Collection, retentionIndex)`（IIdentificationTarget.java:94-104）→ 同比较器 List（供 UI/报告）。

**写回路径**：标识器 `unknown/peak.getTargets().add(...)`（FileIdentifier:299 对 IScanMSD、NIST assignPeakCompounds 对 IPeak 并双写 IIdentificationResult，§7.1/ID-X）——**结果容器（IIdentificationResults）与模型内嵌 targets（Set）双写**。

---

## 2. IComparisonResult：字段、罚分与排序语义（★ ✅ 源码确认）

### 2.1 常量与字段

接口：`model/identifier/IComparisonResult.java`（extends `Serializable, Comparable<IComparisonResult>`），javadoc：「**0 = no match，100 = perfect match**」。

| 常量 | 值 | 说明 |
|---|---|---|
| `FACTOR_BEST_MATCH` / `FACTOR_NO_MATCH` | 100.0f / 0.0f | 满分/零分 |
| `MAX_MATCH_FACTOR` / `MAX_REVERSE_MATCH_FACTOR` | 100.0f | 上限 |
| `DEF_MAX_PENALTY` | `IPenaltyCalculationSettings.DEF_PENALTY_MATCH_FACTOR = 20.0f` | 默认最大罚分 |
| `MIN/MAX_ALLOWED_PENALTY` | 0.0f / 100.0f | 罚分合法区间 |
| `MIN/MAX_ALLOWED_PROBABILITY` | 0.0f / 100.0f | 概率合法区间 |

字段（`AbstractComparisonResult` 内部）与读法：

| 字段 | 原始 getter | 调整后 getter | 说明 |
|---|---|---|---|
| `matchFactor` | `getMatchFactorNotAdjusted()` | `getMatchFactor()` | **MF** = 原始值 − 罚分（`getAdjustedValue(value, penalty)`，`value−penalty`，负值截 0） |
| `reverseMatchFactor` | `getReverseMatchFactorNotAdjusted()` | `getReverseMatchFactor()` | **RMF**，同上 |
| `matchFactorDirect` | `getMatchFactorDirectNotAdjusted()` | `getMatchFactorDirect()` | **MFD**（仅未知谱非零离子，见 §3） |
| `reverseMatchFactorDirect` | `getReverseMatchFactorDirectNotAdjusted()` | `getReverseMatchFactorDirect()` | **RMFD** |
| `probability` | `getProbability()` | — | 构造时校验 0~100 否则置 0 |
| `inLibFactor` | `getInLibFactor()` / `setInLibFactor` | — | 库内因子（NIST 结果 `Compound in Library Factor`） |
| `penalty` | `getPenalty()` | — | `setPenalty` 越界抛 IllegalArgumentException；`addPenalty` 累加并 clamp 0~100 |
| `isMatch` | `isMatch()` / `setMatch(boolean)` | — | 是否命中 |
| `ratingSupplier` | `getRatingSupplier()` | — | 评分器（见 2.3） |

构造器（`ComparisonResult`）：`(float mf)` → 四项全等于 mf、概率=100；`(mf, rmf, mfd, rmfd)`；`(+ probability)`；拷贝构造。

### 2.2 排序语义：compareTo（✅ 逐字）

`AbstractComparisonResult.compareTo`：按 `isMatch`（true 大）→ `matchFactor` → `reverseMatchFactor` → `matchFactorDirect` → `reverseMatchFactorDirect` **升序**。文件库搜索用 `Collections.reverseOrder(IComparisonResult.MATCH_FACTOR_COMPARATOR)`（`FileIdentifier:65`，常量 `MATCH_FACTOR_COMPARATOR = Float.compare(getMatchFactor())`）→ **按 MF 降序取前 N**。

### 2.3 评分/建议（RatingSupplier ✅）

`model/identifier/RatingSupplier.java`（extends `AbstractComparisonRatingSupplier`）：
- **getScore()**（0~100）：`rating = matchFactor`；若 RMF>0 → `rating=(rating+RMF)/2`；若 MFD>0 → 再均；若 RMFD>0 → 再均（渐进均值，只平均**非零**项）。
- **getAdvise()**：MF≥80 且 RMF≤20 → `"Incomplete Target (Bad Conditions)"`（不完整靶）；MF≤20 且 RMF≥80 → `"Convoluted Target (Impurities)"`（共流出杂质）。经典 MF/RMF 矛盾诊断。
- **getStatus()**（`AbstractComparisonRatingSupplier`）：score ≥90 → `VERY_GOOD`；≥80 → `GOOD`；≥70 → `AVERAGE`；≥60 → `BAD`；else `VERY_BAD`。
- ⚠️ 注释明文：**ratingSupplier 不入库**（*.ocb 存 target/comparisonResult，但评分算法类不序列化；重载文件后默认 RatingSupplier 重算，可能状态不一致）。

### 2.4 惰性计算 LazyComparisonResult + MatchConstraints（✅ 重要性能机制）

- `LazyComparisonResult`（`model/identifier/LazyComparisonResult.java`）：四项初始 `NaN`，用 4 个 `DoubleSupplier` **按需计算**；**短路链** `MatchConstraints`（0~1 归一化阈值）：只算 RMF 当 `MF ≥ minMF`；只算 MFD 当 `RMF ≥ minRMF`；只算 RMFD 当 `MFD ≥ minMFD`。cosine 族用它（§3.3）。
- `MatchConstraints` 默认全 0（全算）；文件库搜索构造为 `(minMatchFactor/100, minReverseMatchFactor/100)`。
- **Penalty 与 MF 截断**：`ComparisonResultFinal`（不可变哨兵）——`COMPARISON_RESULT_NO_MATCH(0,false)` / `COMPARISON_RESULT_BEST_MATCH(100,true)`，全部 setter 空实现（跳过比较时返回的预置结果）。

---

## 3. 三种相似度算法公式（★ ✅ 逐字源码，Qt 可照搬）

**通用输入**：两个 `IScanMSD`（质谱）→ 各自 `getExtractedIonSignal()`（`IExtractedIonSignal` = `float[]`，下标 = 标称 m/z − startIon，见 MODULE_02 §3.3）。**输出统一 0~100 百分比**（内部先归一到 0~1 再 ×100）。每个结果填 4 个因子 MF/RMF/MFD/RMFD（熵只填 MF）。

### 3.0 前置归一化原语（`msd.model/xic/ExtractedIonSignal.java`，✅）

```java
normalizeVector(base=1.0f): factor = base / Σ(abundance); 每元素 *= factor   // L1 归一（和为 base）
normalizeIntensity(base=1000.0f): factor = base / max;     每元素 *= factor   // 最大值归一
```

### 3.1 Alfassi 几何距离（`comparison.supplier.alfassi`，✅）

`alfassi/comparator/MassSpectrumComparator.java` + `msd.identifier/comparison/math/GeometricDistanceCalculator.java`。文献：Alfassi, Z. B., "Vector analysis of multi-measurements identification"。

流程（`MassSpectrumComparator.compare`）：
1. 深拷贝两份谱 + `normalize(100)`（**最大值归一到 100**，`AbstractScanMSD.normalize` 见 MODULE_02 §5.3）。
2. `GeometricDistanceCalculator.calculate(unknown, reference, ionRange)`：
   - 离子集 = unknown 提取信号 `[startIon, stopIon]` 全区间（带 range 版本）；不带 range 版本只取 **abundance>0** 的离子。
   - `S_u = √(Σ_{i∈ions} ab_u(i)²)`，`S_r` 同（对 reference 同一离子集）。
   - 任一为 0 → 返回 0。
   - 逐离子：`uValue = ab_u(i)/S_u`，`rValue = ab_r(i)/S_r`，`sumDistance += (uValue − rValue)²`。
   - **`result = (sumDistance + 1)⁻¹`** —— 即两个单位向量的几何距离倒置 + 1 平滑，0<result≤1。
3. ×100：`MF = calc(u,r,uRange)`，`RMF = calc(r,u,rRange)`（**MF 用 unknown 的离子区间，RMF 用 reference 的区间**）；MFD/RMFD 不带区间（只取本谱非零离子）。
4. 结果 `ComparisonResult(mf, rmf, mfd, rmfd)`。

> **关键区别**：Alfassi 的几何距离是**逐离子平方差在单位长度归一后求和再取倒数**（类似 RBF 核），与余弦（点积/长度积）不同。

### 3.2 谱熵相似度（`comparison.supplier.entropy`，✅）

`entropy/comparator/EntropyComparator.java`。文献：Li, Y., Kind, T., Folz, J. et al. "Spectral entropy outperforms MS/MS dot product similarity for small-molecule compound identification", Nat Methods 18, 1524-1531 (2021)。

流程（`compare`）：
1. 取 unknown/reference 提取信号；**virtualSignal = 逐离子两谱丰度之和**（`getVirtualSignal`：start=min(startU,startR)，stop=max(stopU,stopR)，`ab_v(i)=ab_u(i)+ab_r(i)`，>0 才建离子）。
2. 三个信号均 `normalizeVector(1.0f)`（**L1 归一，和为 1**，即概率分布）。
3. 谱熵：`S = −Σ_{i: p_i>0} p_i·ln(p_i)`（`getSpectralEntropy`）。
4. **相似度（MATCH FACTOR）**：
   ```
   unweightedEntropySimilarity = 1 − (2·S_virtual − S_unknown − S_reference) / ln(4)
   matchFactor = unweightedEntropySimilarity × 100
   ```
   （`calculateMatchFactor`；被注释的 `calculateEntropyDistance = (S_v − S_u) + (S_v − S_r)` 是熵距离版，未启用）
5. **只填 MF**：`new ComparisonResult(matchFactor)`（无 RMF/MFD/RMFD，无逆向匹配）。

> 数学注记：`ln(4)` = 熵差的理论最大（两正交分布联合熵），使 0≤sim≤1。S=0（单峰）时完全一致→100；完全不共享离子→0。

### 3.3 距离家族（`comparison.supplier.distance`，✅）

扩展点注册 8 个比较器（`distance/plugin.xml`）：Euclidean Distance、Cosine、Cosine Binary (0|1)、Cosine Multiply (m/z*Intensity)、Pearson Distance、Covariance、Pareto 025/050/075。

**(a) 余弦族**（`AbstractCosineComparator` + 子类）：
- 向量构造：在 **unknown 的 [startIon, stopIon]** 上取两谱丰度（等长对齐）。`getVectorValue` 可覆写：
  - `CosineComparator`：`abundance(i)`（标准余弦）
  - `CosineBinaryComparator`：`abundance(i)>0 ? 1 : 0`（0/1 存在性）
  - `CosineMultiplyComparator`：`mz × abundance(i)`（m/z 加权）
- **MF = cos(φ) = (u·r)/(|u||r|) × 100**（Apache commons `ArrayRealVector.cosine`）；零向量/维度异常 → 0。
- **MFD/RMFD 用 `calculateCosinePhiDirect`**：只取 unknown 非零离子（`ab_u>0` 建列表，reference 同下标对齐）。
- 结果用 `LazyComparisonResult`（§2.4 短路）。

**(b) 距离族**（`AbstractDistanceComparator`，Euclidean/Pearson/Covariance/Pareto）：
- 向量 = unknown 区间内两谱丰度；各自 **unitVector()**（L1 归一化为单位向量）。
- `distance = distanceMeasure.compute(unitU, unitR)`；`match = 0.5 × distance`（注释：两单位向量在第 I 象限最大距离 = √(1²+1²)=√2，故 ×0.5 使 0≤match≤~0.707）。
- **MF = (1 − match) × 100**；RMF/MFD/RMFD 同式（direct 版只取 unknown 非零离子）。
- 距离测度实现（`distance/internal/`）：
  - `EuclideanDistance`：√(Σ(u_i−r_i)²)
  - `PearsonDistance`：`1 − PearsonCorrelation(u,r)`（`PearsonsCorrelation`，无偏）
  - `CovarianceDistance`：`1 − cov(u,r)/cov(u,u)`（协方差归一）
  - `ParetoDistance(p)`：`1 − rXY/rXX`，其中 `rXY = cov(x,y)/(σ_x·σ_y)^p`、`rXX = cov(x,x)/(σ_x²)^p`，p ∈ {0.25, 0.5, 0.75}
  - 零向量/除零 → MathArithmeticException → match=1（最差）。
- 结果 `ComparisonResult(mf, rmf, mfd, rmfd)`（非惰性）。

> **家族异同小结（Qt 选型）**：Alfassi（几何距离倒置）、熵（香农熵信息相似度）、Cosine 系（点积相似度）三者思路不同；Euclidean/Pearson/Covariance/Pareto 是"距离→相似度"的 (1−d)×100 转换。**全部可纯 C++ 实现**，只需 Apache Commons 的 cosine/euclidean/pearson/covariance 公式（§11 给公式）。

### 3.4 比较器门面与扩展点（✅）

- 扩展点：`org.eclipse.chemclipse.chromatogram.msd.comparison.massSpectrumComparisonSupplier`，元素属性 `id / description / comparatorName / massSpectrumComparator / nominalMS / tandemMS / highResolutionMS`（plugin.xml 直证，三族均注册于此）。
- 门面 `msd.identifier/comparison/MassSpectrumComparator.java`（static）：
  - `compare(unknown, reference, comparatorId, usePreOptimization, threshold)`：comparatorId 空 → **默认 cosine**（`...supplier.distance.cosine`）；`getMassSpectrumComparator(id)` 经 `createExecutableExtension("massSpectrumComparator")` 反射实例化 + 注入 supplier；`usePreOptimization` 走 `ComparatorCache`（缓存预过滤）。
  - `getAvailableComparatorIds()` / `getMassSpectrumComparatorSupport()` → `MassSpectrumComparatorSupport`（suppliers 列表，供 UI 下拉）。
- `IMassSpectrumComparisonSupplier`：`getId/getDescription/getComparatorName/supportsNominalMS/supportsTandemMS/supportsHighResolutionMS`（扩展声明 nominalMS/tandemMS/highResolutionMS）。
- 基类 `AbstractMassSpectrumComparator`：`validate(unknown, reference)`（非空 + `getIons()` 非空），`compare(...)` 由子类实现。UI 适配器 `comparison.ui/...IMassSpectrumComparisonSupplierAdapterFactory` 把 supplier 转 `ILabelProvider`（下拉显示 comparatorName/description）。

---

## 4. 鉴定扩展点与 supplier 三件套（★ ✅）

### 4.1 标识接口族（✅ 签名逐字）

| 域 | 接口 | 签名 |
|---|---|---|
| 通用峰 | `model/identifier/peak/IPeakIdentifier` | `identify(IPeak, IPeakIdentifierSettings, IProgressMonitor)`、`identify(IPeak, monitor)`、`identify(List<IPeak>, [settings], monitor)`、`identify(IChromatogramSelection, monitor)` → `IProcessingInfo<?>` |
| MSD 峰 | `chromatogram.msd.identifier/peak/IPeakIdentifierMSD` | `IProcessingInfo<IPeakIdentificationResults> identify(List<? extends IPeakMSD>, IPeakIdentifierSettingsMSD, IProgressMonitor)` |
| CSD 峰 | `chromatogram.csd.identifier/peak/IPeakIdentifierCSD` | 同构（`IPeakCSD` + `IPeakIdentifierSettingsCSD`）+ `getLiteratureReferences()` |
| WSD 峰 | `chromatogram.wsd.identifier/peak/IPeakIdentifierWSD` | 同构（`IPeakWSD`） |
| MSD 质谱 | `msd.identifier/IMassSpectrumIdentifier` | `IProcessingInfo<IMassSpectra> identify(List<IScanMSD>, IMassSpectrumIdentifierSettings, IProgressMonitor)` |
| 色谱 | `chromatogram.{msd|wsd|xxd}.identifier/chromatogram/IChromatogramIdentifier` | `IProcessingInfo<?> identify(IChromatogramSelection, IChromatogramIdentifierSettings, IProgressMonitor)`（+ 无 settings 版 + `getLiteratureReferences()`） |
| 扫描(web) | `chromatogram.xxd.identifier/scan/IScanIdentifierSupplier extends ISupplier` | `URL getURL(IScan)` |
| 目标(web) | `chromatogram.xxd.identifier/targets/ITargetIdentifierSupplier extends ISupplier` | `URL getURL(ILibraryInformation)` |
| WSD 波谱 | `chromatogram.wsd.identifier/wavespectrum/IWaveSpectrumIdentifier` | `IProcessingInfo<...> identify(List<IScanWSD>, ...)` |

> 基类骨架 `AbstractPeakIdentifier{MSD|CSD|WSD}`：仅含 `validatePeak(peak)` / `validateSettings(settings)`（null 抛 ValueMustNotBeNullException）——**标识器实现的最小契约**。

### 4.2 Supplier 三件套（✅ 统一模板，`PeakIdentifierMSD.java` 直证）

```
PeakIdentifierMSD  (static 门面，model 层)
 ├─ EXTENSION_POINT = "org.eclipse.chemclipse.chromatogram.msd.identifier.peakIdentifier"
 ├─ getPeakIdentifierSupport()  → 扫注册表 → PeakIdentifierSupport.add(PeakIdentifierSupplierMSD(...))   // 元素属性 id/description/identifierName/settingsClass
 ├─ getPeakIdentifier(id)      → createExecutableExtension("peakIdentifier") 反射实例化实现
 └─ identify(peaks, settings, id, monitor)  → 实例.identify(...)
```

三件套命名：`I{X}Identifier`（实现接口）、`I{X}IdentifierSupplier`（元数据：id/description/identifierName/getSettingsClass/getLiteratureReferences，extends `model/identifier/core/ISupplier`）、`I{X}IdentifierSupport`（supplier 注册表），外加 `{X}Identifier` 静态门面（扫 Eclipse 扩展注册表 `Platform.getExtensionRegistry().getConfigurationElementsFor(EXTENSION_POINT)`）。
- 示例 supplier：`chromatogram.msd.identifier/peak/PeakIdentifierSupplierMSD.java`、`ChromatogramIdentifierSupplier.java`（xxd）✅。
- 另见 `xxd.identifier/targets/TargetIdentifier.java` 与 `scan/ScanIdentifier.java`：**逐字同一模板**（extension point `...xxd.identifier.targetIdentifier` / `...xxd.identifier.scanIdentifier`，属性 `targetURL` 反射实例化）。
- 新版同时叠加 OSGi DS：`PeakIdentifierFilter`（timeranges）`@Component(service = IProcessTypeSupplier.class)` 走 `IChromatogramSelectionProcessSupplier`（MODULE_03 处理管线机制）——双机制并存（同 MODULE_10 滤波器结论）。

---

## 5. NIST 接口：进程调用（离线，外部程序）★ ✅

插件 `org.eclipse.chemclipse.msd.identifier.supplier.nist`。**形态 = 把待鉴定谱导出为文件、spawn NIST MS Search 可执行程序（`nistms.exe`）、轮询结果文件、正则解析回填**。需本机安装 NIST 库；非 Windows 用 Wine（`LinuxWineSupport/MacWineSupport`）。

### 5.1 全链路（`core/support/Identifier.java`，✅ 逐行）

```
Identifier.runMassSpectrumIdentification(scans, settings, monitor)
 ① 预过滤：LimitSupport.doIdentify(scan.getTargets(), limitMatchFactor)  // 已有 MF≥limit 的目标跳过
 ② 标签：把每个 scan.setIdentifier("ID-1", "ID-2", …)（结果回填时反查）→ identifierTable 备份
 ③ 控制文件备份/清理（AUTOIMP.MSD、FILESPEC.FIL → .openchrom.bak）
 ④ 导出：DatabaseConverter.convert(file, massSpectra, false, MSL_CONVERTER_ID)
      // MASSSPECTRA.MSL（AMDIS MSL 格式，converter id "...amdis.massspectrum.msl"）
 ⑤ 写控制文件：
      AUTOIMP.MSD  = 一行：FILESPEC.FIL 的路径
      FILESPEC.FIL = 一行：MASSSPECTRA.MSL 路径 + " OVERWRITE"
 ⑥ 改 NIST 设置：nistms.INI 里 "Hits to Print=" 替换为目标数（NistSupport.setNumberOfTargets）
 ⑦ runNistApplication：Process = runtimeSupport.executeRunCommand()  （spawn nistms.exe 批处理）
      waitForFile(SRCREADY.TXT) → waitForFile(SRCRESLT.TXT)
      （100ms 轮询存在性 + 1s 轮询文件长度稳定；超时 = settings.getTimeoutInMinutes()×60000ms）
 ⑧ 解析：NistResultFileParser.getCompounds(SRCRESLT.TXT)
 ⑨ 回填：assignMassSpectrumCompounds → getMassSpectrumIdentificationEntry(hit, compound)
 ⑩ 清理临时文件、恢复控制文件、还原 scan identifier
```

### 5.2 文件/常量（`runtime/INistSupport.java`，✅）

| 常量 | 值 |
|---|---|
| `NIST_IDENTIFIER` | `"NIST (extern)"`（旧版 `"NIST"` / `"NIST (external)"` 兼容） |
| 结果文件 | `SRCREADY.TXT`（就绪信号）、`SRCRESLT.TXT`（结果） |
| 控制文件 | `AUTOIMP.MSD`、`FILESPEC.FIL`、`nistms.INI`（`"Hits to Print="` 行改写） |
| 谱文件 | `MASSSPECTRA.MSL`（AMDIS 格式导出） |
| 可执行 | `nistms`（小写前缀校验，`validateExecutable`） |
| 杀进程 | Windows `taskkill /IM nistms.exe`（批处理结束 finally 中执行） |

### 5.3 结果解析格式（`internal/results/NistResultFileParser.java`，✅ 正则）

每化合物块：
```
Unknown: ID-1; Compound in Library Factor = 1.0
Hit 1 : <<名字>>;<<分子式>>; MF: 641; RMF: 650; Prob: 3.47; CAS:1332747-94-0; Mw: 144; Lib: <<mainlib>>; Id: 112275; RI: 1509.
```
- 正则逐段提取：Compound（`Unknown:…Compound in Library Factor = …`）、Hit 行、`<<name>>;<<formula>>`、`MF/RMF/Prob`、`CAS`、`Mw/Lib/Id/RI`。
- **MF/RMF 文件内是 1000 制 → `/10.0f` → 0~100**（`addMatchFactor` 注释明文）。
- 结果文件读为 **ISO8859_1**（防乱码）。
- 回填 target：`LibraryInformation(name, cas, contributor="NIST", referenceIdentifier="NIST", retentionIndex=hit RI)` + `ComparisonResult(MF, RMF, 0, 0, Prob)` + `setInLibFactor(Compound in Library Factor)` + `IdentificationTarget`；峰版用 `PeakLibraryInformation`/`PeakComparisonResult`。
- 命中过滤：`MF ≥ minMatchFactor && RMF ≥ minReverseMatchFactor` → `setIdentifier(NIST_IDENTIFIER)` → `IIdentificationTarget.getTargetsSorted(targets, peak RI)`（§4 排序）→ 取前 `numberOfTargets` 个 `peak.getTargets().add(...)`。

> **Qt 替代启示**：NIST 不是可编程库 API，而是"文件握手 + 进程 + 结果文件解析"的**批处理适配层**。自研 CDS 若不想依赖外部程序，可把 NIST 结果文件格式（SRCRESLT.TXT）作为互操作目标，或直接自建谱库（§11）。

---

## 6. MassBank：在线浏览器跳转（✅，非 JSON 查询）

社区插件 `openchrom/plugins/net.openchrom.msd.identifier.supplier.massbank/`（仅 3 个类）。

- **形态 = 构造 URL 交给系统浏览器打开**（`getURL()` 返回 URL，UI 侧 `Desktop.browse`/Eclipse 浏览器打开；**不是 JSON/HTTP 查询**）。
- `ScanIdentifier implements IScanIdentifierSupplier`：按峰查询——URL = `{domain} + "QpeakResult.jsp?type=quick&searchType=peak&sortKey=not&sortAction=1&pageNo=1&qpeak={top10 离子}&CUTOFF=5&num=20"`；`extractTracesIntensity`：取前 10 个丰度最高离子，m/z 保留 1 位小数，**相对强度 = 丰度 × (1000/maxIntensity)**（归一到基峰 1000），每离子 `mz%20relint%0D%0A`。
- `TargetIdentifier implements ITargetIdentifierSupplier`：按 InChIKey 查询——URL = `{domain} + "Result.jsp?inchikey={inchiKey}&type=inchikey&searchType=inchikey"`。
- 镜像域名（`PreferenceSupplier.getDomain()`）：EU `https://massbank.eu/MassBank/` 或 JP `https://massbank.jp/`。

> 结论：**MassBank 是"在线入口"而非"在线检索 API"**——OpenChrom 不下载/解析 MassBank 数据，只是把查询串拼进浏览器。自研 CDS 若要真正离线/在线检索 MassBank，需另写 HTTP 客户端 + HTML/JSON 解析。

---

## 7. 文件标识器与时间窗标识器（supplier 示例，✅）

### 7.1 File Identifier（离线本地谱库检索，核心参考价值最高）

插件 `chromatogram.xxd.identifier.supplier.file`，核心 `identifier/FileIdentifier.java`：

- 谱库来源：用户配置的本地质谱库文件列表（`PreferenceSupplier.getMassSpectraFiles()`，支持 MSL/MSP 等，`DatabasesCache` 按文件内容变更缓存）。
- 主流程 `compareAgainstDatabase(unknowns, references, settings, identifier, dbName, results, monitor)`：
  1. 预过滤 `LimitSupport.doIdentify(..., limitMatchFactor)`。
  2. 取配置的比较器 `IMassSpectrumComparator`（即 §3 扩展点注册的任意算法）。
  3. 每个未知谱：**`FindMatchingSpectras`（ForkJoin，`THRESHOLD=400` 分片）** 并行遍历全部参考谱：
     - `comparator.compare(unknown, reference, MatchConstraints(minMF/100, minRMF/100))`
     - `DeltaCalculationSupport.useTarget(unknown, reference, settings)`（RT/Rt/RI 窗口过滤，见 7.3）
     - `PenaltyCalculationSupport.applyPenalty(unknown, reference, comparisonResult, settings)`（见 7.4）
     - 命中条件 `MF ≥ minMF && RMF ≥ minRMF` → `Map<IComparisonResult, IScanMSD>` 收集。
  4. 结果按 `reverseOrder(MATCH_FACTOR_COMPARATOR)`（MF 降序）排序 → 取前 `numberOfTargets` 个 → `TargetBuilderMSD.getMassSpectrumTarget(reference, comparisonResult, identifier, dbName)` 建 target → `unknown.getTargets().add(...)` + `identificationResult.add(...)`。
  5. target 的 `LibraryInformation.miscellaneous` 记录比较器名（如 "Cosine"）——**同一目标可追溯用什么算法匹配的**。
- `TargetBuilderMSD.initializeLibraryInformation`（`msd.identifier/support/TargetBuilderMSD.java`）：参考谱若是 `IRegularLibraryMassSpectrum`，**整份拷贝其 `getLibraryInformation()` 全字段**（cas/comments/contributor/database/formula/inChI/miscellaneous/molWeight/name/referenceIdentifier/smiles/synonyms/classifiers），并补 `retentionTime/retentionIndex`（取参考谱本身）；否则 name/cas/miscellaneous = `"???"`（未知）。`getMassSpectrumTarget` 另 `setLibraryScan(reference)`（供 ILibraryService 反查展示）。
- `LibraryService`（`core/LibraryService.java`，extends `AbstractLibraryService`）：`identify(IIdentificationTarget)` 按 target.identifier=="File Identifier" 定位缓存并返回参考谱——**这就是 §1 说 identifier 禁翻译的原因**。

### 7.2 Time Ranges（时间窗打标，✅）

`chromatogram.xxd.identifier.supplier.timeranges/core/PeakIdentifierFilter.java`（OSGi DS `IProcessTypeSupplier`）：
- 每个 `TimeRange`（用户定义时间窗 + 名称）：筛出窗内峰（`peakModel.getPeakMaximum().getRetentionTime()` ∈ [start, stop]，且 `LimitSupport.doIdentify` 通过），按 `PeakFilterOption`（AREA→峰面积 / 默认→峰高）取**唯一最佳峰** → 建 `LibraryInformation(name=时间窗名)` + `ComparisonResult(matchQuality)` + `IdentificationTarget` → `peak.getTargets().add(...)`。

### 7.3 DeltaCalculationSupport（RT/RI 窗口过滤，✅）

`model/identifier/DeltaCalculationSupport.java`：`useTarget` 语义 = **参考值 ∈ [未知值−窗口, 未知值+窗口]**（RETENTION_TIME_MS / RETENTION_TIME_MIN=值÷60000 / RETENTION_INDEX 三档）；未知谱本身无 RT（0）时 `useTarget=false`（默认分支 true）。

### 7.4 PenaltyCalculationSupport（罚分公式，✅）

`model/identifier/PenaltyCalculationSupport.java` + `IPenaltyCalculationSettings`：
- 罚分依据 RT(ms)/RT(min)/RI 与库参考的偏差：`windowRangeCount = |u−r|/window`；`windowRangeCount ≤ 1` → **0 罚**；否则 `penalty = (windowRangeCount−1) × penaltyLevelFactor`，封顶 `maxPenalty`。
- 参考值缺失（=0）→ `penalty = penaltyMissingReference`；结果 clamp 0~100。
- 默认常量：`DEF_PENALTY_LEVEL_FACTOR=5.0f`、`DEF_PENALTY_MATCH_FACTOR=20.0f`、`MIN/MAX_PENALTY_MATCH_FACTOR=0/100`。
- 应用点：`FileIdentifier.findMatchingReferences` 中 `comparisonResult.setPenalty(penalty)` → 进而 `getMatchFactor()` 全部自动减去罚分（§2.1 调整语义）——**RT/RI 偏差作为 MF 减分项，直接影响命中排序**。

### 7.5 WSD blastn（✅ 存在性，非本模块重点）

`chromatogram.wsd.identifier.supplier.blastn`：对 WSD 波长谱做核苷酸 BLAST（本地 `LocalNucleotideBLAST` / 在线 `WebNucleotideBLAST`，XML 结果解析 `XmlReaderVersion1`）——**波谱域特有**，Qt 自研 CDS 一般不需要。

### 7.6 缓存机制：DatabasesCache / ComparatorCache（✅ 源码确认）

两个缓存类都在插件 `org.eclipse.chemclipse.msd.identifier`。

**DatabasesCache**（`msd.identifier/support/DatabasesCache.java`）——File Identifier 谱库缓存（§7.1 的 `DatabasesCache` 即此）：
- **6 个静态字段**（类级共享，跨实例；key = **谱库文件 basename**，非全路径）：
  - `fileSizes: Map<名称, Long>`、`fileModifications: Map<名称, Long>`、`fileNames: Set<名称>`
  - `massSpectraDatabases: Map<名称, IMassSpectra>`（解析结果本体）
  - `allDatabaseNames: Map<名称, Map<name, IScanMSD>>`、`allDatabaseCasNumbers: Map<名称, Map<CAS, IScanMSD>>`（**按 `LibraryInformation.getName()`/`getCasNumber()` 建倒排索引**，仅 `IRegularLibraryMassSpectrum`）
- **失效策略**（`getDatabases`:108）：缓存已有则比较 `file.length()` 与 `file.lastModified()`，**任一变化或首次出现 → 重新 `DatabaseConverter.convert(file)` 全量加载**；无内容 hash。
- **清理**：请求列表外的库从 3 个数据 Map 移除（:117-130）；全部为空抛 `FileNotFoundException`；`resetCache()` 静态清空全部。
- `getDatabaseMassSpectra(IIdentificationTarget, monitor)`（:147-192）：按 target 的 `libraryInformation.getDatabase()` 定位库 → **先 name 后 CAS** 反查参考谱（供比对视图）。
- ⚠️ 细节：reload 时未清空内层 name/CAS 索引（累加不覆盖）；`isLoaded()` 恒 true（仅判 null）。

**ComparatorCache**（`msd.identifier/comparison/internal/ComparatorCache.java`）——比较器预优化缓存（§3.4 `usePreOptimization` 走它）：
- **静态 `Map<Integer, Set<Integer>>`** `unknownTopIons` / `referenceTopIons`：键 = `getIons().hashCode()`，值 = **按丰度降序取前 12 个离子的标称 m/z 集**（`NUMBER_TOP_IONS=12`；标称化 = `AbstractIon.getIon` 即 `IonRoundMethod` 四舍五入）。
- **无显式失效**：javadoc 明文 "It is assumed that the reference is not modified."——谱不可变假设下不重算。
- **预过滤**（`useReferenceForComparison`）：`hits` = 未知谱 top12 中在参考 top12 内命中的 m/z 数；`percentageHits = hits / referenceIons.size()`；**≥ threshold 才进入完整比较**——大库扫描时跳过明显不匹配参考谱。
- ⚠️ **疑似复制粘贴 bug（标 ❓）**：`:65 int keyReference = unknown.getIons().hashCode();` 参考缓存键用了 unknown 的离子 hash（应为 `reference`）。每次调用重算覆盖，功能不致错，但 referenceTopIons 键语义错误（多个 unknown 各存一份参考 top12）。

---

## 8. 目标排序语义（IdentificationTargetComparator，★ ✅）

`model/comparator/IdentificationTargetComparator.java`（`IIdentificationTarget.getTargetsSorted` 使用，`SortOrder.DESC` 时结果取反）：

排序优先级（逐级比较，同则下一级）：
1. **`isVerified`**：人工核验过的优先（`Boolean.compare`）。
2. **Δ保留指数**：`deltaRI = |retentionIndexSource − libInfo.getRetentionIndex()|`（仅当 `PreferenceSupplier.isUseRetentionIndexQC()` 且 source≠0）；**注意方向反转**（注释明文 "OPPOSITE DIRECTION"：偏差最小最好 → 比较时 `delta2 − delta1`）。
3. **matchFactor**（大优先）。
4. **reverseMatchFactor**。
5. **matchFactorDirect**。
6. **reverseMatchFactorDirect**。
7. **probability**。

→ 同一目标集的"最佳命中" = `getIdentificationTarget(Set, retentionIndex)` 排序后取第 0 个（`IIdentificationTarget` 静态方法，§1.1）。

---

## 9. 数据流总结（一条链）

```
用户配置：谱库文件(File Identifier) / NIST 安装路径(NIST) / 时间窗(TimeRanges)
待鉴定对象：List<IPeakMSD> 或 List<IScanMSD>（峰→peak.getExtractedMassSpectrum()）
  ├─ 预过滤：LimitSupport.doIdentify(targets, limitMatchFactor)   // 已有高 MF 目标跳过
  ├─ 每参考谱：IMassSpectrumComparator.compare(unknown, reference, MatchConstraints)
  │    ├─ 谱预处理：extractIonSignal → normalize(100)/normalizeVector(1)
  │    ├─ 算法：Alfassi / 熵 / Cosine* / Euclidean / Pearson / Covariance / Pareto
  │    └─ 结果：IComparisonResult(MF, RMF, MFD, RMFD[, Prob])（熵只 MF）
  ├─ 过滤：DeltaCalculationSupport.useTarget + PenaltyCalculationSupport.applyPenalty
  ├─ 筛选：MF≥minMF && RMF≥minRMF
  ├─ 排序：IComparisonResult 降序 / IdentificationTargetComparator（含 ΔRI、verified）
  ├─ 截断：取前 numberOfTargets
  └─ 写回：TargetBuilderMSD 建 IdentificationTarget(LibraryInformation, ComparisonResult)
             → unknown/peak.getTargets().add(...)   （IIdentificationResult 双写）
显示：UI 读 getTargets() → getTargetsSorted → 列表/标签；getLibraryInformation 展示化合物元数据
外部：NIST=进程+文件握手；MassBank=浏览器 URL 跳转；File=本地谱库离线检索
```

---

## 10. 与 MODULE_02/03/04 的交叉引用（✅）

- **存储**：targets 挂载点 = `ITargetSupplier.getTargets()`（`Set<IIdentificationTarget>` 可变 HashSet，MODULE_02 §1.4/§3）；`AbstractScan`/`AbstractChromatogram` 均有 `identificationTargets` 字段；`IPeak` 同（MODULE_04）。
- **排序读法**：UI/报告用 `IIdentificationTarget.getTargetsSorted`（MODULE_02 交叉证据 C-Q 附近）。
- **处理管线**：identifier 双注册（经典扩展点 + OSGi DS `IProcessTypeSupplier`，MODULE_03）。
- **MSD 谱来源**：峰鉴定取 `peak.getExtractedMassSpectrum()` / `peak.getPeakModel().getPeakMassSpectrum()`（MODULE_04/09）。
- **挂载细节**：`AbstractPeak.getTargets()` 委托 `peakModel.getPeakMaximum().getTargets()`（峰顶扫描的集合，峰自身无独立 targets，见 §1.4）。

---

## 11. Qt/C++ 移植要点（⚠️ 设计笔记，公式可直接照搬）

- **目标结构**：`struct IdentificationTarget { LibraryInformation libInfo; ComparisonResult result; QString identifier; bool verified; }`；`ComparisonResult` 存**原始** 4 因子 + penalty + probability + inLibFactor + isMatch，`getMatchFactor() = qMax(0.0f, raw − penalty)`（复刻 §2.1 调整语义）。`LibraryInformation` 按 §1.3 全字段（QString/QVector；CAS 多值取首项）。
- **Set 去重**：targets 用 `QHash` 时 equals/hashCode 需含 identifier+name+cas+MF+RMF（复刻 AbstractIdentificationTarget），或退化为 `QVector` + 显式去重。
- **三种算法纯 C++ 移植（Qt 无 Apache Commons 依赖）**：
  - 余弦：`MF = (Σu_i·r_i) / (√Σu²·√Σr²) × 100`（注意先做未知谱离子区间对齐）。
  - Euclidean 等距离族：unit 化后 `match = 0.5·√(Σ(u−r)²)`，`MF=(1−match)×100`；Pearson `1−corr`；Covariance `1−cov(u,r)/cov(u,u)`；Pareto 幂公式（§3.3b）。零向量 → 最差 0。
  - Alfassi：`S=√(Σab²)` → `MF=100/(1+Σ(ab_u/S_u − ab_r/S_r)²)`。
  - 熵：L1 归一（`p_i = ab_i/Σab`）→ `S=−Σ p·ln p` → `sim = 1−(2S_v−S_u−S_r)/ln4`，×100。
  - 全部建议 `double` 中间量 + 输入 `QVector<float>`（标称质量轴对齐）；**核心就 4 个公式，Qt 里可直接内联**。
- **排序**：`IdentificationTargetComparator` → Qt `std::sort` 比较器（verified → ΔRI（升序）→ MF → RMF → MFD → RMFD → prob）；"最佳命中" = 排序后 [0]。
- **评分**：RatingSupplier.getScore 渐进均值 + 建议文案（Incomplete/Convoluted）+ 5 档状态，可直接照搬常量（80/20、90/80/70/60）。
- **标识器抽象**：`IPeakIdentifierMSD`/`IMassSpectrumIdentifier` 三件套 ≈ Qt 的 `IdentificationService`（接口）+ `IdentificationPlugin`（工厂注册 `QPluginLoader`/静态注册表）+ `IdentificationResult`（元数据 + 实例化）。接口签名照搬：`identify(QList<PeakMSD*>, Settings*, QProgressMonitor*) → Result`。
- **NIST 替代方案（自研推荐）**：不依赖外部进程，**自建谱库** = SQLite（化合物表 + 谱表 `mz BLOB/丰度 BLOB` + RT/RI/库名/CAS/SMILES/InChI/InChIKey）或 MSP 文本文件；检索 = 预加载到内存 → 任选 §3 算法全库扫描 → 命中过滤（RT/RI 窗口 + 罚分）→ 排序取前 N。NIST 结果文件格式（SRCRESLT.TXT 正则段）可作为**互操作导出/导入格式**参考。
- **MassBank**：若只做"查看"沿用 URL 跳转（`QDesktopServices::openUrl`）；若做真实检索需写 `QNetworkAccessManager` GET + HTML/JSON 解析（OpenChrom 本身未实现）。
- **性能**：`LazyComparisonResult` 短路链（MF 不达标就不算 RMF/MFD/RMFD）值得照搬——大谱库检索是主要优化点；Java ForkJoin(threshold=400) → Qt `QtConcurrent`/线程池按参考谱分片。
- **LibraryService**：identifier 作为"目标→参考谱来源"的键，Qt 里用 `QHash<QString, LibraryProvider*>` 注册（保持标识符稳定、不翻译）。
- **目标挂载（Qt 关键）**：`IScan.getTargets()` 每扫描自带可变集合（Qt `QSet<IdentificationTarget>` 或显式去重 QVector）；**`IPeak.getTargets()` 没有独立集合，委托峰顶扫描**——Qt 里 Peak 持峰顶 Scan 指针/共享容器，避免双份存储。Best Match 不维护排序状态，读取时 `std::sort`（§8 比较器）取 [0]。
- **缓存移植**：File 库缓存 = `QHash<QString, QVector<LibrarySpectrum>>` + `QFileInfo::size()/lastModified()` 失效（复刻 DatabasesCache 的 size+mtime 即可，无需文件 hash）；检索预过滤可移植 ComparatorCache 的"top12 标称 m/z 命中率 ≥ threshold"短路（注意修复其 unknown/reference 键 bug）。

---

## 12. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| ID-A | IIdentificationTarget 结构（libraryInformation+comparisonResult+identifier+verified+makeDeepCopy）、静态工具（getTargetsSorted/getIdentificationTarget/createDefaultTarget）、identifier 禁翻译 | model/identifier/IIdentificationTarget.java | ✅ |
| ID-B | AbstractIdentificationTarget 字段与 null 校验、equals/hashCode 键（identifier+name+cas+MF+RMF） | model/identifier/AbstractIdentificationTarget.java | ✅ |
| ID-C | IdentificationTarget 实现 + libraryScan（IAdaptable 返回）、makeDeepCopy | model/implementation/IdentificationTarget.java | ✅ |
| ID-D | IComparisonResult 常量（0/100、罚分 0~100、概率 0~100、DEF_PENALTY=20）、getMatchFactor=raw−penalty clamp0、getPenalty/addPenalty | model/identifier/IComparisonResult.java + AbstractComparisonResult.java | ✅ |
| ID-E | compareTo 排序（isMatch→MF→RMF→MFD→RMFD 升序） | AbstractComparisonResult.compareTo | ✅ |
| ID-F | RatingSupplier：渐进均值 score、80/20 建议文案、5 档状态（90/80/70/60）；ratingSupplier 不入库 | model/identifier/RatingSupplier.java + AbstractComparisonRatingSupplier.java + RatingStatus.java | ✅ |
| ID-G | LazyComparisonResult 短路链（MF→RMF→MFD→RMFD）+ MatchConstraints（0~1）；ComparisonResultFinal 哨兵 | model/identifier/LazyComparisonResult.java + MatchConstraints.java + ComparisonResultFinal.java | ✅ |
| ID-H | LibraryInformation 全字段（name/synonyms/casNumbers/comments/referenceIdentifier/miscellaneous/formula/inChI/inChIKey/smiles/molWeight/exactMass/database/databaseIndex/contributor/hit/classification/retentionTime/columnIndexMarkers(RI)/flavorMarkers/moleculeStructure/compoundClass） | model/identifier/ILibraryInformation.java + AbstractLibraryInformation.java | ✅ |
| ID-I | Alfassi 公式：normalize(100) → S=√Σab² → result=(Σ((ab_u/S_u)−(ab_r/S_r))²+1)⁻¹ ×100；MF/RMF 用各自谱区间 | alfassi/comparator/MassSpectrumComparator.java + msd.identifier/comparison/math/GeometricDistanceCalculator.java | ✅ |
| ID-J | 熵公式：L1 归一→S=−Σp·ln p→sim=(1−(2S_v−S_u−S_r)/ln4)×100；virtual=逐离子求和；只填 MF | entropy/comparator/EntropyComparator.java（文献 Nat Methods 18,1524-1531,2021） | ✅ |
| ID-K | 距离族：Cosine/CosineBinary(0|1)/CosineMultiply(mz×ab) 的 getVectorValue + 点积余弦×100；MFD 只取未知非零离子；Lazy 结果 | distance/comparator/AbstractCosineComparator.java + CosineComparator.java + CosineBinaryComparator.java + CosineMultiplyComparator.java | ✅ |
| ID-L | 距离族：unit 向量→match=0.5·d→MF=(1−match)×100；Euclidean/Pearson(1−corr)/Covariance(1−covXY/covXX)/Pareto(幂缩放)；零向量→0 | distance/comparator/AbstractDistanceComparator.java + EuclideanComparator.java + internal/PearsonDistance.java + CovarianceDistance.java + ParetoDistance.java | ✅ |
| ID-M | 比较器扩展点 massSpectrumComparisonSupplier（id/description/comparatorName/massSpectrumComparator/nominalMS/tandemMS/highResolutionMS）；门面默认 cosine；反射实例化 | msd.identifier/comparison/MassSpectrumComparator.java + alfassi/entropy/distance plugin.xml | ✅ |
| ID-N | normalizeVector（L1 和=base）、normalizeIntensity（最大值=base）原语 | msd.model/xic/ExtractedIonSignal.java | ✅ |
| ID-O | 标识接口族签名：IPeakIdentifier（通用 4 重载）、IPeakIdentifier{MSD|CSD|WSD}（List+settings）、IMassSpectrumIdentifier（List<IScanMSD>→IMassSpectra）、IChromatogramIdentifier（Selection）、IScanIdentifierSupplier/ITargetIdentifierSupplier（getURL） | model/identifier/peak/IPeakIdentifier.java + chromatogram.msd.identifier/peak/IPeakIdentifierMSD.java + csd.identifier/peak/IPeakIdentifierCSD.java + wsd.identifier/peak/IPeakIdentifierWSD.java + msd.identifier/IMassSpectrumIdentifier.java + xxd.identifier/.../IChromatogramIdentifier.java + scan/IScanIdentifierSupplier.java + targets/ITargetIdentifierSupplier.java | ✅ |
| ID-P | Supplier 三件套模板 + 扩展点 id（msd peakIdentifier / xxd targetIdentifier / xxd scanIdentifier）+ createExecutableExtension | chromatogram.msd.identifier/peak/PeakIdentifierMSD.java + xxd.identifier/targets/TargetIdentifier.java + scan/ScanIdentifier.java + model/identifier/core/ISupplier.java | ✅ |
| ID-Q | NIST=进程+文件握手：导出 MASSSPECTRA.MSL→AUTOIMP.MSD/FILESPEC.FIL 控制文件→nistms.INI 改写→spawn nistms.exe→轮询 SRCREADY.TXT/SRCRESLT.TXT→正则解析→回填；Wine 兼容；taskkill | msd.identifier.supplier.nist/core/support/Identifier.java + runtime/NistSupport.java + WindowsSupport.java + INistSupport.java | ✅ |
| ID-R | NIST 结果格式与正则（Compound/Hit/name/formula/MF/RMF/Prob/CAS/Mw/Lib/Id/RI）；MF/RMF 1000 制÷10；ISO8859_1 | msd.identifier.supplier.nist/internal/results/NistResultFileParser.java | ✅ |
| ID-S | MassBank=浏览器 URL 跳转（非 JSON）：ScanIdentifier 峰查询 URL + 基峰 1000 归一 top10；TargetIdentifier InChIKey 查询；EU/JP 镜像 | net.openchrom.msd.identifier.supplier.massbank/identifier/ScanIdentifier.java + TargetIdentifier.java + preferences/PreferenceSupplier.java | ✅ |
| ID-T | File Identifier 全流程：DatabasesCache 谱库→FindMatchingSpectras ForkJoin(400)→Delta+Penalty 过滤→MF 降序取前 N→TargetBuilderMSD 建 target（库信息整份拷贝，miscellaneous 记录比较器名）→targets.add 双写 | xxd.identifier.supplier.file/identifier/FileIdentifier.java + msd.identifier/support/TargetBuilderMSD.java | ✅ |
| ID-U | TimeRanges 标识器：窗内取面积/峰高最佳峰→ComparisonResult(matchQuality) 打标 | xxd.identifier.supplier.timeranges/core/PeakIdentifierFilter.java | ✅ |
| ID-V | IdentificationTargetComparator 排序（verified→ΔRI(反)→MF→RMF→MFD→RMFD→prob）| model/comparator/IdentificationTargetComparator.java | ✅ |
| ID-W | LimitSupport.doIdentify（已有 MF≥limit 跳过）、DeltaCalculationSupport（RT/Rt/RI 窗口）、PenaltyCalculationSupport（(窗口倍数−1)×因子，封顶 max，缺失引用罚分） | model/support/LimitSupport.java + model/identifier/DeltaCalculationSupport.java + PenaltyCalculationSupport.java + IPenaltyCalculationSettings.java | ✅ |
| ID-X | 结果容器：IIdentificationResult.getBestHit=最大 MF；IIdentificationResults 0-based；NIST 双写（result+peak.getTargets()） | model/identifier/AbstractIdentificationResult.java + IIdentificationResults.java + msd.identifier.supplier.nist Identifier.assignPeakCompounds | ✅ |
| ID-Y | 峰变体：IPeakComparisonResult 增 isMarkerPeak；IPeakLibraryInformation 空；WSD blastn 存在（本地/在线 BLAST，波谱域） | model/identifier/IPeakComparisonResult.java + IPeakLibraryInformation.java + wsd.identifier.supplier.blastn（存在性） | ✅ |
| ID-Z | ITarget=空标记接口（`extends Serializable`，无成员）；AbstractTarget 空抽象（仅 serialVersionUID）；AbstractIdentificationTarget extends AbstractTarget implements IIdentificationTarget 链；statistics/Target 为无关实现 | model/targets/ITarget.java:31 + AbstractTarget.java:15 + identifier/AbstractIdentificationTarget.java:18 + statistics/Target.java:19 | ✅ |
| ID-AA | ITargetReference extends ITargetSupplier（非 ITarget）：getSignal/RT分/RI/getID/type + 默认 getBestIdentificationTarget/getTargetLabel；TargetReference 委托 supplier.getTargets()，ID=type.label()+"."+RT分；静态 getScanReferences/getPeakReferences/createVisibilityFilter | model/targets/ITargetReference.java:20/47-60 + TargetReference.java:33/74-81/107-162 + TargetReferenceType.java:18-21 | ✅ |
| ID-AB | ITargetDisplaySettings：峰/扫描标签开关、DisplayOption 10 档、旋转角/碰撞深度、LibraryField、visibilityMap（key=TargetReference ID）；实现 AbstractMeasurementTarget（与 ITarget 继承链无关） | model/targets/ITargetDisplaySettings.java + DisplayOption.java:17-28 + core/AbstractMeasurementTarget.java:25 | ✅ |
| ID-AC | 挂载：IScan/IPeak/ISpectrumPeak 均 implements ITargetSupplier；AbstractScan 字段 identificationTargets=HashSet 直接返回；AbstractPeak 委托 peakModel.getPeakMaximum().getTargets()（峰顶扫描）；add=HashSet 去重（equals 键见 ID-B）；Best Match=getIdentificationTarget(Set,RI) sort 取 [0]（读取时排序） | core/IScan.java:28 + IPeak.java:24 + ISpectrumPeak.java:15 + AbstractScan.java:52/275-278 + AbstractPeak.java:337-340 + IIdentificationTarget.java:94-163 + TargetSupport.java:44-61 | ✅ |
| ID-AD | DatabasesCache：6 静态字段、basename 键、size+mtime 失效、name/CAS 倒排索引（仅 IRegularLibraryMassSpectrum）、unused 清理、resetCache、getDatabaseMassSpectra 先 name 后 CAS 反查；ComparatorCache：top12 标称 m/z 集、getIons().hashCode() 键、命中率≥threshold 预过滤、无失效（reference 不可变假设）；⚠️ ComparatorCache:65 参考键误用 unknown hash（❓） | msd.identifier/support/DatabasesCache.java + comparison/internal/ComparatorCache.java + msd.model/core/AbstractIon.java:84-87 | ✅（:65 疑 bug 标 ❓） |

## 13. 未确认 / 待验证清单

| # | 事项 | 状态 |
|---|---|---|
| ID-1 | `ITarget`/`AbstractTarget`（IIdentificationTarget 的父链）细节未深读 | ✅ 已深读（ID-Z/§1.3）：ITarget 空标记接口、AbstractTarget 空抽象、statistics/Target 无关实现 |
| ID-2 | `DatabasesCache` 内部实现（谱库文件缓存键/失效策略）未深读 | ✅ 已深读（ID-AD/§7.6）：basename 键 + size/mtime 失效 + name/CAS 倒排索引 |
| ID-3 | `ComparatorCache`（比较器预优化缓存）算法细节未读 | ✅ 已深读（ID-AD/§7.6）：top12 命中率预过滤、无失效；⚠️ :65 参考键用 unknown hash 疑似 bug 标 ❓ |
| ID-4 | NIST 之外商业标识器（如 F-Search/DB-Tools 扩展点声明）未逐个读 | ❓（扩展点机制已确认，具体插件未读） |
| ID-5 | `ILibraryService` 完整接口（AbstractLibraryService）未深读 | ❓（LibraryService 用法已确认） |
