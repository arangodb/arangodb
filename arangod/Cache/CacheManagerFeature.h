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

#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/CacheOptionsProvider.h"
#include "RestServer/arangod.h"

namespace arangodb {
struct CacheOptionsProvider;
class CacheRebalancerThread;

namespace cache {
class Manager;
}

class CacheManagerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() { return "CacheManager"; }

  explicit CacheManagerFeature(Server& server,
                               CacheOptionsProvider const& provider);
  ~CacheManagerFeature();

  void start() override final;
  void beginShutdown() override final;
  void stop() override final;

  /// @brief Pointer to global instance; Can be null if cache is disabled
  cache::Manager* manager();

  std::size_t minValueSizeForEdgeCompression() const noexcept;
  std::uint32_t accelerationFactorForEdgeCompression() const noexcept;

 private:
  std::unique_ptr<cache::Manager> _manager;
  std::unique_ptr<CacheRebalancerThread> _rebalancer;

  CacheOptionsProvider const& _provider;
  CacheOptions _options;
};

}  // namespace arangodb
