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

#include "ServerIdFeature.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agency/AgencyPaths.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/TransactionBuilder.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb::options;
using namespace std::chrono_literals;

namespace arangodb {

ServerId ServerIdFeature::SERVERID{0};

ServerIdFeature::ServerIdFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();

  startsAfter<DatabaseFeature>();
  startsAfter<InitDatabaseFeature>();
  startsAfter<SystemDatabaseFeature>();
}

ServerIdFeature::~ServerIdFeature() { SERVERID = ServerId::none(); }

void ServerIdFeature::start() {
  auto& databasePath = server().getFeature<DatabasePathFeature>();
  _idFilename = databasePath.subdirectoryName("SERVER");

  auto& database = server().getFeature<DatabaseFeature>();

  // read the server id or create a new one
  bool const checkVersion = database.checkVersion();
  auto res = determineId(checkVersion);

  if (res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
    if (checkVersion) {
      // when we are version checking, we will not fail here
      // additionally notify the database feature that we had no VERSION file
      database.isInitiallyEmpty(true);
      return;
    }

    // otherwise fail
    THROW_ARANGO_EXCEPTION(res);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("75509", ERR, arangodb::Logger::FIXME)
        << "reading/creating server id file failed: " << TRI_errno_string(res);
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief generates a new server id
void ServerIdFeature::generateId() {
  TRI_ASSERT(SERVERID.empty());

  do {
    SERVERID = ServerId(RandomGenerator::interval(
        static_cast<uint64_t>(0x0000FFFFFFFFFFFFULL)));

  } while (SERVERID.empty());

  TRI_ASSERT(SERVERID.isSet());
}

/// @brief reads server id from file
ErrorCode ServerIdFeature::readId() {
  if (!TRI_ExistsFile(_idFilename.c_str())) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  ServerId foundId;
  try {
    VPackBuilder builder =
        basics::VelocyPackHelper::velocyPackFromFile(_idFilename);
    VPackSlice content = builder.slice();
    if (!content.isObject()) {
      return TRI_ERROR_INTERNAL;
    }
    VPackSlice idSlice = content.get("serverId");
    if (!idSlice.isString()) {
      return TRI_ERROR_INTERNAL;
    }
    foundId = ServerId(basics::StringUtils::uint64(idSlice.copyString()));
  } catch (...) {
    // Nothing to free
    return TRI_ERROR_INTERNAL;
  }

  LOG_TOPIC("281bf", TRACE, arangodb::Logger::FIXME)
      << "using existing server id: " << foundId;

  if (foundId.empty()) {
    return TRI_ERROR_INTERNAL;
  }

  SERVERID = foundId;

  return TRI_ERROR_NO_ERROR;
}

/// @brief writes server id to file
ErrorCode ServerIdFeature::writeId() {
  // create a VelocyPack Object
  VPackBuilder builder;
  try {
    builder.openObject();

    TRI_ASSERT(SERVERID.isSet());
    builder.add("serverId", VPackValue(std::to_string(SERVERID.id())));

    time_t tt = time(nullptr);
    struct tm tb;
    TRI_gmtime(tt, &tb);
    char buffer[32];
    size_t len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
    builder.add("createdTime", VPackValue(std::string(buffer, len)));

    builder.close();
  } catch (...) {
    // out of memory
    LOG_TOPIC("6cac3", ERR, arangodb::Logger::FIXME)
        << "cannot save server id in file '" << _idFilename
        << "': out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // save json info to file
  LOG_TOPIC("f6cbd", DEBUG, arangodb::Logger::FIXME)
      << "Writing server id to file '" << _idFilename << "'";
  bool ok = arangodb::basics::VelocyPackHelper::velocyPackToFile(
      _idFilename, builder.slice(), true);

  if (!ok) {
    LOG_TOPIC("26de4", ERR, arangodb::Logger::FIXME)
        << "could not save server id in file '" << _idFilename
        << "': " << TRI_last_error();

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief read / create the server id on startup
ErrorCode ServerIdFeature::determineId(bool checkVersion) {
  auto res = readId();

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

/// @brief get deployment ID (single server or cluster ID)
async<ResultT<std::string>> ServerIdFeature::getDeploymentId() {
  // Check if ServerState is available (not during early startup or shutdown)
  if (ServerState::instance() == nullptr) {
    co_return ResultT<std::string>::error(
        TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
        "server state not yet available or already shut down");
  }

  std::string deploymentId;

  // For single servers, use the server ID
  if (ServerState::instance()->isSingleServer()) {
    // Create a UUID with the lower 48 bits from server ID
    // and constant upper 80 bits
    boost::uuids::uuid uuid;

    // Get the server ID (only lower 48 bits are usable)
    uint64_t serverId = ServerIdFeature::getId().id();
    uint64_t serverIdLower48 = serverId & 0xFFFFFFFFFFFFULL;

    // Set the first 10 bytes (80 bits) to a constant pattern
    // Using a recognizable pattern for ArangoDB single server deployment IDs
    uuid.data[0] = 0x61;  // 'a'
    uuid.data[1] = 0x72;  // 'r'
    uuid.data[2] = 0x61;  // 'a'
    uuid.data[3] = 0x6e;  // 'n'
    uuid.data[4] = 0x67;  // 'g'
    uuid.data[5] = 0x6f;  // 'o'
    uuid.data[6] = 0x40;  // Version 4: random bits
    uuid.data[7] = 0x00;
    uuid.data[8] = 0x00;
    uuid.data[9] = 0x00;

    // Set the last 6 bytes (48 bits) to the server ID
    uuid.data[10] = static_cast<uint8_t>((serverIdLower48 >> 40) & 0xFF);
    uuid.data[11] = static_cast<uint8_t>((serverIdLower48 >> 32) & 0xFF);
    uuid.data[12] = static_cast<uint8_t>((serverIdLower48 >> 24) & 0xFF);
    uuid.data[13] = static_cast<uint8_t>((serverIdLower48 >> 16) & 0xFF);
    uuid.data[14] = static_cast<uint8_t>((serverIdLower48 >> 8) & 0xFF);
    uuid.data[15] = static_cast<uint8_t>(serverIdLower48 & 0xFF);

    deploymentId = boost::uuids::to_string(uuid);
    co_return ResultT<std::string>::success(std::move(deploymentId));
  } else if (ServerState::instance()->isCoordinator()) {
    if (AsyncAgencyCommManager::INSTANCE == nullptr) {
      co_return ResultT<std::string>::error(
          TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
          "agency communication not available");
    }

    try {
      // Build a read transaction to query /arango/cluster
      auto rootPath = arangodb::cluster::paths::root()->arango();
      VPackBuffer<uint8_t> trx;
      {
        VPackBuilder builder(trx);
        arangodb::agency::envelope::into_builder(builder)
            .read()
            .key(rootPath->cluster()->str())
            .end()
            .done();
      }

      // Send the read transaction to the agency
      auto result =
          co_await AsyncAgencyComm().sendReadTransaction(60.0s, std::move(trx));

      if (result.ok() && result.statusCode() == fuerte::StatusOK) {
        // Extract the cluster ID from the response
        // The result is an array with one element containing the value
        VPackSlice slice = result.slice();
        if (slice.isArray() && slice.length() > 0) {
          VPackSlice clusterSlice = slice.at(0);
          if (clusterSlice.isObject()) {
            VPackSlice arangoSlice =
                clusterSlice.get(std::vector<std::string>{"arango", "Cluster"});
            if (arangoSlice.isString()) {
              deploymentId = arangoSlice.copyString();
              co_return ResultT<std::string>::success(std::move(deploymentId));
            }
          }
        }
      }

      co_return ResultT<std::string>::error(
          TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
          "failed to query cluster ID from agency");
    } catch (std::exception const& e) {
      co_return ResultT<std::string>::error(TRI_ERROR_HTTP_SERVER_ERROR,
                                            e.what());
    }
  }

  co_return ResultT<std::string>::error(TRI_ERROR_INTERNAL,
                                        "unexpected server state");
}

}  // namespace arangodb
