# Copyright 2018 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5)
# NOTE: Individual boost cmake files might require a higher cmake version

project(boost LANGUAGES CXX)

#=== options ===

# Some libraries' cmake files don't work well with this cmake file, e.g. because
# - they are generally not designed to support the add_subdirectory workflow at all
# - they define targets with conflicting names (e.g. check)
# - require some additional (internal or external) dependencies
#
# Those libraries can be excluded here
set(BOOST_RATIO_IGNORE_LIBS callable_traits;hof;compute;gil;hana;yap;safe_numerics;beast CACHE STRING "List of libraries that will be excluded from cmake build")


#~~~ options ~~~
message(STATUS "[Boost] Excluded libs (BOOST_RATIO_IGNORE_LIBS): ${BOOST_RATIO_IGNORE_LIBS}")

# cmake doesn't require autolinking and currently most cmake files don't produce
# name mangled libraries anyway
add_definitions(-DBOOST_ALL_NO_LIB)
enable_testing()

# Detect and process all CMakeLists files that reside in the root folder of a library
file(GLOB boost_libs_with_cmake_files ../../../../libs/*/CMakeLists.txt)

foreach(cmake_file IN LISTS boost_libs_with_cmake_files)

    get_filename_component(dir ${cmake_file} DIRECTORY)
    get_filename_component(lib_name ${dir} NAME)
    if(NOT lib_name IN_LIST BOOST_RATIO_IGNORE_LIBS)
        add_subdirectory(${dir} ${lib_name})
    endif()

endforeach()