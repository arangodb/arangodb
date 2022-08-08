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
/// @author Andreas Dominik Jung
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdexcept>

#include "TimeZoneFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/application-exit.h"
#include "Basics/directories.h"
#include "Basics/error.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Basics/memory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "date/tz.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

TimeZoneFeature::TimeZoneFeature(Server& server)
    : ArangodFeature{server, *this}, _binaryPath(server.getBinaryPath()) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void TimeZoneFeature::prepareTimeZoneData(
    std::string const& binaryPath, std::string const& binaryExecutionPath,
    std::string const& binaryName) {
  std::string tz_path;
  if (!TRI_GETENV("TZ_DATA", tz_path)) {
    tz_path.clear();
    std::string test_exe =
        FileUtils::buildFilename(binaryExecutionPath, "tzdata");

    if (FileUtils::isDirectory(test_exe)) {
      FileUtils::makePathAbsolute(test_exe);
      FileUtils::normalizePath(test_exe);
      tz_path = test_exe;
    } else {
      std::string argv0 =
          FileUtils::buildFilename(binaryExecutionPath, binaryName);
      std::string path =
          TRI_LocateInstallDirectory(argv0.c_str(), binaryPath.c_str());
      path =
          FileUtils::buildFilename(path, ICU_DESTINATION_DIRECTORY, "tzdata");
      FileUtils::makePathAbsolute(path);
      FileUtils::normalizePath(path);
      tz_path = path;
    }
  }

  if (FileUtils::isDirectory(tz_path)) {
    date::set_install(tz_path);
  } else {
    LOG_TOPIC("67bdc", FATAL, arangodb::Logger::STARTUP)
        << "failed to locate timezone data " << tz_path
        << ". please set the TZ_DATA environment variable to the "
        << "tzdata directory in case you are running an unusual setup";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_TZDATA_INITIALIZATION_FAILED);
  }

  bool gotTZ = true;
  try {
    auto const* zone = date::current_zone();
    gotTZ = zone != nullptr;
  } catch (std::runtime_error const&) {
    gotTZ = false;
  }

  if (!gotTZ) {
    LOG_TOPIC("67bde", ERR, arangodb::Logger::STARTUP)
        << "Could not get current timezone from " << tz_path
        << ". Functionality using timezones may missbehave!";
  }
}

void TimeZoneFeature::prepare() {
  std::string p;
  auto context = ArangoGlobalContext::CONTEXT;
  std::string binaryExecutionPath = context->getBinaryPath();
  std::string binaryName = context->binaryName();

  TimeZoneFeature::prepareTimeZoneData(_binaryPath, binaryExecutionPath,
                                       binaryName);
}

void TimeZoneFeature::start() {
  try {
    date::reload_tzdb();
  } catch (std::runtime_error const& ex) {
    LOG_TOPIC("67bdd", FATAL, arangodb::Logger::STARTUP) << ex.what();
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_TZDATA_INITIALIZATION_FAILED);
  }
}

}  // namespace arangodb
