
add_executable(arangodbtests_coroutines
  main-coroutines.cpp
  EagerCoroTest.cpp
)

add_test(NAME arangodbtests-coroutines
  COMMAND arangodbtests_coroutines --gtest_output=xml)


target_link_libraries(arangodbtests_coroutines
  arango
  gtest
)
