include(GoogleTest)

function(ibs_add_test_suite name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main ibs::compiler_options)
    target_compile_definitions(${name} PRIVATE IBS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    gtest_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()

ibs_add_test_suite(ibs_core_tests
    SOURCES
        unit/core/asset/AssetPathTest.cpp
        unit/core/bmson/BmsonEquivalenceTest.cpp
        unit/core/bmson/BmsonExportTest.cpp
        unit/core/bmson/BmsonImportTest.cpp
        unit/core/mode/KeyModeTest.cpp
        unit/core/model/ProjectTest.cpp
        unit/core/timing/TimingMapTest.cpp
        unit/core/validation/ValidatorTest.cpp
    LIBRARIES ibs::core)

ibs_add_test_suite(ibs_infra_tests
    SOURCES
        unit/infra/bmson/BmsonJsonCodecTest.cpp
        unit/infra/fs/FileIoTest.cpp
        unit/infra/mode/KeyModeJsonLoaderTest.cpp
    LIBRARIES ibs::infra)

ibs_add_test_suite(ibs_app_tests
    SOURCES
        unit/app/ChartServiceTest.cpp
    LIBRARIES ibs::app)

ibs_add_test_suite(ibs_external_tests
    SOURCES
        unit/external/SampleRoundTripTest.cpp
    LIBRARIES ibs::infra)
