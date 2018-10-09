# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH})

set_property(SOURCE pyresearch.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE pyresearch.i PROPERTY SWIG_FLAGS "-py3")
set_property(SOURCE pyresearch.i PROPERTY SWIH_MODULE_NAME pyresearch)
list(APPEND CMAKE_SWIG_FLAGS "-I${IResearch_INCLUDE_DIR}")

swig_add_library(pyresearch LANGUAGE python SOURCES pyresearch.i)
swig_link_libraries(pyresearch iresearch-python ${PYTHON_LIBRARIES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.. ${IResearch_INCLUDE_DIR})

# Files to install with Python
set(PYTHON_INSTALL_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/pyresearch.py
  ${CMAKE_BINARY_DIR}/bin/_pyresearch.so
)

# Configure setup.py and copy to output directory
set(SETUP_PY_IN ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)
set(SETUP_PY_OUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
configure_file(${SETUP_PY_IN} ${SETUP_PY_OUT})

add_custom_target(pyresearch
  DEPENDS _pyresearch ${IResearch_TARGET_NAME}-shared
  COMMAND python3 ${SETUP_PY_OUT} build_ext --inplace
)

add_custom_target(pyresearch-install
  DEPENDS pyresearch
  COMMAND python3 ${SETUP_PY_OUT} install
)
