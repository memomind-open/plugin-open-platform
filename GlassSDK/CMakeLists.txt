cmake_minimum_required(VERSION 3.16)

project(GMPluginSDK LANGUAGES C)

set(GM_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
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
add_library(gm_abi_check OBJECT "${GM_SDK_ROOT}/mk/abi_check.c")
target_link_libraries(gm_abi_check PRIVATE gm_plugin_flags)

function(gm_add_plugin manifest)
  get_filename_component(example_dir "${manifest}" DIRECTORY)
  file(RELATIVE_PATH example "${GM_EXAMPLES_ROOT}" "${example_dir}")
  get_filename_component(plugin_name "${example_dir}" NAME)
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${example_dir}/*.c")
  if(NOT sources)
    return()
  endif()

  string(REPLACE "/" "_" target_suffix "${example}")
  set(target "gm_plugin_${target_suffix}")
  set(object_target "${target}_objects")
  set(output_dir "${GM_OUTPUT_ROOT}/${example}")
  set(elf "${output_dir}/${plugin_name}.elf")
  set(gmp "${output_dir}/${plugin_name}.gmp")

  add_library("${object_target}" OBJECT ${sources})
  target_link_libraries("${object_target}" PRIVATE gm_plugin_flags)

  add_custom_command(
    OUTPUT "${gmp}"
    BYPRODUCTS "${elf}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
    COMMAND "${CMAKE_LINKER}" -m elf32lriscv --no-relax --emit-relocs --gc-sections
            -T "${GM_SDK_ROOT}/mk/gm_plugin.ld" -o "${elf}"
            $<TARGET_OBJECTS:${object_target}>
    COMMAND "${GM_PYTHON_EXECUTABLE}" "${GM_SDK_ROOT}/tools/gmp_pack.py"
            --elf "${elf}" --manifest "${manifest}" --output "${gmp}"
    DEPENDS $<TARGET_OBJECTS:${object_target}> $<TARGET_OBJECTS:gm_abi_check>
            "${manifest}"
            "${GM_SDK_ROOT}/mk/gm_plugin.ld" "${GM_SDK_ROOT}/tools/gmp_pack.py"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
  add_custom_target("${target}" ALL DEPENDS "${gmp}")
endfunction()

file(GLOB_RECURSE manifests CONFIGURE_DEPENDS "${GM_EXAMPLES_ROOT}/*/manifest.json")
foreach(manifest IN LISTS manifests)
  gm_add_plugin("${manifest}")
endforeach()
