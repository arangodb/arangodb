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

#include "gtest/gtest.h"
#include "utils/locale_utils.hpp"
#include "utils/misc.hpp"

namespace tests {
  class LocaleUtilsTestSuite: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };
}

using namespace tests;
using namespace iresearch::locale_utils;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(LocaleUtilsTestSuite, test_get_locale) {
  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "", true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "", false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "koi8-r", true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("c"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "koi8-r", false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("c"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "InvalidString", true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("c"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("invalidstring"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "InvalidString", false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("c"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("invalidstring"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", "", true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", "", false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", "koi8-r", true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", "koi8-r", false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("InvalidName", "utf-8", true);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("invalidname"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("InvalidName", "utf-8", false);

    ASSERT_EQ(std::string("*"), locale.name());
    ASSERT_EQ(std::string("invalidname"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_append) {
  // to unicode internal char
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    char utf8[] = { char(0xd0), char(0xb2), char(0xd1), char(0x85), char(0xd0), char(0xbe), char(0xd0), char(0xb4), char(0xd1), char(0x8f), char(0xd1), char(0x89), char(0xd0), char(0xb8), char(0xd0), char(0xb5), ' ', char(0xd0), char(0xb4), char(0xd0), char(0xb0), char(0xd0), char(0xbd), char(0xd0), char(0xbd), char(0xd1), char(0x8b), char(0xd0), char(0xb5) };
    irs::string_ref utf8_ref(utf8, 29);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_internal(out, koi8r_ref, locale));
    ASSERT_EQ(utf8_ref, irs::string_ref(out));
  }

  // to unicode internal wchar_t
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    wchar_t ucs2[] = { wchar_t(0x0432), wchar_t(0x0445), wchar_t(0x043e), wchar_t(0x0434), wchar_t(0x044f), wchar_t(0x0449), wchar_t(0x0438), wchar_t(0x0435), wchar_t(' '), wchar_t(0x0434), wchar_t(0x0430), wchar_t(0x043d), wchar_t(0x043d), wchar_t(0x044b), wchar_t(0x0435) };
    irs::basic_string_ref<wchar_t> ucs2_ref(ucs2, 15);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::basic_string<wchar_t> out;
    ASSERT_TRUE(irs::locale_utils::append_internal(out, koi8r_ref, locale));
    ASSERT_EQ(ucs2_ref, irs::basic_string_ref<wchar_t>(out));
  }

  // to unicode internal char16_t
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    char16_t utf16[] = { char16_t(0x0432), char16_t(0x0445), char16_t(0x043e), char16_t(0x0434), char16_t(0x044f), char16_t(0x0449), char16_t(0x0438), char16_t(0x0435), char16_t(' '), char16_t(0x0434), char16_t(0x0430), char16_t(0x043d), char16_t(0x043d), char16_t(0x044b), char16_t(0x0435) };
    irs::basic_string_ref<char16_t> utf16_ref(utf16, 15);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::basic_string<char16_t> out;
    ASSERT_TRUE(irs::locale_utils::append_internal(out, koi8r_ref, locale));
    ASSERT_EQ(utf16_ref, irs::basic_string_ref<char16_t>(out));
  }

  // to unicode internal char32_t
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    char32_t utf32[] = { char32_t(0x0432), char32_t(0x0445), char32_t(0x043e), char32_t(0x0434), char32_t(0x044f), char32_t(0x0449), char32_t(0x0438), char32_t(0x0435), char32_t(' '), char32_t(0x0434), char32_t(0x0430), char32_t(0x043d), char32_t(0x043d), char32_t(0x044b), char32_t(0x0435) };
    irs::basic_string_ref<char32_t> utf32_ref(utf32, 15);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::basic_string<char32_t> out;
    ASSERT_TRUE(irs::locale_utils::append_internal(out, koi8r_ref, locale));
    ASSERT_EQ(utf32_ref, irs::basic_string_ref<char32_t>(out));
  }

  // to system internal char ASCII
  {
    char ascii[] = "input data";
    irs::string_ref ascii_ref(ascii, 10);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_internal(out, ascii_ref, locale));
    ASSERT_EQ(ascii_ref, irs::string_ref(out));
  }

  // to system internal wchar_t ASCII
  {
    char ascii[] = "input data";
    irs::string_ref ascii_ref(ascii, 10);
    wchar_t wide[] = L"input data";
    irs::basic_string_ref<wchar_t> wide_ref(wide, 10);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
    std::basic_string<wchar_t> out;
    ASSERT_TRUE(irs::locale_utils::append_internal(out, ascii_ref, locale));
    ASSERT_EQ(wide_ref, irs::basic_string_ref<wchar_t>(out));
  }

  // from unicode internal char
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    char utf8[] = { char(0xd0), char(0xb2), char(0xd1), char(0x85), char(0xd0), char(0xbe), char(0xd0), char(0xb4), char(0xd1), char(0x8f), char(0xd1), char(0x89), char(0xd0), char(0xb8), char(0xd0), char(0xb5), ' ', char(0xd0), char(0xb4), char(0xd0), char(0xb0), char(0xd0), char(0xbd), char(0xd0), char(0xbd), char(0xd1), char(0x8b), char(0xd0), char(0xb5) };
    irs::string_ref utf8_ref(utf8, 29);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_external(out, utf8_ref, locale));
    ASSERT_EQ(koi8r_ref, irs::string_ref(out));
  }

  // from unicode internal wchar_t
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    wchar_t ucs2[] = { wchar_t(0x0432), wchar_t(0x0445), wchar_t(0x043e), wchar_t(0x0434), wchar_t(0x044f), wchar_t(0x0449), wchar_t(0x0438), wchar_t(0x0435), wchar_t(' '), wchar_t(0x0434), wchar_t(0x0430), wchar_t(0x043d), wchar_t(0x043d), wchar_t(0x044b), wchar_t(0x0435) };
    irs::basic_string_ref<wchar_t> ucs2_ref(ucs2, 15);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_external(out, ucs2_ref, locale));
    ASSERT_EQ(koi8r_ref, irs::string_ref(out));
  }

  // from unicode internal char16_t
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    char16_t utf16[] = { char16_t(0x0432), char16_t(0x0445), char16_t(0x043e), char16_t(0x0434), char16_t(0x044f), char16_t(0x0449), char16_t(0x0438), char16_t(0x0435), char16_t(' '), char16_t(0x0434), char16_t(0x0430), char16_t(0x043d), char16_t(0x043d), char16_t(0x044b), char16_t(0x0435) };
    irs::basic_string_ref<char16_t> utf16_ref(utf16, 15);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_external(out, utf16_ref, locale));
    ASSERT_EQ(koi8r_ref, irs::string_ref(out));
  }

  // from unicode internal char32_t
  {
    char koi8r[] = { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    irs::string_ref koi8r_ref(koi8r, 15);
    char32_t utf32[] = { char32_t(0x0432), char32_t(0x0445), char32_t(0x043e), char32_t(0x0434), char32_t(0x044f), char32_t(0x0449), char32_t(0x0438), char32_t(0x0435), char32_t(' '), char32_t(0x0434), char32_t(0x0430), char32_t(0x043d), char32_t(0x043d), char32_t(0x044b), char32_t(0x0435) };
    irs::basic_string_ref<char32_t> utf32_ref(utf32, 15);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_external(out, utf32_ref, locale));
    ASSERT_EQ(koi8r_ref, irs::string_ref(out));
  }

  // from system internal char ASCII
  {
    char ascii[] = "input data";
    irs::string_ref ascii_ref(ascii, 10);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_external(out, ascii_ref, locale));
    ASSERT_EQ(ascii_ref, irs::string_ref(out));
  }

  // from system internal wchar_t ASCII
  {
    char ascii[] = "input data";
    irs::string_ref ascii_ref(ascii, 10);
    wchar_t wide[] = L"input data";
    irs::basic_string_ref<wchar_t> wide_ref(wide, 10);
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
    std::string out;
    ASSERT_TRUE(irs::locale_utils::append_external(out, wide_ref, locale));
    ASSERT_EQ(ascii_ref, irs::string_ref(out));
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_create) {
  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("C"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("C"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "UTF-8", true);

    ASSERT_EQ(std::string("c.utf-8"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("c"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "UTF-8", false);

    ASSERT_EQ(std::string("c.utf-8"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("c"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("*", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("*"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("*"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("*", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("*"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("*"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("C", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("C"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("C", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("C"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("C"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("en"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("en"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("en_US"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("en_US"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US.UTF-8", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("en_US.utf-8"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("en_US.UTF-8", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("en_US.utf-8"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("en"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("US"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("ru_RU.koi8-r"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("ru"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("RU"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("ru_RU.koi8-r"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("ru"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("RU"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", "UTF-8", true);

    ASSERT_EQ(std::string("ru_RU.utf-8"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("ru"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("RU"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("ru_RU.KOI8-R", "UTF-8", false);

    ASSERT_EQ(std::string("ru_RU.utf-8"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("ru"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string("RU"), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), irs::locale_utils::encoding(locale));
    ASSERT_TRUE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("InvalidString", irs::string_ref::NIL, true);

    ASSERT_EQ(std::string("invalidstring"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("invalidstring"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }

  {
    auto locale = irs::locale_utils::locale("InvalidString", irs::string_ref::NIL, false);

    ASSERT_EQ(std::string("invalidstring"), irs::locale_utils::name(locale));
    ASSERT_EQ(std::string("invalidstring"), irs::locale_utils::language(locale));
    ASSERT_EQ(std::string(""), irs::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), irs::locale_utils::encoding(locale));
    ASSERT_FALSE(irs::locale_utils::is_utf8(locale));
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_get) {
  auto zh = irs::locale_utils::locale("zh_CN.BIG5");

  // codecvt (char)
  {
    auto& irscvt = irs::locale_utils::codecvt<char>(zh);
    auto& stdcvt = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh);
    ASSERT_EQ(&stdcvt, &irscvt);
  }

  // codecvt properties (wchar_t)
  {
    auto& irscvt = irs::locale_utils::codecvt<wchar_t>(zh);
    auto& stdcvt = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh);
    ASSERT_EQ(&stdcvt, &irscvt);
  }

  // MSVC2015/MSVC2017 implementations do not support char16_t/char32_t 'codecvt'
  // due to a missing export, as per their comment:
  //   This is an active bug in our database (VSO#143857), which we'll investigate
  //   for a future release, but we're currently working on higher priority things
  #if !defined(_MSC_VER) || _MSC_VER <= 1800 || !defined(_DLL)
    // codecvt properties (char16_t)
    {
      auto& irscvt = irs::locale_utils::codecvt<char16_t>(zh);
      auto& stdcvt = std::use_facet<std::codecvt<char16_t, char, mbstate_t>>(zh);
      ASSERT_EQ(&stdcvt, &irscvt);
    }

    // codecvt properties (char32_t)
    {
      auto& irscvt = irs::locale_utils::codecvt<char32_t>(zh);
      auto& stdcvt = std::use_facet<std::codecvt<char32_t, char, mbstate_t>>(zh);

      #if defined(_MSC_VER) && _MSC_VER <= 1800 && defined(IRESEARCH_DLL) // MSVC2013 shared
        // MSVC2013 does not properly export
        // std::codecvt<char32_t, char, mbstate_t>::id for shared libraries
        ASSERT_NE(&stdcvt, &irscvt);
      #else
        ASSERT_EQ(&stdcvt, &irscvt);
      #endif
    }
  #endif
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_properties) {
  auto c = irs::locale_utils::locale("C");
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251");
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R");
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5");
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8");

  // codecvt properties (char)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh0);
    auto& cvt_c = std::use_facet<std::codecvt<char, char, mbstate_t>>(c);
    auto& cvt_cp1251 = std::use_facet<std::codecvt<char, char, mbstate_t>>(ru0);
    auto& cvt_koi8r = std::use_facet<std::codecvt<char, char, mbstate_t>>(ru1);
    auto& cvt_utf8 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char ch = 'x';

    ASSERT_FALSE(cvt_big5.always_noconv());
    ASSERT_FALSE(cvt_c.always_noconv());
    ASSERT_FALSE(cvt_cp1251.always_noconv());
    ASSERT_FALSE(cvt_koi8r.always_noconv());
    ASSERT_FALSE(cvt_utf8.always_noconv());

    ASSERT_EQ(0, cvt_big5.encoding()); // non-ASCII is always variable-length
    ASSERT_EQ(0, cvt_c.encoding()); // non-ASCII is always variable-length (non-trivial to determine ASCII)
    ASSERT_EQ(0, cvt_cp1251.encoding()); // non-ASCII is always variable-length
    ASSERT_EQ(0, cvt_koi8r.encoding()); // non-ASCII is always variable-length
    ASSERT_EQ(0, cvt_utf8.encoding()); // non-ASCII is always variable-length

    ASSERT_EQ(1, cvt_big5.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_c.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_cp1251.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_koi8r.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_utf8.length(state, &ch, &ch + 1, 1));

    ASSERT_EQ(2, cvt_big5.max_length());
    ASSERT_EQ(1, cvt_c.max_length());
    ASSERT_EQ(1, cvt_cp1251.max_length());
    ASSERT_EQ(1, cvt_koi8r.max_length());
    ASSERT_EQ(3, cvt_utf8.max_length());
  }

  // codecvt properties (wchar)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh0);
    auto& cvt_c = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(c);
    auto& cvt_cp1251 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(ru0);
    auto& cvt_koi8r = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(ru1);
    auto& cvt_utf8 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char ch = 'x';

    ASSERT_FALSE(cvt_big5.always_noconv());
    ASSERT_FALSE(cvt_c.always_noconv());
    ASSERT_FALSE(cvt_cp1251.always_noconv());
    ASSERT_FALSE(cvt_koi8r.always_noconv());
    ASSERT_FALSE(cvt_utf8.always_noconv());

    ASSERT_EQ(0, cvt_big5.encoding()); // bytes in the range 0x00 to 0x7f that are not part of a double-byte character are assumed to be single-byte characters
    ASSERT_EQ(1, cvt_c.encoding());
    ASSERT_EQ(1, cvt_cp1251.encoding());
    ASSERT_EQ(1, cvt_koi8r.encoding());
    ASSERT_EQ(0, cvt_utf8.encoding());

    ASSERT_EQ(1, cvt_big5.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_c.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_cp1251.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_koi8r.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_utf8.length(state, &ch, &ch + 1, 1));

    ASSERT_EQ(2, cvt_big5.max_length());
    ASSERT_EQ(1, cvt_c.max_length());
    ASSERT_EQ(1, cvt_cp1251.max_length());
    ASSERT_EQ(1, cvt_koi8r.max_length());
    ASSERT_EQ(sizeof(wchar_t) > 2 ? 6 : 3, cvt_utf8.max_length()); // ICU only provides max_length per char16_t, so multiply by 2
  }

  // codecvt properties (char16)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char16_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_c = irs::locale_utils::codecvt<char16_t>(c); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_cp1251 = irs::locale_utils::codecvt<char16_t>(ru0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_koi8r = irs::locale_utils::codecvt<char16_t>(ru1); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char16_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char ch = 'x';

    ASSERT_FALSE(cvt_big5.always_noconv());
    ASSERT_FALSE(cvt_c.always_noconv());
    ASSERT_FALSE(cvt_cp1251.always_noconv());
    ASSERT_FALSE(cvt_koi8r.always_noconv());
    ASSERT_FALSE(cvt_utf8.always_noconv());

    ASSERT_EQ(0, cvt_big5.encoding()); // bytes in the range 0x00 to 0x7f that are not part of a double-byte character are assumed to be single-byte characters
    ASSERT_EQ(1, cvt_c.encoding());
    ASSERT_EQ(1, cvt_cp1251.encoding());
    ASSERT_EQ(1, cvt_koi8r.encoding());
    ASSERT_EQ(0, cvt_utf8.encoding());

    ASSERT_EQ(1, cvt_big5.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_c.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_cp1251.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_koi8r.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_utf8.length(state, &ch, &ch + 1, 1));

    ASSERT_EQ(2, cvt_big5.max_length());
    ASSERT_EQ(1, cvt_c.max_length());
    ASSERT_EQ(1, cvt_cp1251.max_length());
    ASSERT_EQ(1, cvt_koi8r.max_length());
    ASSERT_EQ(3, cvt_utf8.max_length());
  }

  // codecvt properties (char32)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char32_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_c = irs::locale_utils::codecvt<char32_t>(c); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_cp1251 = irs::locale_utils::codecvt<char32_t>(ru0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_koi8r = irs::locale_utils::codecvt<char32_t>(ru1); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char32_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char ch = 'x';

    ASSERT_FALSE(cvt_big5.always_noconv());
    ASSERT_FALSE(cvt_c.always_noconv());
    ASSERT_FALSE(cvt_cp1251.always_noconv());
    ASSERT_FALSE(cvt_koi8r.always_noconv());
    ASSERT_FALSE(cvt_utf8.always_noconv());

    ASSERT_EQ(0, cvt_big5.encoding());
    ASSERT_EQ(1, cvt_c.encoding());
    ASSERT_EQ(1, cvt_cp1251.encoding());
    ASSERT_EQ(1, cvt_koi8r.encoding());
    ASSERT_EQ(0, cvt_utf8.encoding());

    ASSERT_EQ(1, cvt_big5.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_c.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_cp1251.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_koi8r.length(state, &ch, &ch + 1, 1));
    ASSERT_EQ(1, cvt_utf8.length(state, &ch, &ch + 1, 1));

    ASSERT_EQ(2, cvt_big5.max_length());
    ASSERT_EQ(1, cvt_c.max_length());
    ASSERT_EQ(1, cvt_cp1251.max_length());
    ASSERT_EQ(1, cvt_koi8r.max_length());
    ASSERT_EQ(6, cvt_utf8.max_length()); // ICU only provides max_length per char16_t, so multiply by 2
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_conversion_ascii_non_unicode) {
  auto c = irs::locale_utils::locale("C", irs::string_ref::NIL, false);
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251", irs::string_ref::NIL, false);
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5", irs::string_ref::NIL, false);
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8", irs::string_ref::NIL, false);

  // ascii (char)
  {
    std::istringstream in;
    std::ostringstream out;
    std::string buf;

    in.imbue(c);
    out.imbue(c);

    in.str("in-test-data");
    in >> buf;
    ASSERT_EQ(std::string("in-test-data"), buf);

    out << "out test data" << std::endl;
    ASSERT_EQ(std::string("out test data\n"), out.str());
  }

  // ascii (wchar)
  {
    std::wistringstream in;
    std::wostringstream out;
    std::wstring buf;

    in.imbue(c);
    out.imbue(c);

    in.str(L"in-test-data");
    in >> buf;
    ASSERT_EQ(std::wstring(L"in-test-data"), buf);

    out << L"out test data" << std::endl;
    ASSERT_EQ(std::wstring(L"out test data\n"), out.str());
  }

  // ascii (char16)
  {
    auto& cvt = irs::locale_utils::codecvt<char16_t>(c); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    std::string from("in test data");
    const char* from_cnext;
    char16_t buf16[12];
    const char16_t* buf16_cnext;
    char16_t* buf16_next;
    char buf8[12];
    char* buf8_next;
    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf16, buf16 + 1, buf16_next)
    );

    ASSERT_EQ(&from[1], from_cnext);
    ASSERT_EQ(&buf16[1], buf16_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(char16_t(from[i]), buf16[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf16, buf16 + IRESEARCH_COUNTOF(buf16), buf16_next)
    );

    ASSERT_EQ(&from[0] + from.size(), from_cnext);
    ASSERT_EQ(&buf16[0] + IRESEARCH_COUNTOF(buf16), buf16_next);

    for (size_t i = 0, count = from.size(); i < count; ++i) {
      ASSERT_EQ(char16_t(from[i]), buf16[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf16, buf16 + IRESEARCH_COUNTOF(buf16), buf16_cnext, buf8, buf8 + 1, buf8_next)
    );

    ASSERT_EQ(&buf16[1], buf16_cnext);
    ASSERT_EQ(&buf8[1], buf8_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(buf16[i], char16_t(buf8[i]));
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf16, buf16 + IRESEARCH_COUNTOF(buf16), buf16_cnext, buf8, buf8 + IRESEARCH_COUNTOF(buf8), buf8_next)
    );

    ASSERT_EQ(&buf16[0] + IRESEARCH_COUNTOF(buf16), buf16_cnext);
    ASSERT_EQ(&buf8[0] + IRESEARCH_COUNTOF(buf8), buf8_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf16); i < count; ++i) {
      ASSERT_EQ(buf16[i], char16_t(buf8[i]));
    }
  }

  // ascii (char32)
  {
    auto& cvt = irs::locale_utils::codecvt<char32_t>(c); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    std::string from("in test data");
    const char* from_cnext;
    char32_t buf32[12];
    const char32_t* buf32_cnext;
    char32_t* buf32_next;
    char buf8[12];
    char* buf8_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf32, buf32 + 1, buf32_next)
    );

    ASSERT_EQ(&from[1], from_cnext);
    ASSERT_EQ(&buf32[1], buf32_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(char32_t(from[i]), buf32[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf32, buf32 + IRESEARCH_COUNTOF(buf32), buf32_next)
    );

    ASSERT_EQ(&from[0] + from.size(), from_cnext);
    ASSERT_EQ(&buf32[0] + IRESEARCH_COUNTOF(buf32), buf32_next);

    for (size_t i = 0, count = from.size(); i < count; ++i) {
      ASSERT_EQ(char32_t(from[i]), buf32[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf32, buf32 + IRESEARCH_COUNTOF(buf32), buf32_cnext, buf8, buf8 + 1, buf8_next)
    );

    ASSERT_EQ(&buf32[1], buf32_cnext);
    ASSERT_EQ(&buf8[1], buf8_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(buf32[i], char32_t(buf8[i]));
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf32, buf32 + IRESEARCH_COUNTOF(buf32), buf32_cnext, buf8, buf8 + IRESEARCH_COUNTOF(buf8), buf8_next)
    );

    ASSERT_EQ(&buf32[0] + IRESEARCH_COUNTOF(buf32), buf32_cnext);
    ASSERT_EQ(&buf8[0] + IRESEARCH_COUNTOF(buf8), buf8_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf32); i < count; ++i) {
      ASSERT_EQ(buf32[i], char32_t(buf8[i]));
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_conversion_ascii_unicode) {
  auto c = irs::locale_utils::locale("C", irs::string_ref::NIL, true);
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251", irs::string_ref::NIL, true);
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5", irs::string_ref::NIL, true);
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8", irs::string_ref::NIL, true);

  // ascii (char)
  {
    std::istringstream in;
    std::ostringstream out;
    std::string buf;

    in.imbue(c);
    out.imbue(c);

    in.str("in-test-data");
    in >> buf;
    ASSERT_EQ(std::string("in-test-data"), buf);

    out << "out test data" << std::endl;
    ASSERT_EQ(std::string("out test data\n"), out.str());
  }

  // ascii (wchar)
  {
    std::wistringstream in;
    std::wostringstream out;
    std::wstring buf;

    in.imbue(c);
    out.imbue(c);

    in.str(L"in-test-data");
    in >> buf;
    ASSERT_EQ(std::wstring(L"in-test-data"), buf);

    out << L"out test data" << std::endl;
    ASSERT_EQ(std::wstring(L"out test data\n"), out.str());
  }

  // ascii (char16)
  {
    auto& cvt = irs::locale_utils::codecvt<char16_t>(c); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    std::string from("in test data");
    const char* from_cnext;
    char16_t buf16[12];
    const char16_t* buf16_cnext;
    char16_t* buf16_next;
    char buf8[12];
    char* buf8_next;
    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf16, buf16 + 1, buf16_next)
    );

    ASSERT_EQ(&from[1], from_cnext);
    ASSERT_EQ(&buf16[1], buf16_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(char16_t(from[i]), buf16[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf16, buf16 + IRESEARCH_COUNTOF(buf16), buf16_next)
    );

    ASSERT_EQ(&from[0] + from.size(), from_cnext);
    ASSERT_EQ(&buf16[0] + IRESEARCH_COUNTOF(buf16), buf16_next);

    for (size_t i = 0, count = from.size(); i < count; ++i) {
      ASSERT_EQ(char16_t(from[i]), buf16[i]);
    }
    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf16, buf16 + IRESEARCH_COUNTOF(buf16), buf16_cnext, buf8, buf8 + 1, buf8_next)
    );

    ASSERT_EQ(&buf16[1], buf16_cnext);
    ASSERT_EQ(&buf8[1], buf8_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(buf16[i], char16_t(buf8[i]));
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf16, buf16 + IRESEARCH_COUNTOF(buf16), buf16_cnext, buf8, buf8 + IRESEARCH_COUNTOF(buf8), buf8_next)
    );

    ASSERT_EQ(&buf16[0] + IRESEARCH_COUNTOF(buf16), buf16_cnext);
    ASSERT_EQ(&buf8[0] + IRESEARCH_COUNTOF(buf8), buf8_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf16); i < count; ++i) {
      ASSERT_EQ(buf16[i], char16_t(buf8[i]));
    }
  }

  // ascii (char32)
  {
    auto& cvt = irs::locale_utils::codecvt<char32_t>(c); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    std::string from("in test data");
    const char* from_cnext;
    char32_t buf32[12];
    const char32_t* buf32_cnext;
    char32_t* buf32_next;
    char buf8[12];
    char* buf8_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf32, buf32 + 1, buf32_next)
    );

    ASSERT_EQ(&from[1], from_cnext);
    ASSERT_EQ(&buf32[1], buf32_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(char32_t(from[i]), buf32[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.in(state, &from[0], &from[0] + from.size(), from_cnext, buf32, buf32 + IRESEARCH_COUNTOF(buf32), buf32_next)
    );

    ASSERT_EQ(&from[0] + from.size(), from_cnext);
    ASSERT_EQ(&buf32[0] + IRESEARCH_COUNTOF(buf32), buf32_next);

    for (size_t i = 0, count = from.size(); i < count; ++i) {
      ASSERT_EQ(char32_t(from[i]), buf32[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf32, buf32 + IRESEARCH_COUNTOF(buf32), buf32_cnext, buf8, buf8 + 1, buf8_next)
    );

    ASSERT_EQ(&buf32[1], buf32_cnext);
    ASSERT_EQ(&buf8[1], buf8_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(buf32[i], char32_t(buf8[i]));
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt.out(state, buf32, buf32 + IRESEARCH_COUNTOF(buf32), buf32_cnext, buf8, buf8 + IRESEARCH_COUNTOF(buf8), buf8_next)
    );

    ASSERT_EQ(&buf32[0] + IRESEARCH_COUNTOF(buf32), buf32_cnext);
    ASSERT_EQ(&buf8[0] + IRESEARCH_COUNTOF(buf8), buf8_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf32); i < count; ++i) {
      ASSERT_EQ(buf32[i], char32_t(buf8[i]));
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_conversion_single_byte_non_unicode) {
  auto c = irs::locale_utils::locale("C", irs::string_ref::NIL, false);
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251", irs::string_ref::NIL, false);
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5", irs::string_ref::NIL, false);
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8", irs::string_ref::NIL, false);

  // single-byte charset (char) koi8-r
  {
    auto& cvt_cp1251 = std::use_facet<std::codecvt<char, char, mbstate_t>>(ru0);
    auto& cvt_koi8r = std::use_facet<std::codecvt<char, char, mbstate_t>>(ru1);
    mbstate_t state = mbstate_t();
    char cp1251[] = { char(0xe2), char(0xf5), char(0xee), char(0xe4), char(0xff), char(0xf9), char(0xe8), char(0xe5), ' ', char(0xe4), char(0xe0), char(0xed), char(0xed), char(0xfb), char(0xe5) };
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    char error[] =  { char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), ' ', char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a) };
    const char* koi8r_cnext;
    char buf[15];
    const char* buf_cnext;
    char* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }
  }

  // single-byte charset (wchar) koi8-r
  {
    auto& cvt_cp1251 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(ru0);
    auto& cvt_koi8r = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(ru1);
    mbstate_t state = mbstate_t();
    char cp1251[] = { char(0xe2), char(0xf5), char(0xee), char(0xe4), char(0xff), char(0xf9), char(0xe8), char(0xe5), ' ', char(0xe4), char(0xe0), char(0xed), char(0xed), char(0xfb), char(0xe5) };
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    char error[] =  { char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), ' ', char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a) };
    const char* koi8r_cnext;
    wchar_t buf[15];
    const wchar_t* buf_cnext;
    wchar_t* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }
  }

  // single-byte charset (char16) koi8-r
  {
    auto& cvt_koi8r = irs::locale_utils::codecvt<char16_t>(ru1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    char16_t utf16[] = { 0x0432, 0x0445, 0x043E, 0x0434, 0x044F, 0x0449, 0x0438, 0x0435, 0x0020, 0x0434, 0x0430, 0x043D, 0x043D, 0x044B, 0x0435 };
    const char* koi8r_cnext;
    const char16_t* utf16_cnext;
    char16_t buf[15];
    char16_t* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, &koi8r[0], &koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&utf16[1], utf16_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }
  }

  // single-byte charset (char32) koi8-r
  {
    auto& cvt_koi8r = irs::locale_utils::codecvt<char32_t>(ru1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    char32_t utf32[] = { 0x0432, 0x0445, 0x043E, 0x0434, 0x044F, 0x0449, 0x0438, 0x0435, 0x0020, 0x0434, 0x0430, 0x043D, 0x043D, 0x044B, 0x0435 };
    const char* koi8r_cnext;
    const char32_t* utf32_cnext;
    char32_t buf[15];
    char32_t* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, &koi8r[0], &koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&utf32[1], utf32_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_conversion_single_byte_unicode) {
  auto c = irs::locale_utils::locale("C", irs::string_ref::NIL, true);
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251", irs::string_ref::NIL, true);
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5", irs::string_ref::NIL, true);
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8", irs::string_ref::NIL, true);

  // single-byte charset (char) koi8-r
  {
    auto& cvt_cp1251 = std::use_facet<std::codecvt<char, char, mbstate_t>>(ru0);
    auto& cvt_koi8r = std::use_facet<std::codecvt<char, char, mbstate_t>>(ru1);
    mbstate_t state = mbstate_t();
    char cp1251[] = { char(0xe2), char(0xf5), char(0xee), char(0xe4), char(0xff), char(0xf9), char(0xe8), char(0xe5), ' ', char(0xe4), char(0xe0), char(0xed), char(0xed), char(0xfb), char(0xe5) };
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    const char* koi8r_cnext;
    char buf[14 * 2 + 1]; // *2 for 2 UTF8 chars per char, +1 for space
    const char* buf_cnext;
    char* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 2, buf_next) // +2 to fit 2 UTF8 chars
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[2], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[2], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(cp1251[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(cp1251[i], out[i]);
    }
  }

  // single-byte charset (wchar) koi8-r
  {
    auto& cvt_cp1251 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(ru0);
    auto& cvt_koi8r = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(ru1);
    mbstate_t state = mbstate_t();
    char cp1251[] = { char(0xe2), char(0xf5), char(0xee), char(0xe4), char(0xff), char(0xf9), char(0xe8), char(0xe5), ' ', char(0xe4), char(0xe0), char(0xed), char(0xed), char(0xfb), char(0xe5) };
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    const char* koi8r_cnext;
    wchar_t buf[15];
    const wchar_t* buf_cnext;
    wchar_t* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(cp1251[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_cp1251.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(cp1251[i], out[i]);
    }
  }

  // single-byte charset (char16) koi8-r
  {
    auto& cvt_koi8r = irs::locale_utils::codecvt<char16_t>(ru1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    char16_t utf16[] = { 0x0432, 0x0445, 0x043E, 0x0434, 0x044F, 0x0449, 0x0438, 0x0435, 0x0020, 0x0434, 0x0430, 0x043D, 0x043D, 0x044B, 0x0435 };
    const char* koi8r_cnext;
    const char16_t* utf16_cnext;
    char16_t buf[15];
    char16_t* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, &koi8r[0], &koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&utf16[1], utf16_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }
  }

  // single-byte charset (char32) koi8-r
  {
    auto& cvt_koi8r = irs::locale_utils::codecvt<char32_t>(ru1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char koi8r[] =  { char(0xd7), char(0xc8), char(0xcf), char(0xc4), char(0xd1), char(0xdd), char(0xc9), char(0xc5), ' ', char(0xc4), char(0xc1), char(0xce), char(0xce), char(0xd9), char(0xc5) };
    char32_t utf32[] = { 0x0432, 0x0445, 0x043E, 0x0434, 0x044F, 0x0449, 0x0438, 0x0435, 0x0020, 0x0434, 0x0430, 0x043D, 0x043D, 0x044B, 0x0435 };
    const char* koi8r_cnext;
    const char32_t* utf32_cnext;
    char32_t buf[15];
    char32_t* buf_next;
    char out[15];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, koi8r, koi8r + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&koi8r[1], koi8r_cnext);
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.in(state, &koi8r[0], &koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&koi8r[0] + IRESEARCH_COUNTOF(koi8r), koi8r_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&utf32[1], utf32_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_koi8r.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(koi8r[i], out[i]);
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_conversion_multibyte_non_unicode) {
  auto c = irs::locale_utils::locale("C", irs::string_ref::NIL, false);
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251", irs::string_ref::NIL, false);
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, false);
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5", irs::string_ref::NIL, false);
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8", irs::string_ref::NIL, false);

  // multi-byte charset (char) Chinese (from big5)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] =  { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] =  { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    char error[] = { char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a) };
    const char* big5_cnext;
    char buf[11];
    const char* buf_cnext;
    char* buf_next;
    char out[11];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(big5 + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }
  }

  // multi-byte charset (char) Chinese (from utf8)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] =  { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] =  { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    char error[] = { char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a) };
    const char* utf8_cnext;
    char buf[11];
    const char* buf_cnext;
    char* buf_next;
    char out[11];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }
  }

  // multi-byte charset (wchar) Chinese (from big5)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] =  { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] =  { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    char error[] = { char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a) };
    const char* big5_cnext;
    wchar_t buf[11];
    const wchar_t* buf_cnext;
    wchar_t* buf_next;
    char out[11];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(big5 + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }
  }

  // multi-byte charset (wchar) Chinese (from utf8)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] =  { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] =  { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    char error[] = { char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a), char(0x1a) };
    const char* utf8_cnext;
    wchar_t buf[11];
    const wchar_t* buf_cnext;
    wchar_t* buf_next;
    char out[11];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(error[i], out[i]);
    }
  }

  // multi-byte charset (char16) Chinese (from big5)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char16_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char16_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char16_t utf16[] = { char16_t(0x4ECA), char16_t(0x5929), char16_t(0x4E0B), char16_t(0x5348), char16_t(0x7684), char16_t(0x592A), char16_t(0x967D), char16_t(0x5F88), char16_t(0x6EAB), char16_t(0x6696), char16_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* big5_cnext;
    const char16_t* utf16_cnext;
    char16_t buf[11];
    char16_t* buf_next;
    char out[33];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, &big5[0], &big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + 3, out_next) // +3 since UTF8 uses 3 bytes per char for Chinese
    );
    ASSERT_EQ(&utf16[1], utf16_cnext);
    ASSERT_EQ(&out[3], out_next); // +3 since UTF8 uses 3 bytes per char for Chinese

    for (size_t i = 0, count = 3; i < count; ++i) { // +3 since UTF8 uses 3 bytes per char for Chinese
      ASSERT_EQ(utf8[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }
  }

  // multi-byte charset (char16) Chinese (from utf8)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char16_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char16_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char16_t utf16[] = { char16_t(0x4ECA), char16_t(0x5929), char16_t(0x4E0B), char16_t(0x5348), char16_t(0x7684), char16_t(0x592A), char16_t(0x967D), char16_t(0x5F88), char16_t(0x6EAB), char16_t(0x6696), char16_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* utf8_cnext;
    const char16_t* utf16_cnext;
    char16_t buf[11];
    char16_t* buf_next;
    char out[22];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, &utf8[0], &utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + 2, out_next) // +2 since BIG5 uses 2 bytes per char
    );
    ASSERT_EQ(&utf16[1], utf16_cnext);
    ASSERT_EQ(&out[2], out_next); // +2 since BIG5 uses 2 bytes per char

    for (size_t i = 0, count = 2; i < count; ++i) { // +2 since BIG5 uses 2 bytes per char
      ASSERT_EQ(big5[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }
  }

  // multi-byte charset (char32) Chinese (from big5)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char32_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char32_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char32_t utf32[] = { char32_t(0x4ECA), char32_t(0x5929), char32_t(0x4E0B), char32_t(0x5348), char32_t(0x7684), char32_t(0x592A), char32_t(0x967D), char32_t(0x5F88), char32_t(0x6EAB), char32_t(0x6696), char32_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* big5_cnext;
    const char32_t* utf32_cnext;
    char32_t buf[11];
    char32_t* buf_next;
    char out[33];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, &big5[0], &big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + 3, out_next)
    );
    ASSERT_EQ(&utf32[1], utf32_cnext);
    ASSERT_EQ(&out[3], out_next); // +3 since UTF8 uses 3 bytes per char for Chinese

    for (size_t i = 0, count = 3; i < count; ++i) { // +3 since UTF8 uses 3 bytes per char for Chinese
      ASSERT_EQ(utf8[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }
  }

  // multi-byte charset (char32) Chinese (from utf8)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char32_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char32_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char32_t utf32[] = { char32_t(0x4ECA), char32_t(0x5929), char32_t(0x4E0B), char32_t(0x5348), char32_t(0x7684), char32_t(0x592A), char32_t(0x967D), char32_t(0x5F88), char32_t(0x6EAB), char32_t(0x6696), char32_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* utf8_cnext;
    const char32_t* utf32_cnext;
    char32_t buf[11];
    char32_t* buf_next;
    char out[22];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, &utf8[0], &utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + 2, out_next) // +2 since BIG5 uses 2 bytes per char
    );
    ASSERT_EQ(&utf32[1], utf32_cnext);
    ASSERT_EQ(&out[2], out_next); // +2 since BIG5 uses 2 bytes per char

    for (size_t i = 0, count = 2; i < count; ++i) { // +2 since BIG5 uses 2 bytes per char
      ASSERT_EQ(big5[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_codecvt_conversion_multibyte_unicode) {
  auto c = irs::locale_utils::locale("C", irs::string_ref::NIL, true);
  auto ru0 = irs::locale_utils::locale("ru_RU.CP1251", irs::string_ref::NIL, true);
  auto ru1 = irs::locale_utils::locale("ru_RU.KOI8-R", irs::string_ref::NIL, true);
  auto zh0 = irs::locale_utils::locale("zh_CN.BIG5", irs::string_ref::NIL, true);
  auto zh1 = irs::locale_utils::locale("zh_CN.UTF-8", irs::string_ref::NIL, true);

  // multi-byte charset (char) Chinese (from big5)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* big5_cnext;
    char buf[33];
    const char* buf_cnext;
    char* buf_next;
    char out[33];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 3, buf_next) // +3 since UTF8 uses 3 bytes per char
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[3], buf_next); // +3 since UTF8 uses 3 bytes per char

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(big5 + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 3, out_next) // +3 since UTF8 uses 3 bytes per char
    );
    ASSERT_EQ(&buf[3], buf_cnext); // +3 since UTF8 uses 3 bytes per char
    ASSERT_EQ(&out[3], out_next); // +3 since UTF8 uses 3 bytes per char

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }

  }

  // multi-byte charset (char) Chinese (from utf8)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<char, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* utf8_cnext;
    char buf[33];
    const char* buf_cnext;
    char* buf_next;
    char out[22];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 3, buf_next) // +3 since UTF8 uses 3 bytes per char for Chinese
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[3], buf_next); // +3 since UTF8 uses 3 bytes per char for Chinese

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 2, out_next) // +2 since BIG5 uses 2 bytes per char
    );
    ASSERT_EQ(&buf[3], buf_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&out[2], out_next); // +2 since BIG5 uses 2 bytes per char

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }
  }

  // multi-byte charset (wchar) Chinese (from big5)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* big5_cnext;
    wchar_t buf[11];
    const wchar_t* buf_cnext;
    wchar_t* buf_next;
    char out[33];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(big5 + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 3, out_next) // +3 since UTF8 uses 3 bytes per char for Chinese
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[3], out_next); // +3 since UTF8 uses 3 bytes per char for Chinese

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }
  }

  // multi-byte charset (wchar) Chinese (from utf8)
  {
    auto& cvt_big5 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh0);
    auto& cvt_utf8 = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(zh1);
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* utf8_cnext;
    wchar_t buf[11];
    const wchar_t* buf_cnext;
    wchar_t* buf_next;
    char out[22];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_next);

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + 1, out_next)
    );
    ASSERT_EQ(&buf[1], buf_cnext);
    ASSERT_EQ(&out[1], out_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, buf, buf + IRESEARCH_COUNTOF(buf), buf_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(buf + IRESEARCH_COUNTOF(buf), buf_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }
  }

  // multi-byte charset (char16) Chinese (from big5)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char16_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char16_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char16_t utf16[] = { char16_t(0x4ECA), char16_t(0x5929), char16_t(0x4E0B), char16_t(0x5348), char16_t(0x7684), char16_t(0x592A), char16_t(0x967D), char16_t(0x5F88), char16_t(0x6EAB), char16_t(0x6696), char16_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* big5_cnext;
    const char16_t* utf16_cnext;
    char16_t buf[11];
    char16_t* buf_next;
    char out[33];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, &big5[0], &big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + 3, out_next) // +3 since UTF8 uses 3 bytes per char for Chinese
    );
    ASSERT_EQ(&utf16[1], utf16_cnext);
    ASSERT_EQ(&out[3], out_next); // +3 since UTF8 uses 3 bytes per char for Chinese

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }
  }

  // multi-byte charset (char16) Chinese (from utf8)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char16_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char16_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char16_t utf16[] = { char16_t(0x4ECA), char16_t(0x5929), char16_t(0x4E0B), char16_t(0x5348), char16_t(0x7684), char16_t(0x592A), char16_t(0x967D), char16_t(0x5F88), char16_t(0x6EAB), char16_t(0x6696), char16_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* utf8_cnext;
    const char16_t* utf16_cnext;
    char16_t buf[11];
    char16_t* buf_next;
    char out[22];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, &utf8[0], &utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf16[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + 2, out_next) // +2 since BIG5 uses 2 bytes per char
    );
    ASSERT_EQ(&utf16[1], utf16_cnext);
    ASSERT_EQ(&out[2], out_next); // +2 since BIG5 uses 2 bytes per char

    for (size_t i = 0, count = 2; i < count; ++i) { // +2 since BIG5 uses 2 bytes per char
      ASSERT_EQ(big5[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf16, utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf16 + IRESEARCH_COUNTOF(utf16), utf16_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }
  }

  // multi-byte charset (char32) Chinese (from big5)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char32_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char32_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char32_t utf32[] = { char32_t(0x4ECA), char32_t(0x5929), char32_t(0x4E0B), char32_t(0x5348), char32_t(0x7684), char32_t(0x592A), char32_t(0x967D), char32_t(0x5F88), char32_t(0x6EAB), char32_t(0x6696), char32_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* big5_cnext;
    const char32_t* utf32_cnext;
    char32_t buf[11];
    char32_t* buf_next;
    char out[33];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, big5, big5 + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&big5[2], big5_cnext); // +2 since BIG5 uses 2 bytes per char
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.in(state, &big5[0], &big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&big5[0] + IRESEARCH_COUNTOF(big5), big5_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + 3, out_next) // +3 since UTF8 uses 3 bytes per char for Chinese
    );
    ASSERT_EQ(&utf32[1], utf32_cnext);
    ASSERT_EQ(&out[3], out_next); // +3 since UTF8 uses 3 bytes per char for Chinese

    for (size_t i = 0, count = 3; i < count; ++i) { // +3 since UTF8 uses 3 bytes per char for Chinese
      ASSERT_EQ(utf8[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(utf8[i], out[i]);
    }
  }

  // multi-byte charset (char32) Chinese (from utf8)
  {
    auto& cvt_big5 = irs::locale_utils::codecvt<char32_t>(zh0); // must get facet via locale_utils to work arond MSVC issues
    auto& cvt_utf8 = irs::locale_utils::codecvt<char32_t>(zh1); // must get facet via locale_utils to work arond MSVC issues
    mbstate_t state = mbstate_t();
    char big5[] = { char(0xa4), char(0xb5), char(0xa4), char(0xd1), char(0xa4), char(0x55), char(0xa4), char(0xc8), char(0xaa), char(0xba), char(0xa4), char(0xd3), char(0xb6), char(0xa7), char(0xab), char(0xdc), char(0xb7), char(0xc5), char(0xb7), char(0x78), char(0xa1), char(0x43) };
    char32_t utf32[] = { char32_t(0x4ECA), char32_t(0x5929), char32_t(0x4E0B), char32_t(0x5348), char32_t(0x7684), char32_t(0x592A), char32_t(0x967D), char32_t(0x5F88), char32_t(0x6EAB), char32_t(0x6696), char32_t(0x3002) };
    char utf8[] = { char(0xe4), char(0xbb), char(0x8a), char(0xe5), char(0xa4), char(0xa9), char(0xe4), char(0xb8), char(0x8b), char(0xe5), char(0x8d), char(0x88), char(0xe7), char(0x9a), char(0x84), char(0xe5), char(0xa4), char(0xaa), char(0xe9), char(0x99), char(0xbd), char(0xe5), char(0xbe), char(0x88), char(0xe6), char(0xba), char(0xab), char(0xe6), char(0x9a), char(0x96), char(0xe3), char(0x80), char(0x82) };
    const char* utf8_cnext;
    const char32_t* utf32_cnext;
    char32_t buf[11];
    char32_t* buf_next;
    char out[22];
    char* out_next;

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, utf8, utf8 + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + 1, buf_next)
    );
    ASSERT_EQ(&utf8[3], utf8_cnext); // +3 since UTF8 uses 3 bytes per char for Chinese
    ASSERT_EQ(&buf[1], buf_next);

    for (size_t i = 0, count = 1; i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_utf8.in(state, &utf8[0], &utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext, buf, buf + IRESEARCH_COUNTOF(buf), buf_next)
    );

    ASSERT_EQ(&utf8[0] + IRESEARCH_COUNTOF(utf8), utf8_cnext);
    ASSERT_EQ(&buf[0] + IRESEARCH_COUNTOF(buf), buf_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(buf); i < count; ++i) {
      ASSERT_EQ(utf32[i], buf[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::partial, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + 2, out_next) // +2 since BIG5 uses 2 bytes per char
    );
    ASSERT_EQ(&utf32[1], utf32_cnext);
    ASSERT_EQ(&out[2], out_next); // +2 since BIG5 uses 2 bytes per char

    for (size_t i = 0, count = 2; i < count; ++i) { // +2 since BIG5 uses 2 bytes per char
      ASSERT_EQ(big5[i], out[i]);
    }

    ASSERT_EQ(
      std::codecvt_base::ok, // MSVC doesn't follow the specification of declaring 'result'
      cvt_big5.out(state, utf32, utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext, out, out + IRESEARCH_COUNTOF(out), out_next)
    );

    ASSERT_EQ(utf32 + IRESEARCH_COUNTOF(utf32), utf32_cnext);
    ASSERT_EQ(out + IRESEARCH_COUNTOF(out), out_next);

    for (size_t i = 0, count = IRESEARCH_COUNTOF(out); i < count; ++i) {
      ASSERT_EQ(big5[i], out[i]);
    }
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_info) {
  {
    std::locale locale = std::locale::classic();

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, irs::string_ref::NIL, false);

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, irs::string_ref::NIL, true);

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("*");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("*"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("*"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("C");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("C"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en_US");

    ASSERT_EQ(std::string("US"), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("en_US"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("en_US.UTF-8");

    ASSERT_EQ(std::string("US"), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("utf-8"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("en"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("en_US.utf-8"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("ru_RU.KOI8-R");

    ASSERT_EQ(std::string("RU"), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("koi8-r"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("ru"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("ru_RU.koi8-r"), iresearch::locale_utils::name(locale));
  }

  {
    std::locale locale = iresearch::locale_utils::locale("InvalidString");

    ASSERT_EQ(std::string(""), iresearch::locale_utils::country(locale));
    ASSERT_EQ(std::string("us-ascii"), iresearch::locale_utils::encoding(locale));
    ASSERT_EQ(std::string("invalidstring"), iresearch::locale_utils::language(locale));
    ASSERT_EQ(std::string("invalidstring"), iresearch::locale_utils::name(locale));
  }
}

TEST_F(LocaleUtilsTestSuite, test_locale_num_put) {
  struct test_numpunct: public std::numpunct<char> {
    virtual std::string do_grouping() const { return ""; }
  };
  struct test_numpunctw: public std::numpunct<char> {
    virtual std::string do_grouping() const { return ""; }
  };

  auto c = irs::locale_utils::locale("C");
  auto de = irs::locale_utils::locale("de");
  auto en = irs::locale_utils::locale("en.IBM-943"); // EBCDIC
  auto ru = irs::locale_utils::locale("ru_RU.KOI8-R");

  // use constant test configuration for num-punct instead of relying on system
  c = std::locale(c, new test_numpunct());
  c = std::locale(c, new test_numpunctw());
  de = std::locale(de, new test_numpunct());
  de = std::locale(de, new test_numpunctw());
  en = std::locale(en, new test_numpunct());
  en = std::locale(en, new test_numpunctw());
  ru = std::locale(ru, new test_numpunct());
  ru = std::locale(ru, new test_numpunctw());

  // bool (char)
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << "|" << std::boolalpha << false
           << "|" << std::boolalpha << true
           << "|" << std::noboolalpha << std::dec << std::uppercase << false
           << "|" << std::noboolalpha << std::hex << std::uppercase << false
           << "|" << std::noboolalpha << std::oct << std::uppercase << false
           << "|" << std::noboolalpha << std::dec << std::uppercase << true
           << "|" << std::noboolalpha << std::hex << std::uppercase << true
           << "|" << std::noboolalpha << std::oct << std::uppercase << true
           << "|" << std::noboolalpha << std::dec << std::nouppercase << false
           << "|" << std::noboolalpha << std::hex << std::nouppercase << false
           << "|" << std::noboolalpha << std::oct << std::nouppercase << false
           << "|" << std::noboolalpha << std::dec << std::nouppercase << true
           << "|" << std::noboolalpha << std::hex << std::nouppercase << true
           << "|" << std::noboolalpha << std::oct << std::nouppercase << true
           << "|" << std::showbase << std::showpos << std::internal << std::boolalpha << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::internal << std::boolalpha << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::internal << std::noboolalpha << std::dec << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::internal << std::noboolalpha << std::hex << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::internal << std::noboolalpha << std::oct << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::internal << std::noboolalpha << std::dec << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::internal << std::noboolalpha << std::hex << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::internal << std::noboolalpha << std::oct << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::left << std::boolalpha << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::left << std::boolalpha << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::left << std::noboolalpha << std::dec << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::left << std::noboolalpha << std::hex << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::left << std::noboolalpha << std::oct << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::left << std::noboolalpha << std::dec << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::left << std::noboolalpha << std::hex << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::left << std::noboolalpha << std::oct << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::right << std::boolalpha << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::right << std::boolalpha << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::right << std::noboolalpha << std::dec << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::right << std::noboolalpha << std::hex << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::right << std::noboolalpha << std::oct << std::setw(10) << false
           << "|" << std::showbase << std::showpos << std::right << std::noboolalpha << std::dec << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::right << std::noboolalpha << std::hex << std::setw(10) << true
           << "|" << std::showbase << std::showpos << std::right << std::noboolalpha << std::oct << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::internal << std::boolalpha << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::internal << std::boolalpha << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::internal << std::noboolalpha << std::dec << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::internal << std::noboolalpha << std::hex << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::internal << std::noboolalpha << std::oct << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::internal << std::noboolalpha << std::dec << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::internal << std::noboolalpha << std::hex << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::internal << std::noboolalpha << std::oct << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::left << std::boolalpha << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::left << std::boolalpha << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::left << std::noboolalpha << std::dec << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::left << std::noboolalpha << std::hex << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::left << std::noboolalpha << std::oct << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::left << std::noboolalpha << std::dec << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::left << std::noboolalpha << std::hex << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::left << std::noboolalpha << std::oct << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::right << std::boolalpha << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::right << std::boolalpha << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::right << std::noboolalpha << std::dec << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::right << std::noboolalpha << std::hex << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::right << std::noboolalpha << std::oct << std::setw(10) << false
           << "|" << std::showbase << std::noshowpos << std::right << std::noboolalpha << std::dec << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::right << std::noboolalpha << std::hex << std::setw(10) << true
           << "|" << std::showbase << std::noshowpos << std::right << std::noboolalpha << std::oct << std::setw(10) << true
           << "|" << std::endl;
    }

    ASSERT_EQ(std::string("|false|true|0|0|0|1|1|1|0|0|0|1|1|1|     false|      true|+        0|+        0|+        0|+        1|+      0x1|+       01|false     |true      |+0        |+0        |+0        |+1        |+0x1      |+01       |     false|      true|        +0|        +0|        +0|        +1|      +0x1|       +01|     false|      true|         0|         0|         0|         1|0x       1|        01|false     |true      |0         |0         |0         |1         |0x1       |01        |     false|      true|         0|         0|         0|         1|       0x1|        01|\n"), c_out.str());
    ASSERT_EQ(std::string("|false|true|0|0|0|1|1|1|0|0|0|1|1|1|     false|      true|+        0|+        0|+        0|+        1|+      0x1|+       01|false     |true      |+0        |+0        |+0        |+1        |+0x1      |+01       |     false|      true|        +0|        +0|        +0|        +1|      +0x1|       +01|     false|      true|         0|         0|         0|         1|0x       1|        01|false     |true      |0         |0         |0         |1         |0x1       |01        |     false|      true|         0|         0|         0|         1|       0x1|        01|\n"), de_out.str());
    ASSERT_EQ(std::string("|false|true|0|0|0|1|1|1|0|0|0|1|1|1|     false|      true|+        0|+        0|+        0|+        1|+      0x1|+       01|false     |true      |+0        |+0        |+0        |+1        |+0x1      |+01       |     false|      true|        +0|        +0|        +0|        +1|      +0x1|       +01|     false|      true|         0|         0|         0|         1|0x       1|        01|false     |true      |0         |0         |0         |1         |0x1       |01        |     false|      true|         0|         0|         0|         1|       0x1|        01|\n"), en_out.str());
    ASSERT_EQ(std::string("|false|true|0|0|0|1|1|1|0|0|0|1|1|1|     false|      true|+        0|+        0|+        0|+        1|+      0x1|+       01|false     |true      |+0        |+0        |+0        |+1        |+0x1      |+01       |     false|      true|        +0|        +0|        +0|        +1|      +0x1|       +01|     false|      true|         0|         0|         0|         1|0x       1|        01|false     |true      |0         |0         |0         |1         |0x1       |01        |     false|      true|         0|         0|         0|         1|       0x1|        01|\n"), ru_out.str());
  }

  // long
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << sizeof(long)
           << "|" << std::dec << std::uppercase << (long)(-1234)
           << "|" << std::hex << std::uppercase << (long)(-1234)
           << "|" << std::oct << std::uppercase << (long)(-1234)
           << "|" << std::dec << std::uppercase << (long)(0)
           << "|" << std::hex << std::uppercase << (long)(0)
           << "|" << std::oct << std::uppercase << (long)(0)
           << "|" << std::dec << std::uppercase << (long)(1234)
           << "|" << std::hex << std::uppercase << (long)(1234)
           << "|" << std::oct << std::uppercase << (long)(1234)
           << "|" << std::dec << std::nouppercase << (long)(-1234)
           << "|" << std::hex << std::nouppercase << (long)(-1234)
           << "|" << std::oct << std::nouppercase << (long)(-1234)
           << "|" << std::dec << std::nouppercase << (long)(0)
           << "|" << std::hex << std::nouppercase << (long)(0)
           << "|" << std::oct << std::nouppercase << (long)(0)
           << "|" << std::dec << std::nouppercase << (long)(1234)
           << "|" << std::hex << std::nouppercase << (long)(1234)
           << "|" << std::oct << std::nouppercase << (long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (long)(1234)
           << "|" << std::endl;
    }

    if (4 == sizeof(long)) {
      ASSERT_EQ(std::string("4|-1234|FFFFFB2E|37777775456|0|0|0|1234|4D2|2322|-1234|fffffb2e|37777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffb2e|+037777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffb2e|+037777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffb2e|+037777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffb2e|037777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), c_out.str());
      ASSERT_EQ(std::string("4|-1234|FFFFFB2E|37777775456|0|0|0|1234|4D2|2322|-1234|fffffb2e|37777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffb2e|+037777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffb2e|+037777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffb2e|+037777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffb2e|037777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), de_out.str());
      ASSERT_EQ(std::string("4|-1234|FFFFFB2E|37777775456|0|0|0|1234|4D2|2322|-1234|fffffb2e|37777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffb2e|+037777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffb2e|+037777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffb2e|+037777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffb2e|037777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), en_out.str());
      ASSERT_EQ(std::string("4|-1234|FFFFFB2E|37777775456|0|0|0|1234|4D2|2322|-1234|fffffb2e|37777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffb2e|+037777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffb2e|+037777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffb2e|+037777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffb2e|037777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffb2e|037777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), ru_out.str());
    } else {
      ASSERT_EQ(std::string("8|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), c_out.str());
      ASSERT_EQ(std::string("8|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), de_out.str());
      ASSERT_EQ(std::string("8|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), en_out.str());
      ASSERT_EQ(std::string("8|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), ru_out.str());
    }
  }

  // long long
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << "|" << std::dec << std::uppercase << (long long)(-1234)
           << "|" << std::hex << std::uppercase << (long long)(-1234)
           << "|" << std::oct << std::uppercase << (long long)(-1234)
           << "|" << std::dec << std::uppercase << (long long)(0)
           << "|" << std::hex << std::uppercase << (long long)(0)
           << "|" << std::oct << std::uppercase << (long long)(0)
           << "|" << std::dec << std::uppercase << (long long)(1234)
           << "|" << std::hex << std::uppercase << (long long)(1234)
           << "|" << std::oct << std::uppercase << (long long)(1234)
           << "|" << std::dec << std::nouppercase << (long long)(-1234)
           << "|" << std::hex << std::nouppercase << (long long)(-1234)
           << "|" << std::oct << std::nouppercase << (long long)(-1234)
           << "|" << std::dec << std::nouppercase << (long long)(0)
           << "|" << std::hex << std::nouppercase << (long long)(0)
           << "|" << std::oct << std::nouppercase << (long long)(0)
           << "|" << std::dec << std::nouppercase << (long long)(1234)
           << "|" << std::hex << std::nouppercase << (long long)(1234)
           << "|" << std::oct << std::nouppercase << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (long long)(-1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (long long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (long long)(1234)
           << "|" << std::endl;
    }

    ASSERT_EQ(std::string("|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), c_out.str());
    ASSERT_EQ(std::string("|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), de_out.str());
    ASSERT_EQ(std::string("|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), en_out.str());
    ASSERT_EQ(std::string("|-1234|FFFFFFFFFFFFFB2E|1777777777777777775456|0|0|0|1234|4D2|2322|-1234|fffffffffffffb2e|1777777777777777775456|0|0|0|1234|4d2|2322|-     1234|+0xfffffffffffffb2e|+01777777777777777775456|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|-1234     |+0xfffffffffffffb2e|+01777777777777777775456|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |     -1234|+0xfffffffffffffb2e|+01777777777777777775456|        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|-     1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|0x     4d2|     02322|-1234     |0xfffffffffffffb2e|01777777777777777775456|0         |0         |0         |1234      |0x4d2     |02322     |     -1234|0xfffffffffffffb2e|01777777777777777775456|         0|         0|         0|      1234|     0x4d2|     02322|\n"), ru_out.str());
  }

  // unsigned long
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << "|" << std::dec << std::uppercase << (unsigned long)(0)
           << "|" << std::hex << std::uppercase << (unsigned long)(0)
           << "|" << std::oct << std::uppercase << (unsigned long)(0)
           << "|" << std::dec << std::uppercase << (unsigned long)(1234)
           << "|" << std::hex << std::uppercase << (unsigned long)(1234)
           << "|" << std::oct << std::uppercase << (unsigned long)(1234)
           << "|" << std::dec << std::nouppercase << (unsigned long)(0)
           << "|" << std::hex << std::nouppercase << (unsigned long)(0)
           << "|" << std::oct << std::nouppercase << (unsigned long)(0)
           << "|" << std::dec << std::nouppercase << (unsigned long)(1234)
           << "|" << std::hex << std::nouppercase << (unsigned long)(1234)
           << "|" << std::oct << std::nouppercase << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (unsigned long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (unsigned long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (unsigned long)(1234)
           << "|" << std::endl;
    }

    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), c_out.str());
    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), de_out.str());
    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), en_out.str());
    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), ru_out.str());
  }

  // unsigned long long (char)
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << "|" << std::dec << std::uppercase << (unsigned long long)(0)
           << "|" << std::hex << std::uppercase << (unsigned long long)(0)
           << "|" << std::oct << std::uppercase << (unsigned long long)(0)
           << "|" << std::dec << std::uppercase << (unsigned long long)(1234)
           << "|" << std::hex << std::uppercase << (unsigned long long)(1234)
           << "|" << std::oct << std::uppercase << (unsigned long long)(1234)
           << "|" << std::dec << std::nouppercase << (unsigned long long)(0)
           << "|" << std::hex << std::nouppercase << (unsigned long long)(0)
           << "|" << std::oct << std::nouppercase << (unsigned long long)(0)
           << "|" << std::dec << std::nouppercase << (unsigned long long)(1234)
           << "|" << std::hex << std::nouppercase << (unsigned long long)(1234)
           << "|" << std::oct << std::nouppercase << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::dec << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::hex << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::oct << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::left << std::dec << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::hex << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::oct << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::showpos << std::right << std::dec << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::hex << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::oct << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::dec << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::hex << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::oct << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::dec << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::hex << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::oct << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (unsigned long long)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::dec << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::hex << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::oct << std::setw(10) << (unsigned long long)(1234)
           << "|" << std::endl;
    }

    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), c_out.str());
    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), de_out.str());
    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), en_out.str());
    ASSERT_EQ(std::string("|0|0|0|1234|4D2|2322|0|0|0|1234|4d2|2322|+        0|+        0|+        0|+     1234|+    0x4d2|+    02322|+0        |+0        |+0        |+1234     |+0x4d2    |+02322    |        +0|        +0|        +0|     +1234|    +0x4d2|    +02322|         0|         0|         0|      1234|0x     4d2|     02322|0         |0         |0         |1234      |0x4d2     |02322     |         0|         0|         0|      1234|     0x4d2|     02322|\n"), ru_out.str());
  }

// GCC v4.x does not support std::defaultfloat or std::hexfloat
#if !defined(__GNUC__) || __GNUC__ > 4
  // double (char)
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (double)(-1003.1415)
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (double)(-1003.1415)
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (double)(-1003.1415)
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (double)(-1003.1415)
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (double)(-64.) // power of 2
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (double)(-64.) // power of 2
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (double)(-64.) // power of 2
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (double)(-64.) // power of 2
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (double)(0.)
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (double)(0.)
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (double)(0.)
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (double)(0.)
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (double)(32.) // power of 2
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (double)(32.) // power of 2
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (double)(32.) // power of 2
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (double)(32.) // power of 2
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (double)(1002.71828)
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (double)(1002.71828)
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (double)(1002.71828)
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (double)(1002.71828)
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (double)(-1003.1415)
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (double)(-1003.1415)
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (double)(-1003.1415)
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (double)(-1003.1415)
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (double)(-64.) // power of 2
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (double)(-64.) // power of 2
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (double)(-64.) // power of 2
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (double)(-64.) // power of 2
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (double)(0.)
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (double)(0.)
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (double)(0.)
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (double)(0.)
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (double)(32.) // power of 2
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (double)(32.) // power of 2
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (double)(32.) // power of 2
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (double)(32.) // power of 2
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (double)(1002.71828)
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (double)(1002.71828)
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (double)(1002.71828)
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::hex << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::hex << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::hex << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::hex << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::hex << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::hex << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (double)(1002.71828)
           << "|" << std::endl;
    }

    ASSERT_EQ(std::string("|-1003.1415|-1003.141500|-0X1.F5921CAC08312P+9|-1.003142E3|-64|-64.000000|-0X1P+6|-6.400000E1|0|0.000000|0X0P+0|0.000000E0|32|32.000000|0X1P+5|3.200000E1|1002.71828|1002.718280|0X1.F55BF0995AAF8P+9|1.002718E3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64.|-64.000000|-0x1.p+6|-6.400000e1|0.|0.000000|0x0.p+0|0.000000e0|32.|32.000000|0x1.p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|+        0|+ 0.000000|+   0x0p+0|+0.000000e0|+       32|+32.000000|+   0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|+0        |+0.000000 |+0x0p+0   |+0.000000e0|+32       |+32.000000|+0x1p+5   |+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|        +0| +0.000000|   +0x0p+0|+0.000000e0|       +32|+32.000000|   +0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|         0|  0.000000|0x    0p+0|0.000000e0|        32| 32.000000|0x    1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|0         |0.000000  |0x0p+0    |0.000000e0|32        |32.000000 |0x1p+5    |3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|         0|  0.000000|    0x0p+0|0.000000e0|        32| 32.000000|    0x1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|\n"), c_out.str());
    ASSERT_EQ(std::string("|-1003,1415|-1003,141500|-0X1.F5921CAC08312P+9|-1,003142E3|-64|-64,000000|-0X1P+6|-6,400000E1|0|0,000000|0X0P+0|0,000000E0|32|32,000000|0X1P+5|3,200000E1|1002,71828|1002,718280|0X1.F55BF0995AAF8P+9|1,002718E3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64.|-64,000000|-0x1.p+6|-6,400000e1|0.|0,000000|0x0.p+0|0,000000e0|32.|32,000000|0x1.p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|+        0|+ 0,000000|+   0x0p+0|+0,000000e0|+       32|+32,000000|+   0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|+0        |+0,000000 |+0x0p+0   |+0,000000e0|+32       |+32,000000|+0x1p+5   |+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|        +0| +0,000000|   +0x0p+0|+0,000000e0|       +32|+32,000000|   +0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|         0|  0,000000|0x    0p+0|0,000000e0|        32| 32,000000|0x    1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|0         |0,000000  |0x0p+0    |0,000000e0|32        |32,000000 |0x1p+5    |3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|         0|  0,000000|    0x0p+0|0,000000e0|        32| 32,000000|    0x1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|\n"), de_out.str());
    ASSERT_EQ(std::string("|-1003.1415|-1003.141500|-0X1.F5921CAC08312P+9|-1.003142E3|-64|-64.000000|-0X1P+6|-6.400000E1|0|0.000000|0X0P+0|0.000000E0|32|32.000000|0X1P+5|3.200000E1|1002.71828|1002.718280|0X1.F55BF0995AAF8P+9|1.002718E3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64.|-64.000000|-0x1.p+6|-6.400000e1|0.|0.000000|0x0.p+0|0.000000e0|32.|32.000000|0x1.p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|+        0|+ 0.000000|+   0x0p+0|+0.000000e0|+       32|+32.000000|+   0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|+0        |+0.000000 |+0x0p+0   |+0.000000e0|+32       |+32.000000|+0x1p+5   |+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|        +0| +0.000000|   +0x0p+0|+0.000000e0|       +32|+32.000000|   +0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|         0|  0.000000|0x    0p+0|0.000000e0|        32| 32.000000|0x    1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|0         |0.000000  |0x0p+0    |0.000000e0|32        |32.000000 |0x1p+5    |3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|         0|  0.000000|    0x0p+0|0.000000e0|        32| 32.000000|    0x1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|\n"), en_out.str());
    ASSERT_EQ(std::string("|-1003,1415|-1003,141500|-0X1.F5921CAC08312P+9|-1,003142E3|-64|-64,000000|-0X1P+6|-6,400000E1|0|0,000000|0X0P+0|0,000000E0|32|32,000000|0X1P+5|3,200000E1|1002,71828|1002,718280|0X1.F55BF0995AAF8P+9|1,002718E3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64.|-64,000000|-0x1.p+6|-6,400000e1|0.|0,000000|0x0.p+0|0,000000e0|32.|32,000000|0x1.p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|+        0|+ 0,000000|+   0x0p+0|+0,000000e0|+       32|+32,000000|+   0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|+0        |+0,000000 |+0x0p+0   |+0,000000e0|+32       |+32,000000|+0x1p+5   |+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|        +0| +0,000000|   +0x0p+0|+0,000000e0|       +32|+32,000000|   +0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|         0|  0,000000|0x    0p+0|0,000000e0|        32| 32,000000|0x    1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|0         |0,000000  |0x0p+0    |0,000000e0|32        |32,000000 |0x1p+5    |3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|         0|  0,000000|    0x0p+0|0,000000e0|        32| 32,000000|    0x1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|\n"), ru_out.str());
  }

  // long double (char)
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << sizeof(long double)
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (long double)(-1003.1415)
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (long double)(-1003.1415)
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (long double)(-1003.1415)
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (long double)(-1003.1415)
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (long double)(-64.) // power of 2
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (long double)(-64.) // power of 2
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (long double)(-64.) // power of 2
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (long double)(-64.) // power of 2
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (long double)(0.)
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (long double)(0.)
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (long double)(0.)
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (long double)(0.)
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (long double)(32.) // power of 2
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (long double)(32.) // power of 2
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (long double)(32.) // power of 2
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (long double)(32.) // power of 2
           << "|" << std::defaultfloat << std::uppercase << std::noshowpoint << (long double)(1002.71828)
           << "|" << std::fixed        << std::uppercase << std::noshowpoint << (long double)(1002.71828)
           << "|" << std::hexfloat     << std::uppercase << std::noshowpoint << (long double)(1002.71828)
           << "|" << std::scientific   << std::uppercase << std::noshowpoint << (long double)(1002.71828)
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (long double)(-1003.1415)
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (long double)(-1003.1415)
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (long double)(-1003.1415)
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (long double)(-1003.1415)
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (long double)(-64.) // power of 2
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (long double)(-64.) // power of 2
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (long double)(-64.) // power of 2
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (long double)(-64.) // power of 2
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (long double)(0.)
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (long double)(0.)
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (long double)(0.)
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (long double)(0.)
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (long double)(32.) // power of 2
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (long double)(32.) // power of 2
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (long double)(32.) // power of 2
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (long double)(32.) // power of 2
           << "|" << std::defaultfloat << std::nouppercase << std::showpoint << (long double)(1002.71828)
           << "|" << std::fixed        << std::nouppercase << std::showpoint << (long double)(1002.71828)
           << "|" << std::hexfloat     << std::nouppercase << std::showpoint << (long double)(1002.71828)
           << "|" << std::scientific   << std::nouppercase << std::showpoint << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::hex << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::hex << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::hex << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::showpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::hex << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::defaultfloat << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::fixed        << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::hexfloat     << std::hex << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::internal << std::scientific   << std::oct << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::hex << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::defaultfloat << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::fixed        << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::hexfloat     << std::hex << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::left << std::scientific   << std::oct << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::hex << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(-1003.1415)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(-64.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(0.)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(32.) // power of 2
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::defaultfloat << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::fixed        << std::dec << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::hexfloat     << std::hex << std::setw(10) << (long double)(1002.71828)
           << "|" << std::showbase << std::noshowpos << std::noshowpoint << std::right << std::scientific   << std::oct << std::setw(10) << (long double)(1002.71828)
           << "|" << std::endl;
    }

    if (8 == sizeof(long double)) {
      ASSERT_EQ(std::string("8|-1003.1415|-1003.141500|-0X1.F5921CAC08312P+9|-1.003142E3|-64|-64.000000|-0X1P+6|-6.400000E1|0|0.000000|0X0P+0|0.000000E0|32|32.000000|0X1P+5|3.200000E1|1002.71828|1002.718280|0X1.F55BF0995AAF8P+9|1.002718E3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64.|-64.000000|-0x1.p+6|-6.400000e1|0.|0.000000|0x0.p+0|0.000000e0|32.|32.000000|0x1.p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|+        0|+ 0.000000|+   0x0p+0|+0.000000e0|+       32|+32.000000|+   0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|+0        |+0.000000 |+0x0p+0   |+0.000000e0|+32       |+32.000000|+0x1p+5   |+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|        +0| +0.000000|   +0x0p+0|+0.000000e0|       +32|+32.000000|   +0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|         0|  0.000000|0x    0p+0|0.000000e0|        32| 32.000000|0x    1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|0         |0.000000  |0x0p+0    |0.000000e0|32        |32.000000 |0x1p+5    |3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|         0|  0.000000|    0x0p+0|0.000000e0|        32| 32.000000|    0x1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|\n"), c_out.str());
      ASSERT_EQ(std::string("8|-1003,1415|-1003,141500|-0X1.F5921CAC08312P+9|-1,003142E3|-64|-64,000000|-0X1P+6|-6,400000E1|0|0,000000|0X0P+0|0,000000E0|32|32,000000|0X1P+5|3,200000E1|1002,71828|1002,718280|0X1.F55BF0995AAF8P+9|1,002718E3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64.|-64,000000|-0x1.p+6|-6,400000e1|0.|0,000000|0x0.p+0|0,000000e0|32.|32,000000|0x1.p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|+        0|+ 0,000000|+   0x0p+0|+0,000000e0|+       32|+32,000000|+   0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|+0        |+0,000000 |+0x0p+0   |+0,000000e0|+32       |+32,000000|+0x1p+5   |+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|        +0| +0,000000|   +0x0p+0|+0,000000e0|       +32|+32,000000|   +0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|         0|  0,000000|0x    0p+0|0,000000e0|        32| 32,000000|0x    1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|0         |0,000000  |0x0p+0    |0,000000e0|32        |32,000000 |0x1p+5    |3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|         0|  0,000000|    0x0p+0|0,000000e0|        32| 32,000000|    0x1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|\n"), de_out.str());
      ASSERT_EQ(std::string("8|-1003.1415|-1003.141500|-0X1.F5921CAC08312P+9|-1.003142E3|-64|-64.000000|-0X1P+6|-6.400000E1|0|0.000000|0X0P+0|0.000000E0|32|32.000000|0X1P+5|3.200000E1|1002.71828|1002.718280|0X1.F55BF0995AAF8P+9|1.002718E3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64.|-64.000000|-0x1.p+6|-6.400000e1|0.|0.000000|0x0.p+0|0.000000e0|32.|32.000000|0x1.p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|+        0|+ 0.000000|+   0x0p+0|+0.000000e0|+       32|+32.000000|+   0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|+0        |+0.000000 |+0x0p+0   |+0.000000e0|+32       |+32.000000|+0x1p+5   |+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|        +0| +0.000000|   +0x0p+0|+0.000000e0|       +32|+32.000000|   +0x1p+5|+3.200000e1|+1002.71828|+1002.718280|+0x1.f55bf0995aaf8p+9|+1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-       64|-64.000000|-   0x1p+6|-6.400000e1|         0|  0.000000|0x    0p+0|0.000000e0|        32| 32.000000|0x    1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|-64       |-64.000000|-0x1p+6   |-6.400000e1|0         |0.000000  |0x0p+0    |0.000000e0|32        |32.000000 |0x1p+5    |3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|-1003.1415|-1003.141500|-0x1.f5921cac08312p+9|-1.003142e3|       -64|-64.000000|   -0x1p+6|-6.400000e1|         0|  0.000000|    0x0p+0|0.000000e0|        32| 32.000000|    0x1p+5|3.200000e1|1002.71828|1002.718280|0x1.f55bf0995aaf8p+9|1.002718e3|\n"), en_out.str());
      ASSERT_EQ(std::string("8|-1003,1415|-1003,141500|-0X1.F5921CAC08312P+9|-1,003142E3|-64|-64,000000|-0X1P+6|-6,400000E1|0|0,000000|0X0P+0|0,000000E0|32|32,000000|0X1P+5|3,200000E1|1002,71828|1002,718280|0X1.F55BF0995AAF8P+9|1,002718E3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64.|-64,000000|-0x1.p+6|-6,400000e1|0.|0,000000|0x0.p+0|0,000000e0|32.|32,000000|0x1.p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|+        0|+ 0,000000|+   0x0p+0|+0,000000e0|+       32|+32,000000|+   0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|+0        |+0,000000 |+0x0p+0   |+0,000000e0|+32       |+32,000000|+0x1p+5   |+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|        +0| +0,000000|   +0x0p+0|+0,000000e0|       +32|+32,000000|   +0x1p+5|+3,200000e1|+1002,71828|+1002,718280|+0x1.f55bf0995aaf8p+9|+1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-       64|-64,000000|-   0x1p+6|-6,400000e1|         0|  0,000000|0x    0p+0|0,000000e0|        32| 32,000000|0x    1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|-64       |-64,000000|-0x1p+6   |-6,400000e1|0         |0,000000  |0x0p+0    |0,000000e0|32        |32,000000 |0x1p+5    |3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|-1003,1415|-1003,141500|-0x1.f5921cac08312p+9|-1,003142e3|       -64|-64,000000|   -0x1p+6|-6,400000e1|         0|  0,000000|    0x0p+0|0,000000e0|        32| 32,000000|    0x1p+5|3,200000e1|1002,71828|1002,718280|0x1.f55bf0995aaf8p+9|1,002718e3|\n"), ru_out.str());
    } else {
      ASSERT_EQ(std::string("16|-1003.1415|-1003.141500|-0XF.AC90E5604189P+6|-1.003142E3|-64|-64.000000|-0X8P+3|-6.400000E1|0|0.000000|0X0P+0|0.000000E0|32|32.000000|0X8P+2|3.200000E1|1002.71828|1002.718280|0XF.AADF84CAD57CP+6|1.002718E3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-64.|-64.000000|-0x8.p+3|-6.400000e1|0.|0.000000|0x0.p+0|0.000000e0|32.|32.000000|0x8.p+2|3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-       64|-64.000000|-   0x8p+3|-6.400000e1|+        0|+ 0.000000|+   0x0p+0|+0.000000e0|+       32|+32.000000|+   0x8p+2|+3.200000e1|+1002.71828|+1002.718280|+0xf.aadf84cad57cp+6|+1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-64       |-64.000000|-0x8p+3   |-6.400000e1|+0        |+0.000000 |+0x0p+0   |+0.000000e0|+32       |+32.000000|+0x8p+2   |+3.200000e1|+1002.71828|+1002.718280|+0xf.aadf84cad57cp+6|+1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|       -64|-64.000000|   -0x8p+3|-6.400000e1|        +0| +0.000000|   +0x0p+0|+0.000000e0|       +32|+32.000000|   +0x8p+2|+3.200000e1|+1002.71828|+1002.718280|+0xf.aadf84cad57cp+6|+1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-       64|-64.000000|-   0x8p+3|-6.400000e1|         0|  0.000000|0x    0p+0|0.000000e0|        32| 32.000000|0x    8p+2|3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-64       |-64.000000|-0x8p+3   |-6.400000e1|0         |0.000000  |0x0p+0    |0.000000e0|32        |32.000000 |0x8p+2    |3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|       -64|-64.000000|   -0x8p+3|-6.400000e1|         0|  0.000000|    0x0p+0|0.000000e0|        32| 32.000000|    0x8p+2|3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|\n"), c_out.str());
      ASSERT_EQ(std::string("16|-1003,1415|-1003,141500|-0XF.AC90E5604189P+6|-1,003142E3|-64|-64,000000|-0X8P+3|-6,400000E1|0|0,000000|0X0P+0|0,000000E0|32|32,000000|0X8P+2|3,200000E1|1002,71828|1002,718280|0XF.AADF84CAD57CP+6|1,002718E3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-64.|-64,000000|-0x8.p+3|-6,400000e1|0.|0,000000|0x0.p+0|0,000000e0|32.|32,000000|0x8.p+2|3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-       64|-64,000000|-   0x8p+3|-6,400000e1|+        0|+ 0,000000|+   0x0p+0|+0,000000e0|+       32|+32,000000|+   0x8p+2|+3,200000e1|+1002,71828|+1002,718280|+0xf.aadf84cad57cp+6|+1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-64       |-64,000000|-0x8p+3   |-6,400000e1|+0        |+0,000000 |+0x0p+0   |+0,000000e0|+32       |+32,000000|+0x8p+2   |+3,200000e1|+1002,71828|+1002,718280|+0xf.aadf84cad57cp+6|+1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|       -64|-64,000000|   -0x8p+3|-6,400000e1|        +0| +0,000000|   +0x0p+0|+0,000000e0|       +32|+32,000000|   +0x8p+2|+3,200000e1|+1002,71828|+1002,718280|+0xf.aadf84cad57cp+6|+1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-       64|-64,000000|-   0x8p+3|-6,400000e1|         0|  0,000000|0x    0p+0|0,000000e0|        32| 32,000000|0x    8p+2|3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-64       |-64,000000|-0x8p+3   |-6,400000e1|0         |0,000000  |0x0p+0    |0,000000e0|32        |32,000000 |0x8p+2    |3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|       -64|-64,000000|   -0x8p+3|-6,400000e1|         0|  0,000000|    0x0p+0|0,000000e0|        32| 32,000000|    0x8p+2|3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|\n"), de_out.str());
      ASSERT_EQ(std::string("16|-1003.1415|-1003.141500|-0XF.AC90E5604189P+6|-1.003142E3|-64|-64.000000|-0X8P+3|-6.400000E1|0|0.000000|0X0P+0|0.000000E0|32|32.000000|0X8P+2|3.200000E1|1002.71828|1002.718280|0XF.AADF84CAD57CP+6|1.002718E3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-64.|-64.000000|-0x8.p+3|-6.400000e1|0.|0.000000|0x0.p+0|0.000000e0|32.|32.000000|0x8.p+2|3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-       64|-64.000000|-   0x8p+3|-6.400000e1|+        0|+ 0.000000|+   0x0p+0|+0.000000e0|+       32|+32.000000|+   0x8p+2|+3.200000e1|+1002.71828|+1002.718280|+0xf.aadf84cad57cp+6|+1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-64       |-64.000000|-0x8p+3   |-6.400000e1|+0        |+0.000000 |+0x0p+0   |+0.000000e0|+32       |+32.000000|+0x8p+2   |+3.200000e1|+1002.71828|+1002.718280|+0xf.aadf84cad57cp+6|+1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|       -64|-64.000000|   -0x8p+3|-6.400000e1|        +0| +0.000000|   +0x0p+0|+0.000000e0|       +32|+32.000000|   +0x8p+2|+3.200000e1|+1002.71828|+1002.718280|+0xf.aadf84cad57cp+6|+1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-       64|-64.000000|-   0x8p+3|-6.400000e1|         0|  0.000000|0x    0p+0|0.000000e0|        32| 32.000000|0x    8p+2|3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|-64       |-64.000000|-0x8p+3   |-6.400000e1|0         |0.000000  |0x0p+0    |0.000000e0|32        |32.000000 |0x8p+2    |3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|-1003.1415|-1003.141500|-0xf.ac90e5604189p+6|-1.003142e3|       -64|-64.000000|   -0x8p+3|-6.400000e1|         0|  0.000000|    0x0p+0|0.000000e0|        32| 32.000000|    0x8p+2|3.200000e1|1002.71828|1002.718280|0xf.aadf84cad57cp+6|1.002718e3|\n"), en_out.str());
      ASSERT_EQ(std::string("16|-1003,1415|-1003,141500|-0XF.AC90E5604189P+6|-1,003142E3|-64|-64,000000|-0X8P+3|-6,400000E1|0|0,000000|0X0P+0|0,000000E0|32|32,000000|0X8P+2|3,200000E1|1002,71828|1002,718280|0XF.AADF84CAD57CP+6|1,002718E3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-64.|-64,000000|-0x8.p+3|-6,400000e1|0.|0,000000|0x0.p+0|0,000000e0|32.|32,000000|0x8.p+2|3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-       64|-64,000000|-   0x8p+3|-6,400000e1|+        0|+ 0,000000|+   0x0p+0|+0,000000e0|+       32|+32,000000|+   0x8p+2|+3,200000e1|+1002,71828|+1002,718280|+0xf.aadf84cad57cp+6|+1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-64       |-64,000000|-0x8p+3   |-6,400000e1|+0        |+0,000000 |+0x0p+0   |+0,000000e0|+32       |+32,000000|+0x8p+2   |+3,200000e1|+1002,71828|+1002,718280|+0xf.aadf84cad57cp+6|+1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|       -64|-64,000000|   -0x8p+3|-6,400000e1|        +0| +0,000000|   +0x0p+0|+0,000000e0|       +32|+32,000000|   +0x8p+2|+3,200000e1|+1002,71828|+1002,718280|+0xf.aadf84cad57cp+6|+1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-       64|-64,000000|-   0x8p+3|-6,400000e1|         0|  0,000000|0x    0p+0|0,000000e0|        32| 32,000000|0x    8p+2|3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|-64       |-64,000000|-0x8p+3   |-6,400000e1|0         |0,000000  |0x0p+0    |0,000000e0|32        |32,000000 |0x8p+2    |3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|-1003,1415|-1003,141500|-0xf.ac90e5604189p+6|-1,003142e3|       -64|-64,000000|   -0x8p+3|-6,400000e1|         0|  0,000000|    0x0p+0|0,000000e0|        32| 32,000000|    0x8p+2|3,200000e1|1002,71828|1002,718280|0xf.aadf84cad57cp+6|1,002718e3|\n"), ru_out.str());
    }
  }
#endif

  // const void* (char)
  {
    std::ostringstream c_out;
    std::ostringstream de_out;
    std::ostringstream en_out;
    std::ostringstream ru_out;
    std::vector<std::ostringstream*> v = { &c_out, &de_out, &en_out, &ru_out };

    c_out.imbue(c);
    de_out.imbue(de);
    en_out.imbue(en);
    ru_out.imbue(ru);

    for (auto out: v) {
      *out << "|" << std::uppercase << (const void*)(0)
           << "|" << std::uppercase << (const void*)(1234)
           << "|" << std::nouppercase << (const void*)(0)
           << "|" << std::nouppercase << (const void*)(1234)
           << "|" << std::showbase << std::showpos << std::internal << std::setw(20) << (const void*)(0)
           << "|" << std::showbase << std::showpos << std::internal << std::setw(20) << (const void*)(1234)
           << "|" << std::showbase << std::showpos << std::left << std::setw(20) << (const void*)(0)
           << "|" << std::showbase << std::showpos << std::left << std::setw(20) << (const void*)(1234)
           << "|" << std::showbase << std::showpos << std::right << std::setw(20) << (const void*)(0)
           << "|" << std::showbase << std::showpos << std::right << std::setw(20) << (const void*)(1234)
           << "|" << std::showbase << std::noshowpos << std::internal << std::setw(20) << (const void*)(0)
           << "|" << std::showbase << std::noshowpos << std::internal << std::setw(20) << (const void*)(1234)
           << "|" << std::showbase << std::noshowpos << std::left << std::setw(20) << (const void*)(0)
           << "|" << std::showbase << std::noshowpos << std::left << std::setw(20) << (const void*)(1234)
           << "|" << std::showbase << std::noshowpos << std::right << std::setw(20) << (const void*)(0)
           << "|" << std::showbase << std::noshowpos << std::right << std::setw(20) << (const void*)(1234)
           << "|" << std::endl;
    }

    ASSERT_EQ(std::string("|0000000000000000|00000000000004D2|0000000000000000|00000000000004d2|+ 0x0000000000000000|+ 0x00000000000004d2|+0x0000000000000000 |+0x00000000000004d2 | +0x0000000000000000| +0x00000000000004d2|0x  0000000000000000|0x  00000000000004d2|0x0000000000000000  |0x00000000000004d2  |  0x0000000000000000|  0x00000000000004d2|\n"), c_out.str());
    ASSERT_EQ(std::string("|0000000000000000|00000000000004D2|0000000000000000|00000000000004d2|+ 0x0000000000000000|+ 0x00000000000004d2|+0x0000000000000000 |+0x00000000000004d2 | +0x0000000000000000| +0x00000000000004d2|0x  0000000000000000|0x  00000000000004d2|0x0000000000000000  |0x00000000000004d2  |  0x0000000000000000|  0x00000000000004d2|\n"), de_out.str());
    ASSERT_EQ(std::string("|0000000000000000|00000000000004D2|0000000000000000|00000000000004d2|+ 0x0000000000000000|+ 0x00000000000004d2|+0x0000000000000000 |+0x00000000000004d2 | +0x0000000000000000| +0x00000000000004d2|0x  0000000000000000|0x  00000000000004d2|0x0000000000000000  |0x00000000000004d2  |  0x0000000000000000|  0x00000000000004d2|\n"), en_out.str());
    ASSERT_EQ(std::string("|0000000000000000|00000000000004D2|0000000000000000|00000000000004d2|+ 0x0000000000000000|+ 0x00000000000004d2|+0x0000000000000000 |+0x00000000000004d2 | +0x0000000000000000| +0x00000000000004d2|0x  0000000000000000|0x  00000000000004d2|0x0000000000000000  |0x00000000000004d2  |  0x0000000000000000|  0x00000000000004d2|\n"), ru_out.str());
  }
}
