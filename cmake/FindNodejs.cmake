# The MIT License (MIT)
# Copyright © 2014-2019 Intel Corporation and others
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# Macro to add directory to NODEJS_INCLUDE_DIRS if it exists and is not /usr/include
macro(add_include_dir dir)
    if (IS_DIRECTORY ${dir} AND NOT ${dir} STREQUAL "/usr/include")
        set(NODEJS_INCLUDE_DIRS ${NODEJS_INCLUDE_DIRS} ${dir})
    endif()
endmacro()

find_program (NODEJS_EXECUTABLE NAMES node nodejs
    HINTS
    $ENV{NODE_DIR}
    PATH_SUFFIXES bin
    DOC "Node.js interpreter")

include (FindPackageHandleStandardArgs)

# If compat-libuv package exists, it must be at start of include path
find_path (UV_ROOT_DIR "uv.h" PATHS /usr/include/compat-libuv010 NO_DEFAULT_PATH)
if (UV_ROOT_DIR)
    # set (NODEJS_INCLUDE_DIRS ${UV_ROOT_DIR})
    add_include_dir(${UV_ROOT_DIR})
endif()

# Now look for node. Flag an error if not found
find_path (NODE_ROOT_DIR
    NAMES node.h src/node.h
    PATH_SUFFIXES node node4 node5 node6 node7 node8 nodejs
    PATHS /usr/include /usr/local/include)

if (NODE_ROOT_DIR)
    add_include_dir(${NODE_ROOT_DIR})
    add_include_dir(${NODE_ROOT_DIR}/deps/uv/include)
    add_include_dir(${NODE_ROOT_DIR}/deps/v8/include)
    add_include_dir(${NODE_ROOT_DIR}/include/deps/uv/include)
    add_include_dir(${NODE_ROOT_DIR}/include/deps/v8/include)
    add_include_dir(${NODE_ROOT_DIR}/include/node)
    add_include_dir(${NODE_ROOT_DIR}/include/src)
    add_include_dir(${NODE_ROOT_DIR}/src)
else()
    unset(NODEJS_INCLUDE_DIRS)
    message(ERROR " - node.h not found")
endif()

# Check that v8.h is in NODEJS_INCLUDE_DIRS
find_path (V8_ROOT_DIR "v8.h" PATHS ${NODEJS_INCLUDE_DIRS})
if (NOT V8_ROOT_DIR)
    unset(NODEJS_INCLUDE_DIRS)
    message(ERROR " - v8.h not found")
endif()

# Check that uv.h is in NODEJS_INCLUDE_DIRS
find_path (UV_ROOT_DIR "uv.h" PATHS ${NODEJS_INCLUDE_DIRS})
if (NOT UV_ROOT_DIR)
    unset(NODEJS_INCLUDE_DIRS)
    message(ERROR " - uv.h not found")
endif()

if (NODEJS_EXECUTABLE)
    execute_process(COMMAND ${NODEJS_EXECUTABLE} --version
        OUTPUT_VARIABLE _VERSION
        RESULT_VARIABLE _NODE_VERSION_RESULT)
    execute_process(COMMAND ${NODEJS_EXECUTABLE} -e "console.log(process.versions.v8)"
        OUTPUT_VARIABLE _V8_VERSION
        RESULT_VARIABLE _V8_RESULT)
    if (NOT _NODE_VERSION_RESULT AND NOT _V8_RESULT)
        string (REPLACE "v" "" NODE_VERSION_STRING "${_VERSION}")
        string (REPLACE "." ";" _VERSION_LIST "${NODE_VERSION_STRING}")
        list (GET _VERSION_LIST 0 NODE_VERSION_MAJOR)
        list (GET _VERSION_LIST 1 NODE_VERSION_MINOR)
        list (GET _VERSION_LIST 2 NODE_VERSION_PATCH)
        set (V8_VERSION_STRING ${_V8_VERSION})
        string (REPLACE "." ";" _V8_VERSION_LIST "${_V8_VERSION}")
        string (REPLACE "." "" V8_DEFINE_STRING "${_V8_VERSION}")
        string (STRIP ${V8_DEFINE_STRING} V8_DEFINE_STRING)
        list (GET _V8_VERSION_LIST 0 V8_VERSION_MAJOR)
        list (GET _V8_VERSION_LIST 1 V8_VERSION_MINOR)
        list (GET _V8_VERSION_LIST 2 V8_VERSION_PATCH)
        # we end up with a nasty newline so strip everything that isn't a number
        string (REGEX MATCH "^[0-9]*" V8_VERSION_PATCH ${V8_VERSION_PATCH})
    else ()
        set (NODE_VERSION_STRING "0.10.30")
        set (NODE_VERSION_MAJOR "0")
        set (NODE_VERSION_MINOR "10")
        set (NODE_VERSION_PATCH "30")
        set (V8_VERSION_MAJOR "3")
        set (V8_VERSION_MINOR "28")
        set (V8_VERSION_PATCH "72")
        set (V8_VERSION_STRING "3.28.72")
        message (STATUS "defaulted to node 0.10.30")
    endif ()
    string (REGEX REPLACE "\n" "" NODE_VERSION_STRING ${NODE_VERSION_STRING})
    string (REGEX REPLACE "\n" "" V8_VERSION_STRING ${V8_VERSION_STRING})

    mark_as_advanced (NODEJS_EXECUTABLE)

    find_package_handle_standard_args (Nodejs
        REQUIRED_VARS NODEJS_EXECUTABLE NODEJS_INCLUDE_DIRS
        VERSION_VAR NODE_VERSION_STRING)
    message(STATUS "Found v8: ${V8_ROOT_DIR}/v8.h (found version \"${V8_VERSION_STRING}\")")
endif ()