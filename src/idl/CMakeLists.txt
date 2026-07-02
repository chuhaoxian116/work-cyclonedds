# =========================================================
# CycloneDDS C++ IDL 生成模块（动态库版）
# =========================================================

# IDL 源文件统一由项目根目录的 idl/ 管理。
file(GLOB IDL_FILES CONFIGURE_DEPENDS
  "${PROJECT_IDL_DIR}/*.idl")

if(NOT IDL_FILES)
  message(FATAL_ERROR
    "No IDL files found in ${PROJECT_IDL_DIR}")
endif()

set(GEN_SRCS "")
set(GEN_HDRS "")

foreach(IDL_FILE IN LISTS IDL_FILES)
  get_filename_component(IDL_NAME "${IDL_FILE}" NAME_WE)

  set(GEN_SRC "${PROJECT_IDL_GENERATED_DIR}/${IDL_NAME}.cpp")
  set(GEN_HDR "${PROJECT_IDL_GENERATED_DIR}/${IDL_NAME}.hpp")

  add_custom_command(
    OUTPUT "${GEN_SRC}" "${GEN_HDR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${PROJECT_IDL_GENERATED_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E chdir
            "${PROJECT_IDL_GENERATED_DIR}"
            "${CMAKE_COMMAND}" -E env
            "LD_LIBRARY_PATH=${PROJECT_IDLC_LIBRARY_PATH}"
            "${PROJECT_IDLC}"
            -l cxx
            -I "${PROJECT_IDL_DIR}"
            -Wno-implicit-extensibility
            "${IDL_FILE}"
    DEPENDS ${IDL_FILES} "${PROJECT_IDLC}" CycloneDDS-CXX::idlcxx
    COMMENT "Generating CycloneDDS C++ bindings: ${IDL_NAME}"
    VERBATIM)

  list(APPEND GEN_SRCS "${GEN_SRC}")
  list(APPEND GEN_HDRS "${GEN_HDR}")
endforeach()

# =========================================================
# 生成目标（确保 IDL 代码先于动态库生成）
# =========================================================

add_custom_target(idl_generate ALL
  DEPENDS ${GEN_SRCS} ${GEN_HDRS})

set_source_files_properties(${GEN_SRCS} PROPERTIES GENERATED TRUE)

# =========================================================
# IDL 动态库（其他模块只需链接 dds_idl）
# =========================================================

add_library(dds_idl SHARED
  ${GEN_SRCS}
  ${GEN_HDRS})

add_dependencies(dds_idl idl_generate)

target_include_directories(dds_idl PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_IDL_GENERATED_DIR}>"
  "$<INSTALL_INTERFACE:include/dds_idl>")

target_link_libraries(dds_idl PUBLIC
  CycloneDDS-CXX::ddscxx)

set_target_properties(dds_idl PROPERTIES
  OUTPUT_NAME dds_idl
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_IDL_LIBRARY_OUTPUT_DIR}"
  VERSION "${PROJECT_VERSION}"
  SOVERSION 0)

# =========================================================
# 安装规则（供后续工程链接）
# =========================================================

include(GNUInstallDirs)

install(TARGETS dds_idl
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(FILES ${GEN_HDRS}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dds_idl")
