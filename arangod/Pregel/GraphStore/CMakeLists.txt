target_sources(arango_pregel PRIVATE
  GraphLoader.cpp
  GraphSerdeConfigBuilder.cpp
  GraphSerdeConfigBuilderCluster.cpp
  GraphSerdeConfigBuilderSingleServer.cpp
  GraphSourceToGraphByCollectionsResolver.cpp
  GraphStorer.cpp
  GraphVPackBuilderStorer.cpp
  Magazine.cpp
  Quiver.cpp)
