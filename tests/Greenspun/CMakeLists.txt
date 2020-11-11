
add_executable(greenspun_repl REPL.cpp)
target_link_libraries(greenspun_repl arango)
target_link_libraries(greenspun_repl arango-greenspun)


add_executable(greenspun_test PrimitivesTest.cpp)
target_link_libraries(greenspun_test arango)
target_link_libraries(greenspun_test arango-greenspun)
target_link_libraries(greenspun_test gtest)

target_include_directories(greenspun_test SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/${ARANGO_ROCKSDB_VERSION}/third-party/gtest-1.8.1/fused-src)
