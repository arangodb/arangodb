add_executable(
        arangodbtests_optimizer2_plan_inspection
        PlanInspectionTest.cpp)

target_link_libraries(arangodbtests_optimizer2_plan_inspection gtest
        arango_tests_basics
        arango_tests_graph
        arango_agency
        arango_cluster_engine
        arango_rocksdb
        arango_v8server
        arangoserver
        boost_boost
        fmt)

target_include_directories(arangodbtests_optimizer2_plan_inspection PRIVATE ${PROJECT_SOURCE_DIR}/tests)