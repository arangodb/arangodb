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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CACHE_CACHE_MANAGER_FEATURE_H
#define ARANGOD_CACHE_CACHE_MANAGER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/Manager.h"

namespace arangodb {

class CacheManagerFeature final : public application_features::ApplicationFeature {
 public:
  explicit CacheManagerFeature(application_features::ApplicationServer& server);
  ~CacheManagerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;

  /// @brief Pointer to global instance; Can be null if cache is disabled
  cache::Manager* manager();

 private:
  static constexpr uint64_t minRebalancingInterval = 500 * 1000;

  std::unique_ptr<cache::Manager> _manager;
  std::unique_ptr<CacheRebalancerThread> _rebalancer;
  std::uint64_t _cacheSize;
  std::uint64_t _rebalancingInterval;
};

}  // namespace arangodb

#endif
