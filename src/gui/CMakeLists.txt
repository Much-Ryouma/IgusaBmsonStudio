find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_library(ibs_gui STATIC
    ArrangerWidget.cpp
    ArrangerWidget.hpp
    ChartEditorWidget.cpp
    ChartEditorWidget.hpp
    MainWindow.cpp
    MainWindow.hpp
    QtText.hpp
    SlicerPanel.cpp
    SlicerPanel.hpp
    SongSettingsDialog.cpp
    SongSettingsDialog.hpp
    StudioController.cpp
    StudioController.hpp
    StudioPalette.hpp
    TimelineMath.hpp
    WaveformSlicerWidget.cpp
    WaveformSlicerWidget.hpp)
add_library(ibs::gui ALIAS ibs_gui)
set_target_properties(ibs_gui PROPERTIES AUTOMOC ON)
target_include_directories(ibs_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ibs_gui
    PUBLIC ibs::app Qt6::Widgets
    PRIVATE ibs::compiler_options)

qt_add_executable(igusa_studio main.cpp)
set_target_properties(igusa_studio PROPERTIES
    OUTPUT_NAME "IgusaBmsonStudio"
    WIN32_EXECUTABLE ON)
target_link_libraries(igusa_studio PRIVATE ibs::gui ibs::compiler_options)

add_custom_command(TARGET igusa_studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/modes $<TARGET_FILE_DIR:igusa_studio>/modes)
