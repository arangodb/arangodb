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

#include <string>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "GeneralServer/ServerSecurityOptionsProvider.h"
#include "Basics/files.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

std::atomic<bool> ServerSecurityFeature::_fedRAMPHigh{false};

ServerSecurityFeature::ServerSecurityFeature(
    application_features::ApplicationServer& server)
    : ServerSecurityFeature(server, ServerSecurityFeatureOptions{}) {}

ServerSecurityFeature::ServerSecurityFeature(
    application_features::ApplicationServer& server,
    ServerSecurityFeatureOptions options)
    : ApplicationFeature{server, *this}, _options(std::move(options)) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();

  // detect FedRAMP High mode as early as possible, so that the flag is
  // already available while other features are being set up.
  // note: any non-empty value enables the mode, regardless of its content
  // (even "0" or "false"). this is intentional, see kFedRAMPHighEnvVar.
  std::string value;
  _fedRAMPHigh.store(TRI_GETENV(kFedRAMPHighEnvVar, value) && !value.empty(),
                     std::memory_order_relaxed);

  // in FedRAMP High mode, security events must always be logged, regardless
  // of the configured log levels.
  Logger::setAlwaysLogSecurityEvents(isFedRAMPHigh());
}

bool ServerSecurityFeature::isRestApiHardened() const noexcept {
  return _options.hardenedRestApi;
}
