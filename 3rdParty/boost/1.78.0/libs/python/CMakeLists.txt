# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.14...3.20)

project(boost_python VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

find_package(Python REQUIRED COMPONENTS Development OPTIONAL_COMPONENTS NumPy)

if(Python_NumPy_FOUND)
  message(STATUS "Boost.Python: using Python ${Python_VERSION} with NumPy at ${Python_NumPy_INCLUDE_DIRS}")
else()
  message(STATUS "Boost.Python: using Python ${Python_VERSION} without NumPy")
endif()

# boost_pythonXY

set(_pyver ${Python_VERSION_MAJOR}${Python_VERSION_MINOR})
set(_boost_python boost_python${_pyver})

add_library(${_boost_python}
  src/dict.cpp
  src/errors.cpp
  src/exec.cpp
  src/import.cpp
  src/list.cpp
  src/long.cpp
  src/module.cpp
  src/object_operators.cpp
  src/object_protocol.cpp
  src/slice.cpp
  src/str.cpp
  src/tuple.cpp
  src/wrapper.cpp
  src/converter/from_python.cpp
  src/converter/registry.cpp
  src/converter/type_id.cpp
  src/converter/builtin_converters.cpp
  src/converter/arg_to_python_base.cpp
  src/object/enum.cpp
  src/object/class.cpp
  src/object/function.cpp
  src/object/inheritance.cpp
  src/object/life_support.cpp
  src/object/pickle_support.cpp
  src/object/iterator.cpp
  src/object/stl_iterator.cpp
  src/object_protocol.cpp
  src/object_operators.cpp
  src/object/function_doc_signature.cpp
)

add_library(Boost::python${_pyver} ALIAS ${_boost_python})

target_include_directories(${_boost_python} PUBLIC include)

target_link_libraries(${_boost_python}
  PUBLIC
    Boost::align
    Boost::bind
    Boost::config
    Boost::conversion
    Boost::core
    Boost::detail
    Boost::foreach
    Boost::function
    Boost::iterator
    Boost::lexical_cast
    Boost::mpl
    Boost::numeric_conversion
    Boost::preprocessor
    Boost::smart_ptr
    Boost::static_assert
    Boost::tuple
    Boost::type_traits
    Boost::utility

    Python::Module

  PRIVATE
    Boost::graph
    Boost::integer
    Boost::property_map
)

target_compile_definitions(${_boost_python}
  PUBLIC BOOST_PYTHON_NO_LIB
  PRIVATE BOOST_PYTHON_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(${_boost_python} PUBLIC BOOST_PYTHON_DYN_LINK)
else()
  target_compile_definitions(${_boost_python} PUBLIC BOOST_PYTHON_STATIC_LINK BOOST_PYTHON_STATIC_LIB)
endif()

# Boost::python alias

add_library(boost_python INTERFACE)
add_library(Boost::python ALIAS boost_python)
target_link_libraries(boost_python INTERFACE Boost::python${_pyver})

# Installation

if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
  boost_install(TARGETS ${_boost_python} boost_python VERSION ${BOOST_SUPERPROJECT_VERSION} HEADER_DIRECTORY include)
endif()

if(Python_NumPy_FOUND)

# boost_numpyXY

set(_boost_numpy boost_numpy${_pyver})

add_library(${_boost_numpy}
  src/numpy/dtype.cpp
  src/numpy/matrix.cpp
  src/numpy/ndarray.cpp
  src/numpy/numpy.cpp
  src/numpy/scalars.cpp
  src/numpy/ufunc.cpp
)

add_library(Boost::numpy${_pyver} ALIAS ${_boost_numpy})

target_include_directories(${_boost_numpy} PUBLIC include)

target_link_libraries(${_boost_numpy}
  PUBLIC
    Boost::config
    Boost::core
    Boost::detail
    Boost::mpl
    Boost::python
    Boost::smart_ptr

    Python::NumPy
)

target_compile_definitions(${_boost_numpy}
  PUBLIC BOOST_NUMPY_NO_LIB
  PRIVATE BOOST_NUMPY_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(${_boost_numpy} PUBLIC BOOST_NUMPY_DYN_LINK)
else()
  target_compile_definitions(${_boost_numpy} PUBLIC BOOST_NUMPY_STATIC_LINK BOOST_NUMPY_STATIC_LIB)
endif()

# Boost::numpy alias

add_library(boost_numpy INTERFACE)
add_library(Boost::numpy ALIAS boost_numpy)
target_link_libraries(boost_numpy INTERFACE Boost::numpy${_pyver})

# Installation

if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
  boost_install(TARGETS ${_boost_numpy} boost_numpy VERSION ${BOOST_SUPERPROJECT_VERSION})
endif()

endif()

unset(_pyver)
unset(_boost_python)
unset(_boost_numpy)

# Testing

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
