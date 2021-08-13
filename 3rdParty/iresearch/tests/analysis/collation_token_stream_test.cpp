////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include <unicode/coll.h>
#include <unicode/sortkey.h>

#include "analysis/collation_token_stream.hpp"
#include "utils/locale_utils.hpp"

TEST(collation_token_stream_test, consts) {
  static_assert("collation" == irs::type<irs::analysis::collation_token_stream>::name());
}

TEST(collation_token_stream_test, construct) {
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({ "locale": "en-EN.UTF-8" })");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::text>::get(),
      "en-EN");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "{\"locale\":1}"));
  }
}

TEST(collation_token_stream_test, check_tokens_utf8) {
  auto err = UErrorCode::U_ZERO_ERROR;

  constexpr irs::string_ref locale_name = "en-EN.UTF-8";

  const auto locale = irs::locale_utils::locale(locale_name, irs::string_ref::NIL, true);

  const icu::Locale icu_locale{
    std::string(irs::locale_utils::language(locale)).c_str(),
    std::string(irs::locale_utils::country(locale)).c_str() };

  icu::CollationKey key;
  std::unique_ptr<icu::Collator> coll{
    icu::Collator::createInstance(icu_locale, err) };
  ASSERT_NE(nullptr, coll);
  ASSERT_TRUE(U_SUCCESS(err));

  auto get_collation_key = [&](irs::string_ref data) -> irs::bytes_ref {
    err = UErrorCode::U_ZERO_ERROR;
    coll->getCollationKey(
      icu::UnicodeString::fromUTF8(
        icu::StringPiece{data.c_str(), static_cast<int32_t>(data.size())}),
      key, err);
    EXPECT_TRUE(U_SUCCESS(err));

    int32_t size = 0;
    const irs::byte_type* p = key.getByteArray(size);
    EXPECT_NE(nullptr, p);
    EXPECT_NE(0, size);

    return { p, static_cast<size_t>(size)-1 };
  };

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({ "locale" : "en.UTF-8" })");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      const irs::string_ref data{irs::string_ref::NIL};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }

    {
      const irs::string_ref data{irs::string_ref::EMPTY};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }

    {
      constexpr irs::string_ref data{"quick"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }

    {
      constexpr irs::string_ref data{"foo"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }

    {
      constexpr irs::string_ref data{"the quick Brown fox jumps over the lazy dog"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }
}

TEST(collation_token_stream_test, check_tokens) {
  auto err = UErrorCode::U_ZERO_ERROR;

  constexpr irs::string_ref locale_name = "de-DE";

  const auto locale = irs::locale_utils::locale(locale_name, irs::string_ref::NIL, true);

  const icu::Locale icu_locale{
    std::string(irs::locale_utils::language(locale)).c_str(),
    std::string(irs::locale_utils::country(locale)).c_str() };

  icu::CollationKey key;
  std::unique_ptr<icu::Collator> coll{
    icu::Collator::createInstance(icu_locale, err) };
  ASSERT_NE(nullptr, coll);
  ASSERT_TRUE(U_SUCCESS(err));

  auto get_collation_key = [&](irs::string_ref data) -> irs::bytes_ref {
    err = UErrorCode::U_ZERO_ERROR;
    coll->getCollationKey(
      icu::UnicodeString::fromUTF8(
        icu::StringPiece{data.c_str(), static_cast<int32_t>(data.size())}),
      key, err);
    EXPECT_TRUE(U_SUCCESS(err));

    int32_t size = 0;
    const irs::byte_type* p = key.getByteArray(size);
    EXPECT_NE(nullptr, p);
    EXPECT_NE(0, size);

    return { p, static_cast<size_t>(size)-1 };
  };

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({ "locale" : "de_DE" })");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      std::wstring unicodeData = L"\u00F6\u00F5";

      std::string data;
      ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, unicodeData, locale));

      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }
}
