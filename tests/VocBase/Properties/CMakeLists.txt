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
)

target_sources(
        arangodbtests PUBLIC
        ${ARANGODB_VOCBASE_PROPERTIES_TEST_SOURCES}
)
