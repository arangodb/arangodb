////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for RandomGenerator class
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
/// @author Max Neunhoeffer
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct RandomGeneratorSetup {
  RandomGeneratorSetup () {
    BOOST_TEST_MESSAGE("setup RandomGenerator");
    RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  }

  ~RandomGeneratorSetup () {
    BOOST_TEST_MESSAGE("tear-down RandomGenerator");
    RandomGenerator::shutdown();
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE (RandomGeneratorTest, RandomGeneratorSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test_RandomGenerator16
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_RandomGenerator16) {
  {
    int16_t topLow = INT16_MIN + 65000;
    int16_t botHigh = INT16_MAX - 65000;
    int16_t low = INT16_MIN;
    while (true) {
      int16_t high = INT16_MAX;
      while (true) {
        for (int i = 0; i < 32; ++i) {
          int16_t r = RandomGenerator::interval(low, high);
          BOOST_CHECK(r >= low);
          BOOST_CHECK(r <= high);
        }
        if (high == botHigh) {
          break;
        }
        high -= 1000;
        if (high < low) {
          break;
        }
      }
      if (low == topLow) {
        break;
      }
      low += 1000;
    }
  }
  {
    uint16_t botHigh = UINT16_MAX - 65000;
    uint16_t high = UINT16_MAX;
    while (true) {
      for (int i = 0; i < 32; ++i) {
        uint16_t r = RandomGenerator::interval(high);
        BOOST_CHECK(r <= high);
      }
      if (high == botHigh) {
        break;
      }
      high -= 1000;
    }
  }

  // One element intervals:
  for (int i = 0; i < 100; ++i) {
    int16_t x = 0;
    int16_t r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = 10000;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = -10000;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = INT16_MAX;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = INT16_MIN;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_RandomGenerator32
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_RandomGenerator32) {
  {
    int32_t topLow = INT32_MIN + 4288000000;
    int32_t botHigh = INT32_MAX - 4288000000;
    int32_t low = INT32_MIN;
    while (true) {
      int32_t high = INT32_MAX;
      while (true) {
        for (int i = 0; i < 32; ++i) {
          int32_t r = RandomGenerator::interval(low, high);
          BOOST_CHECK(r >= low);
          BOOST_CHECK(r <= high);
        }
        if (high == botHigh) {
          break;
        }
        high -= 32000000;
        if (high < low) {
          break;
        }
      }
      if (low == topLow) {
        break;
      }
      low += 32000000;
    }
  }
  {
    uint32_t botHigh = UINT32_MAX - 4288000000;
    uint32_t high = UINT32_MAX;
    while (true) {
      for (int i = 0; i < 32; ++i) {
        uint32_t r = RandomGenerator::interval(high);
        BOOST_CHECK(r <= high);
      }
      if (high == botHigh) {
        break;
      }
      high -= 32000000;
    }
  }

  // One element intervals:
  for (int i = 0; i < 100; ++i) {
    int32_t x = 0;
    int32_t r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = 10000000;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = -10000000;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = INT32_MAX;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = INT32_MIN;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test_RandomGenerator64
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_RandomGenerator64) {
  {
    int64_t topLow = INT64_MIN + 18410000000000000000ull;
    int64_t botHigh = INT64_MAX - 18410000000000000000ull;
    int64_t low = INT64_MIN;
    while (true) {
      int64_t high = INT64_MAX;
      while (true) {
        for (int i = 0; i < 32; ++i) {
          int64_t r = RandomGenerator::interval(low, high);
          BOOST_CHECK(r >= low);
          BOOST_CHECK(r <= high);
        }
        if (high == botHigh) {
          break;
        }
        high -= 70000000000000000ll;
        if (high < low) {
          break;
        }
      }
      if (low == topLow) {
        break;
      }
      low += 70000000000000000ll;
    }
  }
  {
    uint64_t botHigh = UINT64_MAX - 18410000000000000000ull;
    uint64_t high = UINT64_MAX;
    while (true) {
      for (int i = 0; i < 32; ++i) {
        uint64_t r = RandomGenerator::interval(high);
        BOOST_CHECK(r <= high);
      }
      if (high == botHigh) {
        break;
      }
      high -= 70000000000000000ull;
    }
  }

  // One element intervals:
  for (int i = 0; i < 100; ++i) {
    int64_t x = 0;
    int64_t r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = 10000000;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = -10000000;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = INT64_MAX;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
    x = INT64_MIN;
    r = RandomGenerator::interval(x, x);
    BOOST_CHECK(r == x);
  }

  // Case used in the agency:
  for (int i = 0; i < 10000; ++i) {
    int64_t low = 1000000;
    int64_t high = 5000000;
    int64_t r = RandomGenerator::interval(low, high);
    BOOST_CHECK(low <= r && r <= high);
  }

  // Known bad cases before bug fix:
  for (int i = 0; i < 10000; ++i) {
    int64_t low = 0;
    int64_t high = 3ll << 31;
    int64_t r = RandomGenerator::interval(low, high);
    BOOST_CHECK(low <= r && r <= high);
  }

}



////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
