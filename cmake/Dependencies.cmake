include(FetchContent)

# Why not vcpkg / conan: 依存がまだ少なく、ビルド手順を「CMake だけ」に保つほうが参加者の敷居が低いため

if(IBS_BUILD_CLI)
    FetchContent_Declare(CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.2
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(CLI11)
endif()

if(IBS_BUILD_TESTS)
    # Why not 静的 CRT: GoogleTest の既定 (/MT) のままだと他ターゲット (/MD) とリンク時に衝突するため
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(googletest)
endif()
