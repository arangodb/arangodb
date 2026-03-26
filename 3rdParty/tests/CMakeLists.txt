################################################################################
### CmdLine library
################################################################################

add_library(
  iresearch-cmdline
  INTERFACE
)

target_include_directories(
  iresearch-cmdline
  SYSTEM
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cmdline
)

################################################################################
### RapidJSON library
################################################################################

add_library(
  iresearch-rapidjson
  INTERFACE
)

target_include_directories(
  iresearch-rapidjson
  SYSTEM
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson
)

################################################################################
### utfcpp library
################################################################################

add_library(
  iresearch-utfcpp
  INTERFACE
)

target_include_directories(
  iresearch-utfcpp
  SYSTEM
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/utfcpp
)
