add_library(ibs_app STATIC
    src/ChartService.cpp
    src/ChartStorage.cpp)
add_library(ibs::app ALIAS ibs_app)

target_include_directories(ibs_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ibs_app
    PUBLIC ibs::core ibs::infra
    PRIVATE ibs::compiler_options)
