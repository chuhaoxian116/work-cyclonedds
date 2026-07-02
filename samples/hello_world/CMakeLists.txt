# =========================================================
# C++ 示例程序：只需链接统一的 IDL 动态库
# =========================================================

add_executable(dds_cpp_publisher publisher.cpp)
target_link_libraries(dds_cpp_publisher PRIVATE dds_idl)

add_executable(dds_cpp_subscriber subscriber.cpp)
target_link_libraries(dds_cpp_subscriber PRIVATE dds_idl)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dds_cpp_publisher PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(dds_cpp_subscriber PRIVATE -Wall -Wextra -Wpedantic)
endif()
