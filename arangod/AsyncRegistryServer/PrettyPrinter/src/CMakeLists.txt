add_custom_target(async_registry_pretty_printer_python_test ALL
  COMMAND ${PYTHON_EXECUTABLE} -m unittest discover ${CMAKE_CURRENT_SOURCE_DIR})
