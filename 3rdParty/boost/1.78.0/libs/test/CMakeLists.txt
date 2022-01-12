# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_test VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

# Dependencies (please retain formatting, one target per line, no punct.)

set(_boost_test_dependencies
  Boost::algorithm
  Boost::assert
  Boost::bind
  Boost::config
  Boost::core
  Boost::detail
  Boost::exception
  Boost::function
  Boost::io
  Boost::iterator
  Boost::mpl
  Boost::numeric_conversion
  Boost::optional
  Boost::preprocessor
  Boost::smart_ptr
  Boost::static_assert
  Boost::type_traits
  Boost::utility
)

# Compiled targets

function(boost_test_add_library name)

  add_library(boost_${name} ${ARGN})
  add_library(Boost::${name} ALIAS boost_${name})

  target_include_directories(boost_${name} PUBLIC include)
  target_link_libraries(boost_${name} PUBLIC ${_boost_test_dependencies})

  target_compile_definitions(boost_${name}
    PUBLIC BOOST_TEST_NO_LIB
    # Source files already define BOOST_TEST_SOURCE
    # PRIVATE BOOST_TEST_SOURCE
  )

  if(BUILD_SHARED_LIBS)
    target_compile_definitions(boost_${name} PUBLIC BOOST_TEST_DYN_LINK)
  else()
    target_compile_definitions(boost_${name} PUBLIC BOOST_TEST_STATIC_LINK)
  endif()

endfunction()

boost_test_add_library(prg_exec_monitor
  src/cpp_main.cpp
  src/debug.cpp
  src/execution_monitor.cpp
)

set(SOURCES
  src/compiler_log_formatter.cpp
  src/debug.cpp
  src/decorator.cpp
  src/execution_monitor.cpp
  src/framework.cpp
  src/junit_log_formatter.cpp
  src/plain_report_formatter.cpp
  src/progress_monitor.cpp
  src/results_collector.cpp
  src/results_reporter.cpp
  src/test_framework_init_observer.cpp
  src/test_tools.cpp
  src/test_tree.cpp
  src/unit_test_log.cpp
  src/unit_test_main.cpp
  src/unit_test_monitor.cpp
  src/unit_test_parameters.cpp
  src/xml_log_formatter.cpp
  src/xml_report_formatter.cpp
)

boost_test_add_library(test_exec_monitor STATIC ${SOURCES} src/test_main.cpp)
boost_test_add_library(unit_test_framework ${SOURCES})

# Header-only targets

function(boost_test_add_included_library name)

  add_library(boost_${name} INTERFACE)
  add_library(Boost::${name} ALIAS boost_${name})

  target_include_directories(boost_${name} INTERFACE include)
  target_link_libraries(boost_${name} INTERFACE ${_boost_test_dependencies})

endfunction()

boost_test_add_included_library(included_prg_exec_monitor)
boost_test_add_included_library(included_test_exec_monitor)
boost_test_add_included_library(included_unit_test_framework)

# Installation

if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)

  boost_install(
    TARGETS
      boost_prg_exec_monitor boost_test_exec_monitor boost_unit_test_framework
      boost_included_prg_exec_monitor boost_included_test_exec_monitor boost_included_unit_test_framework
    VERSION ${BOOST_SUPERPROJECT_VERSION}
    HEADER_DIRECTORY include
  )

endif()
