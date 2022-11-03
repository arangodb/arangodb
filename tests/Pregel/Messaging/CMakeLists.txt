target_sources(arangodbtests PRIVATE
  AggregateTest.cpp
  MessageQueueTest.cpp)

if (USE_SEPARATE_PREGEL_TESTS_BINARY)
  target_sources(arangodbtests_pregel PRIVATE
    AggregateTest.cpp
    MessageQueueTest.cpp)
endif()
