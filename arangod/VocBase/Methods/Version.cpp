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
/// @author Simon Gräter
////////////////////////////////////////////////////////////////////////////////

#include "Version.h"
#include "Basics/Common.h"
#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
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

static uint32_t parseVersion(char const* str, size_t len) {
  uint32_t result = 0;
  uint32_t tmp = 0;
  for (size_t i = 0; i < len; i++) {
    char c = str[i];
    if ('0' <= c && c <= '9') {
      tmp = tmp * 10 + static_cast<size_t>(c - '0');
    } else if (c == '.') {
      result = result * 100 + tmp;
    }  // ignore other characters
  }
  return result + tmp;
}

/// @brief "(((major * 100) + minor) * 100) + patch"
uint32_t Version::current() {
  return parseVersion(ARANGODB_VERSION, strlen(ARANGODB_VERSION));
}

VersionResult Version::check(TRI_vocbase_t* vocbase) {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);

  std::string versionFile = engine->versionFilename(vocbase->id());
  if (!basics::FileUtils::exists(versionFile)) {
    LOG_TOPIC(WARN, Logger::STARTUP) << "VERSION file not found: "
                                     << versionFile;
    return VersionResult{VersionResult::NO_VERSION_FILE, 0, 0, {}};
  }
  std::string versionInfo = basics::FileUtils::slurp(versionFile);
  LOG_TOPIC(DEBUG, Logger::STARTUP) << "Found VERSION file: " << versionInfo;
  if (versionInfo.empty()) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "Empty VERSION file";
    return VersionResult{VersionResult::CANNOT_READ_VERSION_FILE, 0, 0, {}};
  }

  uint32_t lastVersion = UINT32_MAX;
  std::map<std::string, bool> tasks;

  try {
    std::shared_ptr<VPackBuilder> parsed =
        velocypack::Parser::fromJson(versionInfo);
    VPackSlice versionVals = parsed->slice();
    if (!versionVals.isObject() || !versionVals.get("version").isNumber()) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "Invalud VERSION file " << versionInfo;
      return VersionResult{VersionResult::CANNOT_PARSE_VERSION_FILE, 0, 0,
                           tasks};
    }
    lastVersion = versionVals.get("version").getNumber<uint32_t>();
    VPackSlice run = versionVals.get("tasks");
    if (!run.isNone() || !run.isObject()) {
      return VersionResult{VersionResult::CANNOT_PARSE_VERSION_FILE, 0, 0,
                           tasks};
    }
    for (VPackObjectIterator::ObjectPair pair : VPackObjectIterator(run)) {
      tasks.emplace(pair.key.copyString(), pair.value.getBool());
    }
  } catch (velocypack::Exception const& e) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "Cannot parse VERSION file "
                                    << versionInfo;
    return VersionResult{VersionResult::CANNOT_PARSE_VERSION_FILE, 0, 0, tasks};
  }
  TRI_ASSERT(lastVersion != UINT32_MAX);
  uint32_t serverVersion = Version::current();

  VersionResult res = {VersionResult::NO_VERSION_FILE, serverVersion,
                       lastVersion, tasks};
  if (lastVersion / 100 == serverVersion / 100) {
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "version match: last version "
                                      << lastVersion << ", current version "
                                      << serverVersion;
    res.status = VersionResult::VERSION_MATCH;
  } else if (lastVersion > serverVersion) {  // downgrade??
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "downgrade: last version "
                                      << lastVersion << ", current version "
                                      << serverVersion;
    res.status = VersionResult::DOWNGRADE_NEEDED;
  } else if (lastVersion < serverVersion) {  // upgrade
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "upgrade: last version " << lastVersion
                                      << ", current version " << serverVersion;
    res.status = VersionResult::UPGRADE_NEEDED;
  } else {
    LOG_TOPIC(ERR, Logger::STARTUP) << "should not happen: last version "
                                    << lastVersion << ", current version "
                                    << serverVersion;
  }

  return res;
}
