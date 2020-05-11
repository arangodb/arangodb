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

std::size_t getTimeout(arangodb::application_features::ApplicationServer& server, std::string const& database, std::string const& collection) {
  using namespace arangodb::velocypack;

  std::size_t timeout = 600;

  arangodb::AgencyReadTransaction trx(arangodb::AgencyCommManager::path("Target/Pending"));

  arangodb::AgencyComm agency(server);
  arangodb::AgencyCommResult result = agency.sendTransactionWithFailover(trx, 60.0);
  if (!result.successful()) {
    return timeout;
  } else {
    Slice list = result.slice()[0].get(std::vector<std::string>({
      arangodb::AgencyCommManager::path(), "Target", "Pending"
    }));
    for (ObjectIteratorPair it : ObjectIterator(list)) {
      Slice typeSlice = it.value.get("type");
      Slice dbSlice = it.value.get(arangodb::maintenance::DATABASE);
      Slice cSlice = it.value.get(arangodb::maintenance::COLLECTION);
      Slice timeSlice = it.value.get(arangodb::maintenance::TIMEOUT);
      if (typeSlice.isString() &&
          typeSlice.isEqualString(arangodb::maintenance::UPGRADE_COLLECTION) &&
          dbSlice.isString() && dbSlice.isEqualString(database) && cSlice.isString() &&
          cSlice.isEqualString(collection) && timeSlice.isInteger()) {
        timeout = std::max(timeout, timeSlice.getNumber<std::size_t>());
      }
    }
  }

  return timeout;
}

bool getSmartChild(arangodb::application_features::ApplicationServer& server,
                   std::string const& database, std::string const& collection) {
  using namespace arangodb::velocypack;

  bool isSmartChild = false;

  arangodb::AgencyReadTransaction trx(
      arangodb::AgencyCommManager::path("Target/Pending"));

  arangodb::AgencyComm agency(server);
  arangodb::AgencyCommResult result = agency.sendTransactionWithFailover(trx, 60.0);
  if (!result.successful()) {
    return isSmartChild;
  } else {
    Slice list = result.slice()[0].get(std::vector<std::string>(
        {arangodb::AgencyCommManager::path(), "Target", "Pending"}));
    for (ObjectIteratorPair it : ObjectIterator(list)) {
      Slice typeSlice = it.value.get("type");
      Slice dbSlice = it.value.get(arangodb::maintenance::DATABASE);
      Slice cSlice = it.value.get(arangodb::maintenance::COLLECTION);
      Slice smartSlice = it.value.get(arangodb::StaticStrings::IsSmartChild);
      if (typeSlice.isString() &&
          typeSlice.isEqualString(arangodb::maintenance::UPGRADE_COLLECTION) &&
          dbSlice.isString() && dbSlice.isEqualString(database) && cSlice.isString() &&
          cSlice.isEqualString(collection) && smartSlice.isBoolean()) {
        isSmartChild = smartSlice.getBoolean();
      }
    }
  }

  return isSmartChild;
}

bool haveError(arangodb::LogicalCollection& collection) {
  std::unique_lock<std::mutex> lock(collection.upgradeStatusLock());
  arangodb::LogicalCollection::UpgradeStatus& status = collection.upgradeStatus();
  return !status.errorMessage().empty();
}

std::pair<arangodb::Result, bool> updateStatusFromCurrent(
    arangodb::ClusterInfo& ci, arangodb::LogicalCollection& collection,
    std::string const& shard, std::unique_lock<std::mutex>& lock) {
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

  TRI_ASSERT(lock.owns_lock());
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

  return std::make_pair(res, localChanges);
}

void writeStatusToCurrent(arangodb::LogicalCollection& collection,
                          arangodb::maintenance::ActionDescription const& desc,
                          std::unique_lock<std::mutex> const& lock) {
  arangodb::velocypack::Builder statusBuilder;
  {
    TRI_ASSERT(lock.owns_lock());
    auto const& status = collection.upgradeStatus();
    arangodb::velocypack::ObjectBuilder object(&statusBuilder);
    status.toVelocyPack(statusBuilder, false);
  }

  auto operations = std::vector<arangodb::AgencyOperation>{};

  std::string const statusKey =
      "/Current/Collections/" + desc.get(arangodb::maintenance::DATABASE) +
      "/" + desc.get(arangodb::maintenance::COLLECTION) + "/" +
      desc.get(arangodb::maintenance::SHARD) + "/" + arangodb::maintenance::UPGRADE_STATUS;
  operations.emplace_back(statusKey, arangodb::AgencyValueOperationType::SET,
                          statusBuilder.slice());

  std::string const versionKey = "/Current/Version";
  operations.emplace_back(versionKey, arangodb::AgencySimpleOperationType::INCREMENT_OP);

  auto trx = arangodb::AgencyWriteTransaction{operations};

  arangodb::AgencyComm comm(collection.vocbase().server());
  arangodb::AgencyCommResult result = comm.sendTransactionWithFailover(trx);
  if (!result.successful()) {
    throw std::runtime_error(
        "failed to send and execute transaction to set shard upgrade status");
  }
}

