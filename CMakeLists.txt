cmake_minimum_required(VERSION 3.16)

project(work_cyclonedds_demos VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CYCLONEDDS_PREBUILT_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cyclonedds-0.10.5-prebuilt"
    CACHE PATH "CycloneDDS 0.10.5 prebuilt repository")
set(CYCLONEDDS_C_PREFIX
    "${CYCLONEDDS_PREBUILT_ROOT}/cyclonedds-install")
set(CYCLONEDDS_CXX_PREFIX
    "${CYCLONEDDS_PREBUILT_ROOT}/cyclonedds-cxx-install")

if(NOT EXISTS "${CYCLONEDDS_C_PREFIX}/lib/cmake/CycloneDDS/CycloneDDSConfig.cmake"
   OR NOT EXISTS "${CYCLONEDDS_CXX_PREFIX}/lib/cmake/CycloneDDS-CXX/CycloneDDS-CXXConfig.cmake")
  message(FATAL_ERROR
    "CycloneDDS prebuilt dependency is missing. Run:\n"
    "  git submodule update --init --recursive")
endif()

# These prebuilt binaries are currently GNU/Linux x86-64 artifacts.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
   OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  message(FATAL_ERROR
    "The checked-in CycloneDDS package supports GNU/Linux x86-64 only. "
    "Use a prebuilt package compiled for the target platform.")
endif()

list(PREPEND CMAKE_PREFIX_PATH
  "${CYCLONEDDS_C_PREFIX}"
  "${CYCLONEDDS_CXX_PREFIX}")

find_package(CycloneDDS 0.10.5 CONFIG REQUIRED)
# CycloneDDS-CXX 0.10.5 does not install a ConfigVersion.cmake file, so its
# version is pinned by the prebuilt submodule rather than find_package.
find_package(CycloneDDS-CXX CONFIG REQUIRED)

# =========================================================
# IDL 生成环境（所有子模块统一使用）
# =========================================================

set(PROJECT_IDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/idl")
set(PROJECT_IDL_GENERATED_DIR "${CMAKE_BINARY_DIR}/src/idl")
set(PROJECT_IDL_LIBRARY_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(PROJECT_IDLC "${CYCLONEDDS_C_PREFIX}/bin/idlc")
set(PROJECT_IDLC_LIBRARY_PATH
    "${CYCLONEDDS_C_PREFIX}/lib:${CYCLONEDDS_CXX_PREFIX}/lib:$ENV{LD_LIBRARY_PATH}")

file(MAKE_DIRECTORY
  "${PROJECT_IDL_DIR}"
  "${PROJECT_IDL_GENERATED_DIR}"
  "${PROJECT_IDL_LIBRARY_OUTPUT_DIR}")

# Make build-tree executables runnable without installing the demos.
set(CMAKE_BUILD_RPATH
  "${CYCLONEDDS_C_PREFIX}/lib"
  "${CYCLONEDDS_CXX_PREFIX}/lib")

add_subdirectory(src/idl)
add_subdirectory(samples)
