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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Ssl/SslFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace arangodb {
namespace application_features {

class BasicFeaturePhaseClient : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept {
    return "BasicsPhaseClient";
  }

  explicit BasicFeaturePhaseClient(
      application_features::ApplicationServer& server)
      : ApplicationFeaturePhase(server, *this) {
    setOptional(false);
    if (server.template hasFeature<GreetingsFeaturePhase>()) {
      startsAfter<GreetingsFeaturePhase>();
    }
#ifdef USE_ENTERPRISE
    startsAfter<EncryptionFeature>();
#endif
    if (server.template hasFeature<SslFeature>()) {
      startsAfter<SslFeature>();
    }
    if (server.template hasFeature<HttpEndpointProvider>()) {
      startsAfter<HttpEndpointProvider>();
    }
  }
};

}  // namespace application_features
}  // namespace arangodb
