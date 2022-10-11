add_executable(arangodbtests_optimizer2
  ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp
  main.cpp)

target_include_directories(arangodbtests_optimizer2 PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod)

target_link_libraries(arangodbtests_optimizer2 velocypack fmt gtest)

