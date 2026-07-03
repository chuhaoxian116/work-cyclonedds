# 1 kHz 机械臂状态发送端。
add_executable(dds_arm_state_publisher
  publisher.cpp)
target_link_libraries(dds_arm_state_publisher PRIVATE dds_idl)

# 1 kHz 机械臂状态接收与周期统计端。
add_executable(dds_arm_state_subscriber
  subscriber.cpp)
target_link_libraries(dds_arm_state_subscriber PRIVATE dds_idl)

# 对示例代码启用常见编译警告；不改变优化级别。
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dds_arm_state_publisher
    PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(dds_arm_state_subscriber
    PRIVATE -Wall -Wextra -Wpedantic)
endif()
