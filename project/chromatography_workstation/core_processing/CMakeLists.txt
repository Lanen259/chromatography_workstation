# core_processing —— 处理引擎（契约 §4.2，依赖 core_model）
cdsw_add_module(core_processing core_model)

# 算法实现类头在 src/（模块内私有，契约 §0.2/§4.2）：
#   · 测试直接构造算法实现类做金标准验证，需把 src/ 加进测试目标 include 路径；
#   · 库内 src/*.cpp 同目录相对引用即可，显式加路径是为子目录/无歧义 include 兜底。
target_include_directories(core_processing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(TARGET core_processing_tests)
    target_include_directories(core_processing_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
