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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "VocBase/Identifiers/RevisionId.h"

#include <random>
#include <string>
#include <vector>

using namespace arangodb;

TEST(RevisionIdTest, test_safe_roundtrip_boundaries) {
  std::vector<uint64_t> values;
  values.reserve(125'000);

  for (uint64_t i = 1ULL; i < 100'000ULL; ++i) {
    values.emplace_back(i);
  }

  for (uint64_t i = 100'000ULL; i < 2'000'000ULL; i += 100'00ULL) {
    values.emplace_back(i);
  }

  for (uint64_t i = 5'000'000ULL; i < 100'000'000ULL; i += 1'000'000ULL) {
    values.emplace_back(i);
  }

  for (uint64_t i = 1'000'000'000ULL; i < 1'000'000'000'000ULL;
       i += 1'000'000'000ULL) {
    values.emplace_back(i);
  }

  for (uint64_t i = RevisionId::tickLimit - 10'000ULL;
       i < RevisionId::tickLimit + 10'000ULL; ++i) {
    values.emplace_back(i);
  }

  for (auto const& v : values) {
    RevisionId original{v};
    std::string encoded = original.toString();
    RevisionId decoded = RevisionId::fromString(encoded);

    ASSERT_EQ(original.id(), decoded.id());
  }
}

TEST(RevisionIdTest, test_safe_roundtrip_random_till_ticklimit) {
  auto rd{std::random_device{}()};
  auto mt{std::mt19937_64(rd)};
  std::uniform_int_distribution<uint64_t> uni{
      1, RevisionId::tickLimit + 10'000'000ULL};

  for (size_t i = 0; i < 10'000'000; ++i) {
    RevisionId original{uni(mt)};
    std::string encoded = original.toString();
    RevisionId decoded = RevisionId::fromString(encoded);

    ASSERT_EQ(original.id(), decoded.id());
  }
}

TEST(RevisionIdTest, test_safe_roundtrip_encoded_boundaries_) {
  {
    uint64_t value = 1;

    for (size_t digits = 0; digits <= 19; ++digits) {
      std::string encoded = std::to_string(value);
      RevisionId decoded = RevisionId::fromString(encoded);
      std::string encoded2 = std::to_string(decoded.id());

      EXPECT_EQ(encoded, encoded2);
      value *= 10;
    }
  }

  {
    uint64_t value = 9;

    for (size_t digits = 0; digits <= 18; ++digits) {
      std::string encoded = std::to_string(value);
      RevisionId decoded = RevisionId::fromString(encoded);
      std::string encoded2 = std::to_string(decoded.id());

      EXPECT_EQ(encoded, encoded2);
      value *= 10;
      value += 9;
    }
  }
}

TEST(RevisionIdTest, test_safe_roundtrip_encoded_boundaries) {
  {
    uint64_t value = 1;

    for (size_t digits = 0; digits <= 19; ++digits) {
      RevisionId original{value};
      std::string encoded = original.toString();
      RevisionId decoded = RevisionId::fromString(encoded);

      EXPECT_EQ(original.id(), decoded.id());
      value *= 10;
    }
  }

  {
    uint64_t value = 9;

    for (size_t digits = 0; digits <= 18; ++digits) {
      RevisionId original{value};
      std::string encoded = original.toString();
      RevisionId decoded = RevisionId::fromString(encoded);

      EXPECT_EQ(original.id(), decoded.id());
      value *= 10;
      value += 9;
    }
  }
}
