cmake_minimum_required(VERSION 3.13)
project(find-vars-of-type CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Clang REQUIRED CONFIG)
# find_package(LLVM REQUIRED CONFIG)
# llvm_map_components_to_libnames(LLVM_LIBS support)

add_executable(find-vars-of-type find_activities.cpp)
target_link_libraries(find-vars-of-type PRIVATE
    clang-cpp
    # clangTooling
    # clangASTMatchers
    # clangFrontend
    # clangAST
    # clangBasic
    LLVM
    # ${LLVM_LIBS}
)
target_include_directories(find-vars-of-type PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})
target_compile_definitions(find-vars-of-type PRIVATE
    ${LLVM_DEFINITIONS_LIST})
# target_link_libraries(find-vars-of-type PRIVATE
#     clangTooling
#     clangASTMatchers
#     clangBasic
# )
