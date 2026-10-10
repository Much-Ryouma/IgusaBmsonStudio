add_library(ibs_infra STATIC
    src/bmson/BmsonFileRepository.cpp
    src/bmson/BmsonJsonCodec.cpp
    src/fs/LocalFileSystem.cpp
    src/fs/Utf8Path.cpp
    src/json/PrettyJsonWriter.cpp
    src/mode/KeyModeJsonLoader.cpp)
add_library(ibs::infra ALIAS ibs_infra)

target_include_directories(ibs_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ibs_infra
    PUBLIC ibs::core nlohmann_json::nlohmann_json
    PRIVATE ibs::compiler_options)
