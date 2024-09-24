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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
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
