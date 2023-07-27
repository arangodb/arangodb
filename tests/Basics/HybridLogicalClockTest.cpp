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

#include "Basics/HybridLogicalClock.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace arangodb;

namespace {

class HybridLogicalClockWithFixedTime final
    : public basics::HybridLogicalClock {
 public:
  explicit HybridLogicalClockWithFixedTime(uint64_t t) : _fixed(t) {}

  void goBack() noexcept {
    // make time go backwards 🚀
    --_fixed;
  }

 protected:
  uint64_t getPhysicalTime() override {
    // arbitrary timestamp from Sep 30, 2022.
    return _fixed;
  }

 private:
  uint64_t _fixed;
};

}  // namespace

TEST(HybridLogicalClockTest, test_encode_decode_timestamp) {
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
    std::string encoded =
        basics::HybridLogicalClock::encodeTimeStamp(value.first);
    ASSERT_EQ(value.second, encoded);

    // encode into a buffer using a velocypack::ValuePair
    char buffer[11];
    b.clear();
    b.add(basics::HybridLogicalClock::encodeTimeStampToValuePair(value.first,
                                                                 &buffer[0]));
    ASSERT_EQ(value.first,
              basics::HybridLogicalClock::decodeTimeStamp(b.slice()));

    // decode
    ASSERT_EQ(value.first,
              basics::HybridLogicalClock::decodeTimeStamp(encoded));

    // decode from velocypack string
    b.clear();
    b.add(velocypack::Value(value.second));
    ASSERT_EQ(value.first,
              basics::HybridLogicalClock::decodeTimeStamp(b.slice()));
  }
}

TEST(HybridLogicalClockTest, test_decode_invalid) {
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
    uint64_t decoded =
        basics::HybridLogicalClock::decodeTimeStamp(std::string(value.second));
    ASSERT_EQ(value.first, decoded);
  }
}

TEST(HybridLogicalClockTest, test_extract_time_and_count) {
  std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> values = {
      {0ULL, 0ULL, 0ULL},
      {1ULL, 0ULL, 1ULL},
      {2ULL, 0ULL, 2ULL},
      {10ULL, 0ULL, 10ULL},
      {100ULL, 0ULL, 100ULL},
      {100000ULL, 0ULL, 100000ULL},
      {1000000ULL, 0ULL, 1000000ULL},
      {10000000ULL, 9ULL, 562816ULL},
      {100000000ULL, 95ULL, 385280ULL},
      {1000000000ULL, 953ULL, 707072ULL},
      {10000000000ULL, 9536ULL, 779264ULL},
      {100000000000ULL, 95367ULL, 452608ULL},
      {1000000000000ULL, 953674ULL, 331776ULL},
      {10000000000000ULL, 9536743ULL, 172032ULL},
      {100000000000000ULL, 95367431ULL, 671744ULL},
      {1000000000000000ULL, 953674316ULL, 425984ULL},
      {10000000000000000ULL, 9536743164ULL, 65536ULL},
      {100000000000000000ULL, 95367431640ULL, 655360ULL},
      {1000000000000000000ULL, 953674316406ULL, 262144ULL},
      {10000000000000000000ULL, 9536743164062ULL, 524288ULL},
      {18446744073709551614ULL, 17592186044415ULL, 1048574ULL},
      {18446744073709551615ULL, 17592186044415ULL, 1048575ULL},
  };

  for (auto const& it : values) {
    uint64_t timePart =
        basics::HybridLogicalClock::extractTime(std::get<0>(it));
    ASSERT_EQ(std::get<1>(it), timePart);

    uint64_t countPart =
        basics::HybridLogicalClock::extractCount(std::get<0>(it));
    ASSERT_EQ(std::get<2>(it), countPart);

    ASSERT_EQ(std::get<0>(it), basics::HybridLogicalClock::assembleTimeStamp(
                                   timePart, countPart));
  }
}

