add_library(arango_indexes STATIC
  EdgeIndexDefinition.cpp
  FulltextIndexDefinition.cpp
  Geo1IndexDefinition.cpp
  Geo2IndexDefinition.cpp
  GeoIndexDefinition.cpp
  Index.cpp
  IndexDefinition.cpp
  IndexFactory.cpp
  IndexIterator.cpp
  IndexTypeCatalog.cpp
  MdiIndexDefinition.cpp
  MdiPrefixedIndexDefinition.cpp
  PrimaryIndexDefinition.cpp
  SecondaryIndexDefinition.cpp
  SimpleAttributeEqualityMatcher.cpp
  SortedIndexAttributeMatcher.cpp
  TtlIndexDefinition.cpp
  VectorIndexDefinition.cpp
  ${PROJECT_SOURCE_DIR}/arangod/GeoIndex/Index.cpp
  ${PROJECT_SOURCE_DIR}/arangod/GeoIndex/Covering.cpp
  ${PROJECT_SOURCE_DIR}/arangod/GeoIndex/Near.cpp
  ${PROJECT_SOURCE_DIR}/arangod/VectorIndex/FaissFactory.cpp)

target_link_libraries(arango_indexes
  PUBLIC
    arango_replication2
    arango_geo
    arango::validation
    faiss
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
