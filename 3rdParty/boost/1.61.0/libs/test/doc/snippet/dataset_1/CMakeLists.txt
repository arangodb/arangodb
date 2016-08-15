cmake_minimum_required(VERSION 2.8.11)
project(my_first_test)
enable_testing()

# not meant to be included in the documentation
set(BOOST_ROOT ${CMAKE_SOURCE_DIR}/../../../../../..)
message(STATUS "$BOOST_ROOT = ${BOOST_ROOT}")
# creates the executable
add_executable(test_executable test_file.cpp)
# indicates the include paths
target_include_directories(test_executable PRIVATE ${BOOST_ROOT})
# indicates the link paths
target_link_libraries(test_executable ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

# declares a test with our executable
add_test(NAME test1 COMMAND test_executable)


