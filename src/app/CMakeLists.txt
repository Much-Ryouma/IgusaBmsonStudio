add_library(ibs_app STATIC
    src/ArrangementCommands.cpp
    src/AssetImportService.cpp
    src/AudioLibrary.cpp
    src/BgaCommands.cpp
    src/ChartCommands.cpp
    src/ChartService.cpp
    src/ChartStatisticsService.cpp
    src/ChartStorage.cpp
    src/EditorDocument.cpp
    src/NullTestService.cpp
    src/PlaybackService.cpp
    src/PreviewService.cpp
    src/ProjectAudio.cpp
    src/RecentFiles.cpp
    src/SlicingService.cpp
    src/SlicingSession.cpp
    src/TimelineViewport.cpp)
add_library(ibs::app ALIAS ibs_app)

target_include_directories(ibs_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ibs_app
    PUBLIC ibs::core ibs::infra
    PRIVATE ibs::compiler_options)
