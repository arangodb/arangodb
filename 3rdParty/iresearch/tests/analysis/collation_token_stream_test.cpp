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

#include "analysis/collation_token_stream.hpp"

#include <unicode/coll.h>
#include <unicode/sortkey.h>
#include <unicode/locid.h>

#include "tests_shared.hpp"
#include "utils/locale_utils.hpp"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

TEST(collation_token_stream_test, consts) {
  static_assert("collation" == irs::type<irs::analysis::collation_token_stream>::name());
}

TEST(collation_token_stream_test, construct_from_str) {
  // json
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "en-US"})");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de_DE_phonebook"})");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de_DE.utf-8@phonebook"})");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de_DE.UTF-8@collation=phonebook"})");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  // invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), R"({ "locale" : "en.utf-16"})"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), R"({ "locale" : "de-DE.ascii@phonebook"})"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), R"({ "locale" : "de_DE.utf-32@collation=phonebook"})"));
  }
}

TEST(collation_token_stream_test, construct_from_obj) {
  // json
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({ "locale": {"language" : "en", "country": "US", "encoding" : "UTF-8"}})");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({ "locale": {"language" : "en", "country": "US", "variant" : "phonebook", "encoding" : "utf-8"}})");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({ "locale": {"country": "US"}})");
    ASSERT_EQ(nullptr, stream);
  }

  // vpack
  {
    std::string config = R"({ "locale": {"language" : "en", "country": "US", "variant" : "phonebook"}})";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    irs::string_ref in_str(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    auto stream = irs::analysis::analyzers::get("collation", irs::type<irs::text_format::vpack>::get(), in_str);
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    std::string config = R"({ "locale": {"language" : "en", "variant" : "phonebook"}})";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    irs::string_ref in_str(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    auto stream = irs::analysis::analyzers::get("collation", irs::type<irs::text_format::vpack>::get(), in_str);
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::collation_token_stream>::id(), stream->type());
  }

  {
    std::string config = R"({ "locale": {"variant" : "phonebook"}})";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    irs::string_ref in_str(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    auto stream = irs::analysis::analyzers::get("collation", irs::type<irs::text_format::vpack>::get(), in_str);
    ASSERT_EQ(nullptr, stream);
  }

  {
    std::string config = R"({ "locale": {"country": "US"}})";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    irs::string_ref in_str(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    auto stream = irs::analysis::analyzers::get("collation", irs::type<irs::text_format::vpack>::get(), in_str);
    ASSERT_EQ(nullptr, stream);
  }

  // invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), R"({ "locale" : {"language" : "de_DE", "variant" : "_phonebook", "encoding" : "utf-32"}})"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), R"({ "locale" : {"language" : 123}})"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("collation", irs::type<irs::text_format::json>::get(), R"({ "locale": {"language" : "en", "country": "US", "variant" : "phonebook", "encoding" : "ascii"}})"));
  }
}

TEST(collation_token_stream_test, check_collation) {

  auto err = UErrorCode::U_ZERO_ERROR;

  constexpr irs::string_ref locale_name = R"(en)";
  const icu::Locale icu_locale = icu::Locale::createFromName(locale_name.c_str());

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
      R"({"locale": {"language" : "en"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"å b z a"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "sv"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"a å b z"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);

      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }
}

// collation defined by keywords
TEST(collation_token_stream_test, check_collation_with_variant1) {

  auto err = UErrorCode::U_ZERO_ERROR;

  constexpr irs::string_ref locale_name = R"(de@collation=phonebook)";
  const icu::Locale icu_locale = icu::Locale::createFromName(locale_name.c_str());

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

  // locale defined as object
  // different variants, should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de", "variant" : "pinyan"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as string
  // different variants, should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de_pinyan"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as string
  // different variants, should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de@pinyan"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as string
  // different variants, should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de@collation=pinyan"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as object
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de", "variant" : "_phonebook"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as object
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de_phonebook"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as object
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de@collation=phonebook"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as object
  // same variants, but collation is ignored, because input as old string. should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de@collation=phonebook"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as string
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de_phonebook"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
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

TEST(collation_token_stream_test, check_collation_with_variant2) {

  auto err = UErrorCode::U_ZERO_ERROR;

  constexpr irs::string_ref locale_name = "de_phonebook";
  const icu::Locale icu_locale = icu::Locale::createFromName(locale_name.c_str());

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

  // locale defined as object
  // different variants, should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de", "variant" : "pinyan"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as object
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de", "variant" : "_phonebook"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as object
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale": {"language" : "de", "variant" : "@collation=phonebook"}})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as string
  // same variants, should be EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de_phonebook"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_EQ(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }

  // locale defined as string
  // same variants, but collation is ignored, because input as old string. should be NOT EQUAL
  {
    auto stream = irs::analysis::analyzers::get(
      "collation", irs::type<irs::text_format::json>::get(),
      R"({"locale" : "de@collation=phonebook"})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      constexpr irs::string_ref data{"Ärger Ast Aerosol Abbruch Aqua Afrika"};
      ASSERT_TRUE(stream->reset(data));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(data.size(), offset->end);
      ASSERT_NE(get_collation_key(data), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }
}

TEST(collation_token_stream_test, check_tokens_utf8) {
  auto err = UErrorCode::U_ZERO_ERROR;

  constexpr irs::string_ref locale_name = "en-EN.UTF-8";

  const auto icu_locale =icu::Locale::createFromName(locale_name.c_str());

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
      R"({ "locale" : {"language" : "en" }})");

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

  const auto icu_locale =icu::Locale::createFromName(locale_name.c_str());

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
      R"({ "locale" : {"language" : "de_DE" }})");

    ASSERT_NE(nullptr, stream);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    {
      std::string unicodeData = "\xE2\x82\xAC";

      ASSERT_TRUE(stream->reset(unicodeData));
      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(unicodeData.size(), offset->end);
      ASSERT_EQ(get_collation_key(unicodeData), term->value);
      ASSERT_EQ(1, inc->value);
      ASSERT_FALSE(stream->next());
    }
  }
}

TEST(collation_token_stream_test, normalize) {
  {
    std::string config = R"({ "locale" : {"language" : "de_DE", "variant" : "_phonebook", "encoding" : "utf-8"}})";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "collation", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson(R"({ "locale" : {"language" : "de", "country" : "DE", "variant" : "PHONEBOOK","encoding":"utf-8"}})")->toString(), actual);
  }

  {
    std::string config = R"({ "locale" : {"language" : "de_DE", "321variant" : "phonebook", "encoding" : "utf-8"}})";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "collation", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson(R"({ "locale" : {"language" : "de", "country" : "DE","encoding":"utf-8"}})")->toString(), actual);
  }

  {
    std::string config = R"({ "locale" : {"language" : "de_DE", "321variant" : "phonebook"}})";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "collation", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson(R"({ "locale" : {"language" : "de", "country" : "DE","encoding":"utf-8"}})")->toString(), actual);
  }

  {
    std::string config = R"({ "locale" : {"language" : "de_DE", "321variant" : "phonebook", "encoding" : "utf-8"}})";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "collation", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson(R"({ "locale" : {"language" : "de", "country" : "DE", "encoding":"utf-8"}})")->toString(), out_slice.toString());
  }
}
