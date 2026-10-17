find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_library(ibs_gui STATIC
    AboutDialog.cpp
    AboutDialog.hpp
    AppIcon.cpp
    AppIcon.hpp
    ArrangerWidget.cpp
    ArrangerWidget.hpp
    BgaPanel.cpp
    BgaPanel.hpp
    ChartEditorWidget.cpp
    CutLayoutCopyDialog.cpp
    CutLayoutCopyDialog.hpp
    ChartEditorWidget.hpp
    ChartMinimap.cpp
    ChartMinimap.hpp
    MainWindow.cpp
    MainWindow.hpp
    QtProcessLauncher.hpp
    QtText.hpp
    QtTranslations.cpp
    QtTranslations.hpp
    SlicerPanel.cpp
    SlicerPanel.hpp
    SongSettingsDialog.cpp
    NullTestDialog.cpp
    NullTestDialog.hpp
    StatisticsDialog.cpp
    StatisticsDialog.hpp
    SongSettingsDialog.hpp
    StudioSettingsDialog.cpp
    StudioSettingsDialog.hpp
    StudioSettings.cpp
    StudioSettings.hpp
    StudioController.cpp
    StudioController.hpp
    StudioPalette.hpp
    TimelineMath.hpp
    TimelineScrollBar.hpp
    WaveformSlicerWidget.cpp
    WaveformSlicerWidget.hpp)
add_library(ibs::gui ALIAS ibs_gui)
set_target_properties(ibs_gui PROPERTIES AUTOMOC ON)
target_include_directories(ibs_gui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ibs_gui
    PUBLIC ibs::app Qt6::Widgets
    PRIVATE ibs::compiler_options)

set(ibs_gui_resource_files "${PROJECT_SOURCE_DIR}/packaging/THIRD_PARTY_NOTICES.txt")
if(EXISTS "${PROJECT_SOURCE_DIR}/resources/icons/app.png")
    list(APPEND ibs_gui_resource_files "${PROJECT_SOURCE_DIR}/resources/icons/app.png")
endif()
qt_add_resources(ibs_gui ibs_gui_resources
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES ${ibs_gui_resource_files})

qt_add_executable(igusa_studio main.cpp)
set_target_properties(igusa_studio PROPERTIES
    OUTPUT_NAME "IgusaBmsonStudio"
    WIN32_EXECUTABLE ON)
target_link_libraries(igusa_studio PRIVATE ibs::gui ibs::compiler_options)
ibs_add_windows_resources(igusa_studio "IgusaBmsonStudio" "IgusaBmsonStudio - bmson chart authoring tool")

install(TARGETS igusa_studio RUNTIME DESTINATION .)
if(WIN32 AND COMMAND qt_generate_deploy_script)
    # Why not windeployqt の既定のまま: MSVC では DLL ではなく vc_redist の
    # インストーラーを置くため、ランタイムは InstallRequiredSystemLibraries で exe の横に置く。
    # 訳は Qt 標準のボタンを日本語にするためだけに使うので、日本語だけを入れる
    qt_generate_deploy_script(
        TARGET igusa_studio
        OUTPUT_SCRIPT ibs_deploy_script
        CONTENT "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"$<TARGET_FILE_NAME:igusa_studio>\"
    BIN_DIR .
    PLUGINS_DIR .
    DEPLOY_TOOL_OPTIONS --translations ja
    NO_COMPILER_RUNTIME)
")
    install(SCRIPT "${ibs_deploy_script}")
endif()

add_custom_command(TARGET igusa_studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/modes $<TARGET_FILE_DIR:igusa_studio>/modes)
