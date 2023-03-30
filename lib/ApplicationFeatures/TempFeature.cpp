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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
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

namespace arangodb {

void TempFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("temp-path", "temp.path");

  options->addSection("temp", "temporary files");

  options
      ->addOption("--temp.path", "The path for temporary files.",
                  new StringParameter(&_path))
      .setLongDescription(R"(ArangoDB uses the path for storing temporary
files, for extracting data from uploaded zip files (e.g. for Foxx services),
and other things.

Ideally, the temporary path is set to an instance-specific subdirectory of the
operating system's temporary directory. To avoid data loss, the temporary path
should not overlap with any directories that contain important data, for
example, the instance's database directory.

If you set the temporary path to the same directory as the instance's database
directory, a startup error is logged and the startup is aborted.)");
}

void TempFeature::validateOptions(std::shared_ptr<ProgramOptions> /*options*/) {
  if (!_path.empty()) {
    // replace $PID in basepath with current process id
    _path = basics::StringUtils::replace(
        _path, "$PID", std::to_string(Thread::currentProcessId()));

    basics::FileUtils::makePathAbsolute(_path);
  }
}

void TempFeature::prepare() {
  TRI_SetApplicationName(_appname);
  if (!_path.empty()) {
    TRI_SetTempPath(_path);
  }
}

void TempFeature::start() {
#ifdef _WIN32
  CrashHandler::setMiniDumpDirectory(TRI_GetTempPath());
#endif
}

}  // namespace arangodb
