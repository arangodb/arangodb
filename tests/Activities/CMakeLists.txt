add_library(arango_tests_activities OBJECT
  ActivitiesAsyncTest.cpp
  ActivitiesSchedulerTest.cpp
  ActivityRegistryTest.cpp
  GenericActivityTest.cpp)
target_link_libraries(arango_tests_activities PRIVATE
  arango_activities_global
  arango_tests_basics 
  arango_async
  gtest
)

add_executable(arangodbtests_activities EXCLUDE_FROM_ALL)
target_link_libraries(arangodbtests_activities
  arango_tests_activities
  arango_tests_basics 
  arango_async
  gtest_main)

