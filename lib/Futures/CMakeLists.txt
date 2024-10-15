add_library(arango_futures STATIC
  src/Future.cpp)

target_include_directories(arango_futures
  PUBLIC
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/3rdParty/iresearch/external/function2
  include)

target_link_libraries(arango_futures
  PUBLIC
  arango_inspection
  arango_assertions
  arango_async_registry
  arango_utils
  velocypack)
