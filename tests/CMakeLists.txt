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
        unit/core/audio/AudioBufferTest.cpp
        unit/core/audio/WaveformPyramidTest.cpp
        unit/core/bmson/BmsonEquivalenceTest.cpp
        unit/core/chart/ChartEditingTest.cpp
        unit/core/bmson/BmsonExportTest.cpp
        unit/core/bmson/BmsonImportTest.cpp
        unit/core/midi/MidiExporterTest.cpp
        unit/core/mode/KeyModeTest.cpp
        unit/core/model/ProjectTest.cpp
        unit/core/playback/BmsonRendererTest.cpp
        unit/core/playback/PlaybackRangeTest.cpp
        unit/core/sidecar/SidecarTest.cpp
        unit/core/slicing/CutPlannerTest.cpp
        unit/core/slicing/FftTest.cpp
        unit/core/slicing/OnsetDetectorTest.cpp
        unit/core/timing/BarLinesTest.cpp
        unit/core/timing/TimingEditsTest.cpp
        unit/core/timing/TimingMapTest.cpp
        unit/core/validation/ValidatorTest.cpp
    LIBRARIES ibs::core)

ibs_add_test_suite(ibs_infra_tests
    SOURCES
        unit/infra/audio/AudioFileDecoderTest.cpp
        unit/infra/audio/MiniaudioOutputTest.cpp
        unit/infra/bmson/BmsonJsonCodecTest.cpp
        unit/infra/fs/FileIoTest.cpp
        unit/infra/image/ImageSizeTest.cpp
        unit/infra/midi/StandardMidiFileTest.cpp
        unit/infra/mode/KeyModeJsonLoaderTest.cpp
        unit/infra/process/ProcessRunnerTest.cpp
        unit/infra/sidecar/SidecarJsonCodecTest.cpp
    LIBRARIES ibs::infra)

ibs_add_test_suite(ibs_app_tests
    SOURCES
        unit/app/ArrangementCommandsTest.cpp
        unit/app/AssetImportServiceTest.cpp
        unit/app/AudioLibraryTest.cpp
        unit/app/BgaCommandsTest.cpp
        unit/app/ChartCommandsTest.cpp
        unit/core/chart/ChartStatisticsTest.cpp
        unit/app/ChartServiceTest.cpp
        unit/app/EditorDocumentTest.cpp
        unit/app/NullTestServiceTest.cpp
        unit/app/PlaybackServiceTest.cpp
        unit/app/PreviewServiceTest.cpp
        unit/app/RecentFilesTest.cpp
        unit/app/SlicingServiceTest.cpp
        unit/app/SlicingSessionTest.cpp
        unit/app/TimelineViewportTest.cpp
    LIBRARIES ibs::app)

ibs_add_test_suite(ibs_external_tests
    SOURCES
        unit/external/SampleRoundTripTest.cpp
    LIBRARIES ibs::infra)

if(IBS_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(ibs_gui_tests gui/GuiTestMain.cpp gui/StudioGuiTest.cpp gui/ChartEditorGuiTest.cpp)
    target_link_libraries(ibs_gui_tests PRIVATE ibs::gui Qt6::Test GTest::gtest ibs::compiler_options)
    # Why not gtest_discover_tests で 1 件ずつ登録する: 一覧を取り出すときにも Qt の DLL が必要で、
    # Windows では PATH を通す前に実行されて失敗するため、まとめて 1 件として登録し、環境変数を設定して実行する
    add_test(NAME StudioGui COMMAND ibs_gui_tests)
    set(ibs_gui_test_environment "QT_QPA_PLATFORM=set:offscreen")
    if(WIN32)
        get_target_property(ibs_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(NOT ibs_qt_core_location)
            get_target_property(ibs_qt_core_location Qt6::Core IMPORTED_LOCATION)
        endif()
        get_filename_component(ibs_qt_bin_dir "${ibs_qt_core_location}" DIRECTORY)
        list(APPEND ibs_gui_test_environment "PATH=path_list_prepend:${ibs_qt_bin_dir}")
    endif()
    set_tests_properties(StudioGui PROPERTIES ENVIRONMENT_MODIFICATION "${ibs_gui_test_environment}")
endif()
