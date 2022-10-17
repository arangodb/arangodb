add_library(optimizer2_plan_nodes STATIC ReturnNode.cpp )

target_include_directories(optimizer2_plan_nodes PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
        ${PROJECT_BINARY_DIR}/lib
        ${PROJECT_SOURCE_DIR}/arangod)

target_link_libraries(optimizer2_plan_nodes velocypack fmt gtest)