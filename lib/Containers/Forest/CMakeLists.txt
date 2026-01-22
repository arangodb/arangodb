add_library(arango_forest INTERFACE
  depth_first.h
  forest.h)
target_include_directories(arango_forest INTERFACE
  ${PROJECT_SOURCE_DIR}/lib)
