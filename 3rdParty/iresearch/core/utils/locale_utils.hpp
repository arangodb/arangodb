//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_LOCALE_UTILS_H
#define IRESEARCH_LOCALE_UTILS_H

#include "shared.hpp"

NS_ROOT
NS_BEGIN( locale_utils )

/**
 * @brief extract the locale country from a locale
 * @param locale the locale from which to extract the country
 **/
IRESEARCH_API std::string country(std::locale const& locale);

/**
 * @brief extract the locale encoding from a locale
 * @param locale the locale from which to extract the encoding
 **/
IRESEARCH_API std::string encoding(std::locale const& locale);

/**
 * @brief extract the locale language from a locale
 * @param locale the locale from which to extract the language
 **/
IRESEARCH_API std::string language(std::locale const& locale);

/**
 * @brief create a locale from czName
 * @param czName name of the locale to create (nullptr == "C")
 * @param bForceUTF8 force locale to have UTF8 encoding
 **/
IRESEARCH_API std::locale locale(char const* czName, bool bForceUTF8 = false);

/**
 * @brief create a locale from sName
 * @param sName name of the locale to create
 * @param bForceUTF8 force locale to have UTF8 encoding
 **/
IRESEARCH_API std::locale locale(std::string const& sName, bool bForceUTF8 = false);

/**
 * @brief build a locale from <czLanguage>_<czCountry>.<czEncoding>@<czVariant>
 * @param sLanguage locale language (required)
 * @param sCountry locale country
 * @param sEncoding locale character encoding
 * @param sVariant locale additional back-end specific parameters
 **/
IRESEARCH_API std::locale locale(std::string const& sLanguage, std::string const& sCountry, std::string const& sEncoding, std::string const& sVariant = "");

/**
 * @brief extract the locale name from a locale
 * @param locale the locale from which to extract the name
 **/
IRESEARCH_API std::string name(std::locale const& locale);

/**
 * @brief extract the locale utf8 from a locale
 * @param locale the locale from which to extract the language
 **/
IRESEARCH_API bool utf8(std::locale const& locale);

NS_END
NS_END

#endif
