# Why not 空の STATIC ライブラリ: infra と同じ理由。ユースケースを実装する M1 以降で STATIC に切り替える
add_library(ibs_app INTERFACE)
add_library(ibs::app ALIAS ibs_app)
target_link_libraries(ibs_app INTERFACE ibs::core ibs::infra)
