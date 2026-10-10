cmake_minimum_required(VERSION 3.24)

project(IgusaBmsonStudio
    VERSION 0.1.0
    DESCRIPTION "bmson chart authoring tool: slicing, chart editing and preview"
    LANGUAGES CXX)

option(IBS_BUILD_GUI   "Build the Qt GUI application" ON)
option(IBS_BUILD_CLI   "Build the command line tool" ON)
option(IBS_BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
