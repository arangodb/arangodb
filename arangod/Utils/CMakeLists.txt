add_library(arango_utils STATIC
  Cursor.cpp
  ExecContext.cpp
  OperationOptions.cpp
  SupportInfoBuilder.cpp
  UrlHelper.cpp)

if (NOT USE_ENTERPRISE)
  target_sources(arango_utils PRIVATE
    Events.cpp)
endif ()

target_link_libraries(arango_utils
  boost_boost
  arango::validation
  arango_replication2
  immer)

target_include_directories(arango_utils PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
