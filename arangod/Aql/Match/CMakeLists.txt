target_sources(arango_aql PRIVATE
  Builder.cpp
  PatternNormalizer.cpp
  PatternTypes.cpp
  CollectionAccessBuilder.cpp
  FilterBuilder.cpp
  ProjectionBuilder.cpp
  VariableScope.cpp
  PatternBuildState.cpp
  PathConstruction.cpp
  VertexPatternBuilder.cpp
  RelationshipPatternBuilder.cpp
)
