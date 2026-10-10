add_executable(igusa_cli
    main.cpp
    ExitCode.hpp)

set_target_properties(igusa_cli PROPERTIES OUTPUT_NAME "igusa-cli")
target_link_libraries(igusa_cli PRIVATE ibs::app CLI11::CLI11 ibs::compiler_options)
