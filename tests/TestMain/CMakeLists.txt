add_library(arangodbtests_main
  main.cpp)

target_include_directories(arangodbtests_main
  PUBLIC
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_SOURCE_DIR}/arangod
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_BINARY_DIR}/lib)

target_link_libraries(arangodbtests_main
  arangodbtests_lib
  arangoserver
  velocypack
  boost_system
  gtest)
