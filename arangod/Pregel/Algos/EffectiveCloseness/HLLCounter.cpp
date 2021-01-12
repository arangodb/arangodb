////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "Basics/fasthash.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/Graph.h"

using namespace arangodb::pregel;

// leading zeros of an integer
#if defined(__has_builtin) && (defined(__GNUC__) || defined(__clang__))

#define _GET_CLZ(x, b) (uint8_t) std::min(b, ::__builtin_clz(x)) + 1

#else

inline uint8_t _get_leading_zero_count(uint32_t x, uint8_t b) {

#if defined(_MSC_VER)
  unsigned long leading_zero_len = 32;
  _BitScanReverse(&leading_zero_len, x);
  --leading_zero_len;
  return std::min(b, (uint8_t)leading_zero_len);
#else
  uint8_t v = 1;
  while (v <= b && !(x & 0x80000000)) {
    v++;
    x <<= 1;
  }
  return v;
#endif
}
#define _GET_CLZ(x, b) _get_leading_zero_count(x, b)
#endif /* defined(__GNUC__) */

static uint32_t hashPregelId(PregelID const& pregelId) {
  uint32_t h1 = fasthash32(pregelId.key.data(), pregelId.key.length(), 0xf007ba11UL);
  uint64_t h2 = fasthash64_uint64(pregelId.shard, 0xdefec7edUL);
  uint32_t h3 = (uint32_t)(h2 - (h2 >> 32));
  return h1 ^ (h3 << 1);
}

void HLLCounter::addNode(PregelID const& pregelId) {
  uint32_t hashid = hashPregelId(pregelId);
  // last 6 bits as bucket index
  uint32_t index = hashid >> (32 - 6);
  uint8_t rank = _GET_CLZ((hashid << 6), 32 - 6);
  if (rank > _buckets[index]) {
    _buckets[index] = rank;
  }
}

static const double pow_2_32 = 4294967296.0;       ///< 2^32
static const double neg_pow_2_32 = -4294967296.0;  ///< -(2^32)
uint32_t HLLCounter::getCount() {
  double alphaMM = ALPHA * NUM_BUCKETS * NUM_BUCKETS;
  double sum = 0.0;
  for (uint32_t i = 0; i < NUM_BUCKETS; i++) {
    sum += 1.0 / (1 << _buckets[i]);
  }
  double estimate = alphaMM / sum;  // E in the original paper
  if (estimate <= 2.5 * NUM_BUCKETS) {
    uint32_t zeros = 0;
    for (uint32_t i = 0; i < NUM_BUCKETS; i++) {
      if (_buckets[i] == 0) {
        zeros++;
      }
    }
    if (zeros != 0) {
      estimate = NUM_BUCKETS * std::log(((double)NUM_BUCKETS / zeros));
    }
  } else if (estimate > (1.0 / 30.0) * pow_2_32) {
    estimate = neg_pow_2_32 * log(1.0 - (estimate / pow_2_32));
  }
  return (uint32_t)estimate;
}

void HLLCounter::merge(HLLCounter const& other) {
  for (size_t i = 0; i < NUM_BUCKETS; ++i) {
    if (_buckets[i] < other._buckets[i]) {
      _buckets[i] |= other._buckets[i];
    }
  }
}
