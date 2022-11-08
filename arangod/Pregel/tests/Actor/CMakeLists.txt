
add_executable(arangodbtests_actor
  ${PROJECT_SOURCE_DIR}/arangod/Pregel/Actor/Actor.cpp
  main.cpp
  ActorTest.cpp)

target_include_directories(arangodbtests_actor PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_actor boost_boost velocypack fmt gtest)
