cmake_minimum_required(VERSION 3.24)

project(IgusaBmsonStudio
    VERSION 0.1.0
    DESCRIPTION "bmson chart authoring tool: slicing, chart editing and preview"
    LANGUAGES C CXX)

option(IBS_BUILD_GUI   "Build the Qt GUI application" ON)
option(IBS_BUILD_CLI   "Build the command line tool" ON)
option(IBS_BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

foreach(ibs_path IN ITEMS "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")
    if(NOT ibs_path MATCHES "^[ -~]*$")
        # Why not 日本語で表示する: CMake は UTF-8 で出力するが、Qt Creator などは CP932 として表示するため文字化けする
        message(WARNING
            "The path contains non-ASCII characters: ${ibs_path}\n"
            "Some tools may fail with such paths. "
            "If the build fails, see README: 'Building in a folder with Japanese characters'.")
    endif()
endforeach()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

add_subdirectory(src/core)
add_subdirectory(src/infra)
add_subdirectory(src/app)

if(IBS_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

if(IBS_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

if(IBS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
