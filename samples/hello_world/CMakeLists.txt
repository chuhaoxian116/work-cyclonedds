set(DEMO_IDL "${CMAKE_CURRENT_SOURCE_DIR}/idl/DemoMessage.idl")

# Generate language-specific bindings from one IDL. Both sets use the same
# DDS type identity and topic name, so every publisher/subscriber combination
# is interoperable.
idlc_generate(
  TARGET demo_types_c
  FILES "${DEMO_IDL}"
  WARNINGS no-implicit-extensibility)

idlcxx_generate(
  TARGET demo_types_cpp
  FILES "${DEMO_IDL}"
  WARNINGS no-implicit-extensibility)

add_executable(dds_c_publisher c/publisher.c)
target_link_libraries(dds_c_publisher PRIVATE demo_types_c CycloneDDS::ddsc)

add_executable(dds_c_subscriber c/subscriber.c)
target_link_libraries(dds_c_subscriber PRIVATE demo_types_c CycloneDDS::ddsc)

add_executable(dds_cpp_publisher cpp/publisher.cpp)
target_link_libraries(
  dds_cpp_publisher PRIVATE demo_types_cpp CycloneDDS-CXX::ddscxx)

add_executable(dds_cpp_subscriber cpp/subscriber.cpp)
target_link_libraries(
  dds_cpp_subscriber PRIVATE demo_types_cpp CycloneDDS-CXX::ddscxx)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dds_c_publisher PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(dds_c_subscriber PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dds_cpp_publisher PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(dds_cpp_subscriber PRIVATE -Wall -Wextra -Wpedantic)
endif()
