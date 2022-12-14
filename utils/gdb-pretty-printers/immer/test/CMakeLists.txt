cmake_minimum_required(VERSION 3.20)

project(immer-pretty-printer-test CXX)

add_executable(flex_vector_test flex_vector_test.cpp)

target_link_libraries(flex_vector_test immer)
target_link_libraries(flex_vector_test fmt::fmt)

set_target_properties(flex_vector_test
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
        )

enable_testing()

add_test(NAME flex_vector_test
        COMMAND gdb
        $<TARGET_FILE:flex_vector_test>
        -x ${CMAKE_CURRENT_SOURCE_DIR}/../gdbinit
        -x ${CMAKE_CURRENT_SOURCE_DIR}/flex_vector_test.gdbscript
        -batch

        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
