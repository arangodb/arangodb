////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "analysis/text_token_stemming_stream.hpp"
#include "utils/locale_utils.hpp"

namespace {

class text_token_stemming_stream_tests: public ::testing::Test { };

} // namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST_F(text_token_stemming_stream_tests, consts) {
  static_assert("stem" == irs::type<irs::analysis::text_token_stemming_stream>::name());
}

TEST_F(text_token_stemming_stream_tests, test_stemming) {
  // test stemming (locale irs::string_ref::NIL)
  // there is no Snowball stemmer for "C" locale
  {
    irs::string_ref data("running");
    irs::analysis::text_token_stemming_stream stream(
        irs::locale_utils::locale(irs::string_ref::NIL));
    ASSERT_EQ(irs::type<irs::analysis::text_token_stemming_stream>::id(), stream.type());

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test stemming (stemmer exists)
  {
    irs::string_ref data("running");
    irs::analysis::text_token_stemming_stream stream(
        irs::locale_utils::locale("en"));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test stemming (stemmer does not exist)
  // there is no Snowball stemmer for Chinese
  {
    irs::string_ref data("running");
    irs::analysis::text_token_stemming_stream stream(
        irs::locale_utils::locale("zh"));

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(text_token_stemming_stream_tests, test_load) {
  // load jSON string
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), "\"en\"");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // load jSON object
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), "{\"locale\":1}"));
  }

  // load text
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("stem", irs::type<irs::text_format::text>::get(), "en");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }
}


TEST_F(text_token_stemming_stream_tests, test_make_config_json) {
  //with unknown parameter
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"invalid_parameter\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stem", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\"}", actual);
  }
}

TEST_F(text_token_stemming_stream_tests, test_invalid_locale) {
  auto stream = irs::analysis::analyzers::get(
      "stem", irs::type<irs::text_format::json>::get(),
      "{\"locale\":\"invalid12345.UTF-8\"}");
  ASSERT_EQ(nullptr, stream);
}

TEST_F(text_token_stemming_stream_tests, test_make_config_text) {
  std::string config = "RU";
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stem", irs::type<irs::text_format::text>::get(), config));
  ASSERT_EQ("ru", actual);
}

TEST_F(text_token_stemming_stream_tests, test_make_config_invalid_format) {
  std::string config = "ru_RU.utfF-8";
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stem", irs::type<irs::text_format::csv>::get(), config));
}
