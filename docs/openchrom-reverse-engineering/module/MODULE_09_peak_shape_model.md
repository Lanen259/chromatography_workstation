# MODULE_09 — PeakModel 峰形内部数学 + PeakBuilder 建峰变体（★ 重点）

> **状态：🟡 分析中（IPeakModel 全接口 ✅、AbstractPeakModel 实现 ✅、拐点方程族数学 ✅、IPeakIntensityValues 存储/归一化 ✅、PeakBuilderCSD/MSD/WSD 三变体 ✅、峰对象挂载关系 ✅）**
> 与 MODULE_04 互补：MODULE_04 覆盖**峰检测算法**（一阶导数/SNIP/基线/积分）与峰模型**接口层面**旧结论；本文深挖**峰形内部数学与实现类细节**。**严格遵守：✅ 源码确认 vs ⚠️ 推测，不许混。**
> 源码根：`.fetch/chemclipse-src/plugins/`（下文 Source 均相对此根；`chemclipse.model` 缩写 `cmodel`）

---

## 0. 结论速览（峰形数学的本质）

1. **峰不是数学函数（无 Gaussian/EMG 拟合）**：峰模型 = 强度分布表（`NavigableMap<RT, 相对强度%>`）+ 两条**拐点直线**。✅
2. **拐点方程 ≠ 二阶导零点**：是**峰形上升段/下降段中 |斜率| 最大的相邻点连线**（离散点峰形的"最陡侧翼切线"）。✅
3. **拐点峰顶/峰高 = 两条拐点直线交点**（x=RT, y=峰高）。✅
4. **各高度宽度 = 拐点直线与水平线 `y = height×峰高` 交点的 x 差 + 1ms**；基线宽 = 与 `y=0` 交点。✅
5. **tailing 有两条计算路径**：strict（拐点切线 × 10% 高线）与非 strict（强度表 50% 高线线性插值）——**两路径高度不同**。✅
6. **leading = 1/tailing**（互为倒数，理想=1）。✅
7. **背景 = 过峰两端 (RT, 背景丰度) 的线性方程**，随峰对象固化；峰信号 = 原始信号 − 背景（0 基纯峰）。✅
8. 建峰统一流程：抽总信号 → 定两端背景（VV/CB/BB）→ 线性背景方程 → 逐点扣背景 → **归一化 100%** → 存强度表 → 峰顶扫描（纯峰高）→ `PeakModel(峰顶, 强度表, 起/止背景)` → `ChromatogramPeak(峰模型, 色谱)`。✅

---

## 1. IPeakModel 接口成员（✅ 全接口源码确认）

文件 `org.eclipse.chemclipse.model/src/org/eclipse/chemclipse/model/core/IPeakModel.java`（`IPeakModel extends IPeakModelStrict, Serializable`，`MINIMUM_SCANS = 3`）。

| 类别 | 成员 | 签名/语义 |
|---|---|---|
| 模式 | `isStrictModel()` / `setStrictModel(boolean)` | strict=计算上升/下降切线；失败自动回退 false（见 2.6）|
| 背景 | `getBackgroundAbundance()` | **峰顶处**背景（0 基，ms）|
| 背景 | `getBackgroundAbundance(int retentionTime)` | 给定 RT 背景；越界返回 0 |
| 峰丰度 | `getPeakAbundance()` | 峰顶纯信号（0 基，**不含背景**）|
| 峰丰度 | `getPeakAbundance(int retentionTime)` | 给定 RT 纯峰信号（= 峰高 × 相对强度%/100）；越界 0 |
| 宽度 | `getWidthBaselineTotal()` | 基线总宽(ms) = stopRT − startRT + 1 |
| 起止 | `getStartRetentionTime()` / `getStopRetentionTime()` | ms |
| 峰顶 | `getRetentionTimeAtPeakMaximum()` | 表格式峰顶 RT（强度表最高键）|
| 替换 | `replaceRetentionTimes(List<Integer>)` | RT 网格替换（长度必须等于点数）|
| 扫描数 | `getNumberOfScans()` | = 强度表 size |
| 梯度角 | `getGradientAngle()` | 背景随时间增/减（见 2.3）|
| 峰形 | `getLeading()` / `getTailing()` | 前伸/拖尾（=1 理想）|
| RT 表 | `getRetentionTimes()` | 升序 RT 列表 |
| 高度基线 | `getPercentageHeightBaselineEquation(float height)` | 0–1 高度百分比处的**水平**基线方程 |
| 扫描 | `getPeakMaximum()`(IScan) / `getPeakScan(int rt)`(拷贝) | 峰顶扫描 / 按 RT 取扫描副本 |
| 强度 | `getIntensity(int retentionTime)` | 相对强度（0–100%）|
| 临时数据 | `getTemporarilyInfo(String)` / `setTemporarilyInfo(String,Object)` | 不持久化 |

> **拐点方程族全部在 `IPeakModelStrict`（见 3 节）**：`areInflectionPointsAvailable / calculateIntersection / getPeakAbundanceByInflectionPoints / getWidthBaselineByInflectionPoints / getWidthByInflectionPoints()(=0.5f) / getWidthByInflectionPoints(height) / getRetentionTimeAtPeakMaximumByInflectionPoints / getIncreasing·DecreasingInflectionPointAbundance`。✅
> `IPeakModel.java` 注释提到 `getWidthBaseline()` —— **该名不存在**（注释过时）；实际为 `getWidthBaselineByInflectionPoints()`。✅（同 MODULE_04 结论）

