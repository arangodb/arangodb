set(ARANGODB_VOCBASE_PROPERTIES_TEST_SOURCES
        ClusteringConstantPropertiesTest.cpp
        ClusteringMutablePropertiesTest.cpp
        ClusteringPropertiesTest.cpp
        CollectionPropertiesTest.cpp
        CollectionConstantPropertiesTest.cpp
        CollectionMutablePropertiesTest.cpp
        CollectionInternalPropertiesTest.cpp
        CollectionCreateOptionsTest.cpp
        CreateCollectionBodyTest.cpp
        KeyGeneratorPropertiesTest.cpp

        ${PROJECT_SOURCE_DIR}/arangod/VocBase/Identifiers/DataSourceId.cpp
)

target_sources(
        arangodbtests PUBLIC
        ${ARANGODB_VOCBASE_PROPERTIES_TEST_SOURCES}
)

add_executable(
        arangodb_vocbase_properties_test EXCLUDE_FROM_ALL
        ${ARANGODB_VOCBASE_PROPERTIES_TEST_SOURCES}
        main.cpp
)

target_include_directories(arangodb_vocbase_properties_test PRIVATE
    ${PROJECT_SOURCE_DIR}/arangod
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_BINARY_DIR}/lib
)

target_link_libraries(
  arangodb_vocbase_properties_test
  arango_vocbase_api
  gtest
)
