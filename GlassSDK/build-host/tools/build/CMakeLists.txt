cmake_minimum_required(VERSION 3.16)

project(GMPluginSDK LANGUAGES C)

get_filename_component(GM_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(GM_EXAMPLES_ROOT "${GM_SDK_ROOT}/examples")
if(NOT DEFINED GM_OUTPUT_ROOT)
  set(GM_OUTPUT_ROOT "${CMAKE_BINARY_DIR}")
endif()
get_filename_component(GM_OUTPUT_ROOT "${GM_OUTPUT_ROOT}" ABSOLUTE)

if(NOT DEFINED GM_PYTHON_EXECUTABLE)
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  set(GM_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()

add_library(gm_plugin_flags INTERFACE)
target_include_directories(gm_plugin_flags INTERFACE "${GM_SDK_ROOT}/include")
if(GM_REVIEW_SYSTEM_INCLUDE_DIRS)
  target_compile_options(gm_plugin_flags INTERFACE -nostdinc)
  target_include_directories(gm_plugin_flags SYSTEM INTERFACE ${GM_REVIEW_SYSTEM_INCLUDE_DIRS})
endif()
target_compile_options(gm_plugin_flags INTERFACE
  -march=rv32imafc
  -mabi=ilp32f
  -mcmodel=medany
  -mno-relax
  -Os
  -ffunction-sections
  -fdata-sections
  -fvisibility=hidden
  -fno-builtin
  -fno-common
  -fno-jump-tables
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  -fno-stack-protector
  -nostdlib
  -Wall
  -Wextra
  -Werror
  -std=gnu99
)

# Compile the ABI checks once; every GMP target depends on this verification.
add_library(gm_abi_check OBJECT "${CMAKE_CURRENT_LIST_DIR}/abi_check.c")
target_link_libraries(gm_abi_check PRIVATE gm_plugin_flags)

function(gm_add_plugin manifest)
  get_filename_component(example_dir "${manifest}" DIRECTORY)
  get_filename_component(plugin_name "${example_dir}" NAME)
  if(GM_PROJECT_DIR)
    set(example "${plugin_name}")
  else()
    file(RELATIVE_PATH example "${GM_EXAMPLES_ROOT}" "${example_dir}")
  endif()
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${example_dir}/*.c")
  # Match the source snapshot's excluded build/cache directories.
  foreach(source IN LISTS sources)
    file(RELATIVE_PATH relative_source "${example_dir}" "${source}")
    if(relative_source MATCHES "(^|/)(\\.git|\\.build|build|__pycache__)/")
      list(REMOVE_ITEM sources "${source}")
    endif()
  endforeach()
  if(NOT sources)
    if(GM_PROJECT_DIR)
      message(FATAL_ERROR "No C sources in ${example_dir}")
    endif()
    return()
  endif()

  string(REPLACE "/" "_" target_suffix "${example}")
  set(target "gm_plugin_${target_suffix}")
  set(object_target "${target}_objects")
  set(output_dir "${GM_OUTPUT_ROOT}/${example}")
  set(intermediate_dir "${CMAKE_BINARY_DIR}/artifacts/${example}")
  set(elf "${intermediate_dir}/${plugin_name}.elf")
  set(gmp "${output_dir}/${plugin_name}.gmp")

  add_library("${object_target}" OBJECT ${sources})
  target_link_libraries("${object_target}" PRIVATE gm_plugin_flags)
  if(GM_PROJECT_DIR)
    target_include_directories("${object_target}" PRIVATE ${GM_PROJECT_INCLUDE_DIRS})
  endif()

  add_custom_command(
    OUTPUT "${gmp}"
    BYPRODUCTS "${elf}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${intermediate_dir}"
    COMMAND "${CMAKE_LINKER}" -m elf32lriscv --no-relax --emit-relocs --gc-sections
            -T "${CMAKE_CURRENT_LIST_DIR}/gm_plugin.ld" -o "${elf}"
            $<TARGET_OBJECTS:${object_target}>
    COMMAND "${GM_PYTHON_EXECUTABLE}" "${GM_SDK_ROOT}/build-host/tools/gmp_pack.py"
            --elf "${elf}" --manifest "${manifest}" --output "${gmp}"
    DEPENDS $<TARGET_OBJECTS:${object_target}> $<TARGET_OBJECTS:gm_abi_check>
            "${manifest}"
            "${CMAKE_CURRENT_LIST_DIR}/gm_plugin.ld"
            "${GM_SDK_ROOT}/build-host/tools/gmp_pack.py"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
  add_custom_target("${target}" ALL DEPENDS "${gmp}")
endfunction()

if(GM_PROJECT_DIR)
  gm_add_plugin("${GM_PROJECT_DIR}/manifest.json")
else()
  file(GLOB_RECURSE manifests CONFIGURE_DEPENDS "${GM_EXAMPLES_ROOT}/*/manifest.json")
  foreach(manifest IN LISTS manifests)
    gm_add_plugin("${manifest}")
  endforeach()
endif()
