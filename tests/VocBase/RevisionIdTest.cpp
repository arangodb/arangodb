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

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace arangodb;

TEST(RevisionIdTest, test_from_to_hlc) {
  std::vector<std::pair<uint64_t, std::string_view>> values = {
      {0ULL, ""},
      {1ULL, "_"},
      {2ULL, "A"},
      {10ULL, "I"},
      {100ULL, "_i"},
      {100000ULL, "WYe"},
      {1000000ULL, "ByH-"},
      {10000000ULL, "kHY-"},
      {100000000ULL, "D7cC-"},
      {1000000000ULL, "5kqm-"},
      {10000000000ULL, "HSA8O-"},
      {100000000000ULL, "_bGbse-"},
      {1000000000000ULL, "MhSnP--"},
      {10000000000000ULL, "APfMao--"},
      {100000000000000ULL, "UtKOci--"},
      {1000000000000000ULL, "BhV4ivm--"},
      {10000000000000000ULL, "hftHtuO--"},
      {100000000000000000ULL, "DhPVfbge--"},
      {1000000000000000000ULL, "1erpMlX---"},
      {10000000000000000000ULL, "GpFGuQH4---"},
      {18446744073709551614ULL, "N9999999998"},
      {18446744073709551615ULL, "N9999999999"},
  };

  velocypack::Builder b;

  for (auto const& value : values) {
    // encode
    std::string encoded = RevisionId{value.first}.toHLC();
    ASSERT_EQ(value.second, encoded);

    // encode into a buffer using a velocypack::ValuePair
    char buffer[11];
    b.clear();
    b.add(RevisionId{value.first}.toHLCValuePair(&buffer[0]));
    ASSERT_EQ(value.first, RevisionId::fromHLC(b.slice().stringView()).id());

    // decode
    ASSERT_EQ(value.first, RevisionId::fromHLC(encoded).id());

    // decode from velocypack string
    b.clear();
    b.add(VPackValue(value.second));
    ASSERT_EQ(value.first, RevisionId::fromHLC(b.slice().stringView()).id());
  }
}

TEST(RevisionIdTest, test_from_hlc_invalid) {
  std::vector<std::pair<uint64_t, std::string_view>> values = {
      {0ULL, ""},
      {UINT64_MAX, " "},
      {51ULL, "x"},
      {869219571ULL, "xxxxx"},
      {UINT64_MAX, "xxxxxxxxxxxxxxxxxxxxxxxxxxxx"},
      {UINT64_MAX, "N9999999999"},
      {17813666640376327606ULL, "Na000000000"},
      {988218432520154550ULL, "O0000000000"},
  };

  for (auto const& value : values) {
    uint64_t decoded = RevisionId::fromHLC(value.second).id();
    ASSERT_EQ(value.first, decoded);
  }
}
