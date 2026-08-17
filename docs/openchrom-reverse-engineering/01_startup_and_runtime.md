# 01 — Application Startup & Runtime（程序启动与运行时）

> **当前状态：🔶 框架版（源码未就位）**
> 目标：回答「程序从启动到打开一个色谱数据文件，完整调用链是什么」。
> 本文件先定义**追踪路径**与**待回答问题**，并把已知的 Eclipse RCP 通用机制作为背景假设列出；源码到位后逐级回填 ✅ 证据。

---

## 1. 本 Phase 要回答的问题

1. 进程入口在哪？（JVM 入口 → `Application` 类）
2. Application 类如何初始化 OSGi / Eclipse Platform？
3. 插件（bundle）何时被加载、如何被加载？
4. 主窗口（Main window / Workbench）如何创建？
5. Workbench 的 Perspective / Editor / View 如何装配？
6. 从「打开文件」动作到「Document/Editor 就绪」的调用链？
7. 数据模型对象（Chromatogram 等）由谁在什么时候创建？

---

## 2. 追踪路径（本 Phase 的必查主线）

```text
JVM 入口 (launcher)
  → 产品 Application 类 (eclipse.application / .product 文件指定)
  → Application.start()
  → WorkbenchAdvisor.createFirstRunWindow / preWindowOpen
  → Perspective（定义布局）
  → 主菜单 / ActionBarAdvisor
  → 打开文件动作
    → 文件选择 + 数据供应商(Supplier)匹配
    → 解析器(Parser)读取文件
    → 构建数据模型(Chromatogram)
    → Editor 打开显示
```

> 每条边必须找到真实方法调用才算确认（❌ 不允许用类名联想补边）。

---

## 3. 背景假设（⚠️ 待验证，仅作线索）

> 以下基于 Eclipse RCP 框架常识与 OpenChrom 项目背景，**不是**本次源码确认结果。

| # | 假设 | 说明 | 状态 |
|---|---|---|---|
| S1 | 基于 Eclipse RCP（SWT/JFace/Equinox） | 从根 pom 与 MANIFEST 依赖推断 ⚠️ | ⚠️ |
| S2 | 入口是某个 `Application` 子类（`IApplication`） | 由 `.product` 文件 `application` 属性指定 ⚠️ | ⚠️ |
| S3 | 用 `WorkbenchAdvisor`/`WorkbenchWindowAdvisor` 子类配置窗口 | RCP 惯例 ⚠️ | ⚠️ |
| S4 | 主界面由至少一个 `Perspective` 定义（perspective 含 editor 区 + view 区） | RCP 惯例 ⚠️ | ⚠️ |
| S5 | 插件通过 `plugin.xml` 的 extension/extension-point 注册 | OSGi + ExtensionRegistry 惯例 ⚠️ | ⚠️ |
| S6 | 「打开文件」走扩展点派发，匹配数据供应商 | 需在 08 中确认扩展点真实名称 ⚠️ | ⚠️ |
| S7 | 数据模型对象在供应商解析器内创建，随后交给 UI editor | 待 02/03 确认 ⚠️ | ⚠️ |

---

## 4. 关键类登记表（🔲 待填充）

| 角色 | 类名 | 文件路径 | 关键方法 | 状态 |
|---|---|---|---|---|
| 进程入口 / launcher | | | | 🔲 |
| Application | | | `start()` / `stop()` | 🔲 |
| WorkbenchAdvisor | | | | 🔲 |
| WorkbenchWindowAdvisor | | | | 🔲 |
| Perspective | | | | 🔲 |
| ActionBarAdvisor（主菜单） | | | | 🔲 |
| 打开文件动作/命令 | | | | 🔲 |
| 数据供应商匹配/选择 | | | | 🔲 |
| Editor（Chromatogram 编辑器） | | | | 🔲 |

> 字段含义：**类名**=实际 Java 类；**文件路径**=相对仓库根的源码文件；**关键方法**=本追踪主线中真正被调用的方法。

---

## 5. 待确认问题清单（❓）

| # | 问题 | 关联假设 |
|---|---|---|
| S1Q | 产品 `.product` 文件在哪个 bundle？application 属性值是什么？ | S2 |
| S2Q | Application 类全限定名？`start()` 中先做了什么？ | S2 |
| S3Q | WorkbenchAdvisor 如何被实例化（plugin.xml 配置 or 代码）？ | S3 |
| S4Q | 有哪些 Perspective？各自的 editor/view 布局？ | S4 |
| S5Q | 插件发现是纯 OSGi（Equinox）还是叠加了自定义注册表？ | S5 |
| S6Q | 打开文件命令的 handler 类？内部如何枚举数据供应商？ | S6 |
| S7Q | Editor 创建数据模型还是只做展示？模型由谁实例化？ | S7 |

---

## 6. 输出要求（回填后必须给出）

1. 一张从 JVM 入口到 Editor 打开的**真实调用链图**（Mermaid sequence 图），每条边注明 文件/类/方法。
2. 主窗口装配表：Perspective → View/Editor 的父子关系。
3. 插件加载时序：Equinox 何时激活 bundle、extension registry 何时可用。
4. 「打开数据文件」完整命令链（从菜单/按钮 → 最终模型就绪）。
