add_executable(gdb_pretty_printer
  async_registry_test.cpp)
target_link_libraries(gdb_pretty_printer
  PRIVATE
  arango_async_registry)

enable_testing()

add_test(NAME test_registry_pretty_printer
  COMMAND gdb
  $<TARGET_FILE:gdb_pretty_printer>
  -ix ${PROJECT_SOURCE_DIR}/arangod/AsyncRegistryServer/PrettyPrinter/.gdbinit
  -x ${CMAKE_CURRENT_SOURCE_DIR}/test.gdbscript
  -batch

  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
