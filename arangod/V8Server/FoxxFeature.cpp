////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "FoxxFeature.h"

#include "FeaturePhases/ServerFeaturePhase.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

FoxxFeature::FoxxFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, "FoxxQueues"),
      _pollInterval(1.0),
      _enabled(true),
      _startupWaitForSelfHeal(false) {
  setOptional(true);
  startsAfter<ServerFeaturePhase>();
}

void FoxxFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("foxx", "Configure Foxx");

  options->addOldOption("server.foxx-queues", "foxx.queues");
  options->addOldOption("server.foxx-queues-poll-interval",
                        "foxx.queues-poll-interval");

  options->addOption("--foxx.queues", "enable Foxx queues",
                     new BooleanParameter(&_enabled),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle));

  options->addOption("--foxx.queues-poll-interval",
                     "poll interval (in seconds) for Foxx queue manager",
                     new DoubleParameter(&_pollInterval),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle));

  options->addOption("--foxx.force-update-on-startup",
                     "ensure all Foxx services are synchronized before "
                     "completeing the boot sequence",
                     new BooleanParameter(&_startupWaitForSelfHeal),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle))
                     .setIntroducedIn(30705);
}

bool FoxxFeature::startupWaitForSelfHeal() const {
  return _startupWaitForSelfHeal;
}

void FoxxFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // use a minimum for the interval
  if (_pollInterval < 0.1) {
    _pollInterval = 0.1;
  }
}

}  // namespace arangodb
