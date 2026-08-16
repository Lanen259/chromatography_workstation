cmake_minimum_required(VERSION 3.5)

# chromatography_workstation —— Qt 主工程（master build）
# 契约文档：../../00_工程骨架与模块契约.md
# 本文件是唯一构建入口：add_subdirectory 全部 6 个模块 + 主程序壳。
# Qt Creator 打开本目录即可编译整机并运行各模块测试（ctest）。

project(chromatography_workstation LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 模块化构建约定（契约 §5）：cdsw_add_module 函数
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cdsw_module)

# 测试开关（契约 §6）：默认构建并注册各模块测试；CDSW_BUILD_TESTS=OFF 可关闭
option(CDSW_BUILD_TESTS "构建并注册各模块测试" ON)
if(CDSW_BUILD_TESTS)
    include(CTest)
endif()

# Qt：主程序需要 Widgets；模块测试需要 Test
find_package(Qt5 COMPONENTS Core Widgets Test REQUIRED)

# 模块（依赖方向：ui → core/acq/io/report → core_processing → core_model）
add_subdirectory(core_model)
add_subdirectory(core_processing)
add_subdirectory(acq)
add_subdirectory(io)
add_subdirectory(report)
add_subdirectory(ui)

# 主程序壳（原 app 模块位置，契约 §4.7；M7 集成时把模块装配到窗口）
add_executable(chromatography_workstation
    main.cpp
    widget.cpp
    widget.h
    widget.ui
)
target_link_libraries(chromatography_workstation PRIVATE
    Qt5::Widgets
    core_model
    core_processing
    acq
    io
    report
    ui
)
