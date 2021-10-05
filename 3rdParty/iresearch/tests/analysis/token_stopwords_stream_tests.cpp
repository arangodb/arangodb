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

#include "analysis/token_stopwords_stream.hpp"

#include "gtest/gtest.h"

#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST(token_stopwords_stream_tests, consts) {
  static_assert("stopwords" == irs::type<irs::analysis::token_stopwords_stream>::name());
}

TEST(token_stopwords_stream_tests, test_masking) {
  // test mask nothing
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("ghi");
    irs::analysis::token_stopwords_stream::stopwords_set mask;
    irs::analysis::token_stopwords_stream stream(std::move(mask));
    ASSERT_EQ(irs::type<irs::analysis::token_stopwords_stream>::id(), stream.type());

    auto* offset = irs::get<irs::offset>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data0));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());

    ASSERT_TRUE(stream.reset(data1));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test mask something
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("ghi");
    irs::analysis::token_stopwords_stream::stopwords_set mask = {"abc"};
    irs::analysis::token_stopwords_stream stream(std::move(mask));

    auto* offset = irs::get<irs::offset>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data0));
    ASSERT_FALSE(stream.next());

    ASSERT_TRUE(stream.reset(data1));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST(token_stopwords_stream_tests, test_load) {
  // load jSON array (mask string)
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("ghi");

    auto testFunc = [](const irs::string_ref& data0, const irs::string_ref& data1, irs::analysis::analyzer* stream) {
      ASSERT_NE(nullptr, stream);
      ASSERT_TRUE(stream->reset(data0));
      ASSERT_FALSE(stream->next());
      ASSERT_TRUE(stream->reset(data1));

      auto* offset = irs::get<irs::offset>(*stream);
      auto* term = irs::get<irs::term_attribute>(*stream);

      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(3, offset->end);
      ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
      ASSERT_FALSE(stream->next());
    };
    auto stream = irs::analysis::analyzers::get(
      "stopwords", irs::type<irs::text_format::json>::get(), "[ \"abc\", \"646566\", \"6D6e6F\" ]");
    testFunc(data0, data1, stream.get());

    // check with another order of mask
    auto stream2 = irs::analysis::analyzers::get(
      "stopwords", irs::type<irs::text_format::json>::get(), "[ \"6D6e6F\", \"abc\", \"646566\" ]");
    testFunc(data0, data1, stream2.get());

    auto streamFromJsonObjest = irs::analysis::analyzers::get(
      "stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[ \"abc\", \"646566\", \"6D6e6F\" ]}");
    testFunc(data0, data1, streamFromJsonObjest.get());
  }

  // load jSON object (mask string hex)
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("646566");

    auto testFunc = [](const irs::string_ref& data0, const irs::string_ref& data1, irs::analysis::analyzer* stream) {
      ASSERT_NE(nullptr, stream);
      ASSERT_TRUE(stream->reset(data0));
      ASSERT_FALSE(stream->next());
      ASSERT_TRUE(stream->reset(data1));

      auto* offset = irs::get<irs::offset>(*stream);
      auto* term = irs::get<irs::term_attribute>(*stream);

      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(6, offset->end);
      ASSERT_EQ("646566", irs::ref_cast<char>(term->value));
      ASSERT_FALSE(stream->next());
    };
    auto streamFromJsonObjest = irs::analysis::analyzers::get(
      "stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[ \"616263\", \"6D6e6F\" ], \"hex\":true}");
    ASSERT_TRUE(streamFromJsonObjest);
    testFunc(data0, data1, streamFromJsonObjest.get());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "\"abc\""));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":1, \"hex\":\"text\"}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"aa\", \"bb\"], \"hex\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"1aa\", \"bb\"], \"hex\":true}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"aaia\", \"bb\"], \"hex\":true}"));
  }
}

TEST(token_stopwords_stream_tests, normalize_invalid) {
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "1"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "\"abc\""));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "{\"case\":1}"));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"aa\", \"bb\"], \"hex\":1}"));
}

TEST(token_stopwords_stream_tests, normalize_valid_array) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "[\"QWRT\", \"qwrt\"]"));
  ASSERT_EQ(actual, "{\n  \"hex\" : false,\n  \"stopwords\" : [\n    \"QWRT\",\n    \"qwrt\"\n  ]\n}");
}

TEST(token_stopwords_stream_tests, normalize_valid_object) {
  {
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
      irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"QWRT\", \"qwrt\"]}"));
    ASSERT_EQ(actual, "{\n  \"hex\" : false,\n  \"stopwords\" : [\n    \"QWRT\",\n    \"qwrt\"\n  ]\n}");
  }


  // test vpack
  {
    std::string config = "{\"stopwords\":[\"QWRT\", \"qwrt\"]}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "stopwords", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ("{\n  \"hex\" : false,\n  \"stopwords\" : [\n    \"QWRT\",\n    \"qwrt\"\n  ]\n}", out_slice.toString());
  }
}

TEST(token_stopwords_stream_tests, normalize_valid_object_unknown) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"QWRT\", \"qwrt\"], \"unknown_field\":1}"));
  ASSERT_EQ(actual, "{\n  \"hex\" : false,\n  \"stopwords\" : [\n    \"QWRT\",\n    \"qwrt\"\n  ]\n}");
}

TEST(token_stopwords_stream_tests, normalize_valid_object_hex) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"01aB\", \"02Cd\"], \"hex\": true}"));
  ASSERT_EQ(actual, "{\n  \"hex\" : true,\n  \"stopwords\" : [\n    \"01aB\",\n    \"02Cd\"\n  ]\n}");
}

TEST(token_stopwords_stream_tests, normalize_valid_object_nohex) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"01aB\", \"02Cd\"], \"hex\": false}"));
  ASSERT_EQ(actual, "{\n  \"hex\" : false,\n  \"stopwords\" : [\n    \"01aB\",\n    \"02Cd\"\n  ]\n}");
}

