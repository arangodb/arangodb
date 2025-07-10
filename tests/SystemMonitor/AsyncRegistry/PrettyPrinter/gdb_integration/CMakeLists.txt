add_executable(async_registry_gdb_pretty_printer
  async_registry_gdb_pretty_printer_test.cpp)
target_link_libraries(async_registry_gdb_pretty_printer
  PRIVATE
  arango_async_registry
  arango
)

add_test(NAME async_registry_gdb_pretty_printer_test
  COMMAND gdb
  $<TARGET_FILE:async_registry_gdb_pretty_printer>
  -ix ${PROJECT_SOURCE_DIR}/arangod/SystemMonitor/AsyncRegistry/PrettyPrinter/.gdbinit
  -x ${CMAKE_CURRENT_SOURCE_DIR}/test.gdbscript
  -batch

  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
