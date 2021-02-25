# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(Python 3 REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python_INCLUDE_DIRS})

set_property(SOURCE pyresearch.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE pyresearch.i PROPERTY SWIG_FLAGS "-py3")
set_property(SOURCE pyresearch.i PROPERTY SWIH_MODULE_NAME pyresearch)
list(APPEND CMAKE_SWIG_FLAGS "-I${IResearch_INCLUDE_DIR}")

swig_add_library(pyresearch LANGUAGE python SOURCES pyresearch.i)
swig_link_libraries(pyresearch iresearch-python ${Python_LIBRARIES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.. ${IResearch_INCLUDE_DIR})

set(SETUP_PY_IN ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)
set(SETUP_PY_OUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py)

# Files to install with Python (Build-specific for MSVC)
if(MSVC)
set(PYTHON_INSTALL_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/pyresearch.py
  ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/_pyresearch.pyd
  ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/iresearch-python.dll
)
else()
set(PYTHON_INSTALL_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/pyresearch.py
  ${CMAKE_BINARY_DIR}/bin/_pyresearch.so
)
endif()
# Configure setup.py and copy to output directory
configure_file(${SETUP_PY_IN} ${SETUP_PY_OUT})

if(MSVC)
# on MSVC  we prepare setup file for each build type to make use of generator expressions
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/setup_$<CONFIG>.py INPUT ${SETUP_PY_OUT})
add_custom_target(pyresearch
  DEPENDS _pyresearch ${IResearch_TARGET_NAME}-static-allinone
  COMMAND ${Python_EXECUTABLE} ${CMAKE_BINARY_DIR}/setup_$<CONFIG>.py build_ext --inplace
)
add_custom_target(pyresearch-install
  DEPENDS pyresearch
  COMMAND ${Python_EXECUTABLE} ${CMAKE_BINARY_DIR}/setup_$<CONFIG>.py install
)
else()
add_custom_target(pyresearch
  DEPENDS _pyresearch ${IResearch_TARGET_NAME}-shared
  COMMAND ${Python_EXECUTABLE} ${SETUP_PY_OUT} build_ext --inplace
)
add_custom_target(pyresearch-install
  DEPENDS pyresearch
  COMMAND ${Python_EXECUTABLE} ${SETUP_PY_OUT} install
)
endif()
