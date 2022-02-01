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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel3Feature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/application-exit.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Parameters.h"
#include "Logger/LogMacros.h"
#include "Cluster/ServerState.h"

#include <memory>

using namespace arangodb::options;
using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::pregel3;

Pregel3Feature::Pregel3Feature(Server& server) : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<application_features::V8FeaturePhase>();
}

void Pregel3Feature::prepare() {
  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isAgent()) {
    setEnabled(false);
    return;
  }
}

void Pregel3Feature::collectOptions(std::shared_ptr<ProgramOptions> options) {
#if defined(ARANGODB_ENABLE_MAINTAINER_MODE)
  options->addSection("pregel3", "Options for Pregel3");

  options->addOption("--pregel3.parallelism",
                     "magic number for fastness. Much Good. So Parallel.",
                     new SizeTParameter(&_settings._parallelism));
#endif
}

void Pregel3Feature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // We accept it all
}

Pregel3Feature::~Pregel3Feature() = default;

void Pregel3Feature::createQuery(std::string queryId,
                                 const pregel3::GraphSpecification& graph) {
  // TODO: setup query
  LOG_DEVEL << "Create a query now";

  _queries.emplace(queryId, Query(queryId, graph));
}