TEST(HybridLogicalClockTest, test_get_timestamp) {
  // arbitrary timestamp from Sep 30, 2022, that is supposed to
  // be in the past whenever this test runs
  constexpr uint64_t dateInThePast = 1664561862434ULL;

  basics::HybridLogicalClock hlc;

  uint64_t initial = hlc.getTimeStamp();

  for (size_t i = 0; i < 4'000'000; ++i) {
    uint64_t stamp = hlc.getTimeStamp();
    // every stamp generated must be >= than current time
    ASSERT_GT(stamp, dateInThePast);
    // stamps must be increasing
    ASSERT_GT(stamp, initial);
    initial = stamp;
  }
}

TEST(HybridLogicalClockTest, test_values_increase_for_same_physical_time) {
  ::HybridLogicalClockWithFixedTime hlc(1664561862434ULL);

  uint64_t initial = hlc.getTimeStamp();
  ASSERT_EQ(1664561862434ULL << 20ULL, initial);

  for (size_t i = 0; i < 10'000'000; ++i) {
    uint64_t stamp = hlc.getTimeStamp();
    // stamps must be ever-increasing
    ASSERT_GT(stamp, initial);
    initial = stamp;
  }
}

TEST(HybridLogicalClockTest,
     test_values_increase_when_two_clocks_play_ping_pong) {
  ::HybridLogicalClockWithFixedTime ping(1664561862434ULL);
  ::HybridLogicalClockWithFixedTime pong(1664561862434ULL);

  uint64_t initialPing = ping.getTimeStamp();
  uint64_t initialPong = pong.getTimeStamp();
  ASSERT_EQ(1664561862434ULL << 20ULL, initialPing);
  ASSERT_EQ(1664561862434ULL << 20ULL, initialPong);

  for (size_t i = 0; i < 10'000'000; ++i) {
    uint64_t stamp = ping.getTimeStamp(initialPong);
    // stamps must be ever-increasing
    ASSERT_GT(stamp, initialPing);
    ASSERT_GT(stamp, initialPong);
    initialPing = stamp;

    stamp = pong.getTimeStamp(initialPing);
    ASSERT_GT(stamp, initialPing);
    ASSERT_GT(stamp, initialPong);
    initialPong = stamp;
  }
}

TEST(
    HybridLogicalClockTest,
    test_values_increase_when_two_clocks_play_ping_pong_and_one_clock_is_far_behind) {
  ::HybridLogicalClockWithFixedTime ping(1664561862434ULL);
  // pong has a lag of more than 1 year...
  ::HybridLogicalClockWithFixedTime pong(1640482451649ULL);

  uint64_t initialPing = ping.getTimeStamp();
  uint64_t initialPong = pong.getTimeStamp();
  ASSERT_EQ(1664561862434ULL << 20ULL, initialPing);
  ASSERT_EQ(1640482451649ULL << 20ULL, initialPong);

  for (size_t i = 0; i < 10'000'000; ++i) {
    uint64_t stamp = ping.getTimeStamp(initialPong);
    // stamps must be ever-increasing
    ASSERT_GT(stamp, initialPing);
    ASSERT_GT(stamp, initialPong);
    initialPing = stamp;

    stamp = pong.getTimeStamp(initialPing);
    ASSERT_GT(stamp, initialPing);
    ASSERT_GT(stamp, initialPong);
    initialPong = stamp;
  }
}

TEST(HybridLogicalClockTest,
     test_values_increase_even_if_physical_time_goes_backwards) {
  ::HybridLogicalClockWithFixedTime hlc(1664561862434ULL);

  uint64_t initial = hlc.getTimeStamp();
  ASSERT_EQ(1664561862434ULL << 20ULL, initial);

  for (size_t i = 0; i < 10'000'000; ++i) {
    uint64_t stamp = hlc.getTimeStamp();
    // stamps must be ever-increasing
    ASSERT_GT(stamp, initial);
    initial = stamp;

    hlc.goBack();
  }
}
