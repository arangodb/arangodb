////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ServerFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb {
class SslServerFeature;
}

namespace arangodb::application_features {

ServerFeaturePhase::ServerFeaturePhase(ArangodServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<AqlFeaturePhase, ArangodServer>();

  startsAfter<HttpEndpointProvider, ArangodServer>();
  startsAfter<GeneralServerFeature, ArangodServer>();
  startsAfter<NetworkFeature, ArangodServer>();
  startsAfter<ServerFeature, ArangodServer>();
  startsAfter<SslServerFeature, ArangodServer>();
  startsAfter<StatisticsFeature, ArangodServer>();
  startsAfter<UpgradeFeature, ArangodServer>();
}
}  // namespace arangodb::application_features
