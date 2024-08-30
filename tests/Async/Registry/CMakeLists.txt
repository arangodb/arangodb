target_sources(arangodbtests
  PRIVATE
  RegistryTest.cpp
  ThreadRegistryTest.cpp)

add_executable(registry_test
	RegistryTest.cpp
	ThreadRegistryTest.cpp)
target_include_directories(registry_test PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod)
target_link_libraries(registry_test
	arango_async_registry
  arango_crashhandler_light
  arango_lightweight
	GTest::gtest_main)
