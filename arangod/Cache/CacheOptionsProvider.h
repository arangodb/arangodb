////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>

namespace arangodb {

struct CacheOptions {
  // lower fill ratio for a hash table. if a hash table's load factor is
  // less than this ratio, it is subject to shrinking
  double idealLowerFillRatio = 0.04;
  // upper fill ratio for a hash table. if a hash table's load factor is
  // higher than this ratio, it is subject to doubling in size!
  double idealUpperFillRatio = 0.25;
  // 1GB, so effectively compression is disabled by default.
  std::size_t minValueSizeForEdgeCompression = 1'073'741'824ULL;
  // lz4-internal acceleration factor for compression.
  // values > 1 could mean slower compression, but faster decompression
  std::uint32_t accelerationFactorForEdgeCompression = 1;
  // cache size will be set dynamically later based on available RAM
  std::uint64_t cacheSize = 0;
  std::uint64_t rebalancingInterval = 2'000'000ULL;  // 2s
  // maximum memory usage for spare hash tables kept around by the cache.
  std::uint64_t maxSpareAllocation = 67'108'864ULL;  // 64MB
  // whether or not we want recent hit rates. if this is turned off,
  // we only get global hit rates over the entire lifetime of a cache
  bool enableWindowedStats = true;
};

struct CacheOptionsProvider {
  virtual ~CacheOptionsProvider() = default;

  virtual CacheOptions getOptions() const = 0;
};

}  // namespace arangodb
