# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_serialization VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_serialization
  src/archive_exception.cpp
  src/basic_archive.cpp
  src/basic_iarchive.cpp
  src/basic_iserializer.cpp
  src/basic_oarchive.cpp
  src/basic_oserializer.cpp
  src/basic_pointer_iserializer.cpp
  src/basic_pointer_oserializer.cpp
  src/basic_serializer_map.cpp
  src/basic_text_iprimitive.cpp
  src/basic_text_oprimitive.cpp
  src/basic_xml_archive.cpp
  src/binary_iarchive.cpp
  src/binary_oarchive.cpp
  src/codecvt_null.cpp
  src/extended_type_info.cpp
  src/extended_type_info_no_rtti.cpp
  src/extended_type_info_typeid.cpp
  src/polymorphic_binary_iarchive.cpp
  src/polymorphic_binary_oarchive.cpp
  src/polymorphic_iarchive.cpp
  src/polymorphic_oarchive.cpp
  src/polymorphic_text_iarchive.cpp
  src/polymorphic_text_oarchive.cpp
  src/polymorphic_xml_iarchive.cpp
  src/polymorphic_xml_oarchive.cpp
  src/stl_port.cpp
  src/text_iarchive.cpp
  src/text_oarchive.cpp
  src/utf8_codecvt_facet.cpp
  src/void_cast.cpp
  src/xml_archive_exception.cpp
  src/xml_grammar.cpp
  src/xml_iarchive.cpp
  src/xml_oarchive.cpp
)

add_library(boost_wserialization
  src/basic_text_wiprimitive.cpp
  src/basic_text_woprimitive.cpp
  src/binary_wiarchive.cpp
  src/binary_woarchive.cpp
  src/polymorphic_text_wiarchive.cpp
  src/polymorphic_text_woarchive.cpp
  src/polymorphic_xml_wiarchive.cpp
  src/polymorphic_xml_woarchive.cpp
  src/text_wiarchive.cpp
  src/text_woarchive.cpp
  src/xml_wgrammar.cpp
  src/xml_wiarchive.cpp
  src/xml_woarchive.cpp
)

add_library(Boost::serialization ALIAS boost_serialization)
add_library(Boost::wserialization ALIAS boost_wserialization)

target_include_directories(boost_serialization PUBLIC include)
target_include_directories(boost_wserialization PUBLIC include)

target_link_libraries(boost_serialization
  PUBLIC
    Boost::array
    Boost::assert
    Boost::config
    Boost::core
    Boost::detail
    Boost::integer
    Boost::io
    Boost::iterator
    Boost::move
    Boost::mpl
    Boost::optional
    Boost::predef
    Boost::preprocessor
    Boost::smart_ptr
    Boost::spirit
    Boost::static_assert
    Boost::type_traits
    Boost::unordered
    Boost::utility
    Boost::variant
  PRIVATE
    Boost::function
)

target_link_libraries(boost_wserialization PUBLIC boost_serialization)

target_compile_definitions(boost_serialization
  PUBLIC BOOST_SERIALIZATION_NO_LIB
  # Source files already define BOOST_SERIALIZATION_SOURCE
  # PRIVATE BOOST_SERIALIZATION_SOURCE
)

target_compile_definitions(boost_wserialization
  PUBLIC BOOST_SERIALIZATION_NO_LIB
  # Source files already define BOOST_SERIALIZATION_SOURCE
  # PRIVATE BOOST_SERIALIZATION_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_serialization PUBLIC BOOST_SERIALIZATION_DYN_LINK)
  target_compile_definitions(boost_wserialization PUBLIC BOOST_SERIALIZATION_DYN_LINK)
else()
  target_compile_definitions(boost_serialization PUBLIC BOOST_SERIALIZATION_STATIC_LINK)
  target_compile_definitions(boost_wserialization PUBLIC BOOST_SERIALIZATION_STATIC_LINK)
endif()

if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
  boost_install(TARGETS boost_serialization boost_wserialization VERSION ${BOOST_SUPERPROJECT_VERSION} HEADER_DIRECTORY include)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
