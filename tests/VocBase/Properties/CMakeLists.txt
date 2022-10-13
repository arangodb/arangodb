set(ARANGODB_VOCBASE_PROPERTIES_TEST_SOURCES
        CreateCollectionBodyTest.cpp
        CollectionConstantPropertiesTest.cpp
        CollectionMutablePropertiesTest.cpp
        CollectionInternalPropertiesTest.cpp
        CollectionCreateOptionsTest.cpp
        )

target_sources(
        arangodbtests PUBLIC
        ${ARANGODB_VOCBASE_PROPERTIES_TEST_SOURCES}
)
