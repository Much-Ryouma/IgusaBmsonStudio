configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ibs/core/BuildInfo.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ibs/core/BuildInfo.hpp
    @ONLY)

add_library(ibs_core STATIC
    src/asset/AssetPath.cpp
    src/bmson/BmsonEquivalence.cpp
    src/bmson/BmsonExporter.cpp
    src/bmson/BmsonImporter.cpp
    src/mode/KeyMode.cpp
    src/model/Project.cpp
    src/timing/TimingMap.cpp
    src/validation/Validator.cpp)
add_library(ibs::core ALIAS ibs_core)

target_include_directories(ibs_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Why not Qt や JSON ライブラリをリンクする: CLI や将来の派生ツールから、GUI やファイル形式に縛られずにロジックを再利用するため (docs/CODING_RULES.md)
target_link_libraries(ibs_core PRIVATE ibs::compiler_options)
