////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Metrics/Fwd.h"
#include "RestServer/FlushOptionsProvider.h"
#include "RestServer/FlushSubscription.h"
#include "RestServer/IFlushControl.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

namespace arangodb {
namespace metrics {
struct IRegistry;
}  // namespace metrics
class FlushThread;

class FlushFeature final : public application_features::ApplicationFeature,
                           public IFlushControl {
 public:
  static constexpr std::string_view name() noexcept { return "Flush"; }

  FlushFeature(application_features::ApplicationServer& server,
               metrics::IRegistry& metricsRegistry);

  ~FlushFeature();

  /// @brief register a flush subscription that will ensure replay of all WAL
  ///        entries after the latter of registration or the last successful
  ///        token commit
  /// @param subscription to register
  void registerFlushSubscription(
      std::shared_ptr<FlushSubscription> const& subscription) override;

  /// @brief release all ticks not used by the flush subscriptions
  /// returns number of active flush subscriptions removed, the number of stale
  /// flush scriptions removed, and the tick value up to which the storage
  /// engine could release ticks. if no active or stale flush subscriptions were
  /// found, the returned tick value is 0.
  bool isEnabled() const noexcept override {
    return application_features::ApplicationFeature::isEnabled();
  }
  std::tuple<std::size_t, std::size_t, TRI_voc_tick_t> releaseUnusedTicks()
      override;

  void stop() override;

 private:
  std::mutex _flushSubscriptionsMutex;
  std::vector<std::weak_ptr<FlushSubscription>> _flushSubscriptions;
  bool _stopped;

  metrics::Gauge<uint64_t>& _metricsFlushSubscriptions;
};

}  // namespace arangodb
