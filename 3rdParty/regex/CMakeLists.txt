# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                    COMMON DEFINES
## -----------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.6)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE string "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

################################################################################
### @brief regex library
################################################################################

project(regex)

set(LIB_REGEX regex)

include_directories(.)

if (MSVC)
  foreach (flag_var
	   CMAKE_C_FLAGS
	   CMAKE_C_FLAGS_DEBUG
	   CMAKE_C_FLAGS_RELEASE
	   CMAKE_C_FLAGS_MINSIZEREL
	   CMAKE_C_FLAGS_RELWITHDEBINFO
	   CMAKE_CXX_FLAGS
	   CMAKE_CXX_FLAGS_DEBUG
	   CMAKE_CXX_FLAGS_RELEASE
	   CMAKE_CXX_FLAGS_MINSIZEREL
	   CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if (${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MDd?" "" ${flag_var} "${${flag_var}}")
    endif ()

    if (flag_var MATCHES "DEBUG")
      set(${flag_var} "${${flag_var}} /MTd")
    else ()
      set(${flag_var} "${${flag_var}} /MT")
    endif ()

    message(STATUS "${flag_var}=${${flag_var}}")
  endforeach ()
endif ()

## -----------------------------------------------------------------------------
## --SECTION--                                                         LIBRARIES
## -----------------------------------------------------------------------------

################################################################################
### @brief LIB_REGEX
################################################################################

add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
add_definitions("-DREGEX_STATIC=1")

add_library(
    ${LIB_REGEX}
    STATIC
    regex.c
)

set(REGEX_INCLUDE_DIR
  ${PROJECT_SOURCE_DIR}
  CACHE INTERNAL
  "${PROJECT_NAME}: Include Directories"
  FORCE
)
