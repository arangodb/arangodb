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

#include "ApplicationFeatures/TempFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "CrashHandler/CrashHandler.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::options;
extern char** environ;

namespace arangodb {

void TempFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("temp", "temporary files");

  options
      ->addOption("--temp.dumpenv", "Dump the full environment to the logs.",
                  new boolParameter(&_dumpEnv))
      .setLongDescription(R"will dump the full environment to the logfiles");
}

void TempFeature::prepare() {
  if (_dumpEnv) {
    int size = 0;
    while (environ[size]) {
      LOG_TOPIC("a7777", INFO, arangodb::Logger::FIXME) environ[i];
      size++;
    }
  }
}

}  // namespace arangodb
