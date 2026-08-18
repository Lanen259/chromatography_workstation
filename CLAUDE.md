# chromatography_workstation —— Qt/C++ 色谱工作站（CDS）

嵌入式色谱工作站，逆向 OpenChrom 得到架构：**模块间只通过接口说话**（对应 OpenChrom 插件扩展点）。
开发方式：多 agent 并行，每模块一个 worktree 分支，独立测试，全绿才合并回 main。

## 开工前必读（按序）
1. `开发治理规范.md` —— 治理公约（谁拥有哪些文件 / 走哪条路线 / 怎么提交合并 / 什么情况停下等人工 / 完成定义）
2. `编码风格规范.md` —— 编码风格（命名 / 分层 / 可扩展性 / 可读性，改任何代码前必读）
3. `PROJECT.md` —— 进度看板（你在哪个 worktree、做哪个里程碑、参考哪份逆向文档、当前阻塞）
4. `00_工程骨架与模块契约.md` 重点章节 —— §0 接口规则（最高铁律）、§2 依赖方向、§4 接口签名（冻结）、§5/§6 构建与测试

## 三条铁律（违反 = 返工）
1. **模块间只通过接口说话**：只用别的模块 `include/<模块>/` 下的接口头；禁止 include 别人的 `src/`、禁止依赖别的模块内部实现。
2. **依赖只许朝下**：`ui → core/acq/io/report → core_processing → core_model`；core 系模块只许 QtCore，只有 `ui` 能用 QtWidgets。**唯一例外：acq 的 IPC 端点（QLocalServer 适配 CtrlPanel）可用 QtNetwork，封装在传输适配器后，`HwRealtimeReceiver` 保持 QtCore 纯**（2026-08-18 拍板）。
3. **独立测试验证**：改完自己模块必须跑通自己的 `ctest`，全绿才允许合并回 main。

## 工作方式（按复杂度选，详见治理规范 §9）
- **L0 直接执行**：简单 bug / 编译错误 / 单函数 / 小改 → 直接改 + 验证，不调技能。
- **L1 自主规划**：中等功能 / 多文件 / 模块内重构 → 自己分析规划后执行。
- **L2 Superpowers**：大型功能 / 跨模块 / 长任务 / 需求不确定 → 用 brainstorming / writing-plans / TDD 等技能。

## 环境与路径
- Qt **5.14.2 MinGW 64-bit**：`D:/Program_flies/qt_creat/APP/5.14.2/mingw73_64`（构建命令见契约 §5）；C++17。
- 唯一构建入口（Qt Creator 打开这里）：`project/chromatography_workstation/`
- 模块源码：`project/chromatography_workstation/<模块>/{include,src,tests}/`
- 你当前在哪个 worktree，就**只许改该模块**自己的 `include/ src/ tests/ CMakeLists.txt`，禁止动其他模块的文件。