void removeStatusFromCurrent(arangodb::LogicalCollection& collection,
                             arangodb::maintenance::ActionDescription const& desc) {
  auto operations = std::vector<arangodb::AgencyOperation>{};

  std::string const statusKey =
      "/Current/Collections/" + desc.get(arangodb::maintenance::DATABASE) +
      "/" + desc.get(arangodb::maintenance::COLLECTION) + "/" +
      desc.get(arangodb::maintenance::SHARD) + "/" + arangodb::maintenance::UPGRADE_STATUS;
  operations.emplace_back(statusKey, arangodb::AgencySimpleOperationType::DELETE_OP);

  std::string const versionKey = "/Current/Version";
  operations.emplace_back(versionKey, arangodb::AgencySimpleOperationType::INCREMENT_OP);

  auto trx = arangodb::AgencyWriteTransaction{operations};

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
  std::unique_lock<std::mutex> lock(collection.upgradeStatusLock());

  arangodb::LogicalCollection::UpgradeStatus& status = collection.upgradeStatus();
  for (auto& it : status.map()) {
    map[it.second].emplace_back(it.first);
  }

  return map;
}

void setStatusesForRollback(arangodb::LogicalCollection& collection,
                            std::unique_lock<std::mutex>& lock) {
  TRI_ASSERT(lock.owns_lock());
  arangodb::LogicalCollection::UpgradeStatus& status = collection.upgradeStatus();
  status.setError("");

  auto& map = status.map();
  for (auto const& [server, state] : map) {
    if (state == ::UpgradeState::ToDo || state == ::UpgradeState::Prepare) {
      // effectively already rolled back, so mark it as such
      status.set(server, ::UpgradeState::Rollback);
    }
  }
}
}  // namespace

