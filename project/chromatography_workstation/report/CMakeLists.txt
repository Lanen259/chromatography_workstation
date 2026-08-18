# report —— 报告（契约 §4.5，依赖 core_processing 的 QuantEntry/Peak）
cdsw_add_module(report core_model core_processing)

# 金样数据：测试运行目录不定，把 golden 目录编译期注入（绝对源码路径）
if(CDSW_BUILD_TESTS)
    target_compile_definitions(report_tests PRIVATE
        REPORT_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
endif()
