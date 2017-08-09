////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::FrequencyBuffer
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

#include "Cache/FrequencyBuffer.h"
#include "Basics/Common.h"

#include "catch.hpp"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

TEST_CASE("cache::FrequencyBuffer", "[cache]") {
  SECTION("test buffer with uint8_t entries") {
    uint8_t zero = 0;
    uint8_t one = 1;
    uint8_t two = 2;

    // check that default construction is as expected
    REQUIRE(uint8_t() == zero);

    FrequencyBuffer<uint8_t> buffer(8);
    REQUIRE(buffer.memoryUsage() ==
            sizeof(FrequencyBuffer<uint8_t>) + sizeof(std::vector<uint8_t>) +
                8);

    for (size_t i = 0; i < 4; i++) {
      buffer.insertRecord(two);
    }
    for (size_t i = 0; i < 2; i++) {
      buffer.insertRecord(one);
    }

    auto frequencies = buffer.getFrequencies();
    REQUIRE(static_cast<uint64_t>(2) == frequencies.size());
    REQUIRE(one == frequencies[0].first);
    REQUIRE(static_cast<uint64_t>(2) == frequencies[0].second);
    REQUIRE(two == frequencies[1].first);
    REQUIRE(static_cast<uint64_t>(4) == frequencies[1].second);

    for (size_t i = 0; i < 8; i++) {
      buffer.insertRecord(one);
    }

    frequencies = buffer.getFrequencies();
    REQUIRE(static_cast<size_t>(1) == frequencies.size());
    REQUIRE(one == frequencies[0].first);
    REQUIRE(static_cast<uint64_t>(8) == frequencies[0].second);
  }

  SECTION("test buffer with weak_ptr entries") {
    struct cmp_weak_ptr {
      bool operator()(std::weak_ptr<int> const& left,
                      std::weak_ptr<int> const& right) const {
        return !left.owner_before(right) && !right.owner_before(left);
      }
    };

    struct hash_weak_ptr {
      size_t operator()(std::weak_ptr<int> const& wp) const {
        auto sp = wp.lock();
        return std::hash<decltype(sp)>()(sp);
      }
    };

    typedef FrequencyBuffer<std::weak_ptr<int>, cmp_weak_ptr, hash_weak_ptr>
        BufferType;

    std::shared_ptr<int> p0(nullptr);

    // check that default construction is as expected
    REQUIRE(std::shared_ptr<int>() == p0);

    std::shared_ptr<int> p1(new int());
    *p1 = static_cast<int>(1);
    std::shared_ptr<int> p2(new int());
    *p2 = static_cast<int>(2);

    BufferType buffer(8);
    REQUIRE(buffer.memoryUsage() ==
            sizeof(BufferType) + sizeof(std::vector<std::weak_ptr<int>>) +
                (8 * sizeof(std::weak_ptr<int>)));

    for (size_t i = 0; i < 4; i++) {
      buffer.insertRecord(p1);
    }
    for (size_t i = 0; i < 2; i++) {
      buffer.insertRecord(p2);
    }

    auto frequencies = buffer.getFrequencies();
    REQUIRE(static_cast<uint64_t>(2) == frequencies.size());
    REQUIRE(p2 == frequencies[0].first.lock());
    REQUIRE(static_cast<uint64_t>(2) == frequencies[0].second);
    REQUIRE(p1 == frequencies[1].first.lock());
    REQUIRE(static_cast<uint64_t>(4) == frequencies[1].second);
  }
}
