# CMake build control file for Serialization Library tests

cmake_minimum_required(VERSION 3.0)

if (POLICY CMP0054)
  cmake_policy (SET CMP0054 NEW)
endif (POLICY CMP0054)

if (POLICY CMP0063)
  cmake_policy (SET CMP0063 NEW)
endif (POLICY CMP0063)

if(Boost_USE_STATIC_LIBS)
  project("Serialization-Static")
else()
  project("Serialization-Shared")
endif()

#
# Compiler settings
#

message(STATUS "compiler is ${CMAKE_CXX_COMPILER_ID}" )
add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})

if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
  add_definitions( -ftemplate-depth=300 )
  # we use gcc to test for C++03 compatibility
  add_definitions( -std=c++03 )
  message(STATUS "compiler is g++ c++03")
  set(COMPILER_SUPPORTS_CXX11 FALSE)
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
  add_definitions( /wd4996 )
  message(STATUS "compiler is MSVC")
  set(COMPILER_SUPPORTS_CXX11 TRUE)
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=300")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" )
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3" )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -dead_strip")
  set(COMPILER_SUPPORTS_CXX11 TRUE)
endif()

#
# IDE settings
#

if( CMAKE_HOST_APPLE )
  # note: it seems that bjam builds both address models in any case
  # so we can defer this decision to the IDE just as we do for debug/release
  # so we'll not use this now
  # set(Boost_ADDRESS_MODEL 64 CACHE INTEGER "32/64 bits")
  set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Link to Boost static libraries")
  set(Boost_USE_MULTITHREADED ON)
else()
  set(Boost_ADDRESS_MODEL 64 CACHE INTEGER "32/64 bits")
  set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Link to Boost static libraries")
  set(Boost_USE_MULTITHREADED ON)
endif()

#
# Locate Project Prerequisites 
#

# Boost

# note: we're assuming that boost has been built with:
# ./b2 —-layout=versioned toolset=clang-darwin link=static,shared variant=debug,release stage

###########################
# special notes for Xcode.

# these three should result in CMake setting the variables
# Boost_SERIALIZATION_LIBRARY_DEBUG … to the correct values.  

# But my current version of CMake doesn't automatically set the library names
# to point to the the libraries to be used.  The variables are created
# but they are not initialized.  So be prepared to set these variables by hand.
# If you want to use the static libraries - point to the boost libraries ending
# in ".a".  If you want to use the shared boost libraries - point to the libraries
# ending in ".dylib".

# But wait - there's more.
# if both lib.a and lib.dylib both exist in the library directory, Xcode will
# automatically chose the *.dylib one - and there's nothing you can do to fix this.
# So my recommendation is 
# a) to place the compiled libraries in two different directories
#    - e.g. stage/lib-static/*.a and stage/lib-shared/*.dylib
#    and set the CMake variable Boost_LIBRARY_DIR to point to one or the other
# b) create two different CMake build directories - build-static and build-shared
#    and switch between projects as desired.  I like to test both since
#    there are things like dead code elimination and visibility which vary
#    between the two environments.
#
#    caveat - as I write this, I've been unable to get the tests on the shared
#    library to function. Problem is that one needs to either put the shared
#    libraries in a special known place or set an environmental
#    variable which points to the shared library directory.  I prefer the latter
#    but I've been unable to figure out how to get Xcode to do on a global basis
#    and it's not practical to do this for 247 test targets one at a time.

# c) The targets in the project will by default be built as universal 32/64 binaries
#    I have yet to experiment with these yet so I just set the target to 64 bit.

# end special note for Xcode
############################

set(Boost_DEBUG true)
set(Boost_DETAILED_FAILURE_MSG true)
set(Boost_FOUND true)

find_package(Boost REQUIRED COMPONENTS system filesystem)
include(CheckIncludeFileCXX)

message(STATUS "Boost_FOUND is ${Boost_FOUND}")

if(Boost_FOUND)
    message(STATUS "Boost Found!")
    message(STATUS "Boost is ${BOOST_ROOT}")
    message(STATUS "Boost directories found at ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries found at ${Boost_LIBRARY_DIRS}")
    message(STATUS "Boost libraries prefix is  ${Boost_LIB_PREFIX}")
    message(STATUS "Boost component libraries to be linked are ${Boost_LIBRARIES}")
    message(STATUS "Boost version found is ${Boost_VERSION}")
    #include_directories("../include" "${Boost_INCLUDE_DIRS}")
    #link_directories("${Boost_LIBRARY_DIRS}")
else()
    message(STATUS "Boost NOT Found!")
endif()

if(Boost_USE_STATIC_LIBS)
  message(STATUS "Use static libraries")
  set(LINK_TYPE "STATIC")
else()
  message(STATUS "Building shared libraries")
  set(LINK_TYPE "SHARED")
  add_definitions( "-DBOOST_ALL_DYN_LINK=1")
  add_definitions( "-DBOOST_ALL_NO_LIB=1")
  add_definitions( "-DBOOST_LIB_DIAGNOSTICS=1")
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(VISIBILITY_INLINES_HIDDEN)
endif()

