////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include <set>

#include "Cluster/Maintenance/Actions/UpgradeCollection.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/Maintenance/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Network/Utils.h"
#include "Transaction/ClusterUtils.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

namespace {
using UpgradeState = arangodb::LogicalCollection::UpgradeStatus::State;

struct Config {
 private:
  arangodb::DatabaseGuard _guard;

 public:
  TRI_vocbase_t& vocbase;
  std::shared_ptr<arangodb::LogicalCollection> collection;

  explicit Config(arangodb::maintenance::ActionDescription const& desc)
      : _guard(desc.get(arangodb::maintenance::DATABASE)), vocbase(_guard.database()) {
    [[maybe_unused]] arangodb::Result found =
        arangodb::methods::Collections::lookup(vocbase, desc.get(arangodb::maintenance::SHARD),
                                               collection);
  }
};

std::pair<arangodb::Result, bool> updateStatusFromCurrent(arangodb::ClusterInfo& ci,
                                                          arangodb::LogicalCollection& collection,
                                                          std::string const& shard) {
  arangodb::Result res;
  bool localChanges = false;
  std::shared_ptr<std::vector<std::string>> currentServers =
      ci.getResponsibleServer(shard);
  if (!currentServers) {
    res.reset(TRI_ERROR_INTERNAL,
              "could not get list of servers responsible for shard");
    return std::make_pair(res, localChanges);
  }
  std::set<std::string> servers{currentServers->begin(), currentServers->end()};
  {
    WRITE_LOCKER(lock, collection.upgradeStatusLock());
    arangodb::LogicalCollection::UpgradeStatus& status = collection.upgradeStatus();
    status = arangodb::LogicalCollection::UpgradeStatus::fetch(collection);
    auto const& map = status.map();
    for (auto& it : map) {
      if (servers.find(it.first) == servers.end()) {
        // out-dated entry
        status.remove(it.first);
        localChanges = true;
      }
    }
    for (auto& it : servers) {
      if (map.find(it) == map.end()) {
        // missing entry
        status.set(it, arangodb::LogicalCollection::UpgradeStatus::State::ToDo);
        localChanges = true;
      }
    }
  }

  return std::make_pair(res, localChanges);
}

void writeStatusToCurrent(arangodb::LogicalCollection& collection,
                          arangodb::maintenance::ActionDescription const& desc) {
  arangodb::velocypack::Builder statusBuilder;
  {
    READ_LOCKER(lock, collection.upgradeStatusLock());
    auto const& status = collection.upgradeStatus();
    arangodb::velocypack::ObjectBuilder object(&statusBuilder);
    status.toVelocyPack(statusBuilder, false);
  }

  std::string agencyKey =
      "/Current/Collections/" + desc.get(arangodb::maintenance::DATABASE) +
      "/" + desc.get(arangodb::maintenance::COLLECTION) + "/" +
      desc.get(arangodb::maintenance::SHARD) + "/" + arangodb::maintenance::UPGRADE_STATUS;

  auto trx = arangodb::AgencyWriteTransaction{
      arangodb::AgencyOperation{agencyKey, arangodb::AgencyValueOperationType::SET,
                                statusBuilder.slice()}};

  arangodb::AgencyComm comm(collection.vocbase().server());
  arangodb::AgencyCommResult result = comm.sendTransactionWithFailover(trx);
  if (!result.successful()) {
    throw std::runtime_error(
        "failed to send and execute transaction to set shard upgrade status");
  }
}

std::unordered_map<::UpgradeState, std::vector<std::string>> serversByStatus(
    arangodb::LogicalCollection& collection) {
  std::unordered_map<::UpgradeState, std::vector<std::string>> map;
  WRITE_LOCKER(lock, collection.upgradeStatusLock());

  arangodb::LogicalCollection::UpgradeStatus& status = collection.upgradeStatus();
  for (auto& it : status.map()) {
    map[it.second].emplace_back(it.first);
  }

  return map;
}
}  // namespace

