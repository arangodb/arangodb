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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cache/CacheOptionsProvider.h"
#include "RestServer/arangod.h"

namespace arangodb {

class CacheOptionsFeature final : public ArangodFeature,
                                  public CacheOptionsProvider {
 public:
  static constexpr std::string_view name() { return "CacheOptions"; }

  explicit CacheOptionsFeature(Server& server);
  ~CacheOptionsFeature() = default;

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  double idealLowerFillRatio() const noexcept override;
  double idealUpperFillRatio() const noexcept override;
  std::size_t minValueSizeForEdgeCompression() const noexcept override;
  std::uint32_t accelerationFactorForEdgeCompression() const noexcept override;
  std::uint64_t cacheSize() const noexcept override;
  std::uint64_t rebalancingInterval() const noexcept override;
  std::uint64_t maxSpareAllocation() const noexcept override;

 private:
  static constexpr std::uint64_t minRebalancingInterval = 500 * 1000;

  CacheOptions _options;
};

}  // namespace arangodb
