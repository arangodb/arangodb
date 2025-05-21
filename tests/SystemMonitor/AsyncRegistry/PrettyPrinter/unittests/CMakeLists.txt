add_test(NAME async_registry_python_pretty_printer_test
  COMMAND ${PYTHON_EXECUTABLE} -m unittest discover ${CMAKE_CURRENT_SOURCE_DIR}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/arangod/SystemMonitor/AsyncRegistry/PrettyPrinter/src)
