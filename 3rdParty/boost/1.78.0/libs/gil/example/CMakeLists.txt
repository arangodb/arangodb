#
# Copyright (c) 2017 Mateusz Loskot <mateusz at loskot dot net>
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
message(STATUS "Boost.GIL: Configuring examples")

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12.0)
  file(GLOB_RECURSE _examples ${CMAKE_CURRENT_LIST_DIR}/*.cpp CONFIGURE_DEPEND)
else()
  file(GLOB_RECURSE _examples ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
endif()

foreach(_example ${_examples})
  get_filename_component(_name ${_example} NAME_WE)
  add_executable(example_${_name} ${_name}.cpp)
  target_compile_definitions(example_${_name} PRIVATE BOOST_GIL_USE_CONCEPT_CHECK=1)
  # Unfortunately, ALIAS of imported target is not supported
  # see https://github.com/conan-io/conan/issues/2125
  if(BOOST_GIL_USE_CONAN)
    target_link_libraries(example_${_name}
      PRIVATE
        gil_compile_options
        gil_include_directories
        Boost::disable_autolinking
        Boost::filesystem
        CONAN_PKG::libjpeg
        CONAN_PKG::libpng
        CONAN_PKG::libtiff)
  else()
    target_link_libraries(example_${_name}
      PRIVATE
        gil_compile_options
        gil_include_directories
        gil_dependencies)
  endif()

  unset(_name)
endforeach()

unset(_example)
unset(_examples)