###########################
# library builds

add_library(serialization ${LINK_TYPE}
  ../src/archive_exception.cpp
  ../src/basic_archive.cpp
  ../src/basic_iarchive.cpp
  ../src/basic_iserializer.cpp
  ../src/basic_oarchive.cpp
  ../src/basic_oserializer.cpp
  ../src/basic_pointer_iserializer.cpp
  ../src/basic_pointer_oserializer.cpp
  ../src/basic_serializer_map.cpp
  ../src/basic_text_iprimitive.cpp
  ../src/basic_text_oprimitive.cpp
  ../src/basic_xml_archive.cpp
  ../src/binary_iarchive.cpp
  ../src/binary_oarchive.cpp
  ../src/extended_type_info_no_rtti.cpp
  ../src/extended_type_info_typeid.cpp
  ../src/extended_type_info.cpp
  ../src/polymorphic_iarchive.cpp
  ../src/polymorphic_oarchive.cpp
  ../src/stl_port.cpp
  ../src/text_iarchive.cpp
  ../src/text_oarchive.cpp
  ../src/void_cast.cpp
  ../src/xml_archive_exception.cpp
  ../src/xml_iarchive.cpp
  ../src/xml_oarchive.cpp
  ../src/xml_grammar.cpp
  ../src/utf8_codecvt_facet.cpp
  ../src/basic_xml_grammar.ipp # doesn't show up in "Source Files" in Xcode"'
)

add_library(wserialization  ${LINK_TYPE}
  ../src/codecvt_null.cpp
  ../src/basic_text_wiprimitive.cpp
  ../src/basic_text_woprimitive.cpp
  ../src/text_wiarchive.cpp
  ../src/text_woarchive.cpp
  ../src/xml_wiarchive.cpp
  ../src/xml_woarchive.cpp
  ../src/xml_wgrammar.cpp
  ../src/basic_xml_grammar.ipp # doesn't show up in "Source Files" in Xcode"'
)

# end library build
###########################

###########################
# test targets

function( serialization_test test_name)
  set(arglist)
  foreach(a IN ITEMS ${ARGN} )
    set(arglist ${arglist} ../test/${a}.cpp)
  endforeach()
  message(STATUS ${test_name})
  add_executable( ${test_name} ../test/${test_name}.cpp ${arglist} )
  target_link_libraries(${test_name} serialization wserialization ${Boost_LIBRARIES})
  add_test( ${test_name} ${test_name} )
endfunction(serialization_test)

function(archive_test test_name)
  set(arglist)
  foreach(a IN ITEMS ${ARGN} )
    set(arglist ${arglist} ../test/${a}.cpp)
  endforeach()
  foreach(
    archive-name
    IN ITEMS text_archive text_warchive binary_archive xml_archive xml_warchive
  )
    set(amended_test_name ${test_name}_${archive-name})
    message(STATUS ${amended_test_name})
    add_executable(${amended_test_name} ../test/${test_name}.cpp ${arglist})
    set_property(
      TARGET ${amended_test_name}
      PROPERTY COMPILE_DEFINITIONS BOOST_ARCHIVE_TEST=${archive-name}.hpp
    )
    target_link_libraries(${amended_test_name} serialization wserialization ${Boost_LIBRARIES})
    add_test(${amended_test_name} ${amended_test_name})
  endforeach()
endfunction(archive_test)

function(polymorphic_archive_test test_name)
  set(arglist)
  foreach(a IN ITEMS ${ARGN} )
    set(arglist ${arglist} ../test/${a}.cpp)
  endforeach()
  foreach(
    archive-name
    IN ITEMS text_archive text_warchive binary_archive xml_archive xml_warchive
  )
    set(amended_test_name ${test_name}_polymorphic_${archive-name})
    message(STATUS ${amended_test_name})
    add_executable(${amended_test_name} ../test/${test_name}.cpp ${arglist})
    set_property(
      TARGET ${amended_test_name}
      PROPERTY COMPILE_DEFINITIONS BOOST_ARCHIVE_TEST=polymorphic_${archive-name}.hpp
    )
    target_link_libraries(${amended_test_name} serialization wserialization ${Boost_LIBRARIES})
    add_test(${amended_test_name} ${amended_test_name})
  endforeach()
endfunction(polymorphic_archive_test)

enable_testing()

serialization_test(test_dll_exported polymorphic_derived2 polymorphic_base)

# serialization(test_dll_simple dll_a_lib)
# compile test_dll_plugin.cpp
# Running the following test requires that the test know the directory 
# in which the dll is stored. I don't know how to extract this from bjam
# serialization(test_dll_plugin : : dll_polymorphic_derived2_lib)

serialization_test(test_private_ctor)
serialization_test(test_reset_object_address A)
serialization_test(test_void_cast)
serialization_test(test_mult_archive_types)
serialization_test(test_iterators)
serialization_test(test_iterators_base64)
serialization_test(test_inclusion)
serialization_test(test_inclusion2)
serialization_test(test_smart_cast)
serialization_test(test_codecvt_null ../src/codecvt_null)
serialization_test(test_strong_typedef)

