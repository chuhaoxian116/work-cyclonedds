add_executable(dds_arm_state_publisher
  publisher.cpp)
target_link_libraries(dds_arm_state_publisher PRIVATE dds_idl)

add_executable(dds_arm_state_subscriber
  subscriber.cpp)
target_link_libraries(dds_arm_state_subscriber PRIVATE dds_idl)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dds_arm_state_publisher
    PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(dds_arm_state_subscriber
    PRIVATE -Wall -Wextra -Wpedantic)
endif()
