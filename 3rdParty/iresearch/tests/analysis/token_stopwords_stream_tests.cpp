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
#include "analysis/token_stopwords_stream.hpp"

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

    auto testFunc = [](const irs::string_ref& data0, const irs::string_ref& data1, irs::analysis::analyzer::ptr stream) {
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
    auto stream = irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "[ \"abc\", \"646566\", \"6D6e6F\" ]");
    testFunc(data0, data1, stream);

    auto streamFromJsonObjest = irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[ \"abc\", \"646566\", \"6D6e6F\" ]}");
    testFunc(data0, data1, streamFromJsonObjest);

  }

  // load jSON object (mask string hex)
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("646566");
 

    auto testFunc = [](const irs::string_ref& data0, const irs::string_ref& data1, irs::analysis::analyzer::ptr stream) {
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

    auto stream = irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "[ \"abc\", \"646566\", \"6D6e6F\" ]");
    testFunc(data0, data1, stream);


    auto streamFromJsonObjest = irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":[ \"abc\", \"646566\", \"6D6e6F\" ]}");
    testFunc(data0, data1, streamFromJsonObjest);

  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "\"abc\""));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stopwords", irs::type<irs::text_format::json>::get(), "{\"stopwords\":1}"));
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
}

TEST(token_stopwords_stream_tests, normalize_valid_array) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "[\"QWRT\", \"qwrt\"]"));
  ASSERT_EQ(actual, "{\n  \"stopwords\" : [\n    \"QWRT\",\n    \"qwrt\"\n  ]\n}");
}

TEST(token_stopwords_stream_tests, normalize_valid_object) {
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "stopwords",
    irs::type<irs::text_format::json>::get(), "{\"stopwords\":[\"QWRT\", \"qwrt\"]}"));
  ASSERT_EQ(actual, "{\n  \"stopwords\" : [\n    \"QWRT\",\n    \"qwrt\"\n  ]\n}");
}
