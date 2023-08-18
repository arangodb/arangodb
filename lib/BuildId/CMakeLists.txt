add_library(arango_build_id
  STATIC
    src/BuildId.cpp)

target_link_options(arango_build_id
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/BuildId.ld)

target_include_directories(arango_build_id
  PUBLIC
  include)
