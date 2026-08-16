# 色谱工作站 · 根构建文件（契约 §5 / 里程碑 M0）
# 依赖方向：ui → core/acq/io/report → core_processing → core_model
cmake_minimum_required(VERSION 3.16)
project(chromatography_workstation VERSION 0.1.0 LANGUAGES CXX)

# 选项（契约 §5.4 / §8.3）：嵌入式目标可关 UI 只编核心三件套
option(CDSW_BUILD_TESTS "构建并注册模块测试" ON)
option(CDSW_BUILD_UI "构建 UI 模块" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt：优先 Qt6，缺则降 Qt5（契约 §5.4）。
# M0 骨架不强制 Qt：找不到也能配置/构建空模块；模块接入真实代码后必须提供 Qt。
find_package(Qt6 COMPONENTS Core Test QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Core Test QUIET)
endif()
if(Qt6_FOUND)
    set(CDSW_QT_VERSION 6)
elseif(Qt5_FOUND)
    set(CDSW_QT_VERSION 5)
else()
    message(WARNING "未找到 Qt（需 Qt6 或 Qt5>=5.15，可设 CMAKE_PREFIX_PATH 指向 Qt 安装目录）。\
骨架仍可配置/构建，但模块实现后编译需要 Qt（契约 §5.4）")
endif()

# 测试注册（契约 §6：每模块独立 ctest）
include(CTest)

# 模块化构建约定（契约 §5.1/§5.2）
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cdsw_module)

# 模块装配（顺序 = 合并顺序 §7.4；add_subdirectory 顺序即依赖声明顺序）
add_subdirectory(core_model)
add_subdirectory(core_processing)
add_subdirectory(acq)
add_subdirectory(io)
add_subdirectory(report)
if(CDSW_BUILD_UI)
    add_subdirectory(ui)
endif()
add_subdirectory(app)
