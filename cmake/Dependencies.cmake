include(FetchContent)

# Why not vcpkg / conan: 依存がまだ少なく、ビルド手順を「CMake だけ」に保つほうが参加者の敷居が低いため

# Why not FetchContent で取得する: アーカイブの展開と依存先の project() 呼び出しが、日本語を含むパスや
# Qt Creator の自動設定スクリプトと組み合わさると構成ごと失敗することがあるため、ハッシュ付きの単一ヘッダだけを取得する
function(ibs_add_single_header_library target alias url sha256 relative_path)
    set(include_root "${CMAKE_BINARY_DIR}/_deps/${target}")
    set(header "${include_root}/${relative_path}")
    if(NOT EXISTS "${header}")
        file(DOWNLOAD "${url}" "${header}" EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS status)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE "${header}")
            message(FATAL_ERROR "Failed to download ${url}: ${status}")
        endif()
    endif()
    add_library(${target} INTERFACE)
    add_library(${alias} ALIAS ${target})
    target_include_directories(${target} SYSTEM INTERFACE "${include_root}")
endfunction()

ibs_add_single_header_library(ibs_nlohmann_json nlohmann_json::nlohmann_json
    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
    nlohmann/json.hpp)

if(IBS_BUILD_CLI)
    ibs_add_single_header_library(ibs_cli11 CLI11::CLI11
        https://github.com/CLIUtils/CLI11/releases/download/v2.4.2/CLI11.hpp
        3935283e4c9db7089ed304476dbab2805c9311c41b125a55e92da4659df0d660
        CLI/CLI.hpp)
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
