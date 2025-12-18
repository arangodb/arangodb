#pragma once

#include <memory>

#include "../SchedulerMetrics.h"

#include "RestServer/arangod.h"

#include "Cluster/ServerState.h"

#include "Metrics/Gauge.h"

#include "Network/NetworkFeature.h"

namespace arangodb {
class NetworkFeature;

class LowPrioAntiOverwhelm final {
 public:
  explicit LowPrioAntiOverwhelm(
      ArangodServer const& server, uint64_t ongoingLowPriorityLimit,
      std::shared_ptr<SchedulerMetrics> const& metrics);

  // This code will most-likely be deleted/moved to the AcceptanceQueue and thus
  // this whole file will be removed. This function is called in the
  // SupervisedScheduler::canPullFromQueue() and should not take more time than
  // needed. Testing via BoundedQueuePerfTest concluded that `always_inline` is
  // needed here to not introduce a huge overhead in the Scheduler.
  [[nodiscard]] __attribute__((always_inline)) bool canPullFromLowPrioQueue()
      const noexcept {
    // We limit the number of ongoing low priority jobs to prevent a cluster
    // from getting overwhelmed.
    if (_ongoingLowPriorityLimit > 0) {
      TRI_ASSERT(!ServerState::instance()->isDBServer());
      // note: _ongoingLowPriorityLimit will be 0 on DB servers, as in a
      // cluster the coordinators will act as the gatekeepers that control
      // the amount of requests that get into the system
      if (_metrics->_ongoingLowPriorityGauge.load() >=
          _ongoingLowPriorityLimit) {
        return false;
      }
      // Because jobs may fan out to multiple servers and shards, we also limit
      // dequeuing based on the number of internal requests in flight
      if (_nf.isSaturated()) {
        return false;
      }
    }
    return true;
  }

 private:
  uint64_t const _ongoingLowPriorityLimit;
  std::shared_ptr<SchedulerMetrics> const _metrics;
  NetworkFeature const& _nf;
};
}  // namespace arangodb