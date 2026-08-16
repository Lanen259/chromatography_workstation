# app —— 组装目标（契约 §4.6/§9 M7，唯一可运行 exe）
# M0 占位壳：只链接核心库，不接 UI；M7 集成时替换 src/main.cpp 并接入 ui。
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE core_model core_processing acq io report)
