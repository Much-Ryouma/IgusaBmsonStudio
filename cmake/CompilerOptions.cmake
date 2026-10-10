add_library(ibs_compiler_options INTERFACE)
add_library(ibs::compiler_options ALIAS ibs_compiler_options)

if(MSVC)
    # Why not default code page: 日本語を含むソースを CP932 として解釈されると文字化けするため /utf-8 を明示する
    target_compile_options(ibs_compiler_options INTERFACE /utf-8 /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(ibs_compiler_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(ibs_compiler_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