---

## 2. PeakModel 实现类（✅ `implementation/PeakModel.java` + `core/AbstractPeakModel.java`）

`PeakModel`（`org.eclipse.chemclipse.model/.../implementation/PeakModel.java`）是**薄子类**：构造 `(peakMaximum, peakIntensityValues, startBackgroundAbundance, stopBackgroundAbundance)` 调 `super(..., true)` —— **默认 strictModel = true**（源码注释："By default, the strict model is used to ensure backward compatibility. If the increasing/decreasing tangent can't be calculated, the less strict model is used."）。全部实现在 `AbstractPeakModel`。

### 2.1 字段（✅）

`core/AbstractPeakModel.java`：`strictModel`(boolean，**类内默认 false**，由子类构造传入 true)、`peakMaximum`(IScan)、`backgroundEquation`(LinearEquation)、`peakIntensityValues`(IPeakIntensityValues)、`gradientAngle`(double)、`startBackgroundAbundance`/`stopBackgroundAbundance`(float)、`temporarilyInfo`(Map<String,Object>)。

### 2.2 构造校验链 `calculatePeakModel()`（✅）

```text
calculatePeakModel()
 ├─ checkModelConditions(peakMaximum, peakIntensityValues)
 │    ├─ peakMaximum != null 否则 PeakException
 │    ├─ peakIntensityValues != null && size() >= MINIMUM_SCANS(3) 否则 PeakException
 │    ├─ getHighestIntensityValue() != null
 │    │    └─ ★ 强度表必须存在"值恰好 == 100.0f"的点（float 精确相等），否则 PeakException
 │    └─ peakMaximum.setRetentionTime(强度表最高键)   // 峰顶扫描 RT ← 强度表 apex
 ├─ backgroundEquation = calculateBackgroundEquation(startBg, stopBg)
 │    ├─ 负背景钳 0；过 (startRT, startBg)、(stopRT, stopBg) 两点建 LinearEquation
 │    └─ (Equations.createLinearEquation，见 3.4)
 ├─ gradientAngle = calculateGradientAngle()          // 见 2.3
 └─ validateStrictModel()                             // 见 2.6
```

### 2.3 梯度角 `calculateGradientAngle()`（✅）

```text
b = getWidthBaselineTotal()                  // stopRT - startRT + 1
start = getBackgroundAbundance(startRT)      // 背景方程在起点的值
stop  = getBackgroundAbundance(stopRT)
if(stop == start || b == 0) return 0
a = stop - start
return Math.toDegrees(Math.atan(a / b))
```

- **数学 = 背景线斜率 arctan 的度数**。`a>0`（stop>start）→ 角度为正。
- **接口注释（权威）**：`IPeakModel.getGradientAngle` —— "If the angle is positive, the background increases over time. If the angle is negative, the background decreases over time."
- ⚠️ **字段注释矛盾**：`AbstractPeakModel.java` 字段注释写反（"alpha negative → baseline raises"）。**以接口 Javadoc 为准**：角度正 = 背景随时间上升。

### 2.4 几何 getter（✅ 全部实现确认）

| getter | 实现 |
|---|---|
| `getStartRetentionTime` / `getStopRetentionTime` | 强度表 `firstKey()` / `lastKey()` |
| `getRetentionTimeAtPeakMaximum` | 强度表最高键 |
| `getWidthBaselineTotal` | `stopRT - startRT + 1` |
| `getPeakAbundance()` | `peakMaximum.getTotalSignal()`（纯峰高）|
| `getPeakAbundance(rt)` | 在 [start,stop] 内 → `peakMaximum.getTotalSignal() × 相对强度% / 100` |
| `getBackgroundAbundance(rt)` | 在范围内 → `backgroundEquation.calculateY(rt)`，否则 0 |
| `getBackgroundAbundance()` | 背景方程在峰顶 RT 处取值 |
| `getIntensity(rt)` | 强度表 `floorEntry(rt).getValue()`（≤RT 最近点），无则 0 |
| `getNumberOfScans` | 强度表 size |
| `getPeakScan(rt)` | `new Scan(intensity)` —— ⚠️ **返回扫描的 totalSignal = 相对强度%（0–100），非绝对丰度**（实现怪癖，见 3.5）|

### 2.5 峰形 getter（✅ 两路径，重点）

`getTailing()`：
```text
percentageHeightBaseline = getPercentageHeightBaselineEquation(0.1f)   // y = 0.1 × 峰高（水平线）
if(strictModel)  return calculateTailingByInflectionPoints(percentageHeightBaseline)   // 路径 A：拐点切线
else             return calculateTailingByIntensityValues()                            // 路径 B：强度表
```

