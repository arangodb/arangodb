add_library(arango_assertions STATIC
  AssertionConditionalLogger.cpp
  AssertionLogger.cpp)

target_link_libraries(arango_assertions
  PUBLIC
  arango_crashhandler
)

target_include_directories(arango_assertions
  PUBLIC
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib)

