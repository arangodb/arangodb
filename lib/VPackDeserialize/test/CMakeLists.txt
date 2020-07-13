add_executable(deserializer-test test.cpp)

target_link_libraries(deserializer-test deserializer-lib)
target_link_libraries(deserializer-test velocypack)

target_compile_options(deserializer-test PRIVATE "-ftemplate-backtrace-limit=0")
