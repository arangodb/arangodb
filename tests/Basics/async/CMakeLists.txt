# added for testing purpose
add_executable(coro-registry
	CoroutineRegistry.cpp)
target_link_libraries(coro-registry
	arango_crashhandler_light
	arango_assertions)

add_executable(thread_registry_test
	CoroutineRegistryTest.cpp)
target_link_libraries(thread_registry_test
	arango_crashhandler_light
	arango_assertions
	GTest::gtest_main)

add_executable(promise_registry_test
	PromiseRegistryTest.cpp)
target_link_libraries(promise_registry_test
	arango_crashhandler_light
	arango_assertions
	GTest::gtest_main)
