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

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

class TtlThread;

struct TtlStatistics {
  // number of times the background thread was running
  uint64_t runs = 0;
  // number of documents removed
  uint64_t documentsRemoved = 0;
  // number of times the background thread stopped prematurely because it hit the configured limit
  uint64_t limitReached = 0;

  TtlStatistics& operator+=(TtlStatistics const& other) {
    runs += other.runs;
    documentsRemoved += other.documentsRemoved;
    limitReached += other.limitReached;
    return *this;
  }
  
  TtlStatistics& operator+=(arangodb::velocypack::Slice const& other);

  void toVelocyPack(arangodb::velocypack::Builder& out) const;
};
  
struct TtlProperties {
  static constexpr uint64_t minFrequency = 1 * 1000; // milliseconds
  uint64_t frequency = 30 * 1000; // milliseconds
  uint64_t maxTotalRemoves = 1000000;
  uint64_t maxCollectionRemoves = 1000000;
  bool onlyLoadedCollections = true;
  
  void toVelocyPack(arangodb::velocypack::Builder& out, bool isActive) const;
  Result fromVelocyPack(arangodb::velocypack::Slice const& properties);
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

  /// @brief allow the TTL thread to run or not
  /// this is not a user-facing method, but it is used by the active-failover
  /// setup internally. the value is orthogonal to the value of _active, so it
  /// is possible that allowRunning is true (leader in active failover setup)
  /// but the user has set _active to false.
  void allowRunning(bool value);

  /// @brief wait until the TTL thread has successfully stopped working
  void waitForThreadWork();

  /// @brief turn expiring/removing outdated documents on
  void activate();
  /// @brief turn expiring/removing outdated documents off, blocks until
  /// the TTL thread has left the actual document removal routine
  void deactivate();

  /// @brief whether or not expiring/removing outdated documents is currently turned on
  bool isActive() const;

  void statsToVelocyPack(arangodb::velocypack::Builder& out) const;

  void updateStats(TtlStatistics const& stats);
  
  void propertiesToVelocyPack(arangodb::velocypack::Builder& out) const;
  Result propertiesFromVelocyPack(arangodb::velocypack::Slice const& slice, arangodb::velocypack::Builder& out);
  
  TtlProperties properties() const;
  
 private:
  void shutdownThread() noexcept;
  
 private:
  /// @brief protects _properties and _active
  mutable Mutex _propertiesMutex;
  TtlProperties _properties;
  
  /// @brief protects _statistics
  mutable Mutex _statisticsMutex; 
  TtlStatistics _statistics;
  
  /// @brief protects _thread
  mutable Mutex _threadMutex; 
  std::unique_ptr<TtlThread> _thread;
 
  /// @brief internal active flag, used by HeartbeatThread in active failover setups
  /// the value is orthogonal to the user-facing _active flag
  bool _allowRunning; 

  /// @brief user-facing active flag, can be changed by end users
  bool _active;
};

}  // namespace arangodb

#endif