namespace arangodb::maintenance {

UpgradeCollection::UpgradeCollection(MaintenanceFeature& feature, ActionDescription const& desc)
    : ActionBase(feature, desc) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!desc.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!desc.has(SHARD)) {
    error << "shard must be specified. ";
  }
  TRI_ASSERT(desc.has(SHARD));

  if (desc.has(UPGRADE_STATUS)) {
    _planStatus = velocypack::Parser::fromJson(_description.get(UPGRADE_STATUS));
  }

  if (!error.str().empty()) {
    LOG_TOPIC("a6e4c", ERR, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

UpgradeCollection::~UpgradeCollection() = default;

bool UpgradeCollection::first() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = _description.get(SHARD);

  auto guard = scopeGuard([this, &database, &collection, &shard]() -> void {
    if (_result.fail()) {
      _feature.storeShardError(database, collection, shard,
                               _description.get(SERVER_ID), _result);
      _trx.reset();
    }
  });

  try {
    ::Config config(_description);
    if (config.collection) {
      LOG_TOPIC("61543", DEBUG, Logger::MAINTENANCE)
          << "Updating local collection " + shard;

      if (!_planStatus) {
        // no status found in the plan, clear the local status to report later
        {
          WRITE_LOCKER(lock, config.collection->statusLock());
          LogicalCollection::UpgradeStatus& status = config.collection->upgradeStatus();
          status.clear();
        }
        return false;
      }

      // start an exclusive transaction to block access to the collection
      std::shared_ptr<transaction::Context> context =
          transaction::StandaloneContext::Create(config.vocbase);
      _trx = std::make_unique<SingleCollectionTransaction>(context, *config.collection,
                                                           AccessMode::Type::EXCLUSIVE);
      _result = _trx->begin();
      if (_result.fail()) {
        return false;
      }

      return next();
    } else {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << "in database " + database;
      LOG_TOPIC("620fc", ERR, Logger::MAINTENANCE) << error.str();
      _result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79443", WARN, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
  }

  return true;
}

bool UpgradeCollection::next() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = _description.get(SHARD);

  auto guard = scopeGuard([this, &database, &collection, &shard]() -> void {
    if (_result.fail()) {
      _feature.storeShardError(database, collection, shard,
                               _description.get(SERVER_ID), _result);
      _trx.reset();
    }
  });

  TRI_ASSERT(_planStatus);
  ::UpgradeState targetState =
      LogicalCollection::UpgradeStatus::stateFromSlice(_planStatus->slice());

  try {
    ::Config config(_description);
    if (config.collection) {
      LOG_TOPIC("61543", DEBUG, Logger::MAINTENANCE)
          << "Updating local collection " + shard;

      // will check Current, and fill in any missing servers with ToDo
      bool mustWrite = false;
      auto& feature = config.vocbase.server().getFeature<ClusterFeature>();
      auto& ci = feature.clusterInfo();
      std::tie(_result, mustWrite) =
          ::updateStatusFromCurrent(ci, *config.collection, shard);
      if (_result.fail()) {
        return true;
      }
      if (mustWrite) {
        // make sure it's written back out to Current with up-to-date server list
        ::writeStatusToCurrent(*config.collection, _description);
      }

      switch (targetState) {
        case ::UpgradeState::Prepare: {
          std::unordered_map<UpgradeState, std::vector<std::string>> servers =
              ::serversByStatus(*config.collection);
          decltype(servers)::const_iterator it = servers.find(::UpgradeState::ToDo);
          if (it != servers.end() && !it->second.empty()) {
            for (auto const& server : it->second) {
              decltype(_futures)::iterator f = _futures.find(server);
              if (f == _futures.end()) {
                LOG_DEVEL << "sending 'Prepare' to '" << config.collection->name() << "'";
                _futures.emplace(server, sendRequest(*config.collection, server,
                                                     ::UpgradeState::Prepare));
              } else {
                if (f->second.hasValue()) {
                  Result r = f->second.get();
                  if (r.ok()) {
                    _futures.erase(f);
                  } else {
                    LOG_DEVEL << "have response for '" << server << "', code "
                              << r.errorNumber() << ", message: '"
                              << r.errorMessage() << "'";
                    _result.reset(r);
                  }
                } else if (f->second.hasException()) {
                  if (f->second.getTry().exception()) {
                    std::rethrow_exception(f->second.getTry().exception());
                  }
                } else {
                  LOG_DEVEL << "already have outstanding request for '" << server << "'";
                }
              }
            }
          }
          break;
        }
        case ::UpgradeState::Finalize: {
          break;
        }
        case ::UpgradeState::Rollback: {
          break;
        }
        case ::UpgradeState::Cleanup: {
          break;
        }
        case ::UpgradeState::ToDo:
        default:
          break;
      }
    } else {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << "in database " + database;
      LOG_TOPIC("620fc", ERR, Logger::MAINTENANCE) << error.str();
      _result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79443", WARN, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
  }

  return hasMore();
}

bool UpgradeCollection::hasMore() const {
  if (_futures.empty()) {
    return false;
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));
  return ++_iteration < 100;
}

futures::Future<Result> UpgradeCollection::sendRequest(LogicalCollection& collection,
                                                       std::string const& server,
                                                       ::UpgradeState phase) {
  velocypack::Buffer<uint8_t> reqBuffer;
  velocypack::Builder reqBuilder(reqBuffer);
  {
    velocypack::ObjectBuilder object(&reqBuilder);
    reqBuilder.add(maintenance::UPGRADE_STATUS,
                   LogicalCollection::UpgradeStatus::stateToValue(phase));
  }

  auto* pool = collection.vocbase().server().getFeature<NetworkFeature>().pool();
  std::string url = "/_api/collection/" + collection.name() + "/upgrade";
  network::RequestOptions reqOpts;
  reqOpts.database = collection.vocbase().name();

  return arangodb::network::sendRequestRetry(pool, "server:" + server,
                                             fuerte::RestVerb::Put, url,
                                             std::move(reqBuffer), reqOpts, {})
      .thenValue(handleResponse(server, phase));
}

std::function<Result(network::Response&&)> UpgradeCollection::handleResponse(
    std::string const& server, LogicalCollection::UpgradeStatus::State phase) {
  return [this, server, phase](network::Response&& res) -> Result {
    LOG_DEVEL << "handling response for '" << server << "'";
    Result result;
    int commError = network::fuerteToArangoErrorCode(res);
    if (commError != TRI_ERROR_NO_ERROR) {
      result.reset(commError);
    } else if (res.statusCode() != fuerte::StatusOK) {
      result.reset(TRI_ERROR_INTERNAL,
                   "did not receive expected 200 OK response from server '" +
                       server + "', got " + std::to_string(res.statusCode()) +
                       " instead, '" + res.slice().toJson() + "'");
    }
    if (result.fail()) {
      return result;
    }

    // okay, we executed this operation successfully, let everyone know
    try {
      ::Config config(_description);
      if (config.collection) {
        {
          WRITE_LOCKER(lock, config.collection->upgradeStatusLock());
          arangodb::LogicalCollection::UpgradeStatus& status =
              config.collection->upgradeStatus();
          status.set(server, phase);
        }
        ::writeStatusToCurrent(*config.collection, _description);
      } else {
        std::stringstream error;
        error << "failed to lookup local collection " << _description.get(SHARD)
              << "in database " + _description.get(DATABASE);
        LOG_TOPIC("620fc", ERR, Logger::MAINTENANCE) << error.str();
        result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      }
    } catch (std::exception const& e) {
      std::stringstream error;

      error << "action " << _description << " failed with exception " << e.what();
      LOG_TOPIC("79443", WARN, Logger::MAINTENANCE)
          << "UpgradeCollection: " << error.str();
      result.reset(TRI_ERROR_INTERNAL, error.str());
    }

    return result;
  };
}

}  // namespace arangodb::maintenance