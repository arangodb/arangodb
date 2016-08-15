# Copyright Louis Dionne 2013-2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

find_package(Doxygen)
if (NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen was not found; the 'doc' target will be unavailable.")
    return()
endif()

configure_file(Doxyfile.in Doxyfile @ONLY)

add_custom_target(doc
    COMMAND ${CMAKE_COMMAND} -E remove_directory html
    COMMAND ! ${DOXYGEN_EXECUTABLE} Doxyfile 2>&1 | grep -E "warning|error"
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
)
