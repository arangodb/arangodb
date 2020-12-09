////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "tests_shared.hpp"
#include "analysis/token_attributes.hpp"

TEST(token_attributes_test, offset) {
  static_assert("offset" == irs::type<irs::offset>::name());

  irs::offset offs;
  ASSERT_EQ(0, offs.start);
  ASSERT_EQ(0, offs.end);
}

TEST(token_attributes_test, increment) {
  static_assert("increment" == irs::type<irs::increment>::name());

  irs::increment inc;
  ASSERT_EQ(1, inc.value);
}

TEST(token_attributes_test, term_attribute) {
  static_assert("term_attribute" == irs::type<irs::term_attribute>::name());

  irs::term_attribute term;
  ASSERT_TRUE(term.value.null());
}

TEST(token_attributes_test, payload) {
  static_assert("payload" == irs::type<irs::payload>::name());

  irs::payload pay;
  ASSERT_TRUE(pay.value.null());
}

TEST(token_attributes_test, document) {
  static_assert("document" == irs::type<irs::document>::name());

  irs::document doc;
  ASSERT_TRUE(!irs::doc_limits::valid(doc.value));
}

TEST(token_attributes_test, frequency) {
  static_assert("frequency" == irs::type<irs::frequency>::name());

  irs::frequency freq;
  ASSERT_EQ(0, freq.value);
}

TEST(token_attributes_test, granularity_prefix) {
  static_assert("iresearch::granularity_prefix" == irs::type<irs::granularity_prefix>::name());
}

TEST(token_attributes_test, norm) {
  static_assert("norm" == irs::type<irs::norm>::name());
  static_assert(1.f == irs::norm::DEFAULT());

  irs::norm norm;
  ASSERT_TRUE(norm.empty());
}

TEST(token_attributes_test, position) {
  static_assert("position" == irs::type<irs::position>::name());
}

TEST(token_attributes_test, attribute_provider_change) {
  static_assert("attribute_provider_change" == irs::type<irs::attribute_provider_change>::name());
}