- **路径 A（strict）**：见 3.2 —— 用**拐点切线**在 **10% 峰高**线求宽。
- **路径 B（非 strict）`calculateTailingByIntensityValues()`（✅ 实现细节）**：
  - `halfHeight = 最高值/2`（强度表 max=100 → **50% 高**）⚠️ 与路径 A 的 10% 不同
  - 扫描 RT 表：找**首个 强度 > 50%** 的点 i → `leftA=(rt[i-1],int[i-1])`、`leftB=(rt[i],int[i])`（峰起处首点即 >50% 时 leftA=leftB=该点）
  - 其后再找**首个 强度 < 50%** 的点 j → `rightA=(rt[j-1],int[j-1])`、`rightB=(rt[j],int[j])`
  - `pre/post` = 两点线性方程；`startRT = round(pre.calculateX(50))`、`stopRT = round(post.calculateX(50))`、`centerRT = 峰顶RT`
  - `tailing = (centerRT - startRT) / (stopRT - centerRT)` = **右宽/左宽**
- `getLeading()` = `1 / getTailing()`（tailing≠0 时）。✅
- ⚠️ 观测：**strict 与非 strict 的 tailing 数值不可比**（10% vs 50% 高度、切线 vs 强度表插值）。

### 2.6 strict 模式与失败回退（✅）

```text
validateStrictModel():
  if(strictModel):
      success = calculateInflectionPointEquations()   // 见 3.1
      if(!success) strictModel = false                 // ★ 回退非 strict
```
- `setStrictModel(true)` 也会触发 validateStrictModel。
- 回退后 `getTailing()` 走强度表 50% 路径；**其余拐点 getter（宽度等）返回 0**（因切线为空，见 3 节 `areInflectionPointsAvailable()==false` 分支）。✅

---

## 3. 拐点方程数学（✅ `core/AbstractPeakModelStrict.java` + `core/AbstractPeakIntensityValuesStrict.java` + `support/TwoPoints.java` + `numeric/equations/Equations.java`）

### 3.1 拐点方程的计算 `calculateInflectionPointEquations()`（✅）

```text
increasingInflectionPointEquation = peakIntensityValues.calculateIncreasingInflectionPointEquation(peakMaximum.getTotalSignal())
decreasingInflectionPointEquation = peakIntensityValues.calculateDecreasingInflectionPointEquation(peakMaximum.getTotalSignal())
（任一抛异常 → 两者皆 null → areInflectionPointsAvailable() = false）
```

委托到 `AbstractPeakIntensityValuesStrict`：

```text
calculateIncreasingInflectionPointEquation(values, totalSignal, maxIntensity):
    entry = getHighestIntensityValue()                  // 峰顶 Map.Entry
    increasingValues = intensityValues.headMap(entry.getKey(), true)   // 起点..峰顶（含）
    return calculateInflectionPointEquation(increasingValues, ...)

calculateDecreasingInflectionPointEquation(...):
    decreasingValues = intensityValues.tailMap(entry.getKey(), true)   // 峰顶..终点（含）
    return calculateInflectionPointEquation(decreasingValues, ...)

calculateInflectionPointEquation(values, totalSignal, maxIntensity):
    slopes = TreeMap<Double, TwoPoints>()               // key = |斜率|，自动升序
    for i in 0..size-2:                                 // 逐相邻点对
        p1 = (rt_i,      (v_i  /maxIntensity) × totalSignal)
        p2 = (rt_{i+1},  (v_{i+1}/maxIntensity) × totalSignal)   // 相对% → 绝对信号
        slopes.put(Math.abs(slope(p1,p2)), new TwoPoints(p1,p2))
    entry = slopes.lastEntry()                          // ★ |斜率| 最大的线段
    return entry.getLinearEquation()                    // 该两点的线性方程
```

**本质**：上升侧取 [起点, 峰顶] 内 |斜率| 最大的相邻两点连线；下降侧取 [峰顶, 终点] 内 |斜率| 最大的相邻两点连线。**不是二阶导零点，是离散点最陡侧翼切线**。⚠️ 若两侧存在等 |斜率| 的多条线段，`TreeMap.put` 后者覆盖前者（取靠后者）。✅

`TwoPoints.getLinearEquation()` = `Equations.createLinearEquation(p1,p2)`；`getSlope()` = `Equations.calculateSlope(p1,p2)`。✅

### 3.2 拐点 getter 实现（✅ `AbstractPeakModelStrict.java` 全方法）

| 方法 | 数学 |
|---|---|
| `areInflectionPointsAvailable()` | 两切线都非 null |
| `calculateIntersection()` | 两切线交点 `(x,y)`（`Equations.calculateIntersection`，平行/重合抛 SolverException）|
| `getPeakAbundanceByInflectionPoints()` | 交点 y（**拐点峰高**）|
| `getRetentionTimeAtPeakMaximumByInflectionPoints()` | 交点 x（**拐点峰顶 RT**）|
| `getWidthBaselineByInflectionPoints()` | 上升切线 ∩ `y=0` 基线 → p1；下降切线 ∩ y=0 → p2；`width = p2.x − p1.x + 1` |
| `getWidthByInflectionPoints()` | `getWidthByInflectionPoints(0.5f)`（半峰宽）|
| `getWidthByInflectionPoints(height)` | 水平线 `y = height×拐点峰高`；`p1 = 上升切线 ∩ 该线`，`p2 = 下降切线 ∩ 该线`；`width = p2.x − p1.x + 1` |
| `getPercentageHeightBaselineEquation(height)` | `height<0 或 >1 → null`；`new LinearEquation(0, 拐点峰高 × height)` —— **水平线**（峰与背景已分离，无需随背景角斜移）|
| `getIncreasing/DecreasingInflectionPointAbundance(rt)` | 切线方程 `calculateY(rt)` |
| `calculateTailingByInflectionPoints(baseline)` | `p1 = 上升切线 ∩ 10%线`，`p2 = 下降切线 ∩ 10%线`；`maxX = 两切线交点 x`；`leftWidth = maxX − p1.x`，`rightWidth = p2.x − maxX`；`tailing = rightWidth/leftWidth` |

