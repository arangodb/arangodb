# Copyright - 2019 - Jan Christoph Uhde <Jan@UhdeJC.com>

cmake_minimum_required(VERSION 3.8)

if(NOT EXT_GIT_VERSION_OUT_FILE)
    set(EXT_GIT_VERSION_OUT_FILE "${CMAKE_CURRENT_LIST_DIR}/version.h")
endif()

execute_process(
    COMMAND git rev-parse HEAD
    OUTPUT_VARIABLE GIT_REV
    ERROR_QUIET
)

if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV    "not available")
    set(GIT_BRANCH "not available")
    message("could not get git revision:(")
else()
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH
    )
    string(STRIP "${GIT_REV}"    GIT_REV)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
    message("found git revision: " ${GIT_REV})
    message("found git branch:   " ${GIT_BRANCH})
endif()

set(VERSION_INFO "
#pragma once
constexpr char const* GIT_REV=\"${GIT_REV}\";
constexpr char const* GIT_BRANCH=\"${GIT_BRANCH}\";
"
)

if(EXISTS ${EXT_GIT_VERSION_OUT_FILE})
    file(READ ${EXT_GIT_VERSION_OUT_FILE} OLD_VERSION_INFO)
else()
    set(OLD_VERSION_INFO "")
endif()

if (NOT "${VERSION_INFO}" STREQUAL "${OLD_VERSION_INFO}")
    file(WRITE ${EXT_GIT_VERSION_OUT_FILE} "${VERSION_INFO}")
endif()
