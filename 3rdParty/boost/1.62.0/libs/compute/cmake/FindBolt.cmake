############################################################################                                                                                     
#   © 2012,2014 Advanced Micro Devices, Inc. All rights reserved.                                     
#                                                                                    
#   Licensed under the Apache License, Version 2.0 (the "License");   
#   you may not use this file except in compliance with the License.                 
#   You may obtain a copy of the License at                                          
#                                                                                    
#       http://www.apache.org/licenses/LICENSE-2.0                      
#                                                                                    
#   Unless required by applicable law or agreed to in writing, software              
#   distributed under the License is distributed on an "AS IS" BASIS,              
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.         
#   See the License for the specific language governing permissions and              
#   limitations under the License.                                                   

############################################################################                                                                                     

# Locate an BOLT implementation.
#
# Defines the following variables:
#
#   BOLT_FOUND - Found an Bolt imlementation
#
# Also defines the library variables below as normal
# variables.
#
#   BOLT_LIBRARIES - These contain debug/optimized keywords when a debugging library is found
#   BOLT_INCLUDE_DIRS - All relevant Bolt include directories
#
# Accepts the following variables as input:
#
#   BOLT_ROOT - (as a CMake or environment variable)
#                The root directory of an BOLT installation
#
#   FIND_LIBRARY_USE_LIB64_PATHS - Global property that controls whether FindBOLT should search for 
#                              64bit or 32bit libs
#
#-----------------------
# Example Usage:
#
#    find_package(BOLT REQUIRED)
#    include_directories(${BOLT_INCLUDE_DIRS})
#
#    add_executable(foo foo.cc)
#    target_link_libraries(foo ${BOLT_LIBRARIES})
#
#-----------------------

# This module helps to use BOLT_FIND_COMPONENTS, BOLT_FIND_REQUIRED, BOLT_FIND_QUIETLY
include( FindPackageHandleStandardArgs )

# Search for 64bit libs if FIND_LIBRARY_USE_LIB64_PATHS is set to true in the global environment, 32bit libs else
get_property( LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS )

# Debug print statements
#message( "BOLT_LIBRARY_PATH_SUFFIXES: ${BOLT_LIBRARY_PATH_SUFFIXES}" )
#message( "ENV{BOLT_ROOT}: $ENV{BOLT_ROOT}" )
#message( "BOLT_FIND_COMPONENTS: ${BOLT_FIND_COMPONENTS}" )
#message( "BOLT_FIND_REQUIRED: ${BOLT_FIND_REQUIRED}" )

# Set the component to find if the user does not specify explicitely
if( NOT BOLT_FIND_COMPONENTS )
    set( BOLT_FIND_COMPONENTS CL )
endif( )
if(WIN32)
if( MSVC_VERSION VERSION_LESS 1600 )
    set( myMSVCVer "vc90" )
elseif( MSVC_VERSION VERSION_LESS 1700 )
    set( myMSVCVer "vc100" )
elseif( MSVC_VERSION VERSION_LESS 1800 )
    set( myMSVCVer "vc110" )
else()
    set( myMSVCVer "vc120" )
endif( )
else()
    set( myMSVCVer "gcc" )
endif()

if(WIN32)
 set( BoltLibName "clBolt.runtime.${myMSVCVer}") 
 set( LIB_EXT "lib")    
else()
 set( BoltLibName "libclBolt.runtime.${myMSVCVer}")
 set( LIB_EXT "a")  
endif()

# Eventually, Bolt may support multiple backends, but for now it only supports CL
list( FIND BOLT_FIND_COMPONENTS CL find_CL )
if( NOT find_CL EQUAL -1 )
    set( BOLT_LIBNAME_BASE ${BoltLibName} )
endif( )

if( NOT find_CL EQUAL -1 )
    # Find and set the location of main BOLT static lib file
    find_library( BOLT_LIBRARY_STATIC_RELEASE
        NAMES ${BOLT_LIBNAME_BASE}.${LIB_EXT}
        HINTS
            ${BOLT_ROOT}
            ENV BOLT_ROOT
        DOC "BOLT static library path"
        PATH_SUFFIXES lib
    )
    mark_as_advanced( BOLT_LIBRARY_STATIC_RELEASE )

    # Find and set the location of main BOLT static lib file
    find_library( BOLT_LIBRARY_STATIC_DEBUG
        NAMES ${BOLT_LIBNAME_BASE}.debug.${LIB_EXT}
        HINTS
            ${BOLT_ROOT}
            ENV BOLT_ROOT
        DOC "BOLT static library path"
        PATH_SUFFIXES lib
    )
    mark_as_advanced( BOLT_LIBRARY_STATIC_DEBUG )
    
    if( BOLT_LIBRARY_STATIC_RELEASE )
        set( BOLT_LIBRARY_STATIC optimized ${BOLT_LIBRARY_STATIC_RELEASE} )
    else( )
        set( BOLT_LIBRARY_STATIC "" )
        message( "${BOLT_LIBNAME_BASE}.${LIB_EXT}: Release static bolt library not found" )
    endif( )

    if( BOLT_LIBRARY_STATIC_DEBUG )
        set( BOLT_LIBRARY_STATIC ${BOLT_LIBRARY_STATIC} debug ${BOLT_LIBRARY_STATIC_DEBUG} )
    else( )
        message( "${BOLT_LIBNAME_BASE}.debug.${LIB_EXT}: Debug static bolt library not found" )
    endif( )
    
    find_path( BOLT_INCLUDE_DIRS
        NAMES bolt/cl/bolt.h
        HINTS
            ${BOLT_ROOT}
            ENV BOLT_ROOT
        DOC "BOLT header file path"
        PATH_SUFFIXES include
    )
    mark_as_advanced( BOLT_INCLUDE_DIRS )

    FIND_PACKAGE_HANDLE_STANDARD_ARGS( BOLT DEFAULT_MSG BOLT_LIBRARY_STATIC BOLT_INCLUDE_DIRS )
endif( )

if( BOLT_FOUND )
    list( APPEND BOLT_LIBRARIES ${BOLT_LIBRARY_STATIC} )
else( )
    if( NOT BOLT_FIND_QUIETLY )
        message( WARNING "FindBOLT could not find the BOLT library" )
        message( STATUS "Did you remember to set the BOLT_ROOT environment variable?" )
    endif( )
endif()