> **宽度语义**：`p2.x − p1.x + 1`（含两端，1ms 单位；源码注释：不要用 `Equations.calculateWidth`（那是欧氏距离），值太大）。✅

### 3.3 交点/方程数学（✅ `numeric/equations/Equations.java`）

```text
createLinearEquation(p1,p2):
    a = (p2.y - p1.y) / (p2.x - p1.x)      // x 相等 → 0
    b = p1.y - a·p1.x
    return LinearEquation(a, b)             // f(x) = a·x + b

calculateSlope(p1,p2): (p2.y-p1.y)/(p2.x-p1.x)；分母 0 → 0

calculateIntersection(eq1, eq2):            // f(x)=ax+b, g(x)=cx+d
    eq1==eq2 → SolverException（重合）
    denominator = eq1.a - eq2.a;  ==0 → SolverException（平行）
    x = (eq2.b - eq1.b) / (eq1.a - eq2.a);  y = eq1.calculateY(x)
```

`LinearEquation`：`calculateY(x) = a·x + b`；`calculateX(y) = (y−b)/a`（**a==0 → NaN**）。✅

---

## 4. IPeakIntensityValues 存储/操作（✅ `core/IPeakIntensityValues.java` + `core/AbstractPeakIntensityValues.java`）

### 4.1 数据结构与 add（✅ `AbstractPeakIntensityValues`）

- 存储：`NavigableMap<Integer, Float> intensityValues = new TreeMap<>()`（**RT[ms] → 相对强度 0–100**，TreeMap 按键**自动升序**）；`maxIntensity` 默认 `MAX_INTENSITY = 100.0f`。
- `addIntensityValue(rt, rel)`：**`rt < 0 或 rel < 0 或 rel > maxIntensity` → 跳过**；否则 `put`（**同 RT 覆盖**，升序自动维护）。✅
- `getHighestIntensityValue()`：**首个 `getValue() == maxIntensity`（float 精确相等）** 的 entry，无则 null —— 是 `checkModelConditions` 的判据来源。✅
- `getIntensityValue(rt)`：`rt ∈ [start,stop]` → `floorEntry(rt)`（≤RT 的最近点）；否则 null。✅
- `getStartRetentionTime` / `getStopRetentionTime`：`firstKey()` / `lastKey()`。✅
- `getRetentionTimes()`：`new ArrayList<>(keySet())`（升序）。✅
- `replaceRetentionTimes(list)`：**size 必须相等**，否则忽略；按原顺序重建新 TreeMap。✅

### 4.2 normalize()（✅ 关键，保证最高值恰好 == 100）

```text
normalize():
    max = Collections.max(所有值)
    maxIntensity = MAX_INTENSITY
    for key in 升序键:
        value = max==value ? 100.0f                        // ★ 最大值 → 恰好 100.0f
              : (100.0f / max) × value                     // 否则按比例缩放
```
> 目的（源码注释）：`(100/max)×value` 可能得 99.99999f，后续所有要求"存在 == MAX_INTENSITY 值"的方法会失败；故最大值特殊处理成**精确 100.0f**。✅

### 4.3 拐点方程入口（✅）

`calculateIncreasing/DecreasingInflectionPointEquation(totalSignal)` 委托 `calculateInflectionPointEquation(intensityValues, totalSignal, maxIntensity)`（见 3.1）。⚠️ 注意：`addIntensityValue` 的越界条件是 `> maxIntensity` 而非 `> MAX_INTENSITY`，因此用自定义 maxIntensity 构造后 `normalize()` 是必须的（构造器注释明示）。

---

## 5. PeakBuilderCSD.createPeak（✅ `org.eclipse.chemclipse.csd.model/.../core/support/PeakBuilderCSD.java` 全方法）

### 5.1 主入口 `createPeak(IChromatogramCSD, IScanRange, PeakType)`（✅ 完整流程）

```text
validateChromatogram / validateScanRange / checkScanRange   // 起止扫描 ∈ [1, 色谱扫描数]
totalScanSignals = TotalScanSignalExtractor(chromatogram).getTotalScanSignals(startScan, stopScan)
startBg = totalScanSignals[startScan].getTotalSignal()      // 峰两端实测信号
stopBg  = totalScanSignals[stopScan ].getTotalSignal()
switch(peakType):                                           // ★ 背景来源三选一
    VV → BackgroundAbundanceRange(startBg, stopBg)          // 两端信号即背景（谷底）
    CB → chromatogram.getBaselineModel().getBackground(startRT / stopRT)   // 色谱基线模型
    其余 → base = min(startBg, stopBg); (base, base)        // 较低端水平背景
backgroundEquation = getBackgroundEquation(totalScanSignals, scanRange, bgRange)
    // = Equations.createLinearEquation((startRT, startBg), (stopRT, stopBg))
peakIntensityTotalScanSignals = adjustTotalScanSignals(totalScanSignals, backgroundEquation)
    // ① makeDeepCopy
    // ② 逐扫描 adjustedSignal = max(0, totalSignal − backgroundEquation.calculateY(rt))
    // ③ TotalScanSignalsModifier.normalize(..., IPeakIntensityValues.MAX_INTENSITY=100)
peakIntensityValues = getPeakIntensityValues(...)
    // new PeakIntensityValues(); 逐信号 addIntensityValue(rt, totalSignal)（已 0–100 归一化）
supplierScanCSD = getPeakScan(totalScanSignals, backgroundEquation)
    // = 段内最大信号扫描；adjustedSignal = totalSignal − backgroundEquation.calculateY(rt)；new ScanCSD(rt, adjustedSignal)
peakModel = new PeakModelCSD(supplierScanCSD, peakIntensityValues, startBg, stopBg)
return new ChromatogramPeakCSD(peakModel, chromatogram)
```

