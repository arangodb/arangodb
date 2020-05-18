# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

# This is copied from:
#   https://github.com/vector-of-bool/CMakeCM/blob/master/modules/FindFilesystem.cmake

#[=======================================================================[.rst:

FindFilesystem
##############

This module supports the C++17 standard library's filesystem utilities. Use the
:imp-target:`std::filesystem` imported target to

Options
*******

The ``COMPONENTS`` argument to this module supports the following values:

.. find-component:: Experimental
    :name: fs.Experimental

    Allows the module to find the "experimental" Filesystem TS version of the
    Filesystem library. This is the library that should be used with the
    ``std::experimental::filesystem`` namespace.

.. find-component:: Final
    :name: fs.Final

    Finds the final C++17 standard version of the filesystem library.

If no components are provided, behaves as if the
:find-component:`fs.Final` component was specified.

If both :find-component:`fs.Experimental` and :find-component:`fs.Final` are
provided, first looks for ``Final``, and falls back to ``Experimental`` in case
of failure. If ``Final`` is found, :imp-target:`std::filesystem` and all
:ref:`variables <fs.variables>` will refer to the ``Final`` version.


Imported Targets
****************

.. imp-target:: std::filesystem

    The ``std::filesystem`` imported target is defined when any requested
    version of the C++ filesystem library has been found, whether it is
    *Experimental* or *Final*.

    If no version of the filesystem library is available, this target will not
    be defined.

    .. note::
        This target has ``cxx_std_17`` as an ``INTERFACE``
        :ref:`compile language standard feature <req-lang-standards>`. Linking
        to this target will automatically enable C++17 if no later standard
        version is already required on the linking target.


.. _fs.variables:

Variables
*********

.. variable:: CXX_FILESYSTEM_IS_EXPERIMENTAL

    Set to ``TRUE`` when the :find-component:`fs.Experimental` version of C++
    filesystem library was found, otherwise ``FALSE``.

.. variable:: CXX_FILESYSTEM_HAVE_FS

    Set to ``TRUE`` when a filesystem header was found.

.. variable:: CXX_FILESYSTEM_HEADER

    Set to either ``filesystem`` or ``experimental/filesystem`` depending on
    whether :find-component:`fs.Final` or :find-component:`fs.Experimental` was
    found.

.. variable:: CXX_FILESYSTEM_NAMESPACE

    Set to either ``std::filesystem`` or ``std::experimental::filesystem``
    depending on whether :find-component:`fs.Final` or
    :find-component:`fs.Experimental` was found.


Examples
********

Using `find_package(Filesystem)` with no component arguments:

.. code-block:: cmake

    find_package(Filesystem REQUIRED)

    add_executable(my-program main.cpp)
    target_link_libraries(my-program PRIVATE std::filesystem)


#]=======================================================================]


cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

if(TARGET std::filesystem)
    # This module has already been processed. Don't do it again.
    return()
endif()

include(CMakePushCheckState)
include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)

cmake_push_check_state()

set(CMAKE_REQUIRED_QUIET ${Filesystem_FIND_QUIETLY})

# All of our tests required C++17 or later
set(CMAKE_CXX_STANDARD 17)

# Normalize and check the component list we were given
set(want_components ${Filesystem_FIND_COMPONENTS})
if(Filesystem_FIND_COMPONENTS STREQUAL "")
    set(want_components Final)
endif()

# Warn on any unrecognized components
set(extra_components ${want_components})
list(REMOVE_ITEM extra_components Final Experimental)
foreach(component IN LISTS extra_components)
    message(WARNING "Extraneous find_package component for Filesystem: ${component}")
endforeach()

# Detect which of Experimental and Final we should look for
set(find_EXPERIMENTAL TRUE)
set(find_FINAL TRUE)
if(NOT "Final" IN_LIST want_components)
    set(find_FINAL FALSE)
endif()
if(NOT "Experimental" IN_LIST want_components)
    set(find_EXPERIMENTAL FALSE)
endif()

set(_fs_have_fs FALSE)
set(_fs_header_FINAL "filesystem")
set(_fs_namespace_FINAL "std::filesystem")

set(_fs_header_EXPERIMENTAL "experimental/filesystem")
set(_fs_namespace_EXPERIMENTAL "std::experimental::filesystem")

set(_fs_libs_NO_LINK)
set(_fs_libs_STDCPPFS           "-lstdc++fs")
set(_fs_libs_CPPFS              "-lc++fs")
set(_fs_libs_CPPFS_EXPERIMENTAL "-lc++fs" "-lc++experimental")

set(_found FALSE)

foreach(_current_header "FINAL" "EXPERIMENTAL")
    if(NOT find_${_current_header})
        continue()
    endif()

    check_include_file_cxx(${_fs_header_${_current_header}} _CXX_FILESYSTEM_HAVE_${_current_header}_HEADER)
    mark_as_advanced(_CXX_FILESYSTEM_HAVE_${_current_header}_HEADER)

    if(_CXX_FILESYSTEM_HAVE_${_current_header}_HEADER)
        set(_fs_have_fs TRUE)
        set(cxx_filesystem_header ${_fs_header_${_current_header}})
        set(cxx_filesystem_namespace ${_fs_namespace_${_current_header}})

        # We have some filesystem library available. Do link checks
        string(CONFIGURE [[
            #include <@cxx_filesystem_header@>

            int main() {
                auto cwd = @cxx_filesystem_namespace@::current_path();
                return static_cast<int>(cwd.string().size());
            }
        ]] code @ONLY)

        set(prev_libraries ${CMAKE_REQUIRED_LIBRARIES})
        foreach(_current_lib "NO_LINK" "STDCPPFS" "CPPFS" "CXXFS_EXP")
            set(CMAKE_REQUIRED_LIBRARIES ${prev_libraries} ${_fs_libs_${_current_lib}})
            check_cxx_source_compiles("${code}" CXX_FILESYSTEM_${_current_header}_${_current_lib}_NEEDED)
            if(CXX_FILESYSTEM_${_current_header}_${_current_lib}_NEEDED)
                set(_fs_use_header "${_current_header}")
                set(_fs_use_lib "${_current_lib}")
                set(_fs_can_link TRUE)
                break()
            endif()
        endforeach()
        set(CMAKE_REQUIRED_LIBRARIES {prev_libraries})

        if(_fs_can_link)
            add_library(std::filesystem INTERFACE IMPORTED)
            target_compile_features(std::filesystem INTERFACE cxx_std_17)
            target_link_libraries(std::filesystem INTERFACE ${_fs_libs_${_fs_use_lib}})
            set(_found TRUE)
            break()
        endif()

    endif(_CXX_FILESYSTEM_HAVE_${_current_header}_HEADER)
endforeach(_current_header "FINAL" "EXPERIMENTAL")

cmake_pop_check_state()

set(CXX_FILESYSTEM_HEADER ${_fs_header_${_fs_use_header}} CACHE STRING "The header that should be included to obtain the filesystem APIs")
set(CXX_FILESYSTEM_NAMESPACE ${_fs_namespace_${_fs_use_header}} CACHE STRING "The C++ namespace that contains the filesystem APIs")
set(CXX_FILESYSTEM_HAVE_FS ${_fs_have_fs} CACHE BOOL "TRUE if we have the C++ filesystem headers")
set(Filesystem_FOUND ${_found} CACHE BOOL "TRUE if we can compile and link a program using std::filesystem" FORCE)

if(Filesystem_FIND_REQUIRED AND NOT Filesystem_FOUND)
    message(FATAL_ERROR "Cannot Compile simple program using std::filesystem")
endif()
