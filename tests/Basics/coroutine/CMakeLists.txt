add_executable(registry_test
	RegistryTest.cpp)
target_link_libraries(registry_test
	arango_crashhandler_light
	arango_assertions
	GTest::gtest_main)

add_executable(thread_registry_test
	ThreadRegistryTest.cpp)
target_link_libraries(thread_registry_test
	arango_crashhandler_light
	arango_assertions
	GTest::gtest_main)
