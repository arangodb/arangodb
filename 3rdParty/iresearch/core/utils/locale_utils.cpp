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

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  #include <boost/locale/generator.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <boost/locale/info.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  #include <boost/locale/util.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "locale_utils.hpp"

NS_LOCAL

std::locale generate_locale(std::string const& sName) {
  // valgrind reports invalid reads for locale_genrator if declared inside function
  boost::locale::generator locale_genrator; // stateful object, cannot be static

  return locale_genrator.generate(sName);
}

NS_END

NS_ROOT
NS_BEGIN( locale_utils )

std::string country(std::locale const& locale) {
  try {
    // try extracting 'info' facet from existing locale
    return std::use_facet<boost::locale::info>(locale).country();
  }
  catch (...) {
    // use Boost to parse the locale name and create a facet
    auto locale_info = boost::locale::util::create_info(locale, locale.name());
    auto& info_facet = std::use_facet<boost::locale::info>(locale_info);

    return info_facet.country();
  }
}

std::string encoding(std::locale const& locale){
  try {
    // try extracting 'info' facet from existing locale
    return std::use_facet<boost::locale::info>(locale).encoding();
  }
  catch (...) {
    // use Boost to parse the locale name and create a facet
    auto locale_info = boost::locale::util::create_info(locale, locale.name());
    auto& info_facet = std::use_facet<boost::locale::info>(locale_info);

    return info_facet.encoding();
  }
}

std::string language(std::locale const& locale){
  try {
    // try extracting 'info' facet from existing locale
    return std::use_facet<boost::locale::info>(locale).language();
  }
  catch (...) {
    // use Boost to parse the locale name and create a facet
    auto locale_info = boost::locale::util::create_info(locale, locale.name());
    auto& info_facet = std::use_facet<boost::locale::info>(locale_info);

    return info_facet.language();
  }
}

std::locale locale(char const* czName, bool bForceUTF8 /*= false*/) {
  if (!czName) {
    return bForceUTF8
      ? locale(std::locale::classic().name(), true)
      : std::locale::classic()
      ;
  }

  const std::string sName(czName);

  return locale(sName, bForceUTF8);
}

std::locale locale(std::string const& sName, bool bForceUTF8 /*= false*/) {
  if (!bForceUTF8) {
    return generate_locale(sName);
  }

  // ensure boost::locale::info facet exists for 'sName' since it is used below
  auto locale = generate_locale(sName);
  auto locale_info = boost::locale::util::create_info(locale, sName);
  auto& info_facet = std::use_facet<boost::locale::info>(locale_info);

  if (info_facet.utf8()) {
    return locale;
  }

  return iresearch::locale_utils::locale(
    info_facet.language(),
    info_facet.country(),
    "UTF-8",
    info_facet.variant()
  );
}

std::locale locale(std::string const& sLanguage, std::string const& sCountry, std::string const& sEncoding, std::string const& sVariant /*= ""*/) {
  bool bValid = sLanguage.find('_') == std::string::npos && sCountry.find('_') == std::string::npos && sEncoding.find('_') == std::string::npos && sVariant.find('_') == std::string::npos
              && sLanguage.find('.') == std::string::npos && sCountry.find('.') == std::string::npos && sEncoding.find('.') == std::string::npos && sVariant.find('.') == std::string::npos
              && sLanguage.find('@') == std::string::npos && sCountry.find('@') == std::string::npos && sEncoding.find('@') == std::string::npos && sVariant.find('@') == std::string::npos;
  std::string sName = sLanguage;// sLanguage.empty() && sCountry.empty() && (!sEncoding.empty() || !sVariant.empty()) ? "C" : sLanguage;

  if (!bValid || !sCountry.empty()) {
    sName.append(1, '_').append(sCountry);
  }

  if (!bValid || !sEncoding.empty()) {
    sName.append(1, '.').append(sEncoding);
  }

  if (!bValid || !sVariant.empty()) {
    sName.append(1, '@').append(sVariant);
  }

  return iresearch::locale_utils::locale(sName);
}

std::string name(std::locale const& locale) {
  try {
    return std::use_facet<boost::locale::info>(locale).name();
  }
  catch (...) {
    return locale.name(); // fall back to default value if failed to get facet
  }
}

bool utf8(std::locale const& locale) {
  try {
    // try extracting 'info' facet from existing locale
    return std::use_facet<boost::locale::info>(locale).utf8();
  }
  catch (...) {
    // use Boost to parse the locale name and create a facet
    auto locale_info = boost::locale::util::create_info(locale, locale.name());
    auto& info_facet = std::use_facet<boost::locale::info>(locale_info);

    return info_facet.utf8();
  }
}

NS_END
NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------