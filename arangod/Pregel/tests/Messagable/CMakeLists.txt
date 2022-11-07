
add_executable(arangodbtests_messagable
  main.cpp
  Messagable.cpp)

target_include_directories(arangodbtests_messagable PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_messagable boost_boost velocypack fmt gtest)
