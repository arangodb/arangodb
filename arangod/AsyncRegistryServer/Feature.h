#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Async/Registry/registry_variable.h"
#include "Async/Registry/Metrics.h"

namespace arangodb::async_registry {

class Feature final : public application_features::ApplicationFeature {
 private:
  static auto create_metrics(arangodb::metrics::MetricsFeature& metrics_feature)
      -> std::shared_ptr<const Metrics>;

 public:
  static constexpr std::string_view name() { return "Coroutines"; }

  template<typename Server>
  Feature(Server& server)
      : application_features::ApplicationFeature{server,
                                                 Server::template id<Feature>(),
                                                 name()},
        metrics{create_metrics(
            server.template getFeature<arangodb::metrics::MetricsFeature>())} {
    coroutine_registry.set_metrics(metrics);
    // startsAfter<Bla, Server>();
  }

 private:
  std::shared_ptr<const Metrics> metrics;
};

}  // namespace arangodb::async_registry
