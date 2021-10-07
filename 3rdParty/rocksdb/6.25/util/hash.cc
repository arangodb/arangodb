//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/hash.h"

#include <string.h>

#include "port/lang.h"
#include "util/coding.h"
#include "util/hash128.h"
#include "util/math128.h"
#include "util/xxhash.h"
#include "util/xxph3.h"

namespace ROCKSDB_NAMESPACE {

uint64_t (*kGetSliceNPHash64UnseededFnPtr)(const Slice&) = &GetSliceHash64;

uint32_t Hash(const char* data, size_t n, uint32_t seed) {
  // MurmurHash1 - fast but mediocre quality
  // https://github.com/aappleby/smhasher/wiki/MurmurHash1
  //
  const uint32_t m = 0xc6a4a793;
  const uint32_t r = 24;
  const char* limit = data + n;
  uint32_t h = static_cast<uint32_t>(seed ^ (n * m));

  // Pick up four bytes at a time
  while (data + 4 <= limit) {
    uint32_t w = DecodeFixed32(data);
    data += 4;
    h += w;
    h *= m;
    h ^= (h >> 16);
  }

  // Pick up remaining bytes
  switch (limit - data) {
    // Note: The original hash implementation used data[i] << shift, which
    // promotes the char to int and then performs the shift. If the char is
    // negative, the shift is undefined behavior in C++. The hash algorithm is
    // part of the format definition, so we cannot change it; to obtain the same
    // behavior in a legal way we just cast to uint32_t, which will do
    // sign-extension. To guarantee compatibility with architectures where chars
    // are unsigned we first cast the char to int8_t.
    case 3:
      h += static_cast<uint32_t>(static_cast<int8_t>(data[2])) << 16;
      FALLTHROUGH_INTENDED;
    case 2:
      h += static_cast<uint32_t>(static_cast<int8_t>(data[1])) << 8;
      FALLTHROUGH_INTENDED;
    case 1:
      h += static_cast<uint32_t>(static_cast<int8_t>(data[0]));
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}

// We are standardizing on a preview release of XXH3, because that's
// the best available at time of standardizing.
//
// In testing (mostly Intel Skylake), this hash function is much more
// thorough than Hash32 and is almost universally faster. Hash() only
// seems faster when passing runtime-sized keys of the same small size
// (less than about 24 bytes) thousands of times in a row; this seems
// to allow the branch predictor to work some magic. XXH3's speed is
// much less dependent on branch prediction.
//
// Hashing with a prefix extractor is potentially a common case of
// hashing objects of small, predictable size. We could consider
// bundling hash functions specialized for particular lengths with
// the prefix extractors.
uint64_t Hash64(const char* data, size_t n, uint64_t seed) {
  return XXPH3_64bits_withSeed(data, n, seed);
}

uint64_t Hash64(const char* data, size_t n) {
  // Same as seed = 0
  return XXPH3_64bits(data, n);
}

uint64_t GetSlicePartsNPHash64(const SliceParts& data, uint64_t seed) {
  // TODO(ajkr): use XXH3 streaming APIs to avoid the copy/allocation.
  size_t concat_len = 0;
  for (int i = 0; i < data.num_parts; ++i) {
    concat_len += data.parts[i].size();
  }
  std::string concat_data;
  concat_data.reserve(concat_len);
  for (int i = 0; i < data.num_parts; ++i) {
    concat_data.append(data.parts[i].data(), data.parts[i].size());
  }
  assert(concat_data.size() == concat_len);
  return NPHash64(concat_data.data(), concat_len, seed);
}

Unsigned128 Hash128(const char* data, size_t n, uint64_t seed) {
  auto h = XXH3_128bits_withSeed(data, n, seed);
  return (Unsigned128{h.high64} << 64) | (h.low64);
}

Unsigned128 Hash128(const char* data, size_t n) {
  // Same as seed = 0
  auto h = XXH3_128bits(data, n);
  return (Unsigned128{h.high64} << 64) | (h.low64);
}

void Hash2x64(const char* data, size_t n, uint64_t* high64, uint64_t* low64) {
  // Same as seed = 0
  auto h = XXH3_128bits(data, n);
  *high64 = h.high64;
  *low64 = h.low64;
}

}  // namespace ROCKSDB_NAMESPACE
