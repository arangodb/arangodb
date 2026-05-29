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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "EndpointFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "RestServer/EndpointOptionsProvider.h"
#include "RestServer/ServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {
using application_features::ApplicationServer;

EndpointFeature::EndpointFeature(ApplicationServer& server)
    : HttpEndpointProvider{server, *this} {
  setOptional(true);
  startsAfter<application_features::AqlFeaturePhase>();

  startsAfter<ServerFeature>();
}

void EndpointFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  EndpointOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void EndpointFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  EndpointOptionsProvider provider;
  provider.validateOptions(options, _options);
}

void EndpointFeature::prepare() {
  buildEndpointLists();

  if (_endpointList.empty()) {
    LOG_TOPIC("2c5f0", FATAL, arangodb::Logger::FIXME)
        << "no endpoints have been specified, giving up, please use the "
           "'--server.endpoint' option";
    FATAL_ERROR_EXIT();
  }
}

std::vector<std::string> EndpointFeature::httpEndpoints() {
  auto httpEntries = _endpointList.all();
  std::vector<std::string> result;

  for (auto http : httpEntries) {
    auto uri = Endpoint::uriForm(http);

    if (!uri.empty()) {
      result.emplace_back(uri);
    }
  }

  return result;
}

void EndpointFeature::buildEndpointLists() {
  for (auto const& it : _options.endpoints) {
    bool ok = _endpointList.add(it, static_cast<int>(_options.backlogSize),
                                _options.reuseAddress);

    if (!ok) {
      LOG_TOPIC("1ddc1", FATAL, arangodb::Logger::FIXME)
          << "invalid endpoint '" << it << "'";
      FATAL_ERROR_EXIT();
    }
  }
}

}  // namespace arangodb
