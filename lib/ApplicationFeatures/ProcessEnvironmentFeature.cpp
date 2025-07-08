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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ProcessEnvironmentFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::options;
extern char** environ;

namespace arangodb {

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void ProcessEnvironmentFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("temp", "temporary files");

  options->addOption("--dump-env", "Dump the full environment to the logs.",
                     new BooleanParameter(&_dumpEnv));
}

void ProcessEnvironmentFeature::prepare() {
  if (_dumpEnv) {
    if (environ == nullptr) {
      return;
    }

    for (char** env = environ; *env != nullptr; ++env) {
      LOG_TOPIC("a7777", INFO, arangodb::Logger::FIXME) << *env;
    }
  }
}
#endif
}  // namespace arangodb
