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
#include "analysis/token_masking_stream.hpp"

namespace {

class token_masking_stream_tests: public ::testing::Test {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).
  }
};

} // namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST_F(token_masking_stream_tests, consts) {
  static_assert("mask" == irs::type<irs::analysis::token_masking_stream>::name());
}

TEST_F(token_masking_stream_tests, test_masking) {
  // test mask nothing
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("ghi");
    std::unordered_set<irs::bstring> mask;
    irs::analysis::token_masking_stream stream(std::move(mask));
    ASSERT_EQ(irs::type<irs::analysis::token_masking_stream>::id(), stream.type());

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data0));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());

    ASSERT_TRUE(stream.reset(data1));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test mask something
  {
    irs::string_ref data0("abc");
    irs::string_ref data1("ghi");
    std::unordered_set<irs::bstring> mask = { irs::ref_cast<irs::byte_type>(irs::string_ref("abc")) };
    irs::analysis::token_masking_stream stream(std::move(mask));

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data0));
    ASSERT_FALSE(stream.next());

    ASSERT_TRUE(stream.reset(data1));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(token_masking_stream_tests, test_load) {
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
      auto* payload = irs::get<irs::payload>(*stream);
      auto* term = irs::get<irs::term_attribute>(*stream);

      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(3, offset->end);
      ASSERT_EQ("ghi", irs::ref_cast<char>(payload->value));
      ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
      ASSERT_FALSE(stream->next());
    };
    auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "[ \"abc\", \"646566\", \"6D6e6F\" ]");
    testFunc(data0, data1, stream);

    auto streamFromJsonObjest = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "{\"mask\":[ \"abc\", \"646566\", \"6D6e6F\" ]}");
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
      auto* payload = irs::get<irs::payload>(*stream);
      auto* term = irs::get<irs::term_attribute>(*stream);

      ASSERT_TRUE(stream->next());
      ASSERT_EQ(0, offset->start);
      ASSERT_EQ(6, offset->end);
      ASSERT_EQ("646566", irs::ref_cast<char>(payload->value));
      ASSERT_EQ("646566", irs::ref_cast<char>(term->value));
      ASSERT_FALSE(stream->next());
    };

    auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "[ \"abc\", \"646566\", \"6D6e6F\" ]");
    testFunc(data0, data1, stream);


    auto streamFromJsonObjest = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "{\"mask\":[ \"abc\", \"646566\", \"6D6e6F\" ]}");
    testFunc(data0, data1, streamFromJsonObjest);

  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "\"abc\""));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), "{\"mask\":1}"));
  }

  // load text (mask hex)
  {
    irs::string_ref data0("ghi");
    irs::string_ref data1("mno");
    auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::text>::get(), "abc \n646566\t6D6e6F");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data0));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());

    ASSERT_TRUE(stream->reset(data1));
    ASSERT_FALSE(stream->next());
  }

  // load text irs::string_ref::NIL
  {
    irs::string_ref data0("ghi");
    irs::string_ref data1("mno");
    auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::text>::get(), irs::string_ref::NIL);

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data0));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());

    ASSERT_TRUE(stream->reset(data1));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("mno", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("mno", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }
}

// commented out due to lack
//TEST_F(token_masking_stream_tests, test_make_config_json) {
//
//  //with unknown parameter
//  {
//    std::string config = "{\"mask\":[\"abc\",\"646566\",\"6D6e6F\"],\"invalid_parameter\":true}";
//    auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), config.c_str());
//    ASSERT_NE(nullptr, stream);
//
//    std::string actual;
//    ASSERT_TRUE(stream->to_string(::irs::type<irs::text_format::json>::get(), actual));
//    ASSERT_EQ("{\"mask\":[\"abc\",\"646566\",\"6D6e6F\"]}", actual);
//  }
//  //with dublicates  removed
//  {
//    std::string config = "{\"mask\":[\"abc\",\"646566\",\"6D6e6F\",\"abc\"],\"invalid_parameter\":true}";
//    auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::json>::get(), config.c_str());
//    ASSERT_NE(nullptr, stream);
//
//    std::string actual;
//    ASSERT_TRUE(stream->to_string(::irs::type<irs::text_format::json>::get(), actual));
//    ASSERT_EQ("{\"mask\":[\"abc\",\"646566\",\"6D6e6F\"]}", actual);
//  }
//
//
//}
//
//TEST_F(token_masking_stream_tests, test_make_config_text) {
//  std::string config = "abc \n646566\t6D6e6F";
//  auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::text>::get(), config.c_str());
//  ASSERT_NE(nullptr, stream);
//
//  std::string actual;
//  ASSERT_TRUE(stream->to_string(::irs::type<irs::text_format::text>::get(), actual));
//  ASSERT_EQ(config, actual);
//}
//
//TEST_F(token_masking_stream_tests, test_make_config_invalid_format) {
//  std::string config = "abc \n646566\t6D6e6F";
//  auto stream = irs::analysis::analyzers::get("mask", irs::type<irs::text_format::text>::get(), config.c_str());
//  ASSERT_NE(nullptr, stream);
//
//  std::string actual;
//  ASSERT_FALSE(stream->to_string(::irs::type<irs::text_format>::csv, actual));
//}
