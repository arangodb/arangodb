# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Filesystem is currently experimental at best
#       and the interface is likely to change in the future

cmake_minimum_required( VERSION 3.5 )
project( BoostFilesystem )

add_library( boost_filesystem
    src/codecvt_error_category.cpp
    src/operations.cpp
    src/path.cpp
    src/path_traits.cpp
    src/portability.cpp
    src/unique_path.cpp
    src/utf8_codecvt_facet.cpp
    src/windows_file_codecvt.cpp
)

add_library( Boost::filesystem ALIAS boost_filesystem )

target_include_directories( boost_filesystem PUBLIC include )

target_compile_definitions( boost_filesystem
    PUBLIC
        # NOTE:
        # We deactivate autolinking, because cmake based builds don't need it
        # and we don't implement name mangling for the library file anyway.
        # Ususally the parent CMakeLists.txt file should already have globally defined BOOST_ALL_NO_LIB
        BOOST_FILESYSTEM_NO_LIB
        $<$<STREQUAL:$<TARGET_PROPERTY:boost_filesystem,TYPE>,SHARED_LIBRARY>:BOOST_FILESYSTEM_DYN_LINK=1>
        $<$<STREQUAL:$<TARGET_PROPERTY:boost_filesystem,TYPE>,STATIC_LIBRARY>:BOOST_FILESYSTEM_STATIC_LINK=1>
)

target_link_libraries( boost_filesystem
    PUBLIC
        Boost::assert
        Boost::config
        Boost::container_hash
        Boost::core
        Boost::detail
        Boost::io
        Boost::iterator
        Boost::smart_ptr
        Boost::system
        Boost::type_traits
)

target_link_libraries( boost_filesystem
    PRIVATE
        Boost::winapi
)
