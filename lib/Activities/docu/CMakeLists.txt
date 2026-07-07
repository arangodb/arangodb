# The whole build already mandates Clang 19 (see CheckCompilerVersion in the
# top-level CMakeLists.txt), so pin find_package to the matching LLVM/Clang 19
# install. This is required because CMake's default search would otherwise pick
# up the unversioned system package in /usr/lib/cmake/clang-16, whose static
# libraries are not installed.
set(ACTIVITY_DOCS_LLVM_ROOT "/usr/lib/llvm-19" CACHE PATH
    "LLVM/Clang 19 installation prefix used by the activities documentation tool")
set(LLVM_DIR "${ACTIVITY_DOCS_LLVM_ROOT}/lib/cmake/llvm")
set(Clang_DIR "${ACTIVITY_DOCS_LLVM_ROOT}/lib/cmake/clang")

find_package(Clang REQUIRED CONFIG)

add_subdirectory(src)