- 背景方程/峰模型背景范围 = **建峰时固化**（`PeakModelCSD` 内部重算一遍相同的背景方程，见 2.2）。✅
- `TotalScanSignalsModifier.normalize(signals, base)`：`factor = base / maxSignal`，逐信号 `×factor` → **峰值恰缩放到 100**（与 4.2 的精确 100 殊途同归）。✅
- 峰顶扫描 = **纯峰高**（原始最大信号 − 该 RT 背景），不含背景。✅

### 5.2 其它重载（✅）

| 重载 | 语义 |
|---|---|
| `createPeak(chrom, scanRange, boolean calculatePeakIncludedBackground)` | **true→PeakType.VV、false→PeakType.BB**（便捷入口）|
| `createPeak(chrom, scanRange, float startIntensity, float stopIntensity)` | 手动背景；**背景>端点信号时钳到端点信号**（`startIntensity = startIntensity <= firstSignal ? startIntensity : firstSignal`）|
| `createPeak(chrom, scanRange, IBackgroundAbundanceRange, boolean checkBackgroundAbundanceRange)` | 手动范围；`check=true` 时 `checkBackgroundAbundanceRange` 钳位（任一端背景>对应信号 → 钳到信号）|

`BackgroundAbundanceRange(start, stop)` 构造钳位：start<MIN(0) 或 >Float.MAX → 0；stop 越界 → Float.MAX_VALUE。✅

---

## 6. PeakBuilderMSD / PeakBuilderWSD 变体（✅ 全源码确认）

### 6.1 三变体共同骨架（✅）

三者核心流程与 CSD 一致（抽总信号 → 定两端背景 → 线性背景方程 → 逐点扣背景 → 归一化 100% → 强度表 → 峰顶扫描 → `PeakModel{CSD,MSD,WSD}` → `ChromatogramPeak{CSD,MSD,WSD}`）。差异在**信号来源**与**峰顶扫描的形态**。

| | CSD | MSD | WSD |
|---|---|---|---|
| 信号提取器 | `TotalScanSignalExtractor` | `TotalIonSignalExtractor`（TIC）/ `getTotalIonSignals(start,stop,excludedIons)` | `TotalScanSignalExtractor` / `TotalWavelengthSignalExtractor`（按波长）|
| 峰顶扫描 | `ScanCSD(rt, 纯信号)` 单值 | **`PeakMassSpectrum`（全质谱）** | **`ScanWSD`（全波长谱）** |
| 峰模型 | `PeakModelCSD` | `PeakModelMSD` | `PeakModelWSD` |
| 峰对象 | `ChromatogramPeakCSD` | `ChromatogramPeakMSD` | `ChromatogramPeakWSD` |
| 离子/波长过滤 | 无 | `includedIons`+`MarkedTraceModus.INCLUDE/EXCLUDE` | `includedWavelengths`+`MarkedTraceModus`；`traces` 集合 |
| 背景来源 | VV/CB/BB（含 PeakType 重载）| VV/CB/BB（含 PeakType 重载）| **仅 boolean 重载**（true→VV、false→min 平线）**无 PeakType/CB** |

### 6.2 MSD 峰顶质谱 `getPeakMassSpectrum(chromatogram, totalIonSignals, backgroundEquation, excludedIons)`（✅ 关键细节）

```text
totalIonSignal = totalIonSignals.getMaxTotalScanSignal()     // 段内最大 TIC 扫描
scan = chromatogram.getScanNumber(totalIonSignal.getRetentionTime())
massSpectrum = excludedIons==null ? chromatogram.getScan(scan) : chromatogram.getScan(scan, excludedIons)
actualSignal   = massSpectrum.getTotalSignal()
backgroundSignal = backgroundEquation.calculateY(峰顶RT)
correctedSignal  = actualSignal - backgroundSignal
percentage = (100.0f / correctedSignal) × actualSignal
peakMassSpectrum = new PeakMassSpectrum(massSpectrum, percentage)
```

`PeakMassSpectrum(IScanMSD, actualPercentageIntensity)`（✅ `msd.model/.../implementation/PeakMassSpectrum.java` + `core/AbstractPeakMassSpectrum.java`）：逐离子 `abundance' = (abundance / percentage) × 100`。**代数结果**：峰顶质谱总信号 = `actualSignal × 100 / percentage = correctedSignal` —— **质谱被缩放到总信号 = 纯峰高（扣背景后）**。因此 `peakModel.getPeakAbundance()` = `peakMaximum.getTotalSignal()` = 纯峰高。✅

