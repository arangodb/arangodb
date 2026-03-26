////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "analysis/delimited_token_stream.hpp"
#include "gtest/gtest.h"
#include "tests_config.hpp"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

namespace {

class delimited_token_stream_tests : public ::testing::Test {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before
    // each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the
    // destructor).
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(delimited_token_stream_tests, consts) {
  static_assert("delimiter" ==
                irs::type<irs::analysis::delimited_token_stream>::name());
}

TEST_F(delimited_token_stream_tests, test_delimiter) {
  // test delimiter std::string_view{}
  {
    std::string_view data("abc,def\"\",\"\"ghi");
    irs::analysis::delimited_token_stream stream(std::string_view{});
    ASSERT_EQ(irs::type<irs::analysis::delimited_token_stream>::id(),
              stream.type());

    ASSERT_TRUE(stream.reset(data));

    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ("abc,def\"\",\"\"ghi", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test delimteter ''
  {
    std::string_view data("abc,\"def\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(1, offset->end);
    ASSERT_EQ("a", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(1, offset->start);
    ASSERT_EQ(2, offset->end);
    ASSERT_EQ("b", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(2, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("c", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(3, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ(",", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter ','
  {
    std::string_view data("abc,\"def,\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(10, offset->end);
    ASSERT_EQ("def,", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter '\t'
  {
    std::string_view data(
      "abc,\t\"def\t\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("\t");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("abc,", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(5, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("def\t", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter '"'
  {
    std::string_view data(
      "abc,\"\"def\t\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("\"");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("abc,", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(5, offset->start);
    ASSERT_EQ(5, offset->end);
    ASSERT_EQ("", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(6, offset->start);
    ASSERT_EQ(10, offset->end);
    ASSERT_EQ("def\t", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(11, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter 'abc'
  {
    std::string_view data(
      "abc,123\"def123\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("123");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("abc,", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(7, offset->start);
    ASSERT_EQ(15, offset->end);
    ASSERT_EQ("def123", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

TEST_F(delimited_token_stream_tests, test_quote) {
  // test quoted field
  {
    std::string_view data(
      "abc,\"def\",\"\"ghi");  // quoted terms should be honoured

    auto testFunc = [](std::string_view data,
                       irs::analysis::analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* offset = irs::get<irs::offset>(*pStream);
      auto* payload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, payload);
      auto* term = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(3, offset->end);
      ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(4, offset->start);
      ASSERT_EQ(9, offset->end);
      ASSERT_EQ("def", irs::ViewCast<char>(term->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(10, offset->start);
      ASSERT_EQ(15, offset->end);
      ASSERT_EQ("\"\"ghi", irs::ViewCast<char>(term->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::delimited_token_stream stream(",");
      testFunc(data, &stream);
    }
    {
      auto stream = irs::analysis::analyzers::get(
        "delimiter", irs::type<irs::text_format::json>::get(),
        "{\"delimiter\":\",\"}");
      testFunc(data, stream.get());
    }
  }

  // test unterminated "
  {
    std::string_view data(
      "abc,\"def\",\"ghi");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(14, offset->end);
    ASSERT_EQ("\"ghi", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test unterminated single "
  {
    std::string_view data("abc,\"def\",\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("\"", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test " escape
  {
    std::string_view data(
      "abc,\"\"\"def\",\"\"ghi");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("\"def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(12, offset->start);
    ASSERT_EQ(17, offset->end);
    ASSERT_EQ("\"\"ghi", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test non-quoted field with "
  {
    std::string_view data(
      "abc,\"def\",ghi\"");  // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(14, offset->end);
    ASSERT_EQ("ghi\"", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

TEST_F(delimited_token_stream_tests, test_load) {
  // load jSON string
  {
    std::string_view data("abc,def,ghi");  // quoted terms should be honoured
    auto stream = irs::analysis::analyzers::get(
      "delimiter", irs::type<irs::text_format::json>::get(), "\",\"");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(8, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("ghi", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // load jSON object
  {
    std::string_view data("abc,def,ghi");  // quoted terms should be honoured
    auto stream = irs::analysis::analyzers::get(
      "delimiter", irs::type<irs::text_format::json>::get(),
      "{\"delimiter\":\",\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(8, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("ghi", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                         "delimiter", irs::type<irs::text_format::json>::get(),
                         std::string_view{}));
    ASSERT_EQ(nullptr,
              irs::analysis::analyzers::get(
                "delimiter", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr,
              irs::analysis::analyzers::get(
                "delimiter", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr,
              irs::analysis::analyzers::get(
                "delimiter", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get(
                         "delimiter", irs::type<irs::text_format::json>::get(),
                         "{\"delimiter\":1}"));
  }

  // load text
  {
    std::string_view data("abc,def,ghi");  // quoted terms should be honoured
    auto stream = irs::analysis::analyzers::get(
      "delimiter", irs::type<irs::text_format::text>::get(), ",");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("def", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(8, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("ghi", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // load text, wide symbols
  {
    std::string_view data(
      "\x61\x62\x63\x2C\xD0\x9F");  // quoted terms should be honoured
    auto stream = irs::analysis::analyzers::get(
      "delimiter", irs::type<irs::text_format::text>::get(), ",");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ViewCast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(6, offset->end);
    ASSERT_EQ("\xD0\x9F", irs::ViewCast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }
}

TEST_F(delimited_token_stream_tests, test_make_config_json) {
  // with unknown parameter
  {
    std::string config = "{\"delimiter\":\",\",\"invalid_parameter\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
      actual, "delimiter", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"delimiter\":\",\"}")->toString(),
              actual);
  }

  // test vpack
  {
    std::string config = "{\"delimiter\":\",\",\"invalid_parameter\":true}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(),
                  in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(
      out_str, "delimiter", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson("{\"delimiter\":\",\"}")->toString(),
              out_slice.toString());
  }
}

TEST_F(delimited_token_stream_tests, test_make_config_text) {
  std::string config = ",";
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(
    actual, "delimiter", irs::type<irs::text_format::text>::get(), config));
  ASSERT_EQ(config, actual);
}
