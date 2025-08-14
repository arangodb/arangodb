target_sources(arango_graph PRIVATE
  ClusterProvider.cpp
  BaseProviderOptions.cpp
  SingleServerProvider.cpp
  ProviderTracer.cpp)

add_subdirectory(SingleServer)
