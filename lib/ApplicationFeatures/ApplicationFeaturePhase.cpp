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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeaturePhase.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {
namespace application_features {

ApplicationFeaturePhase::ApplicationFeaturePhase(ApplicationServer& server,
                                                 std::string const& name)
    : ApplicationFeature(server, name) {}

void ApplicationFeaturePhase::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  LOG_TOPIC("1463e", DEBUG, arangodb::Logger::STARTUP)
      << "ValidateOptions for phase " << name() << " completed";
}

void ApplicationFeaturePhase::prepare() {
  LOG_TOPIC("12f47", DEBUG, arangodb::Logger::STARTUP) << "Prepare for phase " << name() << " completed";
}

void ApplicationFeaturePhase::start() {
  LOG_TOPIC("d730b", DEBUG, arangodb::Logger::STARTUP) << "Start for phase " << name() << " completed";
}

void ApplicationFeaturePhase::beginShutdown() {
  LOG_TOPIC("6565d", DEBUG, arangodb::Logger::STARTUP)
      << "Begin Shutdown for phase " << name() << " received";
}

void ApplicationFeaturePhase::stop() {
  LOG_TOPIC("c9ebb", DEBUG, arangodb::Logger::STARTUP) << "Stop for phase " << name() << " started";
}

void ApplicationFeaturePhase::unprepare() {
  LOG_TOPIC("df6c3", DEBUG, arangodb::Logger::STARTUP) << "Unprepare for phase " << name() << " started";
}

}  // namespace application_features
}  // namespace arangodb
