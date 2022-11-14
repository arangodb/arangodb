add_executable(arangodbtests_optimizer2_inspectables Inspection.cpp)

target_include_directories(
        arangodbtests_optimizer2_inspectables
        PRIVATE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR}/lib
        ${PROJECT_SOURCE_DIR}/arangod ${PROJECT_SOURCE_DIR}/tests/Mocks ${PROJECT_SOURCE_DIR}/tests/Graph)
# Improve include of "tests/Mocks" - currently we define and include this at top-level

target_link_libraries(arangodbtests_optimizer2_inspectables velocypack fmt gtest velocypack_utils)