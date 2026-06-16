add_library(arango_indexes STATIC
  Index.cpp
  IndexFactory.cpp
  IndexIterator.cpp
  SimpleAttributeEqualityMatcher.cpp
  SortedIndexAttributeMatcher.cpp
  ${PROJECT_SOURCE_DIR}/arangod/GeoIndex/Index.cpp
  ${PROJECT_SOURCE_DIR}/arangod/GeoIndex/Covering.cpp
  ${PROJECT_SOURCE_DIR}/arangod/GeoIndex/Near.cpp)

target_link_libraries(arango_indexes
  PUBLIC
    arango_replication2
    arango_geo
    arango::validation
  PRIVATE
    date_interface)
  
target_include_directories(arango_indexes
    PUBLIC ${IRESEARCH_INCLUDE}
)

target_include_directories(arango_indexes PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}"
  "${PROJECT_SOURCE_DIR}/3rdParty/function2"
)