`getPeakMassSpectrum(int retentionTime)`（✅ `core/AbstractPeakModelMSD.java`）：`new PeakMassSpectrum(peakMaximum, getIntensity(rt))` —— 按该 RT 相对强度% 缩放峰顶质谱副本（离子 × intensity/100）。

### 6.3 WSD 峰顶谱 `getPeakScan(chromatogram, totalScanSignals, backgroundEquation)`（✅）

与 MSD 同款百分比技巧：`ScanWSD(scanWSD, percentage)`，`percentage = (100/correctedSignal)×actualSignal` → 全波长谱总信号缩放到纯峰高。✅
另有过载 `createPeak(chrom, scanRange, startIntensity, stopIntensity, Set<Integer> traces)`：用 `ExtractedWavelengthSignalExtractor` 抽提取波长信号，**未被 traces 选中的波长丰度置 0**（`getExtractedWavelengthSignals`），再按最大提取信号扫描建 `ScanWSD`。✅

> MODULE_04 所述 "WSD 无真实基线、CB→当作 VV"：✅ 印证于 PeakBuilderWSD **根本没有 PeakType 重载**（只有 boolean），不存在 CB 分支。

---

## 7. 峰对象与 PeakModel / 色谱 / 基线关系（✅ `implementation/Peak.java` + `core/AbstractPeak.java` + CSD/MSD/WSD 各族）

### 7.1 类层次

```text
IPeak 接口
└─ AbstractPeak (core/AbstractPeak.java)                    // 描述/类型/积分/定量/分类/标记字段
   ├─ AbstractPeakCSD (csd.model/core/AbstractPeakCSD.java) // 持有 IPeakModelCSD peakModel
   │   └─ AbstractChromatogramPeakCSD (core/AbstractChromatogramPeakCSD.java)  // 持有 IChromatogramCSD + 校验
   │       └─ ChromatogramPeakCSD (implementation/ChromatogramPeakCSD.java)    // 薄类
   ├─ AbstractPeakMSD → ... → ChromatogramPeakMSD
   ├─ AbstractPeakWSD → ... → ChromatogramPeakWSD
   └─ Peak (implementation/Peak.java)                       // 通用（非色谱挂载）峰，仅持 IPeakModel
```

`AbstractPeak` 字段（✅）：`modelDescription`、`peakType`(默认 DEFAULT)、`suggestedNumberOfComponents`、`integratorDescription`、`detectorDescription`、`quantifierDescription`、`activeForAnalysis`、`integrationEntries`、`integrationConstraints`、`quantitationEntries`、`internalStandards`、`classifiers`、`temporaryData`(transient)、`markedAsDeleted`。**不含 peakModel 字段** —— 峰模型由各子类持有（`AbstractPeakCSD.peakModel`、`Peak.peakModel`）。✅

### 7.2 ChromatogramPeak 与色谱的绑定（✅ `AbstractChromatogramPeakCSD`）

- 构造 `(peakModel, chromatogram)`：`validateChromatogram`（非 null）+ `validateRetentionTimes`（**峰 start/stop RT 必须落在色谱 RT 边界内**，否则 PeakException）+ 存引用。✅
- `getScanMax()` = `chromatogram.getScanNumber(peakModel.getRetentionTimeAtPeakMaximum())` —— 峰顶扫描号由**色谱 RT→扫描号**反查。✅
- `getSignalToNoiseRatio()` = `IChromatogramPeak` 默认方法：`chromatogram.getSignalToNoiseRatio(peakModel.getPeakAbundance())`（`core/IChromatogramPeak.java`）→ `AbstractChromatogram.getSignalToNoiseRatio(abundance)` = **注入的 noiseCalculator**（`INoiseCalculator`，缺省返回 0）。✅（`IChromatogramPeak` 另有 `getPurity()` 默认 1.0）
- `getWidthBaselineTotalInScans()` = `色谱.getScanNumber(stopRT) − getScanNumber(startRT) + 1`（任一查不到 → 0）。✅
- `getTargets()` = `getPeakModel().getPeakMaximum().getTargets()`（峰顶扫描上挂鉴定目标）。✅

### 7.3 与基线的三层关系（✅ 全链闭合，同 MODULE_04 §3.4 深化）

1. **色谱级基线**：`IChromatogram.getBaselineModel()`（BaselineModel，MODULE_04 §3.1）—— 独立于峰；CB 建峰时被读取。✅
2. **峰自身背景线**：`AbstractPeakModel.backgroundEquation`（`(startRT,startBg)→(stopRT,stopBg)` 线性）—— 建峰固化；`getBackgroundAbundance(rt)` 求值。✅
3. **峰信号 = 原始 − 峰背景**（强度表 100% 归一化前已逐点扣除）；`getPeakAbundance(rt)` 0 基。绘图 = 峰信号 + 背景（接口注释明示）。✅

---

## 8. Qt/C++ 移植要点（⚠️ 设计笔记）

