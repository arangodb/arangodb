add_subdirectory(Rules)

target_sources(arango_aql PRIVATE
    Optimizer.cpp
    OptimizerUtils.cpp)
