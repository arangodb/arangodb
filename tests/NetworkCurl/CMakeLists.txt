

add_executable(network-curl test.cpp)

target_link_libraries(network-curl PRIVATE arango_curl)
target_link_libraries(network-curl PRIVATE arango_crashhandler_light)
