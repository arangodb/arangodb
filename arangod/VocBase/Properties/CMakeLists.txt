add_library(arango_vocbase_api STATIC
        ClusteringConstantProperties.cpp
        ClusteringMutableProperties.cpp
        ClusteringProperties.cpp
        CollectionConstantProperties.cpp
        CollectionCreateOptions.cpp
        CollectionIndexesProperties.cpp
        CollectionInternalProperties.cpp
        CollectionMutableProperties.cpp
        CreateCollectionBody.cpp
        DatabaseConfiguration.cpp
        KeyGeneratorProperties.cpp
        UserInputCollectionProperties.cpp
        UtilityInvariants.cpp
)

target_include_directories(arango_vocbase_api PRIVATE
        ${PROJECT_SOURCE_DIR}/arangod
        ${PROJECT_SOURCE_DIR}/lib
        ${PROJECT_BINARY_DIR}/lib
)

target_compile_options(arango_vocbase_api PRIVATE
        ${CMAKE_DEPRECATE_OPTIONS})


target_link_libraries(arango_vocbase_api PUBLIC
        velocypack
        fmt

        # Required for Result / ResultT
        PRIVATE arango

        # Required for immer, exposed by the AgencySpecification
        PRIVATE arango_replication2
)
