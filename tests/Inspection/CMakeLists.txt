add_library(arango_inspection_test_helper INTERFACE)

target_include_directories(arango_inspection_test_helper
  INTERFACE
  include)

add_library(arango_tests_inspection OBJECT
  InspectionTest.cpp
  JsonPrintTest.cpp
  TransformerTest.cpp
  ValidateTest.cpp
  ValidateTest.cpp
  VPackLoadTest.cpp
  VPackSaveTest.cpp)

target_link_libraries(arango_tests_inspection
  PRIVATE gtest
          velocypack
          arango_lightweight # jezas.
          arango_inspection
          arango_inspection_test_helper)

add_executable(arangodbtests_inspection
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_inspection
    arango_tests_inspection
    arango_crashhandler_light
    velocypack_utils
    gtest_main)

add_test(NAME inspection
         COMMAND arangodbtests_inspection)