archive_test(test_native_array A)
archive_test(test_boost_array A)
if(COMPILER_SUPPORTS_CXX11)
  archive_test(test_array A)
endif()
archive_test(test_binary)
archive_test(test_bitset)
archive_test(test_class_info_save)
archive_test(test_class_info_load)
archive_test(test_complex)
archive_test(test_contained_class A)
archive_test(test_cyclic_ptrs A)
archive_test(test_delete_pointer)
archive_test(test_deque A)
archive_test(test_derived)
archive_test(test_derived_class A)
archive_test(test_diamond)
archive_test(test_diamond_complex)
archive_test(test_exported polymorphic_base)
archive_test(test_forward_list A)
archive_test(test_forward_list_ptrs A)
archive_test(test_helper_support)
archive_test(test_interrupts)
archive_test(test_list A)
archive_test(test_list_ptrs A)
archive_test(test_map A)
archive_test(test_map_boost_unordered A)
archive_test(test_mi)
archive_test(test_multiple_ptrs A)
archive_test(test_multiple_inheritance)
archive_test(test_no_rtti polymorphic_base polymorphic_derived1)
archive_test(test_new_operator A)
archive_test(test_non_intrusive)
archive_test(test_non_default_ctor)
archive_test(test_non_default_ctor2)
archive_test(test_null_ptr)
archive_test(test_nvp A)
archive_test(test_object)
archive_test(test_optional)
archive_test(test_primitive)
archive_test(test_priority_queue A)
archive_test(test_private_base)
archive_test(test_private_base2)
archive_test(test_queue A)
archive_test(test_recursion A)
archive_test(test_registered)
archive_test(test_shared_ptr)
archive_test(test_shared_ptr_multi_base)
archive_test(test_shared_ptr_132)
archive_test(test_simple_class A)
archive_test(test_simple_class_ptr A)
CHECK_INCLUDE_FILE_CXX(slist SLIST_FOUND)
if(SLIST_FOUND)
  message(STATUS "slist header found")
  archive_test(test_slist A)
  archive_test(test_slist_ptr A)
else()
  message(STATUS "slist header NOT found")
endif()
archive_test(test_stack A)
archive_test(test_split)
archive_test(test_tracking)
archive_test(test_unregistered)
archive_test(test_unique_ptr)
archive_test(test_valarray)
archive_test(test_variant A)
archive_test(test_vector A)
archive_test(test_set A)
archive_test(test_set_boost_unordered A)
if(COMPILER_SUPPORTS_CXX11)
  archive_test(test_set_unordered A)
else()
  CHECK_INCLUDE_FILE_CXX(hash_set HASH_SET_FOUND)
  if(HASH_SET_FOUND)
    archive_test(test_set_hashed A)
  endif()
endif()
if(COMPILER_SUPPORTS_CXX11)
  archive_test(test_map_unordered A)
else()
  CHECK_INCLUDE_FILE_CXX(hash_map HASH_MAP_FOUND)
  if(HASH_MAP_FOUND)
    archive_test(test_map_hashed A)
  endif()
endif()

polymorphic_archive_test(test_polymorphic test_polymorphic_A A)
polymorphic_archive_test(test_polymorphic2 test_polymorphic2imp)
polymorphic_archive_test(test_polymorphic_helper)

# end test targets
####################

####################
# add headers in IDE

# for serialisation

file(GLOB x 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/archive/*.hpp"
)
add_custom_target(archive SOURCES ${x})
set_property(TARGET archive PROPERTY FOLDER "serialization")

file(GLOB x 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/archive/detail/*.hpp"
)
add_custom_target(archive-detail SOURCES ${x})
set_property(TARGET archive-detail PROPERTY FOLDER "serialization")

file(GLOB x 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/archive/impl/*.ipp"
)
add_custom_target(archive-impl SOURCES ${x})
set_property(TARGET archive-impl PROPERTY FOLDER "serialization")

file(GLOB x
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/archive/iterators/*.hpp"
)
add_custom_target(archive-iterators SOURCES ${x})
set_property(TARGET archive-iterators PROPERTY FOLDER "serialization")

file(GLOB x 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/serialization/*.hpp"
)
add_custom_target(serialization-headers SOURCES ${x})
set_property(TARGET serialization-headers PROPERTY FOLDER "serialization")

file(GLOB x 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/serialization/detail/*.hpp"
)
add_custom_target(serialization-detail SOURCES ${x})
set_property(TARGET serialization-detail PROPERTY FOLDER "serialization")

# for wserialization

file(GLOB x 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/../include/boost/archive/*_w*.hpp"
)
add_custom_target(wserialization_headers SOURCES ${x})
set_property(TARGET wserialization_headers PROPERTY FOLDER "wserialization")

# end headers in IDE
####################

#####################
# add test project to run misc tests

add_executable( test_z ../test/test_z.cpp)
target_link_libraries(test_z serialization wserialization ${Boost_LIBRARIES})

# end test project
#####################
