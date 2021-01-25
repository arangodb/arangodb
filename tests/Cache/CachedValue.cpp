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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>
#include <string>

#include "Cache/CachedValue.h"

using namespace arangodb::cache;

const std::size_t padding = alignof(std::atomic<std::uint32_t>) - 1;

TEST(CacheCachedValueTest, test_constructor_with_valid_input) {
  std::uint64_t k = 1;
  std::string v("test");
  CachedValue* cv;

  // fixed key, variable value
  cv = CachedValue::construct(&k, sizeof(std::uint64_t), v.data(), v.size());
  ASSERT_NE(nullptr, cv);
  ASSERT_EQ(sizeof(std::uint64_t), cv->keySize());
  ASSERT_EQ(v.size(), cv->valueSize());
  ASSERT_EQ(sizeof(CachedValue) + padding + sizeof(std::uint64_t) + v.size(), cv->size());
  ASSERT_EQ(k, *reinterpret_cast<std::uint64_t const*>(cv->key()));
  ASSERT_EQ(0, memcmp(v.data(), cv->value(), v.size()));
  delete cv;

  // variable key, fixed value
  cv = CachedValue::construct(v.data(), static_cast<std::uint32_t>(v.size()),
                              &k, sizeof(std::uint64_t));
  ASSERT_NE(nullptr, cv);
  ASSERT_EQ(v.size(), cv->keySize());
  ASSERT_EQ(sizeof(std::uint64_t), cv->valueSize());
  ASSERT_EQ(sizeof(CachedValue) + padding + sizeof(std::uint64_t) + v.size(), cv->size());
  ASSERT_EQ(0, memcmp(v.data(), cv->key(), v.size()));
  ASSERT_EQ(k, *reinterpret_cast<std::uint64_t const*>(cv->value()));
  delete cv;

  // fixed key, zero length value
  cv = CachedValue::construct(&k, sizeof(std::uint64_t), nullptr, 0);
  ASSERT_NE(nullptr, cv);
  ASSERT_EQ(sizeof(std::uint64_t), cv->keySize());
  ASSERT_EQ(0ULL, cv->valueSize());
  ASSERT_EQ(sizeof(CachedValue) + padding + sizeof(std::uint64_t), cv->size());
  ASSERT_EQ(k, *reinterpret_cast<std::uint64_t const*>(cv->key()));
  ASSERT_EQ(nullptr, cv->value());
  delete cv;
}

TEST(CacheCachedValueTest, test_that_constructor_rejects_invalid_data) {
  std::uint64_t k = 1;
  std::string v("test");
  CachedValue* cv;

  // zero size key
  cv = CachedValue::construct(&k, 0, v.data(), v.size());
  ASSERT_EQ(nullptr, cv);

  // nullptr key, zero size
  cv = CachedValue::construct(nullptr, 0, v.data(), v.size());
  ASSERT_EQ(nullptr, cv);

  // nullptr key, non-zero size
  cv = CachedValue::construct(nullptr, sizeof(std::uint64_t), v.data(), v.size());
  ASSERT_EQ(nullptr, cv);

  // nullptr value, non-zero length
  cv = CachedValue::construct(&k, sizeof(std::uint64_t), nullptr, v.size());
  ASSERT_EQ(nullptr, cv);

  // too large key size
  cv = CachedValue::construct(&k, 0x1000000, v.data(), v.size());
  ASSERT_EQ(nullptr, cv);

  // too large value size
  cv = CachedValue::construct(&k, sizeof(std::uint64_t), v.data(), 0x100000000ULL);
  ASSERT_EQ(nullptr, cv);
}

TEST(CacheCachedValueTest, copy_should_produce_a_correct_copy) {
  std::uint64_t k = 1;
  std::string v("test");

  // fixed key, variable value
  auto original = CachedValue::construct(&k, sizeof(std::uint64_t), v.data(), v.size());
  ASSERT_NE(nullptr, original);
  auto copy = original->copy();
  ASSERT_NE(nullptr, copy);
  ASSERT_NE(copy, original);
  ASSERT_EQ(sizeof(std::uint64_t), copy->keySize());
  ASSERT_EQ(v.size(), copy->valueSize());
  ASSERT_EQ(sizeof(CachedValue) + padding + sizeof(std::uint64_t) + v.size(), copy->size());
  ASSERT_EQ(k, *reinterpret_cast<std::uint64_t const*>(copy->key()));
  ASSERT_EQ(0, memcmp(v.data(), copy->value(), v.size()));
  delete original;
  delete copy;
}

TEST(CacheCachedValueTest, samekey_method_for_key_comparison_works) {
  std::string k1("test");
  std::string k2("testing");
  std::string k3("TEST");
  std::uint64_t v = 1;

  auto cv = CachedValue::construct(k1.data(), static_cast<std::uint32_t>(k1.size()),
                                   &v, sizeof(std::uint64_t));
  ASSERT_NE(nullptr, cv);

  // same key
  ASSERT_TRUE(cv->sameKey(k1.data(), static_cast<std::uint32_t>(k1.size())));

  // different length, matching prefix
  ASSERT_FALSE(cv->sameKey(k2.data(), static_cast<std::uint32_t>(k2.size())));

  // same length, different key
  ASSERT_FALSE(cv->sameKey(k3.data(), static_cast<std::uint32_t>(k3.size())));

  delete cv;
}
