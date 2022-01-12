/*
 *
 * Copyright (c) 2021 John Maddock
 * Copyright (c) 2021 Daniel Kruegler
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
 
 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         unicode_casefold_test.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: Simple test suite for Unicode case folding.
  */

#include <boost/regex/config.hpp>
#include <boost/detail/lightweight_main.hpp>
#include "../test_macros.hpp"

#if defined(BOOST_HAS_ICU)

#include <boost/regex/icu.hpp>

#include <utility>

#include <unicode/uversion.h>
#include <unicode/uchar.h>

typedef std::pair<int, int> unicode_verinfo;

// Function to query the effective Unicode major and minor
// version, because some spot test cases can only be tested 
// for specific Unicode versions.
unicode_verinfo get_unicode_version()
{
  UVersionInfo versionArray = {};
  u_getUnicodeVersion(versionArray);
  unicode_verinfo result(versionArray[0] , versionArray[1]);
  return result;
}

void latin_1_checks()
{
  typedef boost::icu_regex_traits traits_type;
  traits_type traits;

  // Test range [U+0000, U+0041): Identity fold
  for (traits_type::char_type c = 0x0; c < 0x41; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
  }

  // Test ASCII upper case letters [A, Z]: Each character folds 
  // to its lowercase variant:
  for (traits_type::char_type c = 0x41; c <= 0x5A; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    const int shift = 0x61 - 0x41;
    BOOST_CHECK_EQUAL(nc, c + shift);
    BOOST_CHECK_EQUAL(nc, traits.tolower(c));
  }

  // Test range (U+005A, U+00B5): Identity fold
  for (traits_type::char_type c = 0x5A + 1; c < 0xB5; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
  }

  // U+00B5 maps to its decomposition GREEK SMALL LETTER MU 
  // (U+03BC):
  {
    traits_type::char_type c = 0xB5;
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, 0x03BC);
  }

  // Test range (U+00B5, U+00BF]: Identity fold
  for (traits_type::char_type c = 0xB5 + 1; c <= 0xBF; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
  }

  // Test range [U+00C0, U+00D6]: Each character folds 
  // to its lowercase variant:
  for (traits_type::char_type c = 0xC0; c <= 0xD6; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    traits_type::char_type lc = traits.tolower(c);
    BOOST_CHECK_EQUAL(nc, lc);
    BOOST_CHECK_NE(nc, c);
  }

  // U+00D7: Identity fold
  {
    traits_type::char_type c = 0xD7;
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
  }

  // Test range [U+00D8, U+00DE]: Each character folds 
  // to its lowercase variant:
  for (traits_type::char_type c = 0xD8; c <= 0xDE; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    traits_type::char_type lc = traits.tolower(c);
    BOOST_CHECK_EQUAL(nc, lc);
    BOOST_CHECK_NE(nc, c);
  }

  // Test range [U+00DF, U+00BF]: Identity fold
  // Note that case folding of U+00DF (LATIN SMALL 
  // LETTER SHARP S) does not fold to U+1E9E (LATIN 
  // CAPITAL LETTER SHARP S) due to case folding 
  // stability contract
  for (traits_type::char_type c = 0xDF; c <= 0xFF; ++c)
  {
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
  }
}

void spot_checks()
{
  // test specific values ripped straight out of the Unicode standard
  // to verify that our case folding is the same as theirs:
  typedef boost::icu_regex_traits traits_type;
  traits_type traits;

  const unicode_verinfo unicode_version = get_unicode_version();

  // 'LATIN CAPITAL LETTER SHARP S' folds to
  // 'LATIN SMALL LETTER SHARP S'
  if (unicode_version >= unicode_verinfo(5, 1))
  {
    traits_type::char_type c = 0x1E9E;
    traits_type::char_type nc = traits.translate_nocase(c);
    traits_type::char_type lc = traits.tolower(c);
    BOOST_CHECK_EQUAL(nc, lc);
    BOOST_CHECK_EQUAL(nc, 0xDF);
  }

  // Capital sigma (U+03A3) is the uppercase form of both the regular (U+03C2) 
  // and final (U+03C3) lowercase sigma. All these characters exists since
  // Unicode 1.1.0.
  {
    traits_type::char_type c = 0x03A3;
    traits_type::char_type nc = traits.translate_nocase(c);
    traits_type::char_type lc = traits.tolower(c);
    BOOST_CHECK_EQUAL(nc, lc);
    BOOST_CHECK_EQUAL(nc, 0x03C3);
    c = 0x03C2;
    nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, 0x03C3);
    c = 0x03C3;
    nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
  }

  // In Turkish languages the lowercase letter 'i' (U+0069) maps to an 
  // uppercase dotted I (U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE), 
  // while the uppercase letter 'I' (U+0049) maps to the dotless lowercase 
  // i (U+0131). The Unicode simple default mapping folds U+0130 to itself, 
  // but folds U+0049 to U+0069.
  {
    traits_type::char_type c = 0x0130;
    traits_type::char_type nc = traits.translate_nocase(c);
    BOOST_CHECK_EQUAL(nc, c);
    c = 0x0049;
    nc = traits.translate_nocase(c);
    traits_type::char_type lc = traits.tolower(c);
    BOOST_CHECK_EQUAL(nc, lc);
    BOOST_CHECK_EQUAL(nc, 0x0069);
  }

  // Cherokee small letters were added with Unicode 8.0,
  // but the upper case letters existed before, therefore
  // the small letters case fold to upper case letters.
  if (unicode_version >= unicode_verinfo(8, 0))
  {
    traits_type::char_type c = 0x13F8;
    traits_type::char_type nc = traits.translate_nocase(c);
    traits_type::char_type uc = traits.toupper(c);
    BOOST_CHECK_EQUAL(nc, uc);
    BOOST_CHECK_EQUAL(nc, 0x13F0);
  }

}

#endif

int cpp_main( int, char* [] ) 
{
#if defined(BOOST_HAS_ICU)
  latin_1_checks();
  spot_checks();
#endif
  return boost::report_errors();
}
