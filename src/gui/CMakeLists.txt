find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(igusa_studio
    main.cpp
    MainWindow.hpp
    MainWindow.cpp)

set_target_properties(igusa_studio PROPERTIES
    OUTPUT_NAME "IgusaBmsonStudio"
    WIN32_EXECUTABLE ON)

target_link_libraries(igusa_studio PRIVATE ibs::app Qt6::Widgets ibs::compiler_options)
