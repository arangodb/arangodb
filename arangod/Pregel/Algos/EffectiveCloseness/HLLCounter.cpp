////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "Pregel/CommonFormats.h"
#include "Pregel/Graph.h"

using namespace arangodb::pregel;

// leading zeros
// https://github.com/hideo55/cpp-HyperLogLog/blob/master/include/hyperloglog.hpp
#if defined(__has_builtin) && (defined(__GNUC__) || defined(__clang__))
#define _GET_CLZ(x) ::__builtin_clz(x)
#else

inline uint8_t _get_leading_zero_count(uint32_t x) {
#if defined(_MSC_VER)
  uint32_t leading_zero_len = 32;
  ::_BitScanReverse(&leading_zero_len, x);
  --leading_zero_len;
  return (uint8_t)leading_zero_len;
#else
  uint8_t v = 1;
  while (!(x & 0x80000000)) {
    v++;
    x <<= 1;
  }
  return v;
#endif
}
#define _GET_CLZ(x) _get_leading_zero_count(x)
#endif /* defined(__GNUC__) */

static std::hash<PregelID> _hashFn;
void HLLCounter::addNode(PregelID const& pregelId) {
  size_t hash = _hashFn(pregelId);
  // last 6 bits as bucket index
  size_t mask = NUM_BUCKETS - 1;
  size_t bucketIndex = (size_t)(hash & mask);

  // throw away last 6 bits
  hash >>= 6;
  // make sure the 6 new zeroes don't impact estimate
  hash |= 0xfc00000000000000L;
  // hash has now 58 significant bits left
  _buckets[bucketIndex] = (uint8_t)(_GET_CLZ(hash) + 1);
}

int32_t HLLCounter::getCount() {
  int32_t m2 = NUM_BUCKETS * NUM_BUCKETS;
  double sum = 0.0;
  for (int i = 0; i < NUM_BUCKETS; i++) {
    sum += pow(2.0, -_buckets[i]);
  }
  int32_t estimate = (int32_t)(ALPHA * m2 * (1.0 / sum));
  if (estimate < 2.5 * NUM_BUCKETS) {
    // look for empty buckets
    double V = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
      if (_buckets[i] == 0) {
        V++;
      }
    }
    if (V != 0) {
      return (int32_t)(NUM_BUCKETS * log((double)NUM_BUCKETS / V));
    }
  }
  return estimate;
}

void HLLCounter::merge(HLLCounter const& other) {
  // take the maximum of each bucket pair
  for (size_t i = 0; i < HLLCounter::NUM_BUCKETS; i++) {
    if (_buckets[i] < other._buckets[i]) {
      _buckets[i] = other._buckets[i];
    }
  }
}
