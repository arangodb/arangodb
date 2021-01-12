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
/// @author Jan Steemann
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_FLUSH_FEATURE_H
#define ARANGODB_REST_SERVER_FLUSH_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/ReadWriteLock.h"
#include "Utils/FlushThread.h"
#include "VocBase/voc-types.h"

#include <list>

struct TRI_vocbase_t;

namespace arangodb {

//////////////////////////////////////////////////////////////////////////////
/// @struct FlushSubscription
/// @brief subscription is intenteded to notify FlushFeature
///        on the WAL tick which can be safely released
//////////////////////////////////////////////////////////////////////////////
struct FlushSubscription {
  virtual ~FlushSubscription() = default;
  virtual TRI_voc_tick_t tick() const = 0;
};

class FlushFeature final : public application_features::ApplicationFeature {
 public:
  /// @brief handle a 'Flush' marker during recovery
  /// @param vocbase the vocbase the marker applies to
  /// @param slice the originally stored marker body
  /// @return success
  typedef std::function<Result(TRI_vocbase_t const& vocbase, velocypack::Slice const& slice)> FlushRecoveryCallback;

  explicit FlushFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions> options) override;

  /// @brief register a flush subscription that will ensure replay of all WAL
  ///        entries after the latter of registration or the last successful
  ///        token commit
  /// @param subscription to register
  void registerFlushSubscription(const std::shared_ptr<FlushSubscription>& subscription);

  /// @brief release all ticks not used by the flush subscriptions
  /// @param 'count' a number of released subscriptions
  /// @param 'tick' released tick
  arangodb::Result releaseUnusedTicks(size_t& count, TRI_voc_tick_t& tick);

  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override { }

  static bool isRunning() { return _isRunning.load(); }

 private:
  static std::atomic<bool> _isRunning;

  uint64_t _flushInterval;
  std::unique_ptr<FlushThread> _flushThread;
  basics::ReadWriteLock _threadLock;
  std::list<std::weak_ptr<FlushSubscription>> _flushSubscriptions;
  std::mutex _flushSubscriptionsMutex;
  bool _stopped;
};

}  // namespace arangodb

#endif
