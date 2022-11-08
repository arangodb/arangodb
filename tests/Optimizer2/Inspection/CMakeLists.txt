add_executable(arangodbtests_optimizer2_inspectables ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp Inspection.cpp)

target_include_directories(
        arangodbtests_optimizer2_inspectables
        PRIVATE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR}/lib
        ${PROJECT_SOURCE_DIR}/arangod ${PROJECT_SOURCE_DIR}/tests/Mocks)
# Improve include of "tests/Mocks" - currently we define and include this at top-level

target_link_libraries(arangodbtests_optimizer2_inspectables velocypack fmt gtest)