add_library(arango_inspection INTERFACE)

target_link_libraries(arango_inspection
  INTERFACE
    date_interface
    fmt::fmt)

target_include_directories(arango_inspection
  INTERFACE
  include)
