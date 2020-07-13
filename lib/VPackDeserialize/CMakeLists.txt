
project(vpack-deserializer CXX)

add_library(deserializer-lib INTERFACE)

target_include_directories(deserializer-lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(test)
