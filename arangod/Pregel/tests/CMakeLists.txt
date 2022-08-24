add_executable(arangodbtests_pregel
  AlgorithmTest.cpp)

target_include_directories(arangodbtests_pregel PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_pregel velocypack fmt)
