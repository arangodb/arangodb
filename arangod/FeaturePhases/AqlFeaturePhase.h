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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeaturePhase.h"

namespace arangodb {
class AqlFeature;
class SystemDatabaseFeature;
class QueryRegistryFeature;
namespace aql {
class AqlFunctionFeature;
class OptimizerRulesFeature;
}  // namespace aql
namespace iresearch {

class IResearchAnalyzerFeature;
class IResearchFeature;
}  // namespace iresearch
namespace pregel {
class PregelFeature;
}
namespace application_features {

class CommunicationFeaturePhase;
class V8FeaturePhase;

class AqlFeaturePhase : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "AQLPhase"; }

  template<typename Server>
  explicit AqlFeaturePhase(Server& server)
      : ApplicationFeaturePhase(server, Server::template id<AqlFeaturePhase>(),
                                name()) {
    setOptional(false);
    startsAfter<CommunicationFeaturePhase, Server>();
    startsAfter<V8FeaturePhase, Server>();

    startsAfter<AqlFeature, Server>();
    startsAfter<aql::AqlFunctionFeature, Server>();
    startsAfter<iresearch::IResearchAnalyzerFeature, Server>();
    startsAfter<iresearch::IResearchFeature, Server>();
    startsAfter<aql::OptimizerRulesFeature, Server>();
    startsAfter<pregel::PregelFeature, Server>();
    startsAfter<QueryRegistryFeature, Server>();
    startsAfter<SystemDatabaseFeature, Server>();
  }
};

}  // namespace application_features
}  // namespace arangodb
