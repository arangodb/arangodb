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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/CrashHandlerFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "CrashHandler/CrashHandler.h"
#include "CrashHandler/Dumper.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb;
using namespace arangodb::options;

CrashHandlerFeature::CrashHandlerFeature(
    Server& server, crash_handler::CrashHandler* crashHandler)
    : ArangodFeature{server, *this},
      _crashHandler(crashHandler),
      _enabled(true) {
  setOptional(false);
  // This feature should start very early
  startsAfter<DatabasePathFeature>();
}

void CrashHandlerFeature::start() {
  if (_enabled) {
    setDatabaseDirectory(
        server().getFeature<DatabasePathFeature>().directory());
  }
}

void CrashHandlerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--crash-handler.enable-dumps",
      "Enable crash dump logging to write crash information to disk.",
      new BooleanParameter(&_enabled),
      arangodb::options::makeDefaultFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnSingle));
}

void CrashHandlerFeature::setDatabaseDirectory(std::string path) {
  if (!_enabled) {
    return;
  }
  _crashesDirectory =
      arangodb::basics::FileUtils::buildFilename(path, "crashes");
  if (_crashHandler != nullptr) {
    _crashHandler->getDumper()->setCrashesDirectory(_crashesDirectory);
  }
  // Clean up old crash directories on startup
  crash_handler::Dumper::cleanupOldCrashDirectories(_crashesDirectory,
                                                    /*max*/ 10);
}

std::vector<std::string> CrashHandlerFeature::listCrashes() {
  std::vector<std::string> crashes;
  if (!_enabled || _crashesDirectory.empty() ||
      !arangodb::basics::FileUtils::isDirectory(_crashesDirectory)) {
    return {};
  }
  auto entries = arangodb::basics::FileUtils::listFiles(_crashesDirectory);
  for (auto const& entry : entries) {
    auto fullPath =
        arangodb::basics::FileUtils::buildFilename(_crashesDirectory, entry);
    if (arangodb::basics::FileUtils::isDirectory(fullPath)) {
      crashes.push_back(entry);
    }
  }
  return crashes;
}

std::unordered_map<std::string, std::string>
CrashHandlerFeature::getCrashContents(std::string_view crashId) {
  std::unordered_map<std::string, std::string> contents;
  if (!_enabled || _crashesDirectory.empty()) {
    return contents;
  }
  auto crashDir = arangodb::basics::FileUtils::buildFilename(
      _crashesDirectory, std::string(crashId));
  if (!arangodb::basics::FileUtils::isDirectory(crashDir)) {
    return contents;
  }
  auto const files = arangodb::basics::FileUtils::listFiles(crashDir);
  for (auto const& file : files) {
    auto const filePath =
        arangodb::basics::FileUtils::buildFilename(crashDir, file);
    if (arangodb::basics::FileUtils::isRegularFile(filePath)) {
      try {
        contents[file] = arangodb::basics::FileUtils::slurp(filePath);
      } catch (...) {
        // Skip files that can't be read
      }
    }
  }
  return contents;
}

bool CrashHandlerFeature::deleteCrash(std::string_view crashId) {
  if (!_enabled || _crashesDirectory.empty()) {
    return false;
  }
  auto crashDir = arangodb::basics::FileUtils::buildFilename(
      _crashesDirectory, std::string(crashId));
  if (!arangodb::basics::FileUtils::isDirectory(crashDir)) {
    return false;
  }
  auto res = TRI_RemoveDirectory(crashDir.c_str());
  return res == TRI_ERROR_NO_ERROR;
}
