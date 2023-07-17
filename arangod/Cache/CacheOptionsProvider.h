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

#include <cstdint>

namespace arangodb {

struct CacheOptions {
  double idealLowerFillRatio = 0.04;
  double idealUpperFillRatio = 0.25;
  std::size_t minValueSizeForEdgeCompression = 1'073'741'824ULL;  // 1GB
  std::uint32_t accelerationFactorForEdgeCompression = 1;
  std::uint64_t cacheSize = 0;
  std::uint64_t rebalancingInterval = 2'000'000ULL;
  std::uint64_t maxSpareAllocation = 67'108'864ULL;  // 64MB
};

struct CacheOptionsProvider {
  virtual ~CacheOptionsProvider() = default;

  virtual double idealLowerFillRatio() const noexcept = 0;
  virtual double idealUpperFillRatio() const noexcept = 0;
  virtual std::size_t minValueSizeForEdgeCompression() const noexcept = 0;
  virtual std::uint32_t accelerationFactorForEdgeCompression()
      const noexcept = 0;
  virtual std::uint64_t cacheSize() const noexcept = 0;
  virtual std::uint64_t rebalancingInterval() const noexcept = 0;
  virtual std::uint64_t maxSpareAllocation() const noexcept = 0;
};

}  // namespace arangodb
