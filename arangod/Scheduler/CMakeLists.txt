add_library(arango_scheduler STATIC
  LockfreeThreadPool.cpp
  Scheduler.cpp
  SchedulerFeature.cpp
  SchedulerMetrics.cpp
  SimpleThreadPool.cpp
  SupervisedScheduler.cpp
  ThreadPoolScheduler.cpp
  WorkStealingThreadPool.cpp
)

target_link_libraries(arango_scheduler
  PUBLIC
  velocypack
  arango_futures
  arango_basic_utils
  arango_basic_errors
)

if (USE_V8)
  target_link_libraries(arango_scheduler PUBLIC ${V8_LIBS})
  target_link_libraries(arango_scheduler PUBLIC v8_interface)
endif()

