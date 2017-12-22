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

#include "tests_shared.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"

using namespace iresearch;

TEST(token_streams_tests, boolean_stream) {
  ASSERT_NE(boolean_token_stream::value_false(), boolean_token_stream::value_true());

  // 'false' stream
  {
    auto& expected = boolean_token_stream::value_false();
    boolean_token_stream stream(false);

    ASSERT_EQ(2, stream.attributes().size());
    auto& inc = stream.attributes().get<increment>();
    ASSERT_FALSE(!inc);
    ASSERT_EQ(1, inc->value);
    auto& value = stream.attributes().get<term_attribute>();
    ASSERT_FALSE(!value);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value());
    ASSERT_FALSE(stream.next());

    /* reset stream */
    stream.reset(false);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value());
    ASSERT_FALSE(stream.next());
  }

  // 'true' stream
  {
    auto& expected = boolean_token_stream::value_true();
    boolean_token_stream stream(true);
    ASSERT_EQ(2, stream.attributes().size());
    auto& inc = stream.attributes().get<increment>();
    ASSERT_FALSE(!inc);
    ASSERT_EQ(1, inc->value);
    auto& value = stream.attributes().get<term_attribute>();
    ASSERT_FALSE(!value);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value());
    ASSERT_FALSE(stream.next());

    /* reset stream */
    stream.reset(true);
    ASSERT_TRUE(stream.next());
    ASSERT_EQ(expected, value->value());
    ASSERT_FALSE(stream.next());
  }
}

TEST(token_streams_tests, null_stream) {
  auto& expected = bytes_ref::nil;
  null_token_stream stream;
  ASSERT_EQ(2, stream.attributes().size());
  auto& inc = stream.attributes().get<increment>();
  ASSERT_FALSE(!inc);
    ASSERT_EQ(1, inc->value);
  auto& value = stream.attributes().get<term_attribute>();
  ASSERT_FALSE(!value);
  ASSERT_TRUE(stream.next());
  ASSERT_EQ(expected, value->value());
  ASSERT_FALSE(stream.next());

  /* reset stream */
  stream.reset();
  ASSERT_TRUE(stream.next());
  ASSERT_EQ(expected, value->value());
  ASSERT_FALSE(stream.next());
}

TEST( string_token_stream_tests, ctor ) {
  {
    iresearch::string_token_stream ts;
    ts.reset(string_ref::nil);
    ASSERT_EQ(3U, ts.attributes().size() );
    ASSERT_FALSE(!ts.attributes().get<increment>());
    ASSERT_FALSE(!ts.attributes().get<offset>());
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
  }

  {
    iresearch::string_token_stream ts;
    ts.reset(bytes_ref::nil);
    ASSERT_EQ(3, ts.attributes().size() );
    ASSERT_FALSE(!ts.attributes().get<increment>());
    ASSERT_FALSE(!ts.attributes().get<offset>());
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
  }
}

TEST( string_token_stream_tests, next_end) {
  const std::string str("QBVnCx4NCizekHA");
  const bytes_ref ref = bytes_ref(reinterpret_cast<const byte_type*>( str.c_str()),
                                  str.size());
  string_token_stream ts;
  ts.reset(str);

  // check attributes
  auto& term = ts.attributes().get<term_attribute>();
  ASSERT_FALSE(!term);
  ASSERT_TRUE( term->value().null());
  auto& offs = ts.attributes().get<offset>();
  ASSERT_FALSE(!offs);
  ASSERT_EQ( 0, offs->start);
  ASSERT_EQ( 0, offs->end);

  /* perform step */
  ASSERT_TRUE( ts.next());
  ASSERT_EQ( ref, term->value());
  ASSERT_EQ( 0, offs->start);
  ASSERT_EQ( ref.size(), offs->end);
  ASSERT_FALSE( ts.next());
  ASSERT_FALSE( ts.next());
  ASSERT_EQ( irs::bytes_ref::nil, term->value());
  ASSERT_EQ( 0, offs->start);
  ASSERT_EQ( 0, offs->end);

  /* reset stream */
  ts.reset(str);
  ASSERT_TRUE( ts.next());
  ASSERT_EQ( ref, term->value());
  ASSERT_EQ( 0, offs->start);
  ASSERT_EQ( ref.size(), offs->end);

  ASSERT_FALSE( ts.next());
  ASSERT_FALSE( ts.next());
}

TEST( numeric_token_stream_tests, ctor) {
  {
    numeric_token_stream ts;
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
    ASSERT_FALSE(!ts.attributes().get<increment>());
  }

  /* int */
  {
    numeric_token_stream ts;
    ts.reset(35);
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
    ASSERT_FALSE(!ts.attributes().get<increment>());
  }

  /* long */
  {
    numeric_token_stream ts;
    ts.reset(int64_t(75));
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
    ASSERT_FALSE(!ts.attributes().get<increment>());
  }

  /* float */
  {
    numeric_token_stream ts;
    ts.reset((float_t)35.f);
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
    ASSERT_FALSE(!ts.attributes().get<increment>());
  }

  /* double */
  {
    numeric_token_stream ts;
    ts.reset((double_t)35.);
    ASSERT_FALSE(!ts.attributes().get<term_attribute>());
    ASSERT_FALSE(!ts.attributes().get<increment>());
  }
}

TEST(numeric_token_stream_tests, value) {
  // int
  {
    bstring buf;
    numeric_token_stream ts;
    auto& term = ts.attributes().get<term_attribute>();
    ASSERT_FALSE(!term);
    ts.reset(35);

    auto value = numeric_token_stream::value(buf, 35);
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value(), value); // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 2nd
    ASSERT_EQ(true, !ts.next());
  }

  // long
  {
    bstring buf;
    numeric_token_stream ts;
    auto& term = ts.attributes().get<term_attribute>();
    ASSERT_FALSE(!term);
    ts.reset(int64_t(75));

    auto value = numeric_token_stream::value(buf, int64_t(75));
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value(), value); // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 2nd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 3rd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 4th
    ASSERT_EQ(true, !ts.next());
  }

  // float
  {
    bstring buf;
    numeric_token_stream ts;
    auto& term = ts.attributes().get<term_attribute>();
    ASSERT_FALSE(!term);
    ts.reset((float_t)35.f);

    auto value = numeric_token_stream::value(buf, (float_t)35.f);
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value(), value); // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 2nd
    ASSERT_EQ(true, !ts.next());
  }

  // double
  {
    bstring buf;
    numeric_token_stream ts;
    auto& term = ts.attributes().get<term_attribute>();
    ASSERT_FALSE(!term);
    ts.reset((double_t)35.);

    auto value = numeric_token_stream::value(buf, (double_t)35.);
    ASSERT_EQ(true, ts.next());
    ASSERT_EQ(term->value(), value); // value same as 1st
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 2nd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 3rd
    ASSERT_EQ(true, ts.next());
    ASSERT_NE(term->value(), value); // value not same as 4th
    ASSERT_EQ(true, !ts.next());
  }
}