- **峰模型用"强度表 + 两条切线"而非解析函数**：Qt 可 `struct PeakShape { QVector<QPointF> intensity; // (RT, %) 升序; double xApex; float H; }` + 切线 `(a1,b1),(a2,b2)`。**无需 Gaussian 拟合**。
- **拐点方程算法**（可直接照搬）：
  - 上升侧 = `headMap(apexKey, true)`，下降侧 = `tailMap(apexKey, true)`（QMap 无 head/tail → 用迭代器或 `upperBound`/`lowerBound` 切子表）
  - 逐相邻点 `slope = (y2−y1)/(x2−x1)`，y 已换算绝对信号 `(rel/max)×totalSignal`
  - 取 **|slope| 最大段** 的 `y=a·x+b`（a = slope，b = y1 − a·x1）
- **各高度宽度**：水平线 `y = h×H`；`p1 = 上升切线 ∩ 线`、`p2 = 下降切线 ∩ 线`；`x = (h·H − b)/a`（a==0 → 无交点，判 NaN）；`width = p2.x − p1.x + 1`（ms）。`h=0.5` 即半峰宽；`h=0` 即拐点基线宽。
- **峰顶/峰高（拐点法）**：两切线交点 `x = (b2−b1)/(a1−a2)`，`y = a1·x+b1`；平行/重合判 `a1==a2`。
- **tailing 双路径**：strict = 10% 高线×切线右宽/左宽；非 strict = 强度表 50% 高线性插值。**Qt 建议统一走切线路径**（strict），把回退（强度表 50%）作为退化保护；**注意两路径高度不同，勿混用可比性**。
- **leading = 1/tailing**（不是独立算法）。
- **强度表**：`QMap<qint64,float>`（RT 升序、floorEntry 语义天然对应 `lowerBound`）；`add` 校验 rt≥0 且 0≤rel≤100；重复 RT 覆盖。
- **normalize()**：`max = 最大值`；`值 == max → 恰 100.0f`，否则 `(100/max)×值`。**float 精确 100 是后续 `checkModelConditions` 的硬性要求**。
- **建峰流程**（core_processing）：
  1. 抽扫描段总信号（CSD=扫描信号；MSD=离子 TIC 可带 EXCLUDE/INCLUDE 集；WSD=波长总信号可带波长集）
  2. 背景：VV=两端实测 / CB=色谱基线模型 / BB=min(两端) 水平
  3. 背景方程 `(startRT,startBg)→(stopRT,stopBg)` 线性
  4. 逐点 `max(0, signal − bg(rt))` → 归一化 100% → 强度表
  5. 峰顶扫描 = 最大信号点，`纯峰高 = max − bg(峰顶RT)`
  6. MSD/WSD：峰顶全谱（质谱/波长谱）按 `abundance' = abundance × 100/percentage` 缩放使**总信号 = 纯峰高**（percentage = (100/corrected)×actual）
- **峰对象**：`struct Peak { PeakShape shape; QMap<qint64,float> intensity; PeakType type; QString modelDesc; double area; ... }`；ChromatogramPeak 额外持 `chromatogram*` 与 `getScanMax() = chromatogram.scanAt(apexRT)`。
- **strict 失败回退**：切线任一算不出 → 关 strict，tailing 走强度表；Qt 用异常/标志回退即可。

---

## 9. 证据登记表

