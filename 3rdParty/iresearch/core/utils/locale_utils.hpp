////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_LOCALE_UTILS_H
#define IRESEARCH_LOCALE_UTILS_H

#include <locale>

#include "shared.hpp"
#include "string.hpp"

namespace iresearch {
namespace locale_utils {

/**
 * @brief provide a common way to access the codecvt facet of a locale
 *        will work even on MSVC with missing symbol exports
 * @param locale the locale to query for the desired facet
 **/
template<typename T>
const std::codecvt<T, char, mbstate_t>& codecvt(std::locale const& locale) {
  return std::use_facet<std::codecvt<T, char, mbstate_t>>(locale);
}

#if defined(_MSC_VER) && _MSC_VER <= 1800 && defined(IRESEARCH_DLL) // MSVC2013
  // MSVC2013 does not properly export
  // std::codecvt<char32_t, char, mbstate_t>::id for shared libraries
  template<>
  IRESEARCH_API const std::codecvt<char32_t, char, mbstate_t>& codecvt(
    std::locale const& locale
  );
#elif defined(_MSC_VER) && _MSC_VER <= 1924 // MSVC2015/MSVC2017/MSVC2019
  // MSVC2015/MSVC2017 implementations do not support char16_t/char32_t 'codecvt'
  // due to a missing export, as per their comment:
  //   This is an active bug in our database (VSO#143857), which we'll investigate
  //   for a future release, but we're currently working on higher priority things
  template<>
  IRESEARCH_API const std::codecvt<char16_t, char, mbstate_t>& codecvt(
    std::locale const& locale
  );

  template<>
  IRESEARCH_API const std::codecvt<char32_t, char, mbstate_t>& codecvt(
    std::locale const& locale
  );
#endif

/**
 * @brief convert the input as per locale from an internal representation and
 *        append it to the buffer
 * @param buf the buffer to append the converted string to
 * @param value the internally encoded data to convert
 * @param locale the locale to use to parse 'value'
 * @return success
 **/
template<typename T>
bool append_external(
  std::string& buf,
  const irs::basic_string_ref<T>& value,
  const std::locale& locale
) {
  auto& cvt = codecvt<T>(locale);
  std::mbstate_t state;
  const auto* from_begin = value.c_str();
  const auto* from_end = from_begin + value.size();
  static const size_t to_buf_size = 1024; // arbitrary size
  char to_buf[to_buf_size];
  auto* to_buf_end = to_buf + to_buf_size;
  auto* to_buf_next = to_buf;
  std::codecvt_base::result result;

  do {
    result = cvt.out(
      state,
      from_begin, from_end, from_begin,
      to_buf, to_buf_end, to_buf_next
    );

    if (std::codecvt_base::error == result) {
      return false;
    }

    buf.append(to_buf, to_buf_next - to_buf);
  } while (std::codecvt_base::partial == result);

  return true;
}

/**
 * @brief convert the input as per locale into an internal representation and
 *        append it to the buffer
 * @param buf the buffer to append the internally encoded string to
 * @param value the externally encoded data to convert
 * @param locale the locale to use to parse 'value'
 * @return success
 **/
template<typename T>
bool append_internal(
  std::basic_string<T>& buf,
  const irs::string_ref& value,
  const std::locale& locale
) {
  auto& cvt = codecvt<T>(locale);
  std::mbstate_t state;
  const auto* from_begin = value.c_str();
  const auto* from_end = from_begin + value.size();
  static const size_t to_buf_size = 1024; // arbitrary size
  T to_buf[to_buf_size];
  auto* to_buf_end = to_buf + to_buf_size;
  auto* to_buf_next = to_buf;
  std::codecvt_base::result result;
  buf.reserve(buf.size() + value.size());
  do {
    result = cvt.in(
      state,
      from_begin, from_end, from_begin,
      to_buf, to_buf_end, to_buf_next
    );

    if (std::codecvt_base::error == result) {
      return false;
    }

    buf.append(to_buf, to_buf_next - to_buf);
  } while (std::codecvt_base::partial == result);

  return true;
}

/**
 * @brief create a locale from name (language[_COUNTRY][.encoding][@variant])
 * @param name name of the locale to create (nullptr == "C")
 * @param encodingOverride force locale to have the specified output encoding (nullptr == take from 'name')
 * @param forceUnicodeSystem force the internal system encoding to be unicode
 **/
IRESEARCH_API std::locale locale(
  const irs::string_ref& name,
  const irs::string_ref& encodingOverride = irs::string_ref::NIL,
  bool forceUnicodeSystem = true
);

/**
 * @brief extract the locale country from a locale
 * @param locale the locale from which to extract the country
 **/
IRESEARCH_API const irs::string_ref& country(std::locale const& locale);

/**
 * @brief extract the locale encoding from a locale
 * @param locale the locale from which to extract the encoding
 **/
IRESEARCH_API const irs::string_ref& encoding(std::locale const& locale);

/**
 * @brief extract the locale language from a locale
 * @param locale the locale from which to extract the language
 **/
IRESEARCH_API const irs::string_ref& language(std::locale const& locale);

/**
 * @brief extract the locale name from a locale
 * @param locale the locale from which to extract the name
 **/
IRESEARCH_API const std::string& name(std::locale const& locale);

/**
* @brief extract if locale is UTF8 locale
* @param locale the locale from which to extract info
**/
IRESEARCH_API bool is_utf8(std::locale const& locale);

}
}

#endif
