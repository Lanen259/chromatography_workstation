# core_model 线程记忆

## 2026-08-18 core_model 线程（M1 完成，待合并）
- **做了什么**：§4.1 五个接口头逐字实现（Signal/Chromatogram/Peak/Method/Selection）+ `src/Chromatogram.cpp`、`src/Selection.cpp` + QTest 全套（`tests/t_core_model.cpp`，9 槽覆盖全部测试点）；顺带修了 `cmake/cdsw_module.cmake`（AUTOMOC 问题，见下）。
- **为什么这么设计**：契约冻结照抄，零偏差；`scanNumberAtRetentionTime` 用 `std::lower_bound` 二分实现 1-based floor（空/早于首点→0）；`processedPoints()` 独立 `m_processed` 存储，未设置前为空；`Selection::setRange` 每次 emit、`setPeak` 不 emit。头文件只 include QtCore。
- **遇到并解决的 blocker**：`cdsw_add_module` 原先只把 `src/*.cpp` 收进 target_sources，`include/*.h` 从不列入 → AUTOMOC 同名头规则在 include 目录根查不到 `include/core_model/Selection.h` → `Selection` 的 Q_OBJECT 符号链接失败。修复：cmake 追加 `file(GLOB_RECURSE ... include/*.h)` 并入 target_sources。详情见 `docs/blockers/core_model-2026-08-18.md`（已标解决）。
- **验证**：`cmake --build build` 0 error；`ctest --test-dir build --output-on-failure -R core_model` → 100% passed；全量构建 + ctest 1/1 绿。SDD 全流程（实现子agent ×2 + 任务审查 ×2 + 最终审查）通过，最终审查结论 Ready to merge。
- **下一步**：主控线程按治理规范 §3 流程合并回 main（先软件审查 ctest 全绿 + 人工代码审查放行）。
- **卡点/依赖**：无代码卡点。**协调事项（主控需处理）**：① worktree 的 `00_工程骨架与模块契约.md` 是旧版，冻结 §4.1 变更还在 main 未提交的工作区——请主控把 §4.1 契约文档变更提交进 main，各 worktree 才能同步到冻结版；② `编码风格规范.md` 也在 main 未提交，同样建议提交。③ 本次对 `cdsw_module.cmake` 的修改在 dev-core_model 分支上（3 个 commit 之一），合并回 main 时一并生效；若想让其他模块线程提前受益，可考虑先把该 cmake commit 单独合/应用到 main。
