cmake_minimum_required(VERSION 3.13)
project(activity-docs CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_executable(find-activity-subclasses find_activity_subclasses.cpp)
target_link_libraries(find-activity-subclasses PRIVATE
    clang-cpp
    LLVM
)
target_include_directories(find-activity-subclasses PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})
target_compile_definitions(find-activity-subclasses PRIVATE
    ${LLVM_DEFINITIONS_LIST})

get_filename_component(ARANGODB_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# Cache variable selecting which build-presets compile_commands.json to walk.
set(ACTIVITY_DOCS_COMPILE_DB
    "${ARANGODB_ROOT}/build-presets/my-edition"
    CACHE PATH
    "Directory containing the compile_commands.json to scan")

set(ACTIVITY_DOCS_OUTPUT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    CACHE PATH
    "Directory where generated markdown lands")

add_custom_target(activity-docs
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_docs.sh
            $<TARGET_FILE:find-activity-subclasses>
            ${ACTIVITY_DOCS_COMPILE_DB}
            ${ACTIVITY_DOCS_OUTPUT_DIR}
            ${ARANGODB_ROOT}
    DEPENDS find-activity-subclasses
    USES_TERMINAL
    COMMENT "Generating Activities.md from AST walk")
