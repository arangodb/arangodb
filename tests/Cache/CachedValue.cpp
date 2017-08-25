////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::CachedValue
///
/// @file
///
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cache/CachedValue.h"
#include "Basics/Common.h"

#include "catch.hpp"

#include <stdint.h>
#include <string>

using namespace arangodb::cache;

const size_t padding = alignof(std::atomic<uint32_t>) - 1;

TEST_CASE("cache::CachedValue", "[cache]") {
  SECTION("test constructor with valid input") {
    uint64_t k = 1;
    std::string v("test");
    CachedValue* cv;

    // fixed key, variable value
    cv = CachedValue::construct(&k, sizeof(uint64_t), v.data(), v.size());
    REQUIRE(nullptr != cv);
    REQUIRE(sizeof(uint64_t) == cv->keySize());
    REQUIRE(v.size() == cv->valueSize());
    REQUIRE(sizeof(CachedValue) + padding + sizeof(uint64_t) + v.size() == cv->size());
    REQUIRE(k == *reinterpret_cast<uint64_t const*>(cv->key()));
    REQUIRE(0 == memcmp(v.data(), cv->value(), v.size()));
    delete cv;

    // variable key, fixed value
    cv = CachedValue::construct(v.data(), static_cast<uint32_t>(v.size()), &k,
                                sizeof(uint64_t));
    REQUIRE(nullptr != cv);
    REQUIRE(v.size() == cv->keySize());
    REQUIRE(sizeof(uint64_t) == cv->valueSize());
    REQUIRE(sizeof(CachedValue) + padding + sizeof(uint64_t) + v.size() == cv->size());
    REQUIRE(0 == memcmp(v.data(), cv->key(), v.size()));
    REQUIRE(k == *reinterpret_cast<uint64_t const*>(cv->value()));
    delete cv;

    // fixed key, zero length value
    cv = CachedValue::construct(&k, sizeof(uint64_t), nullptr, 0);
    REQUIRE(nullptr != cv);
    REQUIRE(sizeof(uint64_t) == cv->keySize());
    REQUIRE(0ULL == cv->valueSize());
    REQUIRE(sizeof(CachedValue) + padding + sizeof(uint64_t) == cv->size());
    REQUIRE(k == *reinterpret_cast<uint64_t const*>(cv->key()));
    REQUIRE(nullptr == cv->value());
    delete cv;
  }

  SECTION("test that constructor rejects invalid data") {
    uint64_t k = 1;
    std::string v("test");
    CachedValue* cv;

    // zero size key
    cv = CachedValue::construct(&k, 0, v.data(), v.size());
    REQUIRE(nullptr == cv);

    // nullptr key, zero size
    cv = CachedValue::construct(nullptr, 0, v.data(), v.size());
    REQUIRE(nullptr == cv);

    // nullptr key, non-zero size
    cv = CachedValue::construct(nullptr, sizeof(uint64_t), v.data(), v.size());
    REQUIRE(nullptr == cv);

    // nullptr value, non-zero length
    cv = CachedValue::construct(&k, sizeof(uint64_t), nullptr, v.size());
    REQUIRE(nullptr == cv);

    // too large key size
    cv = CachedValue::construct(&k, 0x1000000, v.data(), v.size());
    REQUIRE(nullptr == cv);

    // too large value size
    cv = CachedValue::construct(&k, sizeof(uint64_t), v.data(), 0x100000000ULL);
    REQUIRE(nullptr == cv);
  }

  SECTION("copy() should produce a correct copy") {
    uint64_t k = 1;
    std::string v("test");

    // fixed key, variable value
    auto original =
        CachedValue::construct(&k, sizeof(uint64_t), v.data(), v.size());
    REQUIRE(nullptr != original);
    auto copy = original->copy();
    REQUIRE(nullptr != copy);
    REQUIRE(copy != original);
    REQUIRE(sizeof(uint64_t) == copy->keySize());
    REQUIRE(v.size() == copy->valueSize());
    REQUIRE(sizeof(CachedValue) + padding + sizeof(uint64_t) + v.size() == copy->size());
    REQUIRE(k == *reinterpret_cast<uint64_t const*>(copy->key()));
    REQUIRE(0 == memcmp(v.data(), copy->value(), v.size()));
    delete original;
    delete copy;
  }

  SECTION("sameKey() method for key comparisons works") {
    std::string k1("test");
    std::string k2("testing");
    std::string k3("TEST");
    uint64_t v = 1;

    auto cv = CachedValue::construct(
        k1.data(), static_cast<uint32_t>(k1.size()), &v, sizeof(uint64_t));
    REQUIRE(nullptr != cv);

    // same key
    REQUIRE(cv->sameKey(k1.data(), static_cast<uint32_t>(k1.size())));

    // different length, matching prefix
    REQUIRE(!cv->sameKey(k2.data(), static_cast<uint32_t>(k2.size())));

    // same length, different key
    REQUIRE(!cv->sameKey(k3.data(), static_cast<uint32_t>(k3.size())));

    delete cv;
  }
}
