add_library(arango_async_registry STATIC
	registry_variable.cpp)
target_link_libraries(arango_async_registry
	PRIVATE
	arango_assertions)
