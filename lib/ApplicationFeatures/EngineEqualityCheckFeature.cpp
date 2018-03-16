////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "EngineEqualityCheckFeature.h"
#include "Logger/Logger.h"
#include "Cluster/ServerState.h"

using namespace arangodb;

EngineEqualityCheckFeature::EngineEqualityCheckFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "EngineEqualityCheck") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

bool equalStorageEngines(){
  //check if dbservers use same storage engine
  return true;
}

void EngineEqualityCheckFeature::prepare() {
  if (ServerState::instance()->isCoordinator() && !equalStorageEngines()) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "The usage of different storage engines is not allowed in the cluster";
  }
}
