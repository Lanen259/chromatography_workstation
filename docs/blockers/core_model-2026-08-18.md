# core_model — AUTOMOC 未为 Selection.h 生成 moc → Q_OBJECT 符号链接失败（已解决）

> 提交于 2026-08-18 · M1 线程开工即受阻。**已解决（2026-08-18）**：主控授权 M1 线程直接在 worktree 修改 `cmake/cdsw_module.cmake`，验证通过。

## ① 现象

按任务书指定方式实现后，构建 `core_model_tests`（链接期）失败：

```
CMakeFiles\core_model_tests.dir/objects.a(t_automo_sel_probe.cpp.obj):t_automo_sel_probe.cpp:(.rdata$.refptr._ZN4cdsw9Selection16staticMetaObjectE[.refptr._ZN4cdsw9Selection16staticMetaObjectE]+0x0): undefined reference to `cdsw::Selection::staticMetaObject'
CMakeFiles\core_model_tests.dir/objects.a(t_automo_sel_probe.cpp.obj):t_automo_sel_probe.cpp:(.rdata$.refptr._ZN4cdsw9Selection19sigSelectionChangedEv[.refptr._ZN4cdsw9Selection19sigSelectionChangedEv]+0x0): undefined reference to `cdsw::Selection::sigSelectionChanged()'
CMakeFiles\core_model_tests.dir/objects.a(t_automo_sel_probe.cpp.obj):t_automo_sel_probe.cpp:(.rdata$.refptr._ZTVN4cdsw9SelectionE[.refptr._ZTVN4cdsw9SelectionE]+0x0): undefined reference to `vtable for cdsw::Selection'
libcore_model.a(Selection.cpp.obj):Selection.cpp:(.text+0x31): undefined reference to `cdsw::Selection::sigSelectionChanged()'
collect2.exe: error: ld returned 1 exit status
```

复现：worktree 内 `cmake -S project/chromatography_workstation -B build ... && cmake --build build --target core_model_tests`。
库目标 `core_model` 能编（static 库不暴露缺失符号），链接测试 exe 时暴露。

**根因证据**：`build/core_model/core_model_autogen/mocs_compilation.cpp` 内容为
```
// No files found that require moc or the moc files are included
```
→ AUTOMOC 对 core_model 目标**没有生成任何 moc 文件**。

## ② 已尝试

1. **按任务书写法**：`src/Selection.cpp` include `Selection.h`（同名头规则）+ `Selection.h` 逐字采用 §4.1 签名、`Q_OBJECT`、`signals:` 齐全。仍失败。
2. **核查 AUTOMOC 是否发现头**：读 `mocs_compilation.cpp` → "No files found that require moc"，确认 AUTOMOC 根本没跑 moc。
3. **排查可用规避路径**（均不可行）：
   - 手动 `#include "moc_Selection.cpp"`：该文件压根没生成，编译期即失败。
   - 改头文件位置/命名：契约 §0.2 定死接口位置 `include/<模块>/`、§4.1 定死命名，无权改。
   - 在 core_model 自己的 CMakeLists.txt 里补 `target_sources`：`cdsw_add_module` 是函数封装，模块级 CMakeLists 只有一行 `cdsw_add_module(core_model)`，补不了；改函数 = 改 main-owned 文件。

## ③ 卡点

根因在 **`project/chromatography_workstation/cmake/cdsw_module.cmake` 的 `cdsw_add_module`**：

```cmake
file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources(${name} PRIVATE ${module_sources})
```

它只把 `src/*.cpp` 收进 `target_sources`，**从不收 `include/*.h`**。AUTOMOC 的"同名头"规则（`Selection.cpp` → `Selection.h`）按 basename 在源目录 + include 目录里查找；而 `Selection.h` 在 `include/core_model/` 下一层（include 目录只有 `include/` 一层），basename 查找够不到 → moc 不生成 → Q_OBJECT 符号缺失。

该文件是 **main-owned 红线文件**（治理规范 §1），M1 线程无权修改。此问题在开工前主控记忆里已被预告为待办，但未先于 M1 处理。

## ④ 需要的帮助

请主控线程在 main 上修改 `cmake/cdsw_module.cmake` 的 `cdsw_add_module`，把接口头也收进 `target_sources`，例如：

```cmake
# src/*.cpp（现有）
file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
# 新增：接口头（AUTOMOC 依赖头在 target_sources 里才能发现 Q_OBJECT）
file(GLOB_RECURSE module_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
target_sources(${name} PRIVATE ${module_sources} ${module_headers})
```

这是 AUTOMOC 的标准做法（头文件列为 target 源 → 自动 moc）。改完后 M1 可续跑：worktree 里 3 个头（Signal.h / Peak.h / Selection.h）+ `src/Selection.cpp` 已按 §4.1 就位，可直接继续补全 Chromatogram.h/Method.h 与实现、测试。

（备选方案：把每个模块的 `include/<name>` 子目录也加进 include 路径使 basename 查找够到——但那样会放宽接口可见性边界，不推荐。）

## ⑤ 解决记录（2026-08-18，主控授权直接修）

已在 `cmake/cdsw_module.cmake` 的 `cdsw_add_module` 中追加（src 收集之后）：

```cmake
# 接口头也收进 target_sources：AUTOMOC 只处理列在 target 源里的头。
# 头文件（含 Q_OBJECT，如 Selection.h）若不列入，同名头规则在 include 目录根
# 找不到（实际在 include/<name>/ 下一层），moc 不生成 → Q_OBJECT 符号链接失败。
file(GLOB_RECURSE module_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
if(module_headers)
    target_sources(${name} PRIVATE ${module_headers})
endif()
```

验证（worktree 内，MinGW 7.3 + Qt 5.14.2，CMake 4.0.3）：
- `mocs_compilation.cpp` 现含 `#include ".../moc_Selection.cpp"`，moc 生成 ✅
- `cmake --build build --target core_model_tests` 链接通过 ✅（此前 `undefined reference to cdsw::Selection::staticMetaObject`）
- Selection 信号测试通过（QSignalSpy 断言 `sigSelectionChanged` 发出 1 次）✅
- 全量 `cmake --build build` 0 error（6 模块 + 主程序壳）✅
- 全量 `ctest` 1/1 passed ✅

改动位于 worktree（dev-core_model 分支），待 M1 合并时随主进 main。
