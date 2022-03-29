# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_stacktrace VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

function(stacktrace_add_library suffix opt libs defs)

  if(NOT opt)
    return()
  endif()

  add_library(boost_stacktrace_${suffix}
    src/${suffix}.cpp
  )

  add_library(Boost::stacktrace_${suffix} ALIAS boost_stacktrace_${suffix})

  target_include_directories(boost_stacktrace_${suffix} PUBLIC include)

  target_link_libraries(boost_stacktrace_${suffix}
    PUBLIC
      Boost::array
      Boost::config
      Boost::container_hash
      Boost::core
      Boost::predef
      Boost::static_assert
      Boost::type_traits
      Boost::winapi
    PRIVATE
      ${libs}
  )

  target_compile_definitions(boost_stacktrace_${suffix}
    PUBLIC BOOST_STACKTRACE_NO_LIB
    PRIVATE BOOST_STACKTRACE_SOURCE ${defs}
  )

  if(BUILD_SHARED_LIBS)
    target_compile_definitions(boost_stacktrace_${suffix} PUBLIC BOOST_STACKTRACE_DYN_LINK)
  else()
    target_compile_definitions(boost_stacktrace_${suffix} PUBLIC BOOST_STACKTRACE_STATIC_LINK)
  endif()

  if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
    boost_install(TARGETS boost_stacktrace_${suffix} VERSION ${BOOST_SUPERPROJECT_VERSION} HEADER_DIRECTORY include)
  endif()

endfunction()

include(CheckCXXSourceCompiles)

function(stacktrace_check var source incs libs defs)

  set(CMAKE_REQUIRED_INCLUDES "${incs}")
  list(APPEND CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/build")
  set(CMAKE_REQUIRED_LIBRARIES "${libs}")
  set(CMAKE_REQUIRED_DEFINITIONS "${defs}")
  check_cxx_source_compiles("#include \"${source}\"" ${var})
  set(${var} ${${var}} PARENT_SCOPE)

endfunction()

stacktrace_check(BOOST_STACKTRACE_HAS_BACKTRACE has_backtrace.cpp "" "backtrace" "")

set(_default_addr2line ON)
if(WIN32 AND NOT CMAKE_CXX_PLATFORM_ID MATCHES "Cygwin")
  set(_default_addr2line OFF)
endif()

stacktrace_check(BOOST_STACKTRACE_HAS_WINDBG has_windbg.cpp "" "dbgeng;ole32" "")
stacktrace_check(BOOST_STACKTRACE_HAS_WINDBG_CACHED has_windbg_cached.cpp "${CMAKE_CURRENT_SOURCE_DIR}/../config/include" "dbgeng;ole32" "")

option(BOOST_STACKTRACE_ENABLE_NOOP "Boost.Stacktrace: build boost_stacktrace_noop" ON)
option(BOOST_STACKTRACE_ENABLE_BACKTRACE "Boost.Stacktrace: build boost_stacktrace_backtrace" ${BOOST_STACKTRACE_HAS_BACKTRACE})
option(BOOST_STACKTRACE_ENABLE_ADDR2LINE "Boost.Stacktrace: build boost_stacktrace_addr2line" ${_default_addr2line})
option(BOOST_STACKTRACE_ENABLE_BASIC "Boost.Stacktrace: build boost_stacktrace_basic" ON)
option(BOOST_STACKTRACE_ENABLE_WINDBG "Boost.Stacktrace: build boost_stacktrace_windbg" ${BOOST_STACKTRACE_HAS_WINDBG})
option(BOOST_STACKTRACE_ENABLE_WINDBG_CACHED "Boost.Stacktrace: build boost_stacktrace_windbg_cached" ${BOOST_STACKTRACE_HAS_WINDBG_CACHED})

unset(_default_addr2line)

message(STATUS "Boost.Stacktrace: "
  "noop ${BOOST_STACKTRACE_ENABLE_NOOP}, "
  "backtrace ${BOOST_STACKTRACE_ENABLE_BACKTRACE}, "
  "addr2line ${BOOST_STACKTRACE_ENABLE_ADDR2LINE}, "
  "basic ${BOOST_STACKTRACE_ENABLE_BASIC}, "
  "windbg ${BOOST_STACKTRACE_ENABLE_WINDBG}, "
  "windbg_cached ${BOOST_STACKTRACE_ENABLE_WINDBG_CACHED}"
)

stacktrace_add_library(noop ${BOOST_STACKTRACE_ENABLE_NOOP} "" "")
stacktrace_add_library(backtrace ${BOOST_STACKTRACE_ENABLE_BACKTRACE} "backtrace" "")
stacktrace_add_library(addr2line ${BOOST_STACKTRACE_ENABLE_ADDR2LINE} "" "")
stacktrace_add_library(basic ${BOOST_STACKTRACE_ENABLE_BASIC} "" "")
stacktrace_add_library(windbg ${BOOST_STACKTRACE_ENABLE_WINDBG} "dbgeng;ole32" "_GNU_SOURCE=1")
stacktrace_add_library(windbg_cached ${BOOST_STACKTRACE_ENABLE_WINDBG_CACHED} "dbgeng;ole32" "_GNU_SOURCE=1")

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
