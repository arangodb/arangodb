////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include <list>
#include <vector>
#include <deque>

#include "gtest/gtest.h"

#include "Containers/Enumerate.h"
#if GTEST_HAS_TYPED_TEST_P

using namespace arangodb;

template<typename T>
struct NonCopyableType {
  explicit NonCopyableType(T t) : t(std::move(t)) {}
  NonCopyableType(NonCopyableType const&) = delete;
  NonCopyableType(NonCopyableType &&) = default;

  NonCopyableType& operator=(NonCopyableType const&) = delete;
  NonCopyableType& operator=(NonCopyableType&&) = default;

  T t;
};

template<template <typename...> typename C>
struct TemplateTemplateType {
  template<typename... T>
  using type = C<T...>;
};

template<typename TT, typename... T>
using template_template_type_t = typename TT::template type<T...>;

template<typename TT>
class EnumerateVectorLikeTest : public ::testing::Test {};
TYPED_TEST_CASE_P(EnumerateVectorLikeTest);

TYPED_TEST_P(EnumerateVectorLikeTest, test_vector_iterate) {
  template_template_type_t<TypeParam, unsigned> v = { 3, 5, 4, 1, 6, 8, 7};

  unsigned i = 0;
  auto iter = v.begin();
  for (auto [idx, e] : enumerate(v)) {
    EXPECT_EQ(idx, i);
    EXPECT_EQ(e, *iter);
    ++i;
    ++iter;
  }

  EXPECT_EQ(i, v.size());
}

TYPED_TEST_P(EnumerateVectorLikeTest, test_vector_modify) {
  template_template_type_t<TypeParam, unsigned> v = { 3, 5, 4, 1, 6, 8, 7};
  for (auto [idx, e] : enumerate(v)) {
    e = idx;
  }

  unsigned i = 0;
  for (auto const& e : v) {
    EXPECT_EQ(i, e);
    i++;
  }
}

TYPED_TEST_P(EnumerateVectorLikeTest, test_vector_no_copy) {
  template_template_type_t<TypeParam, NonCopyableType<unsigned>> v;
  v.emplace_back(1);

  unsigned i = 0;
  auto iter = v.begin();
  for (auto const [idx, e] : enumerate(v)) {
    EXPECT_EQ(idx, i);
    EXPECT_EQ(e.t, iter->t);
    ++i;
    ++iter;
  }
}

REGISTER_TYPED_TEST_CASE_P(EnumerateVectorLikeTest, test_vector_iterate, test_vector_modify, test_vector_no_copy);

using VectorLikeTypes = ::testing::Types<TemplateTemplateType<std::vector>, TemplateTemplateType<std::list>, TemplateTemplateType<std::deque>>;
INSTANTIATE_TYPED_TEST_CASE_P(EnumerateVectorLikeTestInstant, EnumerateVectorLikeTest, VectorLikeTypes);
#endif
