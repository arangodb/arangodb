add_library(arango_tests_internal_rest_handler OBJECT
  InternalRestTraverserHandlerEdgeTest.cpp)
target_link_libraries(arango_tests_internal_rest_handler
        PRIVATE
        arango_lightweight
        arango_tests_basics
        velocypack
        velocypack_utils
        gtest
        fmt)

add_executable(arangodbtests_internal_rest_handler
        EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_internal_rest_handler
        arango_crashhandler_light
        arango_tests_internal_rest_handler
        arango_tests_basics
        arango_tests_graph
        gtest_main)
