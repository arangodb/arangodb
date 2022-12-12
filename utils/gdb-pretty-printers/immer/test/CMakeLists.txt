cmake_minimum_required(VERSION 3.20)

project(immer-pretty-printer-test CXX)

#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../3rdParty/ EXCLUDE_FROM_ALL)

# TODO rename target and file
add_executable(main main.cpp)

target_link_libraries(main immer)
target_link_libraries(main fmt::fmt)
