include(GoogleTest)

add_executable(ibs_core_tests
    unit/core/timing/TimingMapTest.cpp)

target_link_libraries(ibs_core_tests PRIVATE ibs::core GTest::gtest_main ibs::compiler_options)
gtest_discover_tests(ibs_core_tests DISCOVERY_MODE PRE_TEST)
