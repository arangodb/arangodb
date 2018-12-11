include(${SWIG_USE_FILE})
include_directories(${PYTHON_INCLUDE_PATH})

set(CMAKE_SWIG_FLAGS "")
set_property(SOURCE s2.i PROPERTY SWIG_FLAGS "-module" "pywraps2")
set_property(SOURCE s2.i PROPERTY CPLUSPLUS ON)

# Starting in 3.8, swig_add_module is deprecated in favor of swig_add_library.
if (${CMAKE_VERSION} VERSION_LESS "3.8.0")
    swig_add_module(pywraps2 python s2.i)
else()
    swig_add_library(pywraps2 LANGUAGE python SOURCES s2.i)
endif()

swig_link_libraries(pywraps2 ${PYTHON_LIBRARIES} s2)
enable_testing()
add_test(NAME pywraps2_test COMMAND
         ${PYTHON_EXECUTABLE}
	 "${PROJECT_SOURCE_DIR}/src/python/pywraps2_test.py")
set_property(TEST pywraps2_test PROPERTY ENVIRONMENT
             "PYTHONPATH=$ENV{PYTHONPATH}:${PROJECT_BINARY_DIR}/python")

execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c "if True:
                  from distutils import sysconfig as sc;
                  print(sc.get_python_lib(prefix='', plat_specific=True))"
                OUTPUT_VARIABLE PYTHON_SITE
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Install the wrapper.
install(TARGETS _pywraps2 DESTINATION ${PYTHON_SITE})
install(FILES "${PROJECT_BINARY_DIR}/python/pywraps2.py"
        DESTINATION ${PYTHON_SITE})
