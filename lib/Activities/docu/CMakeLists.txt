cmake_minimum_required(VERSION 3.13)
project(activity-docs CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER, "clang-19")
set(CMAKE_CXX_COMPILER, "clang++-19")
set(CMAKE_EXE_LINKER_FLAGS, "-fuse-ld=lld")


# Pin to LLVM/Clang 19. Override at configure time with
# -DClang_DIR=/usr/lib/llvm-XX/lib/cmake/clang if needed.
if(NOT DEFINED Clang_DIR)
    set(Clang_DIR "/usr/lib/llvm-19/lib/cmake/clang" CACHE PATH
        "Path to ClangConfig.cmake")
endif()
if(NOT DEFINED LLVM_DIR)
    set(LLVM_DIR "/usr/lib/llvm-19/lib/cmake/llvm" CACHE PATH
        "Path to LLVMConfig.cmake")
endif()

find_package(Clang REQUIRED CONFIG)

add_subdirectory(src)
add_subdirectory(tests)
