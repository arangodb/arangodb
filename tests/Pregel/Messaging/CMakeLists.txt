target_sources(arangodbtests PRIVATE
  MessageTest.cpp
  AggregateTest.cpp)

if (USE_SEPARATE_PREGEL_TESTS_BINARY)
  target_sources(arangodbtests_pregel PRIVATE
    MessageTest.cpp
    AggregateTest.cpp)
endif()
