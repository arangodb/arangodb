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
class BootstrapFeature;
class FoxxFeature;
class FrontendFeature;
namespace application_features {

class ServerFeaturePhase;

class FoxxFeaturePhase : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "FoxxPhase"; }

  template<typename Server>
  explicit FoxxFeaturePhase(Server& server)
      : ApplicationFeaturePhase(server, Server::template id<FoxxFeaturePhase>(),
                                name()) {
    setOptional(false);
    startsAfter<ServerFeaturePhase, Server>();

    startsAfter<BootstrapFeature, Server>();
    startsAfter<FoxxFeature, Server>();
    startsAfter<FrontendFeature, Server>();
  }
};

}  // namespace application_features
}  // namespace arangodb
