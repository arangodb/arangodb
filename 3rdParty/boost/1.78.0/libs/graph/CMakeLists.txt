# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_graph VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_graph
  src/graphml.cpp
  src/read_graphviz_new.cpp
)

add_library(Boost::graph ALIAS boost_graph)

target_include_directories(boost_graph PUBLIC include)

target_link_libraries(boost_graph
  PUBLIC
    Boost::algorithm
    Boost::any
    Boost::array
    Boost::assert
    Boost::bimap
    Boost::bind
    Boost::concept_check
    Boost::config
    Boost::container_hash
    Boost::conversion
    Boost::core
    Boost::detail
    Boost::foreach
    Boost::function
    Boost::integer
    Boost::iterator
    Boost::lexical_cast
    Boost::math
    Boost::move
    Boost::mpl
    Boost::multi_index
    Boost::optional
    Boost::parameter
    Boost::preprocessor
    Boost::property_map
    Boost::property_tree
    Boost::random
    Boost::range
    Boost::serialization
    Boost::smart_ptr
    Boost::spirit
    Boost::static_assert
    Boost::throw_exception
    Boost::tti
    Boost::tuple
    Boost::type_traits
    Boost::typeof
    Boost::unordered
    Boost::utility
    Boost::xpressive
  PRIVATE
    Boost::regex
)

target_compile_definitions(boost_graph
  PUBLIC BOOST_GRAPH_NO_LIB
  # Source files already define BOOST_GRAPH_SOURCE
  # PRIVATE BOOST_GRAPH_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_graph PUBLIC BOOST_GRAPH_DYN_LINK)
else()
  target_compile_definitions(boost_graph PUBLIC BOOST_GRAPH_STATIC_LINK)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
