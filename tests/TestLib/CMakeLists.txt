add_library(arangodbtests_lib
  icu-helper.h
  icu-helper.cpp
  VelocyPackHelper.h
  VelocyPackHelper.cpp
  schmutz.h
  schmutz.cpp)

target_include_directories(arangodbtests_lib
  PUBLIC
  ${CMAKE_SOURCE_DIR}/tests
  ${CMAKE_SOURCE_DIR}/arangod
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_BINARY_DIR}/lib)

target_link_libraries(arangodbtests_lib
  arangoserver
  velocypack
  boost_system
  gtest)
