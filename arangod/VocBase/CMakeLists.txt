add_library(arango_vocbase STATIC
  ComputedValues.cpp
  KeyGenerator.cpp
  LogicalCollection.cpp
  LogicalDataSource.cpp
  LogicalView.cpp
  Validators.cpp
  VocbaseInfo.cpp
  ticks.cpp
  vocbase.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/QueryCursor.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/StandaloneCalculation.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Replication/DatabaseReplicationApplier.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Utils/CollectionNameResolver.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Utils/CursorRepository.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Utils/DatabaseGuard.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Utils/SingleCollectionTransaction.cpp)

target_link_libraries(arango_vocbase
  arango_agency
  arango_cluster_methods
  arango_indexes
  arango_replication
  arango_replication2
  boost_system
  arango::validation)

target_include_directories(arango_vocbase PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_subdirectory(Identifiers)
add_subdirectory(Methods)
