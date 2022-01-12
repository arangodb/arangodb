# Copyright 2020, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(boost_locale VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_locale
  src/encoding/codepage.cpp
  src/shared/date_time.cpp
  src/shared/format.cpp
  src/shared/formatting.cpp
  src/shared/generator.cpp
  src/shared/ids.cpp
  src/shared/localization_backend.cpp
  src/shared/message.cpp
  src/shared/mo_lambda.cpp
  src/util/codecvt_converter.cpp
  src/util/default_locale.cpp
  src/util/info.cpp
  src/util/locale_data.cpp
)

add_library(Boost::locale ALIAS boost_locale)

target_include_directories(boost_locale PUBLIC include)

target_link_libraries(boost_locale
  PUBLIC
    Boost::assert
    Boost::config
    Boost::function
    Boost::iterator
    Boost::smart_ptr
    Boost::static_assert
    Boost::type_traits
  PRIVATE
    Boost::thread
    Boost::unordered
)

target_compile_definitions(boost_locale
  PUBLIC BOOST_LOCALE_NO_LIB
  # Source files already define BOOST_LOCALE_SOURCE
  # PRIVATE BOOST_LOCALE_SOURCE
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(boost_locale PUBLIC BOOST_LOCALE_DYN_LINK)
else()
  target_compile_definitions(boost_locale PUBLIC BOOST_LOCALE_STATIC_LINK)
endif()

# Build Options

find_package(Iconv QUIET) # requires CMake 3.11
find_package(ICU COMPONENTS data i18n uc QUIET)

set(_default_posix OFF)

if(CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin")
  set(_default_posix ON)
endif()

option(BOOST_LOCALE_ENABLE_ICONV "Boost.Locale: enable iconv backend" ${ICONV_FOUND})
option(BOOST_LOCALE_ENABLE_ICU "Boost.Locale: enable ICU backend" ${ICU_FOUND})
option(BOOST_LOCALE_ENABLE_POSIX "Boost.Locale: enable POSIX backend" ${_default_posix})
option(BOOST_LOCALE_ENABLE_STD "Boost.Locale: enable std::locale backend" ON)
option(BOOST_LOCALE_ENABLE_WINAPI "Boost.Locale: enable Windows API backend" ${WIN32})

unset(_default_posix)

if(BOOST_LOCALE_ENABLE_ICONV)

  find_package(Iconv REQUIRED)

  target_compile_definitions(boost_locale PRIVATE BOOST_LOCALE_WITH_ICONV=1)
  target_link_libraries(boost_locale PRIVATE Iconv::Iconv)

endif()

if(BOOST_LOCALE_ENABLE_ICU)

  find_package(ICU COMPONENTS data i18n uc REQUIRED)

  target_compile_definitions(boost_locale PRIVATE BOOST_LOCALE_WITH_ICU=1)
  target_link_libraries(boost_locale PRIVATE ICU::data ICU::i18n ICU::uc)

  target_sources(boost_locale PRIVATE
    src/icu/boundary.cpp
    src/icu/codecvt.cpp
    src/icu/collator.cpp
    src/icu/conversion.cpp
    src/icu/date_time.cpp
    src/icu/formatter.cpp
    src/icu/icu_backend.cpp
    src/icu/numeric.cpp
    src/icu/time_zone.cpp
  )

endif()

if(BOOST_LOCALE_ENABLE_STD)

  target_sources(boost_locale PRIVATE
    src/std/codecvt.cpp
    src/std/collate.cpp
    src/std/converter.cpp
    src/std/numeric.cpp
    src/std/std_backend.cpp
  )

else()

  target_compile_definitions(boost_locale PRIVATE BOOST_LOCALE_NO_STD_BACKEND=1)

endif()

if(BOOST_LOCALE_ENABLE_WINAPI)

  target_sources(boost_locale PRIVATE
    src/win32/collate.cpp
    src/win32/converter.cpp
    src/win32/numeric.cpp
    src/win32/win_backend.cpp
  )

else()

  target_compile_definitions(boost_locale PRIVATE BOOST_LOCALE_NO_WINAPI_BACKEND=1)

endif()

if(BOOST_LOCALE_ENABLE_WINAPI OR (BOOST_LOCALE_ENABLE_STD AND WIN32))

  target_sources(boost_locale PRIVATE
    src/win32/lcid.cpp
  )

endif()

if(BOOST_LOCALE_ENABLE_POSIX)

  target_sources(boost_locale PRIVATE
    src/posix/collate.cpp
    src/posix/converter.cpp
    src/posix/numeric.cpp
    src/posix/codecvt.cpp
    src/posix/posix_backend.cpp
  )

else()

  target_compile_definitions(boost_locale PRIVATE BOOST_LOCALE_NO_POSIX_BACKEND=1)

endif()

if(BOOST_LOCALE_ENABLE_WINAPI OR BOOST_LOCALE_ENABLE_STD OR BOOST_LOCALE_ENABLE_POSIX)

  target_sources(boost_locale PRIVATE
    src/util/gregorian.cpp
  )

endif()

message(STATUS "Boost.Locale: "
  "iconv ${BOOST_LOCALE_ENABLE_ICONV}, "
  "ICU ${BOOST_LOCALE_ENABLE_ICU}, "
  "POSIX ${BOOST_LOCALE_ENABLE_POSIX}, "
  "std ${BOOST_LOCALE_ENABLE_STD}, "
  "winapi ${BOOST_LOCALE_ENABLE_WINAPI}"
)

# Testing

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
