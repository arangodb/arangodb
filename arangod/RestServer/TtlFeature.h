////////////////////////////////////////////////////////////////////////////////
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_SERVER_TTL_FEATURE_H
#define ARANGOD_REST_SERVER_TTL_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Mutex.h"

#include <chrono>

namespace arangodb {
namespace velocypack {
class Builder;
}

class TtlThread;

struct TtlStatistics {
  // number of times the background thread was running
  uint64_t runs = 0;
  // number of documents removed
  uint64_t removed = 0;
  // number of times the background thread stopped prematurely because it hit the configured limit
  uint64_t reachedLimit = 0;


  TtlStatistics& operator+=(TtlStatistics const& other) {
    runs += other.runs;
    removed += other.removed;
    reachedLimit += other.reachedLimit;
    return *this;
  }

  void toVelocyPack(arangodb::velocypack::Builder&) const;
};

class TtlFeature final : public application_features::ApplicationFeature {
 public:
  explicit TtlFeature(application_features::ApplicationServer& server);
  ~TtlFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void beginShutdown() override final;
  void start() override final;
  void stop() override final;

  /// @brief turn expiring/removing outdated documents on
  void activate() { _active = true; }
  /// @brief turn expiring/removing outdated documents off
  void deactivate() { _active = false; }
  /// @brief whether or not expiring/removing outdated documents is currently turned on
  bool isActive() const { return _active; }

  TtlStatistics stats() const;

  void statsToVelocyPack(arangodb::velocypack::Builder& builder) const;

  void updateStats(TtlStatistics const& stats);
  
 private:
  void shutdownThread() noexcept;
  
 private:
  uint64_t _frequency;
  uint64_t _maxTotalRemoves;
  uint64_t _maxCollectionRemoves;
  bool _onlyLoadedCollections;
  std::atomic<bool> _active;

  std::shared_ptr<TtlThread> _thread;

  mutable Mutex _statisticsMutex; 
  TtlStatistics _statistics;
};

}  // namespace arangodb

#endif
