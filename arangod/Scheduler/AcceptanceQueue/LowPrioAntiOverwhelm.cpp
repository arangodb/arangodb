#include "LowPrioAntiOverwhelm.h"

#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb {
LowPrioAntiOverwhelm::LowPrioAntiOverwhelm(
    application_features::ApplicationServer const& server,
    uint64_t const ongoingLowPriorityLimit,
    std::shared_ptr<SchedulerMetrics> const& metrics)
    : _ongoingLowPriorityLimit(ongoingLowPriorityLimit),
      _metrics(metrics),
      _nf(server.getFeature<NetworkFeature>()) {}

}  // namespace arangodb