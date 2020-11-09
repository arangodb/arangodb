
add_executable(greenspun_repl REPL.cpp)
target_link_libraries(greenspun_repl arango)
target_link_libraries(greenspun_repl arango-greenspun)


add_executable(greenspun_test PrimitivesTest.cpp)
target_link_libraries(greenspun_test arango)
target_link_libraries(greenspun_test arango-greenspun)

