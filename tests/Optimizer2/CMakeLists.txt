add_executable(
        arangodbtests_optimizer2
        main.cpp)

target_include_directories(
        arangodbtests_optimizer2
        PRIVATE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR}/lib
        ${PROJECT_SOURCE_DIR}/arangod ${PROJECT_SOURCE_DIR}/tests/Mocks)
# Improve include of "tests/Mocks" - currently we define and include this at top-level

target_link_libraries(arangodbtests_optimizer2 velocypack fmt gtest velocypack_utils optimizer2)

add_subdirectory(Inspection)
add_subdirectory(PlanNodes)
add_subdirectory(PlanNodeTypes)
add_subdirectory(Plan)
add_subdirectory(PlanInspection)
