////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "RestServer/arangod.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

struct TRI_vocbase_t;

namespace arangodb {
class FlushThread;

//////////////////////////////////////////////////////////////////////////////
/// @struct FlushSubscription
/// @brief subscription is intenteded to notify FlushFeature
///        on the WAL tick which can be safely released
//////////////////////////////////////////////////////////////////////////////
struct FlushSubscription {
  virtual ~FlushSubscription() = default;
  virtual TRI_voc_tick_t tick() const = 0;
};

class FlushFeature final : public ArangodFeature {
 public:
  /// @brief handle a 'Flush' marker during recovery
  /// @param vocbase the vocbase the marker applies to
  /// @param slice the originally stored marker body
  /// @return success
  using FlushRecoveryCallback = std::function<Result(
      TRI_vocbase_t const& vocbase, velocypack::Slice const& slice)>;

  static constexpr std::string_view name() noexcept { return "Flush"; }

  explicit FlushFeature(Server& server);

  ~FlushFeature();

  void collectOptions(
      std::shared_ptr<options::ProgramOptions> options) override;

  /// @brief register a flush subscription that will ensure replay of all WAL
  ///        entries after the latter of registration or the last successful
  ///        token commit
  /// @param subscription to register
  void registerFlushSubscription(
      const std::shared_ptr<FlushSubscription>& subscription);

  /// @brief release all ticks not used by the flush subscriptions
  /// returns number of stale flush subscriptions removed and the tick value
  /// up to which the storage engine could release ticks
  std::pair<size_t, TRI_voc_tick_t> releaseUnusedTicks();

  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;

 private:
  uint64_t _flushInterval;

  basics::ReadWriteLock _threadLock;
  std::unique_ptr<FlushThread> _flushThread;

  std::mutex _flushSubscriptionsMutex;
  std::vector<std::weak_ptr<FlushSubscription>> _flushSubscriptions;
  bool _stopped;
};

}  // namespace arangodb
