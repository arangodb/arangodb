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

## -----------------------------------------------------------------------------
## --SECTION--                                                         LIBRARIES
## -----------------------------------------------------------------------------

################################################################################
### @brief LIB_REGEX
################################################################################

include_directories(.)

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
)
