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

NS_ROOT
NS_BEGIN( locale_utils )

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
 * @param encoding the output encoding of the converter
 * @param forceUnicodeSystem force the internal system encoding to be unicode
 * @return the converter capable of outputing in the specified target encoding
 **/
template<typename Internal>
const std::codecvt<Internal, char, std::mbstate_t>& converter(
    const irs::string_ref& encoding, bool forceUnicodeSystem = true
) {
  return std::use_facet<std::codecvt<Internal, char, std::mbstate_t>>(
    locale(irs::string_ref::NIL, encoding, forceUnicodeSystem)
  );
}

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
 * @brief extract the locale utf8 from a locale
 * @param locale the locale from which to extract the language
 **/
IRESEARCH_API bool utf8(std::locale const& locale);

NS_END
NS_END

#endif