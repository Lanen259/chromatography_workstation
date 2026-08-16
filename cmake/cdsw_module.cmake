# ============================================================
# cdsw_module.cmake —— 模块化构建约定（契约 §5.1/§5.2/§6）
#
# cdsw_add_module(<name> <dep>...)
#   · 创建静态库 target <name>
#   · include/<name>/ 为对外接口目录（PUBLIC）；src/ 为私有实现
#   · 自动收集 src/*.cpp；为空时用 build 目录占位 TU（保证空模块可配置 = M0 验收）
#   · tests/*.cpp 存在时创建 <name>_tests 并 add_test（= 模块独立测试 §6）
#   · 自动 PUBLIC 链接 Qt::Core（若当前找到 Qt）
# ============================================================

function(cdsw_add_module name)
    set(deps ${ARGN})

    add_library(${name} STATIC)
    target_include_directories(${name}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
               $<INSTALL_INTERFACE:include/${name}>
    )
    target_link_libraries(${name} PUBLIC ${deps})

    # Qt::Core 对所有模块必需（契约 §3：全模块最低依赖 QtCore）
    if(TARGET Qt6::Core)
        target_link_libraries(${name} PUBLIC Qt6::Core)
    elseif(TARGET Qt5::Core)
        target_link_libraries(${name} PUBLIC Qt5::Core)
    endif()

    # 源文件：自动收集 src/*.cpp（含子目录）
    file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    if(module_sources)
        target_sources(${name} PRIVATE ${module_sources})
    else()
        # M0 空模块占位 TU：src/ 出现真实文件后自动失效
        set(placeholder "${CMAKE_CURRENT_BINARY_DIR}/${name}_empty.cpp")
        file(WRITE "${placeholder}"
             "// 空模块占位 TU（M0 骨架），由模块实现填充 src/ 后自动移除\n"
             "namespace cdsw { namespace { const char* kEmptyModule = \"${name}\"; } }\n")
        target_sources(${name} PRIVATE "${placeholder}")
    endif()

    # 独立测试（契约 §6：每模块单独 ctest 跑通，可脱离主程序验证）
    if(CDSW_BUILD_TESTS)
        file(GLOB test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
        if(test_sources)
            add_executable(${name}_tests ${test_sources})
            target_link_libraries(${name}_tests PRIVATE ${name})
            if(TARGET Qt6::Test)
                target_link_libraries(${name}_tests PRIVATE Qt6::Test)
            elseif(TARGET Qt5::Test)
                target_link_libraries(${name}_tests PRIVATE Qt5::Test)
            endif()
            add_test(NAME ${name}_tests COMMAND ${name}_tests)
        endif()
    endif()
endfunction()
