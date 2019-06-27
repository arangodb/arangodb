////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_REST_SERVER_FLUSH_FEATURE_H
#define ARANGODB_REST_SERVER_FLUSH_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/ReadWriteLock.h"

struct TRI_vocbase_t;  // forward declaration

namespace arangodb {

class FlushThread;

class FlushFeature final : public application_features::ApplicationFeature {
 public:

  /// @brief handle a 'Flush' marker during recovery
  /// @param vocbase the vocbase the marker applies to
  /// @param slice the originally stored marker body
  /// @return success
  typedef std::function<Result(TRI_vocbase_t const& vocbase, velocypack::Slice const& slice)> FlushRecoveryCallback;

  /// @brief write a 'Flush' marker to the WAL, on success update the
  ///        corresponding TRI_voc_tick_t for the subscription
  struct FlushSubscription {
    virtual ~FlushSubscription() = default;
    virtual Result commit(velocypack::Slice const& data) = 0;
  };
  class FlushSubscriptionBase; // forward declaration

  // used by catch tests
  #ifdef ARANGODB_USE_GOOGLE_TESTS
    typedef std::function<Result(std::string const&, TRI_vocbase_t const&, velocypack::Slice const&)> DefaultFlushSubscription;
    static DefaultFlushSubscription _defaultFlushSubscription;
  #endif

  explicit FlushFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions> options) override;

  /// @brief add a callback for 'Flush' recovery markers of the specified type
  ///        encountered during recovery
  /// @param type the 'Flush' marker type to invoke callback for
  /// @param callback the callback to invoke
  /// @return success, false == handler for the specified type already registered
  /// @note not thread-safe on the assumption of static factory registration
  static bool registerFlushRecoveryCallback( // register callback
    std::string const& type, // marker type
    FlushRecoveryCallback const& callback // marker callback
  );

  /// @brief register a flush subscription that will ensure replay of all WAL
  ///        entries after the latter of registration or the last successful
  ///        token commit
  /// @param type the 'Flush' marker type to invoke callback for
  /// @param vocbase the associate vocbase
  /// @return a token used for marking flush synchronization
  ///         release of the token will unregister the subscription
  ///         nullptr == error
  std::shared_ptr<FlushSubscription> registerFlushSubscription( // register subscription
      std::string const& type, // marker type
      TRI_vocbase_t const& vocbase // marker vocbase
  );

  /// @brief release all ticks not used by the flush subscriptions
  /// @param 'count' a number of released subscriptions
  arangodb::Result releaseUnusedTicks(size_t& count);

  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  static bool isRunning() { return _isRunning.load(); }

 private:
  uint64_t _flushInterval;
  std::unique_ptr<FlushThread> _flushThread;
  static std::atomic<bool> _isRunning;
  basics::ReadWriteLock _threadLock;
  std::unordered_set<std::shared_ptr<FlushSubscriptionBase>> _flushSubscriptions;
  std::mutex _flushSubscriptionsMutex;
  bool _stopped;
};

}  // namespace arangodb

#endif
