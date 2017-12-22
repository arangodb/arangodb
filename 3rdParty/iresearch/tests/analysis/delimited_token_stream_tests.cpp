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

#include "gtest/gtest.h"
#include "tests_config.hpp"
#include "analysis/delimited_token_stream.hpp"

NS_LOCAL

class delimited_token_stream_tests: public ::testing::Test {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).
  }
};

NS_END // NS_LOCAL

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST_F(delimited_token_stream_tests, test_delimiter) {
  // test delimiter string_ref::nil
  {
    irs::string_ref data("abc,def\"\",\"\"ghi");
    irs::analysis::delimited_token_stream stream(irs::string_ref::nil);

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ("abc,def\"\",\"\"ghi", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test delimteter ''
  {
    irs::string_ref data("abc,\"def\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(1, offset->end);
    ASSERT_EQ("a", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("a", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(1, offset->start);
    ASSERT_EQ(2, offset->end);
    ASSERT_EQ("b", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("b", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(2, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("c", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("c", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(3, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ(",", irs::ref_cast<char>(payload->value));
    ASSERT_EQ(",", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("\"def\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter ','
  {
    irs::string_ref data("abc,\"def,\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(10, offset->end);
    ASSERT_EQ("\"def,\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def,", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter '\t'
  {
    irs::string_ref data("abc,\t\"def\t\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("\t");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("abc,", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc,", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(5, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("\"def\t\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def\t", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter '"'
  {
    irs::string_ref data("abc,\"\"def\t\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("\"");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("abc,", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc,", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(5, offset->start);
    ASSERT_EQ(5, offset->end);
    ASSERT_EQ("", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(6, offset->start);
    ASSERT_EQ(10, offset->end);
    ASSERT_EQ("def\t", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def\t", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(11, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test delimiter 'abc'
  {
    irs::string_ref data("abc,123\"def123\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream("123");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("abc,", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc,", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(7, offset->start);
    ASSERT_EQ(15, offset->end);
    ASSERT_EQ("\"def123\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def123", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }
}

TEST_F(delimited_token_stream_tests, test_quote) {
  // test quoted field
  {
    irs::string_ref data("abc,\"def\",\"\"ghi"); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("\"def\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(15, offset->end);
    ASSERT_EQ("\"\"ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("\"\"ghi", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test unterminated "
  {
    irs::string_ref data("abc,\"def\",\"ghi"); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("\"def\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(14, offset->end);
    ASSERT_EQ("\"ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("\"ghi", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test unterminated single "
  {
    irs::string_ref data("abc,\"def\",\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("\"def\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("\"", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test " escape
  {
    irs::string_ref data("abc,\"\"\"def\",\"\"ghi"); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("\"\"\"def\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("\"def", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(12, offset->start);
    ASSERT_EQ(17, offset->end);
    ASSERT_EQ("\"\"ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("\"\"ghi", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test non-quoted field with "
  {
    irs::string_ref data("abc,\"def\",ghi\""); // quoted terms should be honoured
    irs::analysis::delimited_token_stream stream(",");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ("\"def\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(10, offset->start);
    ASSERT_EQ(14, offset->end);
    ASSERT_EQ("ghi\"", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ghi\"", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(delimited_token_stream_tests, test_load) {
  {
    irs::string_ref data("abc,def,ghi"); // quoted terms should be honoured
    auto stream = irs::analysis::analyzers::get("delimited", ",");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(3, offset->end);
    ASSERT_EQ("abc", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("abc", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(4, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("def", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("def", irs::ref_cast<char>(term->value()));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(8, offset->start);
    ASSERT_EQ(11, offset->end);
    ASSERT_EQ("ghi", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ghi", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------