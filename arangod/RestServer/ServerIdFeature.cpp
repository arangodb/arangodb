////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "ServerIdFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb::options;

namespace arangodb {

TRI_server_id_t ServerIdFeature::SERVERID = 0;

ServerIdFeature::ServerIdFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "ServerId") {
  setOptional(false);
  startsAfter("BasicsPhase");
  startsAfter("Database");
  startsAfter("SystemDatabase");
  startsAfter("InitDatabase");
}

void ServerIdFeature::start() {
  auto databasePath = application_features::ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _idFilename = databasePath->subdirectoryName("SERVER");

  auto database = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  
  // read the server id or create a new one
  bool const checkVersion = database->checkVersion();
  int res = determineId(checkVersion);

  if (res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
    if (checkVersion) {
      // when we are version checking, we will not fail here
      // additionally notify the database feature that we had no VERSION file
      database->isInitiallyEmpty(true);
      return;
    }

    // otherwise fail
    THROW_ARANGO_EXCEPTION(res);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "reading/creating server id file failed: " << TRI_errno_string(res);
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief generates a new server id
void ServerIdFeature::generateId() {
  TRI_ASSERT(SERVERID == 0);

  do {
    SERVERID = RandomGenerator::interval(static_cast<uint64_t>(0x0000FFFFFFFFFFFFULL));

  } while (SERVERID == 0);

  TRI_ASSERT(SERVERID != 0);
}


/// @brief reads server id from file
int ServerIdFeature::readId() {
  if (!TRI_ExistsFile(_idFilename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  TRI_server_id_t foundId;
  try {
    VPackBuilder builder = basics::VelocyPackHelper::velocyPackFromFile(_idFilename);
    VPackSlice content = builder.slice();
    if (!content.isObject()) {
      return TRI_ERROR_INTERNAL;
    }
    VPackSlice idSlice = content.get("serverId");
    if (!idSlice.isString()) {
      return TRI_ERROR_INTERNAL;
    }
    foundId = basics::StringUtils::uint64(idSlice.copyString());
  } catch (...) {
    // Nothing to free
    return TRI_ERROR_INTERNAL;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "using existing server id: " << foundId;

  if (foundId == 0) {
    return TRI_ERROR_INTERNAL;
  }

  SERVERID = foundId;

  return TRI_ERROR_NO_ERROR;
}

/// @brief writes server id to file
int ServerIdFeature::writeId() {
  // create a VelocyPack Object
  VPackBuilder builder;
  try {
    builder.openObject();

    TRI_ASSERT(SERVERID != 0);
    builder.add("serverId", VPackValue(std::to_string(SERVERID)));

    time_t tt = time(nullptr);
    struct tm tb;
    TRI_gmtime(tt, &tb);
    char buffer[32];
    size_t len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
    builder.add("createdTime", VPackValue(std::string(buffer, len)));

    builder.close();
  } catch (...) {
    // out of memory
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot save server id in file '" << _idFilename
             << "': out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // save json info to file
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Writing server id to file '" << _idFilename << "'";
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
      _idFilename, builder.slice(), true);

  if (!ok) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not save server id in file '" << _idFilename << "': " << TRI_last_error();

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief read / create the server id on startup
int ServerIdFeature::determineId(bool checkVersion) {
  int res = readId();

  if (res == TRI_ERROR_FILE_NOT_FOUND) {
    if (checkVersion) {
      return TRI_ERROR_ARANGO_EMPTY_DATADIR;
    }

    // id file does not yet exist. now create it
    generateId();
    
    // id was generated. now save it
    res = writeId();
  }

  return res;
}

} // arangodb
