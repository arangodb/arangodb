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

#include "analysis/text_token_stemming_stream.hpp"

#include "gtest/gtest.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

namespace {

class stemming_token_stream_tests: public ::testing::Test { };

} // namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST_F(stemming_token_stream_tests, consts) {
  static_assert("stem" == irs::type<irs::analysis::stemming_token_stream>::name());
}

TEST_F(stemming_token_stream_tests, test_stemming) {
  // test stemming (locale irs::string_ref::NIL)
  // there is no Snowball stemmer for "C" locale
  {
    irs::analysis::stemming_token_stream::options_t opts;
    opts.locale = icu::Locale{"C"};

    irs::string_ref data("running");
    irs::analysis::stemming_token_stream stream(opts);
    ASSERT_EQ(irs::type<irs::analysis::stemming_token_stream>::id(), stream.type());

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test stemming (stemmer exists)
  {
    irs::string_ref data("running");

    irs::analysis::stemming_token_stream::options_t opts;
    opts.locale = icu::Locale::createFromName("en");

    irs::analysis::stemming_token_stream stream(opts);

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("run", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test stemming (stemmer does not exist)
  // there is no Snowball stemmer for Chinese
  {
    irs::string_ref data("running");

    irs::analysis::stemming_token_stream::options_t opts;
    opts.locale = icu::Locale::createFromName("zh");

    irs::analysis::stemming_token_stream stream(opts);

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(stemming_token_stream_tests, test_load) {
  // load jSON object
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get(
      "stem", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
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
    auto stream = irs::analysis::analyzers::get("stem", irs::type<irs::text_format::json>::get(), R"({ "locale":"en" })");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("run", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }
}


TEST_F(stemming_token_stream_tests, test_make_config_json) {
  //with unknown parameter
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"invalid_parameter\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stem", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru\"}")->toString(), actual);
  }

  // test vpack
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"invalid_parameter\":true}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "stem", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru\"}")->toString(), out_slice.toString());
  }

  // test vpack with variant
  {
    std::string config = "{\"locale\":\"ru_RU_TRADITIONAL.UTF-8\",\"invalid_parameter\":true}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "stem", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru\"}")->toString(), out_slice.toString());
  }
}

TEST_F(stemming_token_stream_tests, test_invalid_locale) {
  auto stream = irs::analysis::analyzers::get(
      "stem", irs::type<irs::text_format::json>::get(),
      "{\"locale\":\"invalid12345.UTF-8\"}");
  ASSERT_EQ(nullptr, stream);
}

TEST_F(stemming_token_stream_tests, test_make_config_text) {
  std::string config = "RU";
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stem", irs::type<irs::text_format::text>::get(), config));
}
