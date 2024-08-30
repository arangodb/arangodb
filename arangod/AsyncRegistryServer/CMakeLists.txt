add_library(arango_async_registry_server STATIC
  Feature.cpp
  RestHandler.cpp)
target_link_libraries(arango_async_registry_server 
  PRIVATE
  arango_metrics)

add_executable(arango_async_registry_server_exe
	Feature.cpp
	RestHandler.cpp)
target_link_libraries(arango_async_registry_server_exe
	PRIVATE
	arango_async_registry_server
  arango_crashhandler_light)
