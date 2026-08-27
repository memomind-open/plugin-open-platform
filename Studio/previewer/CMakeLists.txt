cmake_minimum_required(VERSION 3.16)
project(GMPluginPreviewer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE GM_DEVICE_LVGL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/src/*.c")
add_library(gm_device_lvgl STATIC ${GM_DEVICE_LVGL_SOURCES})
target_include_directories(gm_device_lvgl PUBLIC third_party/lvgl)
target_compile_definitions(gm_device_lvgl PUBLIC
    LV_CONF_INCLUDE_SIMPLE=1
    CONFIG_XGIMI_PATCH=1)

add_library(gmplugin_previewer_core
    src/lvgl_host.cpp
    src/previewer.cpp
    src/previewer_c_api.cpp
    src/rv32.cpp
)
target_include_directories(gmplugin_previewer_core PUBLIC src third_party/lvgl)
target_link_libraries(gmplugin_previewer_core PUBLIC gm_device_lvgl)
target_compile_definitions(gmplugin_previewer_core PRIVATE
    GM_PREVIEW_FONT17_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/fonts/lv_font_xgimi_17.bin"
    GM_PREVIEW_FONT20_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/fonts/lv_font_xgimi_20.bin")
if(MSVC)
    target_compile_options(gmplugin_previewer_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(gmplugin_previewer_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(gmplugin-preview-cli src/main_cli.cpp)
target_link_libraries(gmplugin-preview-cli PRIVATE gmplugin_previewer_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(gmplugin-preview-c-api-test tests/c_api_test.cpp)
    target_link_libraries(gmplugin-preview-c-api-test PRIVATE gmplugin_previewer_core)
    if(MSVC)
        target_compile_options(gmplugin-preview-c-api-test PRIVATE /UNDEBUG)
    else()
        target_compile_options(gmplugin-preview-c-api-test PRIVATE -UNDEBUG)
    endif()
    set(GM_PREVIEW_FIGHTER_GMP "${CMAKE_CURRENT_SOURCE_DIR}/../../GlassSDK/build-host/game/fighter_arena/fighter_arena.gmp")
    set(GM_PREVIEW_WEB_BRIDGE_GMP "${CMAKE_CURRENT_SOURCE_DIR}/../../GlassSDK/build-host/web_bridge/web_bridge.gmp")
    set(GM_PREVIEW_EXTENSION_GMP "${CMAKE_CURRENT_SOURCE_DIR}/../../GlassSDK/build-host/extension/extension.gmp")
    set(GM_PREVIEW_LVGL_GMP "${CMAKE_CURRENT_SOURCE_DIR}/../../GlassSDK/build-host/lvgl_ui/lvgl_ui.gmp")
    if(EXISTS "${GM_PREVIEW_FIGHTER_GMP}" AND EXISTS "${GM_PREVIEW_WEB_BRIDGE_GMP}" AND
       EXISTS "${GM_PREVIEW_EXTENSION_GMP}")
        set(GM_PREVIEW_TEST_GMPS
            "${GM_PREVIEW_FIGHTER_GMP}"
            "${GM_PREVIEW_WEB_BRIDGE_GMP}"
            "${GM_PREVIEW_EXTENSION_GMP}")
        if(EXISTS "${GM_PREVIEW_LVGL_GMP}")
            list(APPEND GM_PREVIEW_TEST_GMPS "${GM_PREVIEW_LVGL_GMP}")
        endif()
        add_test(NAME gmplugin-preview-c-api COMMAND gmplugin-preview-c-api-test
            ${GM_PREVIEW_TEST_GMPS})
    else()
        add_test(NAME gmplugin-preview-c-api COMMAND gmplugin-preview-c-api-test)
    endif()
endif()

if(WIN32)
    set(GM_PREVIEW_FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/fonts" CACHE PATH
        "Optional directory containing lv_font_xgimi_17.bin and lv_font_xgimi_20.bin")
    get_filename_component(GM_PREVIEW_FONT17
        "${GM_PREVIEW_FONT_DIR}/lv_font_xgimi_17.bin" ABSOLUTE)
    get_filename_component(GM_PREVIEW_FONT20
        "${GM_PREVIEW_FONT_DIR}/lv_font_xgimi_20.bin" ABSOLUTE)
    set(GM_PREVIEWER_WIN32_SOURCES src/main_win32.cpp)
    if(EXISTS "${GM_PREVIEW_FONT17}" AND EXISTS "${GM_PREVIEW_FONT20}")
        file(TO_CMAKE_PATH "${GM_PREVIEW_FONT17}" GM_PREVIEW_FONT17_RC)
        file(TO_CMAKE_PATH "${GM_PREVIEW_FONT20}" GM_PREVIEW_FONT20_RC)
        configure_file(src/fonts.rc.in "${CMAKE_CURRENT_BINARY_DIR}/fonts.rc" @ONLY)
        list(APPEND GM_PREVIEWER_WIN32_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/fonts.rc")
        set(GM_PREVIEW_HAS_EMBEDDED_FONTS 1)
    else()
        message(STATUS "Firmware fonts not configured; using host-font fallback")
        set(GM_PREVIEW_HAS_EMBEDDED_FONTS 0)
    endif()

    add_executable(GMPluginPreviewer WIN32 ${GM_PREVIEWER_WIN32_SOURCES})
    target_compile_definitions(GMPluginPreviewer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        GM_PREVIEW_HAS_EMBEDDED_FONTS=${GM_PREVIEW_HAS_EMBEDDED_FONTS})
    target_link_libraries(GMPluginPreviewer PRIVATE
        gmplugin_previewer_core comctl32 comdlg32 gdi32 shell32 user32
    )
    if(MINGW)
        target_link_options(GMPluginPreviewer PRIVATE -static -static-libgcc -static-libstdc++ -municode)
        target_link_options(gmplugin-preview-cli PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()
