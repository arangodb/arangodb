

add_executable(network-curl test.cpp)

target_link_libraries(network-curl PRIVATE arango_curl)
# using the network feature for performance comparisons
target_link_libraries(network-curl PRIVATE arango_network)
target_link_libraries(network-curl PRIVATE arango_metrics_base)
target_link_libraries(network-curl PRIVATE arango_crashhandler_light)
target_link_libraries(network-curl PRIVATE libcurl)