| # | 结论 | Source(文件/类/方法) | 状态 |
|---|---|---|---|
| PK-BI | IPeakModelStrict 全接口 + AbstractPeakModelStrict 拐点方程族实现（交点/峰高/峰顶/各高度宽/切线丰度/切线拖尾）| org.eclipse.chemclipse.model/.../core/IPeakModelStrict.java + AbstractPeakModelStrict.java | ✅ |
| PK-BJ | PeakBuilderMSD 建峰（峰顶质谱 percentage 缩放、离子 EXCLUDE/INCLUDE）与 PeakBuilderWSD 变体（波长过滤、无 PeakType/CB 分支）| msd.model/.../support/PeakBuilderMSD.java + wsd.model/.../support/PeakBuilderWSD.java | ✅ |
| PK-BL | IPeakModel 全接口成员清单（含注释语义）| cmodel/model/core/IPeakModel.java | ✅ |
| PK-BM | AbstractPeakModel 字段 + calculatePeakModel 链（checkModelConditions 三校验 + 峰顶扫描 RT 回写 + 归一化 100 硬性要求）| cmodel/model/core/AbstractPeakModel.java | ✅ |
| PK-BN | 背景方程（负值钳 0、两点线性）+ 梯度角 atan((stopBg−startBg)/宽)；接口注释=正角背景上升（字段注释矛盾）| cmodel/model/core/AbstractPeakModel.java (calculateBackgroundEquation/calculateGradientAngle) + IPeakModel.java | ✅ |
| PK-BO | getTailing 双路径（strict→切线×10% 高；非 strict→强度表 50% 高线性插值）；leading=1/tailing | cmodel/model/core/AbstractPeakModel.java (getTailing/getLeading/calculateTailingByIntensityValues) | ✅ |
| PK-BP | 非 strict 拖尾算法：50% 线穿过强度表的相邻点线性插值 RT，tailing=右宽/左宽 | cmodel/model/core/AbstractPeakModel.java (calculateTailingByIntensityValues) | ✅ |
| PK-BQ | IPeakIntensityValues 存储（TreeMap/RT 升序）、add 越界跳过+同 RT 覆盖、floorEntry 查询、firstKey/lastKey、replaceRetentionTimes | cmodel/model/core/AbstractPeakIntensityValues.java | ✅ |
| PK-BR | normalize() 算法（max→恰 100.0f 特殊处理，其余 ×100/max）| cmodel/model/core/AbstractPeakIntensityValues.java | ✅ |
| PK-BS | 拐点方程核心数学：headMap/tailMap 切侧、相对%→绝对信号换算、|slope| 最大相邻段作切线（TreeMap.lastEntry）| cmodel/model/core/AbstractPeakIntensityValuesStrict.java (calculateInflectionPointEquation) | ✅ |
| PK-BT | Equations 数学：createLinearEquation(a=(Δy/Δx),b=y1−a·x1)、calculateSlope（分母 0→0）、calculateIntersection（平行/重合抛异常）；LinearEquation f(x)=ax+b、calculateX a==0→NaN | org.eclipse.chemclipse.numeric/.../equations/Equations.java + LinearEquation.java | ✅ |
| PK-BU | TwoPoints（slope=Equations.calculateSlope；getLinearEquation=两点线）| cmodel/model/support/TwoPoints.java | ✅ |
| PK-BV | PeakBuilderCSD.createPeak(chrom, scanRange, PeakType) 全链（VV/CB/BB 背景来源、扣背景、归一化 100%、纯峰高峰顶扫描）| csd.model/.../core/support/PeakBuilderCSD.java | ✅ |
| PK-BW | PeakBuilderCSD boolean 重载 → VV/BB 映射；手动背景重载钳位逻辑；BackgroundAbundanceRange 钳位（MIN=0/MAX=Float.MAX_VALUE）| csd.model/.../support/PeakBuilderCSD.java + cmodel/model/support/BackgroundAbundanceRange.java | ✅ |
| PK-BX | PeakModelCSD/MSD/WSD 均为薄子类 extends PeakModel（构造默认 strictModel=true）| csd.model/.../implementation/PeakModelCSD.java + msd.model/.../implementation/PeakModelMSD.java + wsd.model/.../core/implementation/PeakModelWSD.java + cmodel/model/implementation/PeakModel.java | ✅ |
| PK-BY | 峰对象层次（AbstractPeak 字段集/AbstractPeakCSD 持 peakModel/AbstractChromatogramPeakCSD 持色谱+校验/Peak 通用峰）| cmodel/model/core/AbstractPeak.java + implementation/Peak.java + csd.model/.../core/AbstractChromatogramPeakCSD.java | ✅ |
| PK-BZ | IChromatogramPeak 默认 S/N = 色谱.getSignalToNoiseRatio(纯峰高)；getScanMax=色谱.scanNumber(峰顶RT)；getPurity 默认 1.0 | cmodel/model/core/IChromatogramPeak.java + AbstractChromatogram.java | ✅ |
| PK-CA | PeakMassSpectrum(IScanMSD, percentage) 逐离子 ×100/percentage → 峰顶质谱总信号=纯峰高（代数证明）| msd.model/.../implementation/PeakMassSpectrum.java + core/AbstractPeakMassSpectrum.java + support/PeakBuilderMSD.java | ✅ |
| PK-CB | AbstractPeakModelMSD.getPeakMassSpectrum(rt) = 峰顶质谱按相对强度%缩放副本；getPeakScan(rt) = new Scan(相对强度%)（⚠️ 非绝对丰度）| msd.model/.../core/AbstractPeakModelMSD.java + cmodel/model/core/AbstractPeakModel.java | ✅ |
| PK-CC | Scan/ScanCSD/ScanWSD 构造（单值信号 / (rt,信号) / (谱, percentage)）| cmodel/model/implementation/Scan.java + csd.model/.../implementation/ScanCSD.java + wsd.model/.../core/implementation/ScanWSD.java | ✅ |
| PK-CD | TotalScanSignalsModifier.normalize(base)：factor=base/max、逐信号×factor（峰值恰=base）| cmodel/model/signals/TotalScanSignalsModifier.java | ✅ |
| PK-CE | IBackgroundAbundanceRange 常量 MIN=0/MAX=Float.MAX_VALUE | cmodel/model/support/IBackgroundAbundanceRange.java | ✅ |

---

## 10. 待回填清单（❓）

> 已解决：~~PK-BI~~（拐点方程族见 §3，✅）、~~PK-BJ~~（MSD/WSD 建峰见 §6，✅）。MODULE_04 遗留 `PK-BK`（sumarea ChromatogramIntegrationSettings 字段）**非本模块范围**，仍 ❓。

| # | 问题 |
|---|---|
| PK-CF | WSD `AbstractPeakModelWSD` / `AbstractChromatogramPeakWSD` 是否在 CSD 族之外有额外实现差异（未读，结构上应与 CSD 平行）❓ |
| PK-CG | MSD `getPeakMassSpectrum` 的 `correctedSignal == 0` 时 `percentage` 除零行为（源码无保护）—— 实际是否会触发、下游表现 ❓ |
| PK-CH | 非 strict 拖尾的 `halfHeight = 50%` 与 strict 的 10% 并存是否为**有意的双定义**（无源码注释解释）⚠️ 待验证 |
| PK-CI | `getPeakScan(rt)` 返回相对强度% 扫描的怪癖是否有调用方依赖（UI 展示？）❓ |
