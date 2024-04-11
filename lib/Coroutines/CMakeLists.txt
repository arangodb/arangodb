add_library(arango_coroutines STATIC
  src/EagerCoro.cpp include/Coroutines/EagerCoro.h
  src/EagerCoroFutures.cpp include/Coroutines/EagerCoroFutures.h
)

target_include_directories(arango_coroutines PUBLIC
  include
  ${PROJECT_SOURCE_DIR}/arangod
)

# for Try
target_link_libraries(arango_coroutines arango_futures)
target_link_libraries(arango_coroutines arango_lightweight)
