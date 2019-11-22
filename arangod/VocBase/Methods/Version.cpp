////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Version.h"
#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;

uint64_t Version::parseVersion(char const* str, size_t len) {
  uint64_t result = 0;
  uint64_t tmp = 0;
  for (size_t i = 0; i < len; i++) {
    char c = str[i];
    if ('0' <= c && c <= '9') {
      tmp = tmp * 10 + static_cast<size_t>(c - '0');
    } else if (c == '.') {
      result = result * 100 + tmp;
      tmp = 0;
    } else {
      // stop at first other character (e.g. "3.4.devel")
      while (result > 0 && result < 100) {
        // do we have 5 digits already? if we, then boost the version
        // number accordingly. this can happen for version strings
        // such as "3.4.devel" or "4.devel"
        result *= 100;
      }
      break;
    }
  }

  return result * 100 + tmp;
}

uint64_t Version::parseVersion(char const* str) {
  return parseVersion(str, strlen(str));
}


/// @brief "(((major * 100) + minor) * 100) + patch"
uint64_t Version::current() {
  return parseVersion(ARANGODB_VERSION);
}

VersionResult::StatusCode Version::compare(uint64_t current, uint64_t other) {
  if (current / 100 == other / 100) {
    return VersionResult::VERSION_MATCH;
  } else if (current > other) {  // downgrade??
    return VersionResult::DOWNGRADE_NEEDED;
  } else if (current < other) {  // upgrade
    return VersionResult::UPGRADE_NEEDED;
  }

  return VersionResult::INVALID;
}

VersionResult Version::check(TRI_vocbase_t* vocbase) {
  uint64_t lastVersion = UINT64_MAX;
  uint64_t serverVersion = Version::current();
  std::map<std::string, bool> tasks;

  if (ServerState::instance()->isCoordinator()) {
    // in a coordinator, we don't have any persistent data, so there is no VERSION
    // file available. In this case we don't know the previous version we are
    // upgrading from, so we can't do anything sensible here.
    return VersionResult{VersionResult::VERSION_MATCH, serverVersion, serverVersion, tasks};
  }
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);

  std::string versionFile = engine->versionFilename(vocbase->id());
  if (!basics::FileUtils::exists(versionFile)) {
    LOG_TOPIC("fde3f", DEBUG, Logger::STARTUP) << "VERSION file '" << versionFile << "' not found";
    return VersionResult{VersionResult::NO_VERSION_FILE, 0, 0, {}};
  }
  std::string versionInfo = basics::FileUtils::slurp(versionFile);
  LOG_TOPIC("3da0f", DEBUG, Logger::STARTUP)
      << "found VERSION file '" << versionFile << "', content: " << versionInfo;
  if (versionInfo.empty()) {
    LOG_TOPIC("dc4de", ERR, Logger::STARTUP) << "VERSION file '" << versionFile << "' is empty";
    return VersionResult{VersionResult::CANNOT_READ_VERSION_FILE, 0, 0, {}};
  }

  try {
    std::shared_ptr<VPackBuilder> parsed = velocypack::Parser::fromJson(versionInfo);
    VPackSlice versionVals = parsed->slice();
    if (!versionVals.isObject() || !versionVals.get("version").isNumber()) {
      LOG_TOPIC("0c863", ERR, Logger::STARTUP) << "cannot parse VERSION file '" << versionFile
                                      << "' content: " << versionInfo;
      return VersionResult{VersionResult::CANNOT_PARSE_VERSION_FILE, 0, 0, tasks};
    }
    lastVersion = versionVals.get("version").getUInt();
    VPackSlice run = versionVals.get("tasks");
    if (run.isNone() || !run.isObject()) {
      LOG_TOPIC("2897d", ERR, Logger::STARTUP) << "invalid VERSION file '" << versionFile
                                      << "' content: " << versionInfo;
      return VersionResult{VersionResult::CANNOT_PARSE_VERSION_FILE, 0, 0, tasks};
    }
    for (VPackObjectIterator::ObjectPair pair : VPackObjectIterator(run)) {
      tasks.try_emplace(pair.key.copyString(), pair.value.getBool());
    }
  } catch (velocypack::Exception const& e) {
    LOG_TOPIC("2d92a", ERR, Logger::STARTUP)
        << "cannot parse VERSION file '" << versionFile << "': " << e.what()
        << ". file content: " << versionInfo;

    return VersionResult{VersionResult::CANNOT_PARSE_VERSION_FILE, 0, 0, tasks};
  }
  TRI_ASSERT(lastVersion != UINT32_MAX);

  VersionResult res = {VersionResult::NO_VERSION_FILE, serverVersion, lastVersion, tasks};

  switch (compare(lastVersion, serverVersion)) {
    case VersionResult::VERSION_MATCH:
      LOG_TOPIC("e9cc3", DEBUG, Logger::STARTUP) << "version match: last version " << lastVersion
                                        << ", current version " << serverVersion;
      res.status = VersionResult::VERSION_MATCH;
      break;
    case VersionResult::DOWNGRADE_NEEDED:
      LOG_TOPIC("73276", DEBUG, Logger::STARTUP) << "downgrade: last version " << lastVersion
                                        << ", current version " << serverVersion;
      res.status = VersionResult::DOWNGRADE_NEEDED;
      break;
    case VersionResult::UPGRADE_NEEDED:
      LOG_TOPIC("0f77f", DEBUG, Logger::STARTUP) << "upgrade: last version " << lastVersion
                                        << ", current version " << serverVersion;
      res.status = VersionResult::UPGRADE_NEEDED;
      break;
    default:
      LOG_TOPIC("b0d3c", ERR, Logger::STARTUP) << "should not happen: last version " << lastVersion;

  }

  return res;
}

Result Version::write(TRI_vocbase_t* vocbase, std::map<std::string, bool> const& tasks, bool sync) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);

  std::string versionFile = engine->versionFilename(vocbase->id());
  if (versionFile.empty()) {
    // cluster engine
    return Result();
  }

  VPackOptions opts;
  opts.buildUnindexedObjects = true;
  VPackBuilder builder(&opts);
  builder.openObject(true);
  builder.add("version", VPackValue(Version::current()));
  builder.add("tasks", VPackValue(VPackValueType::Object));
  for (auto const& task : tasks) {
    builder.add(task.first, VPackValue(task.second));
  }
  builder.close();
  builder.close();

  if (!basics::VelocyPackHelper::velocyPackToFile(versionFile, builder.slice(), sync)) {
    LOG_TOPIC("33860", ERR, Logger::STARTUP) << "writing VERSION file '" << versionFile
                                    << "' failed: " << TRI_last_error();
    return Result(TRI_errno(), TRI_last_error());
  }
  return Result();
}
