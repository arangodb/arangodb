add_executable(
  arangodbtests_pregel
  ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp
  AlgorithmTest.cpp)

target_include_directories(
  arangodbtests_pregel
  PRIVATE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR}/lib
          ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_pregel velocypack fmt gtest)

add_executable(
  arangodbtests_pregel_impl
  ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp
  AlgorithmImplementation.cpp)

target_include_directories(
  arangodbtests_pregel_impl
  PRIVATE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR}/lib
          ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_pregel_impl velocypack fmt gtest
                      arangodb_actors)
