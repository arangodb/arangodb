# added for testing purpose
add_executable(coro-registry
	CoroutineRegistry.cpp)
target_link_libraries(coro-registry
	arango_crashhandler_light
	arango_assertions)

add_executable(coro-test
	CoroutineRegistryTest.cpp)
target_link_libraries(coro-test
	arango_crashhandler_light
	arango_assertions
	GTest::gtest_main)

add_executable(promise_registry_test
	PromiseRegistryTest.cpp)
target_link_libraries(promise_registry_test
	arango_crashhandler_light
	arango_assertions
	arango   # for Result
	GTest::gtest_main)
