# Why not 空の STATIC ライブラリ: ソースがまだ無い段階で STATIC にすると生成器によってはエラーになるため、M1 で実装を追加するまで INTERFACE にしておく
add_library(ibs_infra INTERFACE)
add_library(ibs::infra ALIAS ibs_infra)
target_link_libraries(ibs_infra INTERFACE ibs::core)