namespace arangodb::maintenance {

UpgradeCollection::UpgradeCollection(MaintenanceFeature& feature, ActionDescription const& desc)
    : ActionBase(feature, desc), _timeout(600), _isSmartChild(false), _inRollback(false) {
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

  if (!error.str().empty()) {
    LOG_TOPIC("a6f4c", ERR, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

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
          << "Upgrading local collection " + shard;

      _timeout = ::getTimeout(config.vocbase.server(), database, collection);
      _isSmartChild = ::getSmartChild(config.vocbase.server(), database, collection);

      bool ok = refreshPlanStatus();
      if (!ok) {
        // no upgrade status in Plan, clean status out of Current
        {
          std::unique_lock<std::mutex> lock(config.collection->upgradeStatusLock());
          LogicalCollection::UpgradeStatus& status = config.collection->upgradeStatus();
          status.clear();
        }
        ::removeStatusFromCurrent(*config.collection, _description);
        return false;
      }

      // start an exclusive transaction to block access to the collection
      std::shared_ptr<transaction::Context> context =
          transaction::StandaloneContext::Create(config.vocbase);
      _trx = std::make_unique<SingleCollectionTransaction>(context, *config.collection,
                                                           AccessMode::Type::EXCLUSIVE);
      _result = _trx->begin();
      TRI_IF_FAILURE("UpgradeCollectionDBServer::StartTransaction") {
        _result.reset(TRI_ERROR_INTERNAL, "could not start transaction");
      }
      if (_result.fail()) {
        setError(*config.collection, _result.errorMessage());
        setState(FAILED);
        return false;
      }

      return next();
    } else {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << "in database " + database;
      LOG_TOPIC("620fc", ERR, Logger::MAINTENANCE) << error.str();
      _result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      setState(FAILED);
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79543", WARN, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }

  return false;
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

  bool ok = refreshPlanStatus();
  if (!ok) {
    LOG_TOPIC("61544", DEBUG, Logger::MAINTENANCE)
        << "Upgrading local collection " << shard << "; plan status may be outdated";
  }
  ::UpgradeState targetPhase =
      LogicalCollection::UpgradeStatus::stateFromSlice(_planStatus.slice());

  try {
    ::Config config(_description);
    if (config.collection) {
      LOG_TOPIC("62543", DEBUG, Logger::MAINTENANCE)
          << "Upgrading local collection " + shard;

      Result res;

      {
        std::unique_lock<std::mutex> lock(config.collection->upgradeStatusLock());
        if (targetPhase == ::UpgradeState::Rollback && !_inRollback) {
          _inRollback = true;
          ::setStatusesForRollback(*config.collection, lock);
          ::writeStatusToCurrent(*config.collection, _description, lock);
        }
      }

      // check if we have any async errors since last run
      if (::haveError(*config.collection)) {
        // wait for rollback request
        wait();
        return true;
      }

      // will check Current, and fill in any missing servers with ToDo
      bool localChanges = false;
      auto& feature = config.vocbase.server().getFeature<ClusterFeature>();
      auto& ci = feature.clusterInfo();
      {
        std::unique_lock<std::mutex> lock(config.collection->upgradeStatusLock());
        std::tie(res, localChanges) =
            ::updateStatusFromCurrent(ci, *config.collection, shard, lock);
      }
      TRI_IF_FAILURE("UpgradeCollectionDBServer::UpgradeStatusFromCurrent") {
        res.reset(TRI_ERROR_INTERNAL, "could not update status from current");
      }
      if (res.fail()) {
        setError(*config.collection, res.errorMessage());
        return true;
      }
      if (localChanges && targetPhase != ::UpgradeState::Cleanup) {
        // make sure it's written back out to Current with up-to-date server list
        std::unique_lock<std::mutex> lock(config.collection->upgradeStatusLock());
        ::writeStatusToCurrent(*config.collection, _description, lock);
      }

      std::unordered_map<::UpgradeState, std::vector<std::string>> servers =
          ::serversByStatus(*config.collection);
      switch (targetPhase) {
        case ::UpgradeState::Prepare: {
          [[maybe_unused]] bool haveServers =
              processPhase(servers, ::UpgradeState::ToDo, targetPhase, *config.collection);
          break;
        }
        case ::UpgradeState::Finalize: {
          // first prepare any servers that are still ToDo
          bool haveServers = processPhase(servers, ::UpgradeState::ToDo,
                                          ::UpgradeState::Prepare, *config.collection);
          // if everything was already prepared, proceed to finalize
          if (!haveServers && !::haveError(*config.collection)) {
            haveServers = processPhase(servers, ::UpgradeState::Prepare,
                                       targetPhase, *config.collection);
          }
          break;
        }
        case ::UpgradeState::Rollback: {
          [[maybe_unused]] bool haveServers =
              processPhase(servers, ::UpgradeState::Finalize, targetPhase,
                           *config.collection);
          break;
        }
        case ::UpgradeState::Cleanup: {
          // send cleanup for all non-Cleanup server states, even ToDo, since it
          // might have started Prepare and encountered an error
          [[maybe_unused]] bool haveServers =
              processPhase(servers, ::UpgradeState::ToDo,
                           ::UpgradeState::Cleanup, *config.collection);
          haveServers = processPhase(servers, ::UpgradeState::Prepare,
                                     targetPhase, *config.collection);
          haveServers = processPhase(servers, ::UpgradeState::Finalize,
                                     targetPhase, *config.collection);
          haveServers = processPhase(servers, ::UpgradeState::Rollback,
                                     targetPhase, *config.collection);
          // and process the reponses from cleanup requests
          haveServers = processPhase(servers, ::UpgradeState::Cleanup,
                                     targetPhase, *config.collection);
          break;
        }
        case ::UpgradeState::ToDo:
        default:
          break;
      }
    } else {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << "in database " + database;
      LOG_TOPIC("630fc", ERR, Logger::MAINTENANCE) << error.str();
      _result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      setState(FAILED);
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79447", WARN, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }

  return hasMore(targetPhase);
}

bool UpgradeCollection::hasMore(::UpgradeState targetPhase) const {
  if (_result.fail() || (_futures.empty() && targetPhase == ::UpgradeState::Cleanup) ||
      feature().isShuttingDown()) {
    return false;
  }

  wait();
  return true;
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
    reqBuilder.add(StaticStrings::IsSmartChild, velocypack::Value(_isSmartChild));
  }

  auto* pool = collection.vocbase().server().getFeature<NetworkFeature>().pool();
  std::string url = "/_api/collection/" + collection.name() + "/upgrade";
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(static_cast<double>(_timeout));
  reqOpts.database = collection.vocbase().name();

  return arangodb::network::sendRequestRetry(pool, "server:" + server,
                                             fuerte::RestVerb::Put, url,
                                             std::move(reqBuffer), reqOpts, {})
      .thenValue(handleResponse(server, phase));
}

std::function<Result(network::Response&&)> UpgradeCollection::handleResponse(
    std::string const& server, LogicalCollection::UpgradeStatus::State phase) {
  return [self = shared_from_this(), server, phase](network::Response&& res) -> Result {
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

    try {
      ::Config config(self->_description);
      if (config.collection) {
        if (result.fail()) {
          return result;
        }

        // okay, we executed this operation successfully, let everyone know
        {
          std::unique_lock<std::mutex> lock(config.collection->upgradeStatusLock());
          if (!self->_inRollback || phase == ::UpgradeState::Rollback ||
              phase == ::UpgradeState::Cleanup) {
            arangodb::LogicalCollection::UpgradeStatus& status =
                config.collection->upgradeStatus();
            status.set(server, phase);
            ::writeStatusToCurrent(*config.collection, self->_description, lock);
          }
        }
      } else {
        std::stringstream error;
        error << "failed to lookup local collection " << self->_description.get(SHARD)
              << "in database " + self->_description.get(DATABASE);
        LOG_TOPIC("720fc", ERR, Logger::MAINTENANCE) << error.str();
        result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      }
    } catch (std::exception const& e) {
      std::stringstream error;

      error << "action " << self->_description << " failed with exception " << e.what();
      LOG_TOPIC("7a443", WARN, Logger::MAINTENANCE)
          << "UpgradeCollection: " << error.str();
      result.reset(TRI_ERROR_INTERNAL, error.str());
    }

    return result;
  };
}

bool UpgradeCollection::processPhase(
    std::unordered_map<::UpgradeState, std::vector<std::string>> servers,
    ::UpgradeState searchPhase, ::UpgradeState targetPhase, LogicalCollection& collection) {
  std::size_t serverCount = 0;
  decltype(servers)::const_iterator it = servers.find(searchPhase);
  if (it != servers.end() && !it->second.empty()) {
    serverCount = it->second.size();
    for (auto const& server : it->second) {
      decltype(_futures)::iterator f = _futures.find(server);
      bool mustSendRequest = false;
      if (f == _futures.end() && searchPhase != targetPhase) {
        mustSendRequest = true;
      } else {
        auto& [phase, future] = f->second;
        if (future.isReady()) {
          if (future.hasValue()) {
            Result r = future.get();
            if (r.ok()) {
              if (phase == targetPhase) {
                // this future is from this phase, mark the server done
                --serverCount;
              } else {
                // this future is from a previous phase, go ahead and mark to
                // send so we don't have a missed update
                mustSendRequest = true;
              }
            } else {
              std::unique_lock<std::mutex> lock(collection.upgradeStatusLock());
              if (!_inRollback || phase == ::UpgradeState::Rollback ||
                  phase == ::UpgradeState::Cleanup) {
                setError(collection, r.errorMessage(), lock);
              }
            }
            _futures.erase(f);
          } else if (future.hasException()) {
            decltype(future) theFuture = std::move(future);
            _futures.erase(f);
            LOG_TOPIC("b6f4d", WARN, Logger::MAINTENANCE)
                << "UpgradeCollection '" << _description.get(SHARD) << "': "
                << "encountered exception while processing request to server '"
                << server << "'";
            if (theFuture.getTry().exception()) {
              setError(
                  collection,
                  "encountered exception while processing request to server '" +
                      server + "'");
              std::rethrow_exception(theFuture.getTry().exception());
            }
          }
        }
      }
      if (mustSendRequest) {
        futures::Future<Result> future = sendRequest(collection, server, targetPhase);
        _futures.emplace(server, std::make_pair(targetPhase, std::move(future)));
      }
    }
  }
  return serverCount > 0;
}

bool UpgradeCollection::refreshPlanStatus() {
  ClusterFeature& cf = _feature.server().getFeature<ClusterFeature>();
  ClusterInfo& ci = cf.clusterInfo();

  std::shared_ptr<velocypack::Builder> plan = ci.getPlan();
  if (!plan) {
    return false;
  }

  velocypack::Slice collections = plan->slice().get("Collections");
  if (!collections.isObject()) {
    return false;
  }

  velocypack::Slice db = collections.get(_description.get(DATABASE));
  if (!db.isObject()) {
    return false;
  }

  velocypack::Slice collection = db.get(_description.get(COLLECTION));
  if (!collection.isObject()) {
    return false;
  }

  velocypack::Slice status = collection.get(UPGRADE_STATUS);
  if (!status.isInteger()) {
    return false;
  }

  _planStatus.clear();
  _planStatus.add(status);

  return true;
}

void UpgradeCollection::setError(LogicalCollection& collection, std::string const& message) {
  std::unique_lock<std::mutex> lock(collection.upgradeStatusLock());
  setError(collection, message, lock);
}

void UpgradeCollection::setError(LogicalCollection& collection, std::string const& message,
                                 std::unique_lock<std::mutex>& lock) {
  LOG_TOPIC("b6f4e", WARN, Logger::MAINTENANCE)
      << "UpgradeCollection '" << _description.get(SHARD) << "': " << message;
  LogicalCollection::UpgradeStatus& status = collection.upgradeStatus();
  status.setError(message);
  ::writeStatusToCurrent(collection, _description, lock);
}

void UpgradeCollection::wait() const {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}  // namespace arangodb::maintenance