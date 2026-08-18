add_library(arango_inspection INTERFACE)

target_link_libraries(arango_inspection
  INTERFACE
  velocypack
  taocpp::json
  date_interface)

target_include_directories(arango_inspection
  INTERFACE
  include)
