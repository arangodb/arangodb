include(${SWIG_USE_FILE})
include_directories(${PYTHON_INCLUDE_PATH})
set(CMAKE_SWIG_FLAGS "")
set_property(SOURCE s2.i PROPERTY SWIG_FLAGS "-module" "pywraps2")
set_property(SOURCE s2.i PROPERTY CPLUSPLUS ON)
swig_add_module(pywraps2 python s2.i)
swig_link_libraries(pywraps2 ${PYTHON_LIBRARIES} s2)
enable_testing()
add_test(NAME pywraps2_test COMMAND
         ${PYTHON_EXECUTABLE}
	 "${PROJECT_SOURCE_DIR}/src/python/pywraps2_test.py")
set_property(TEST pywraps2_test PROPERTY ENVIRONMENT
             "PYTHONPATH=$ENV{PYTHONPATH}:${PROJECT_BINARY_DIR}/python")
install(TARGETS _pywraps2 DESTINATION share/python)
install(FILES "${PROJECT_BINARY_DIR}/python/pywraps2.py"
	DESTINATION share/python)
