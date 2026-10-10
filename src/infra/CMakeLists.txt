# Why not ibs::compiler_options を適用する: 外部ライブラリの実装に自分たちの厳しい警告設定をかけると、直せない警告で埋もれるため
add_library(ibs_audio_codecs STATIC thirdparty/AudioCodecs.c)
target_include_directories(ibs_audio_codecs SYSTEM PUBLIC "${IBS_AUDIO_DEPS_DIR}")
if(MSVC)
    target_compile_options(ibs_audio_codecs PRIVATE /w)
else()
    target_compile_options(ibs_audio_codecs PRIVATE -w)
    find_package(Threads REQUIRED)
    target_link_libraries(ibs_audio_codecs PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

add_library(ibs_infra STATIC
    src/audio/AudioFileDecoder.cpp
    src/audio/MiniaudioOutput.cpp
    src/bmson/BmsonFileRepository.cpp
    src/bmson/BmsonJsonCodec.cpp
    src/fs/LocalFileSystem.cpp
    src/fs/Utf8Path.cpp
    src/json/PrettyJsonWriter.cpp
    src/midi/StandardMidiFile.cpp
    src/mode/KeyModeJsonLoader.cpp)
add_library(ibs::infra ALIAS ibs_infra)

target_include_directories(ibs_infra PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ibs_infra
    PUBLIC ibs::core nlohmann_json::nlohmann_json
    PRIVATE ibs_audio_codecs ibs::compiler_options)
