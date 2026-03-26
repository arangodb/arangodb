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

#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "tests_shared.hpp"

using namespace irs;

TEST(token_streams_tests, boolean_stream) {
  ASSERT_EQ(1, boolean_token_stream::value_false().size());
  ASSERT_EQ(0, boolean_token_stream::value_false().front());
  ASSERT_EQ(1, boolean_token_stream::value_true().size());
  ASSERT_EQ(irs::byte_type(0xFF),
            irs::byte_type(boolean_token_stream::value_true().front()));

  ASSERT_NE(boolean_token_stream::value_false(),
            boolean_token_stream::value_true());

  // 'false' stream
  {
    const auto expected =
      irs::ViewCast<irs::byte_type>(boolean_token_stream::value_false());
    boolean_token_stream stream;
    stream.reset(false);

    auto* inc = irs::get<increment>(stream);
    ASSERT_FALSE(!inc);
    ASSERT_EQ(1, inc->value);
    auto* value = irs::get<term_attribute>(stream);
    ASSERT_FALSE(!value);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value);
    ASSERT_FALSE(stream.next());

    /* reset stream */
    stream.reset(false);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value);
    ASSERT_FALSE(stream.next());
  }

  // 'true' stream
  {
    auto expected =
      irs::ViewCast<irs::byte_type>(boolean_token_stream::value_true());
    boolean_token_stream stream;
    stream.reset(true);
    auto* inc = irs::get<increment>(stream);
    ASSERT_FALSE(!inc);
    ASSERT_EQ(1, inc->value);
    auto* value = irs::get<term_attribute>(stream);
    ASSERT_FALSE(!value);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value);
    ASSERT_FALSE(stream.next());

    /* reset stream */
    stream.reset(true);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value);
    ASSERT_FALSE(stream.next());
  }
}

TEST(token_streams_tests, null_stream) {
  ASSERT_EQ(0, null_token_stream::value_null().size());
  ASSERT_NE(nullptr, null_token_stream::value_null().data());

  const auto expected = bytes_view{};
  null_token_stream stream;
  auto* inc = irs::get<increment>(stream);
  ASSERT_FALSE(!inc);
  ASSERT_EQ(1, inc->value);
  auto* value = irs::get<term_attribute>(stream);
  ASSERT_FALSE(!value);
  ASSERT_TRUE(stream.next());
  ASSERT_EQ(expected, value->value);
  ASSERT_FALSE(stream.next());

  /* reset stream */
  stream.reset();
  ASSERT_TRUE(stream.next());
  ASSERT_EQ(expected, value->value);
  ASSERT_FALSE(stream.next());
}

TEST(string_token_stream_tests, next_end) {
  const std::string str("QBVnCx4NCizekHA");
  const bytes_view ref =
    bytes_view(reinterpret_cast<const byte_type*>(str.c_str()), str.size());
  string_token_stream ts;
  ts.reset(str);

  // check attributes
  auto* term = irs::get<term_attribute>(ts);
  ASSERT_FALSE(!term);
  ASSERT_TRUE(IsNull(term->value));
  auto* offs = irs::get<offset>(ts);
  ASSERT_FALSE(!offs);
  ASSERT_EQ(0, offs->start);
  ASSERT_EQ(0, offs->end);

  ASSERT_TRUE(ts.next());
  ASSERT_EQ(ref, term->value);
  ASSERT_EQ(0, offs->start);
  ASSERT_EQ(ref.size(), offs->end);
  ASSERT_FALSE(ts.next());
  ASSERT_FALSE(ts.next());

  ts.reset(str);
  ASSERT_TRUE(ts.next());
  ASSERT_EQ(ref, term->value);
  ASSERT_EQ(0, offs->start);
  ASSERT_EQ(ref.size(), offs->end);

  ASSERT_FALSE(ts.next());
  ASSERT_FALSE(ts.next());
}

TEST(numeric_token_stream_tests, value) {
  // int
  {
    bstring buf;
    numeric_token_stream ts;
    auto* term = irs::get<term_attribute>(ts);
    ASSERT_FALSE(!term);
    ts.reset(35);

    auto value = numeric_token_stream::value(buf, 35);
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value, value);  // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 2nd
    ASSERT_EQ(true, !ts.next());
  }

  // long
  {
    bstring buf;
    numeric_token_stream ts;
    auto* term = irs::get<term_attribute>(ts);
    ASSERT_FALSE(!term);
    ts.reset(int64_t(75));

    auto value = numeric_token_stream::value(buf, int64_t(75));
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value, value);  // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 2nd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 3rd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 4th
    ASSERT_EQ(true, !ts.next());
  }

  // float
  {
    bstring buf;
    numeric_token_stream ts;
    auto* term = irs::get<term_attribute>(ts);
    ASSERT_FALSE(!term);
    ts.reset((float_t)35.f);

    auto value = numeric_token_stream::value(buf, (float_t)35.f);
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value, value);  // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 2nd
    ASSERT_EQ(true, !ts.next());
  }

  // double
  {
    bstring buf;
    numeric_token_stream ts;
    auto* term = irs::get<term_attribute>(ts);
    ASSERT_FALSE(!term);
    ts.reset((double_t)35.);

    auto value = numeric_token_stream::value(buf, (double_t)35.);
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value, value);  // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 2nd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 3rd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value, value);  // value not same as 4th
    ASSERT_EQ(true, !ts.next());
  }
}
