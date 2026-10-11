add_executable(igusa_cli
    main.cpp
    ChartCommands.cpp
    CliSupport.cpp
    CliSupport.hpp
    ExitCode.hpp
    MidiCommand.cpp
    NullTestCommand.cpp
    SliceCommand.cpp
    StatsCommand.cpp)

set_target_properties(igusa_cli PROPERTIES OUTPUT_NAME "igusa-cli")
target_link_libraries(igusa_cli PRIVATE ibs::app CLI11::CLI11 ibs::compiler_options)

add_custom_command(TARGET igusa_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/modes $<TARGET_FILE_DIR:igusa_cli>/modes
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/midi_profiles $<TARGET_FILE_DIR:igusa_cli>/midi_profiles)
