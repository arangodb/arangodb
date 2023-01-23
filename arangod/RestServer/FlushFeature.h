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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "VocBase/voc-types.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

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
  explicit FlushFeature(application_features::ApplicationServer& server);

  ~FlushFeature();

  void collectOptions(
      std::shared_ptr<options::ProgramOptions> options) override;

  /// @brief register a flush subscription that will ensure replay of all WAL
  ///        entries after the latter of registration or the last successful
  ///        token commit
  /// @param subscription to register
  void registerFlushSubscription(
      std::shared_ptr<FlushSubscription> const& subscription);

  arangodb::Result releaseUnusedTicks(size_t& count, TRI_voc_tick_t& tick);
  /// returns number of active flush subscriptions removed, the number of stale
  /// flush scriptions removed, and the tick value up to which the storage
  /// engine could release ticks. if no active or stale flush subscriptions were
  /// found, the returned tick value is 0.
  std::tuple<size_t, size_t, TRI_voc_tick_t> releaseUnusedTicks();

  void stop() override;

 private:
  std::mutex _flushSubscriptionsMutex;
  std::vector<std::weak_ptr<FlushSubscription>> _flushSubscriptions;
  bool _stopped;
};

}  // namespace arangodb
