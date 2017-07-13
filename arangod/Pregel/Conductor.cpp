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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/Conductor.h"
#include "Pregel/Aggregator.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/Utils.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::basics;

const char* arangodb::pregel::ExecutionStateNames[6] = {
    "none", "running", "done", "canceled", "in error", "recovering"};

Conductor::Conductor(uint64_t executionNumber, TRI_vocbase_t* vocbase,
                     std::vector<CollectionID> const& vertexCollections,
                     std::vector<CollectionID> const& edgeCollections,
                     std::string const& algoName, VPackSlice const& config)
    : _vocbaseGuard(vocbase),
      _executionNumber(executionNumber),
      _algorithm(AlgoRegistry::createAlgorithm(algoName, config)),
      _vertexCollections(vertexCollections),
      _edgeCollections(edgeCollections) {
  if (!config.isObject()) {
    _userParams.openObject();
    _userParams.close();
  } else {
    _userParams.add(config);
  }

  if (!_algorithm) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Algorithm not found");
  }
  _masterContext.reset(_algorithm->masterContext(config));
  _aggregators.reset(new AggregatorHandler(_algorithm.get()));

  _maxSuperstep =
      VelocyPackHelper::getNumericValue(config, "maxGSS", _maxSuperstep);
  // configure the async mode as off by default
  VPackSlice async = _userParams.slice().get("async");
  _asyncMode =
      _algorithm->supportsAsyncMode() && async.isBool() && async.getBoolean();
  if (_asyncMode) {
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Running in async mode";
  }
  VPackSlice lazy = _userParams.slice().get("lazyLoading");
  _lazyLoading = _algorithm->supportsLazyLoading();
  _lazyLoading = _lazyLoading && (lazy.isNone() || lazy.getBoolean());
  if (_lazyLoading) {
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Enabled lazy loading";
  }
  VPackSlice storeSlice = config.get("store");
  _storeResults = !storeSlice.isBool() || storeSlice.getBool();
  if (!_storeResults) {
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Will keep results in-memory";
  }
}

Conductor::~Conductor() {
  if (_state != ExecutionState::DEFAULT) {
    try {
      this->cancel();
    } catch (...) {
      // must not throw exception from here
    }
  }
}

void Conductor::start() {
  _startTimeSecs = TRI_microtime();
  _globalSuperstep = 0;
  _state = ExecutionState::RUNNING;

  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Telling workers to load the data";
  int res = _initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::CANCELED;
    LOG_TOPIC(ERR, Logger::PREGEL) << "Not all DBServers started the execution";
  }
}

// only called by the conductor, is protected by the
// mutex locked in finishedGlobalStep
bool Conductor::_startGlobalStep() {
  // send prepare GSS notice
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCountKey, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCountKey, VPackValue(_totalEdgesCount));
  b.close();

  /// collect the aggregators
  _aggregators->resetValues();
  _statistics.resetActiveCount();
  _totalVerticesCount = 0;  // might change during execution
  _totalEdgesCount = 0;
  // we are explicitly expecting an response containing the aggregated
  // values as well as the count of active vertices
  int res = _sendToAllDBServers(
      Utils::prepareGSSPath, b, [&](VPackSlice const& payload) {
        _aggregators->aggregateValues(payload);
        _statistics.accumulateActiveCounts(payload);
        _totalVerticesCount += payload.get(Utils::vertexCountKey).getUInt();
        _totalEdgesCount += payload.get(Utils::edgeCountKey).getUInt();
      });
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::IN_ERROR;
    LOG_TOPIC(ERR, Logger::PREGEL)
        << "Seems there is at least one worker out of order";
    // the recovery mechanisms should take care of this
    return false;
  }

  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool proceed = true;
  if (_masterContext &&
      _globalSuperstep > 0) {  // ask algorithm to evaluate aggregated values
    _masterContext->_globalSuperstep = _globalSuperstep - 1;
    _masterContext->_enterNextGSS = false;
    proceed = _masterContext->postGlobalSuperstep();
    if (!proceed) {
      LOG_TOPIC(DEBUG, Logger::PREGEL) << "Master context ended execution";
    }
  }

  // TODO make maximum configurable
  bool done = _globalSuperstep > 0 && _statistics.noActiveVertices() &&
              _statistics.allMessagesProcessed();
  if (!proceed || done || _globalSuperstep >= _maxSuperstep) {
    _state = ExecutionState::DONE;
    // tells workers to store / discard results
    if (_storeResults) {
      _finalizeWorkers();
    } else {  // just stop the timer
      _endTimeSecs = TRI_microtime();
      LOG_TOPIC(INFO, Logger::PREGEL) << "Done execution took"
                                      << totalRuntimeSecs() << " s";
    }
    return false;
  }
  if (_masterContext) {
    _masterContext->_globalSuperstep = _globalSuperstep;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->preGlobalSuperstep();
  }

  b.clear();
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::vertexCountKey, VPackValue(_totalVerticesCount));
  b.add(Utils::edgeCountKey, VPackValue(_totalEdgesCount));
  _aggregators->serializeValues(b);
  b.close();
  LOG_TOPIC(DEBUG, Logger::PREGEL) << b.toString();

  // start vertex level operations, does not get a response
  res = _sendToAllDBServers(Utils::startGSSPath, b);  // call me maybe
  if (res != TRI_ERROR_NO_ERROR) {
    _state = ExecutionState::IN_ERROR;
    LOG_TOPIC(ERR, Logger::PREGEL) << "Conductor could not start GSS "
                                   << _globalSuperstep;
    // the recovery mechanisms should take care od this
  } else {
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Conductor started new gss "
                                     << _globalSuperstep;
  }
  return res == TRI_ERROR_NO_ERROR;
}

// ============ Conductor callbacks ===============
void Conductor::finishedWorkerStartup(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RUNNING) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "We are not in a state where we expect a response";
    return;
  }

  _totalVerticesCount += data.get(Utils::vertexCountKey).getUInt();
  _totalEdgesCount += data.get(Utils::edgeCountKey).getUInt();
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  LOG_TOPIC(INFO, Logger::PREGEL) << "Running pregel with "
                                  << _totalVerticesCount << " vertices, "
                                  << _totalEdgesCount << " edges";
  if (_masterContext) {
    _masterContext->_globalSuperstep = 0;
    _masterContext->_vertexCount = _totalVerticesCount;
    _masterContext->_edgeCount = _totalEdgesCount;
    _masterContext->_aggregators = _aggregators.get();
    _masterContext->preApplication();
  }

  _computationStartTimeSecs = TRI_microtime();
  if (_startGlobalStep()) {
    // listens for changing primary DBServers on each collection shard
    RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
    if (mngr) {
      mngr->monitorCollections(_vocbaseGuard.vocbase()->name(),
                               _vertexCollections, this);
    }
  }
}

/// Will optionally send a response, to notify the worker of converging
/// aggregator
/// values which can be coninually updated (in async mode)
VPackBuilder Conductor::finishedWorkerStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  // this method can be called multiple times in a superstep depending on
  // whether we are in the async mode
  uint64_t gss = data.get(Utils::globalSuperstepKey).getUInt();
  if (gss != _globalSuperstep ||
      !(_state == ExecutionState::RUNNING ||
        _state == ExecutionState::CANCELED)) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Conductor did received a callback from the wrong superstep";
    return VPackBuilder();
  }

  // track message counts to decide when to halt or add global barriers.
  // In normal mode this will wait for a response from each worker,
  // in async mode this will wait until all messages were processed
  _statistics.accumulateMessageStats(data);
  if (_asyncMode == false) {  // in async mode we wait for all responded
    _ensureUniqueResponse(data);
    // wait for the last worker to respond
    if (_respondedServers.size() != _dbServers.size()) {
      return VPackBuilder();
    }
  } else if (_statistics.clientCount() < _dbServers.size() ||  // no messages
             !_statistics.allMessagesProcessed()) {  // haven't received msgs
    VPackBuilder response;
    _aggregators->aggregateValues(data);
    if (_masterContext) {
      _masterContext->postLocalSuperstep();
    }
    response.openObject();
    _aggregators->serializeValues(response);
    if (_masterContext && _masterContext->_enterNextGSS) {
      response.add(Utils::enterNextGSSKey, VPackValue(true));
    }
    response.close();
    return response;
  }

  LOG_TOPIC(DEBUG, Logger::PREGEL)
      << "Finished gss " << _globalSuperstep << " in "
      << (TRI_microtime() - _computationStartTimeSecs) << "s";
  //_statistics.debugOutput();
  _globalSuperstep++;

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  // don't block the response for workers waiting on this callback
  // this should allow workers to go into the IDLE state
  scheduler->post([this] {
    MUTEX_LOCKER(guard, _callbackMutex);

    if (_state == ExecutionState::RUNNING) {
      _startGlobalStep();  // trigger next superstep
    } else if (_state == ExecutionState::CANCELED) {
      LOG_TOPIC(WARN, Logger::PREGEL)
          << "Execution was canceled, results will be discarded.";
      _finalizeWorkers();  // tells workers to store / discard results
    } else {  // this prop shouldn't occur unless we are recovering or in error
      LOG_TOPIC(WARN, Logger::PREGEL)
          << "No further action taken after receiving all responses";
    }
  });
  return VPackBuilder();
}

void Conductor::finishedRecoveryStep(VPackSlice const& data) {
  MUTEX_LOCKER(guard, _callbackMutex);
  _ensureUniqueResponse(data);
  if (_state != ExecutionState::RECOVERING) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "We are not in a state where we expect a recovery response";
    return;
  }

  // the recovery mechanism might be gathering state information
  _aggregators->aggregateValues(data);
  if (_respondedServers.size() != _dbServers.size()) {
    return;
  }

  // only compensations supported
  bool proceed = false;
  if (_masterContext) {
    proceed = proceed || _masterContext->postCompensation();
  }

  int res = TRI_ERROR_NO_ERROR;
  if (proceed) {
    // reset values which are calculated during the superstep
    _aggregators->resetValues();
    if (_masterContext) {
      _masterContext->preCompensation();
    }

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    _aggregators->serializeValues(b);
    b.close();
    // first allow all workers to run worker level operations
    res = _sendToAllDBServers(Utils::continueRecoveryPath, b);

  } else {
    LOG_TOPIC(INFO, Logger::PREGEL) << "Recovery finished. Proceeding normally";

    // build the message, works for all cases
    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    res = _sendToAllDBServers(Utils::finalizeRecoveryPath, b);
    if (res == TRI_ERROR_NO_ERROR) {
      _state = ExecutionState::RUNNING;
      _startGlobalStep();
    }
  }
  if (res != TRI_ERROR_NO_ERROR) {
    cancel();
    LOG_TOPIC(INFO, Logger::PREGEL) << "Recovery failed";
  }
}

void Conductor::cancel() {
  if (_state == ExecutionState::RUNNING ||
      _state == ExecutionState::RECOVERING ||
      _state == ExecutionState::IN_ERROR) {
    _state = ExecutionState::CANCELED;
    _finalizeWorkers();
  }
}

void Conductor::startRecovery() {
  MUTEX_LOCKER(guard, _callbackMutex);
  if (_state != ExecutionState::RUNNING && _state != ExecutionState::IN_ERROR) {
    return;  // maybe we are already in recovery mode
  } else if (_algorithm->supportsCompensation() == false) {
    LOG_TOPIC(ERR, Logger::PREGEL) << "Algorithm does not support recovery";
    cancel();
    return;
  }

  // we lost a DBServer, we need to reconfigure all remainging servers
  // so they load the data for the lost machine
  _state = ExecutionState::RECOVERING;
  _statistics.reset();

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  boost::asio::io_service* ioService = SchedulerFeature::SCHEDULER->ioService();
  TRI_ASSERT(ioService != nullptr);

  // let's wait for a final state in the cluster
  _boost_timer.reset(new boost::asio::deadline_timer(
      *ioService, boost::posix_time::seconds(2)));
  _boost_timer->async_wait([this](const boost::system::error_code& error) {
    _boost_timer.reset();

    if (error == boost::asio::error::operation_aborted ||
        _state != ExecutionState::RECOVERING) {
      return;  // seems like we are canceled
    }
    std::vector<ServerID> goodServers;
    int res = PregelFeature::instance()->recoveryManager()->filterGoodServers(
        _dbServers, goodServers);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::PREGEL) << "Recovery proceedings failed";
      cancel();
      return;
    }
    _dbServers = goodServers;

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.close();
    _sendToAllDBServers(Utils::cancelGSSPath, b);
    if (_state != ExecutionState::RECOVERING) {
      return;  // seems like we are canceled
    }

    // Let's try recovery
    if (_masterContext) {
      bool proceed = _masterContext->preCompensation();
      if (!proceed) {
        cancel();
      }
    }

    VPackBuilder additionalKeys;
    additionalKeys.openObject();
    additionalKeys.add(Utils::recoveryMethodKey, VPackValue(Utils::compensate));
    _aggregators->serializeValues(b);
    additionalKeys.close();
    _aggregators->resetValues();

    // initialize workers will reconfigure the workers and set the
    // _dbServers list to the new primary DBServers
    res = _initializeWorkers(Utils::startRecoveryPath, additionalKeys.slice());
    if (res != TRI_ERROR_NO_ERROR) {
      cancel();
      LOG_TOPIC(ERR, Logger::PREGEL) << "Compensation failed";
    }
  });
}

// resolves into an ordered list of shards for each collection on each server
static void resolveInfo(
    TRI_vocbase_t* vocbase, CollectionID const& collectionID,
    std::map<CollectionID, std::string>& collectionPlanIdMap,
    std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>& serverMap,
    std::vector<ShardID>& allShards) {
  ServerState* ss = ServerState::instance();
  if (!ss->isRunningInCluster()) {  // single server mode
    LogicalCollection* lc = vocbase->lookupCollection(collectionID);
    if (lc == nullptr || lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                     collectionID);
    }

    collectionPlanIdMap.emplace(collectionID, lc->planId_as_string());
    allShards.push_back(collectionID);
    serverMap[ss->getId()][collectionID].push_back(collectionID);

  } else if (ss->isCoordinator()) {  // we are in the cluster

    ClusterInfo* ci = ClusterInfo::instance();
    std::shared_ptr<LogicalCollection> lc =
        ci->getCollection(vocbase->name(), collectionID);
    if (!lc || lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                     collectionID);
    }
    collectionPlanIdMap.emplace(collectionID, lc->planId_as_string());

    std::shared_ptr<std::vector<ShardID>> shardIDs =
        ci->getShardList(lc->cid_as_string());
    allShards.insert(allShards.end(), shardIDs->begin(), shardIDs->end());

    for (auto const& shard : *shardIDs) {
      std::shared_ptr<std::vector<ServerID>> servers =
          ci->getResponsibleServer(shard);
      if (servers->size() > 0) {
        serverMap[(*servers)[0]][lc->name()].push_back(shard);
      }
    }
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }
}

/// should cause workers to start a new execution or begin with recovery
/// proceedings
int Conductor::_initializeWorkers(std::string const& suffix,
                                  VPackSlice additional) {
  std::string const path =
      Utils::baseUrl(_vocbaseGuard.vocbase()->name(), Utils::workerPrefix) +
      suffix;

  // int64_t vertexCount = 0, edgeCount = 0;
  std::map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap,
      edgeMap;
  std::vector<ShardID> shardList;

  // resolve plan id's and shards on the servers
  for (CollectionID const& collectionID : _vertexCollections) {
    resolveInfo(_vocbaseGuard.vocbase(), collectionID, collectionPlanIdMap,
                vertexMap, shardList);  // store or
  }
  for (CollectionID const& collectionID : _edgeCollections) {
    resolveInfo(_vocbaseGuard.vocbase(), collectionID, collectionPlanIdMap,
                edgeMap, shardList);  // store or
  }

  _dbServers.clear();
  for (auto const& pair : vertexMap) {
    _dbServers.push_back(pair.first);
  }
  // do not reload all shard id's, this list is must stay in the same order
  if (_allShards.size() == 0) {
    _allShards = shardList;
  }

  std::string coordinatorId = ServerState::instance()->getId();
  std::vector<ClusterCommRequest> requests;

  for (auto const& it : vertexMap) {
    ServerID const& server = it.first;
    std::map<CollectionID, std::vector<ShardID>> const& vertexShardMap =
        it.second;
    std::map<CollectionID, std::vector<ShardID>> const& edgeShardMap =
        edgeMap[it.first];

    VPackBuilder b;
    b.openObject();
    b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
    b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
    b.add(Utils::algorithmKey, VPackValue(_algorithm->name()));
    b.add(Utils::userParametersKey, _userParams.slice());
    b.add(Utils::coordinatorIdKey, VPackValue(coordinatorId));
    b.add(Utils::asyncModeKey, VPackValue(_asyncMode));
    b.add(Utils::lazyLoadingKey, VPackValue(_lazyLoading));
    if (additional.isObject()) {
      for (auto const& pair : VPackObjectIterator(additional)) {
        b.add(pair.key.copyString(), pair.value);
      }
    }

    b.add(Utils::vertexShardsKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : vertexShardMap) {
      b.add(pair.first, VPackValue(VPackValueType::Array));
      for (ShardID const& shard : pair.second) {
        b.add(VPackValue(shard));
      }
      b.close();
    }
    b.close();
    b.add(Utils::edgeShardsKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : edgeShardMap) {
      b.add(pair.first, VPackValue(VPackValueType::Array));
      for (ShardID const& shard : pair.second) {
        b.add(VPackValue(shard));
      }
      b.close();
    }
    b.close();
    b.add(Utils::collectionPlanIdMapKey, VPackValue(VPackValueType::Object));
    for (auto const& pair : collectionPlanIdMap) {
      b.add(pair.first, VPackValue(pair.second));
    }
    b.close();
    b.add(Utils::globalShardListKey, VPackValue(VPackValueType::Array));
    for (std::string const& shard : _allShards) {
      b.add(VPackValue(shard));
    }
    b.close();
    b.close();

    // only on single server
    if (ServerState::instance()->getRole() == ServerState::ROLE_SINGLE) {
      std::shared_ptr<IWorker> w =
          PregelFeature::instance()->worker(_executionNumber);
      if (!w) {
        PregelFeature::instance()->addWorker(
            AlgoRegistry::createWorker(_vocbaseGuard.vocbase(), b.slice()),
            _executionNumber);
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "Worker with this execution number already exists.");
      }
      return TRI_ERROR_NO_ERROR;
    }

    auto body = std::make_shared<std::string const>(b.toJson());
    requests.emplace_back("server:" + server, rest::RequestType::POST, path,
                          body);
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Initializing Server " << server;
  }

  std::shared_ptr<ClusterComm> cc = ClusterComm::instance();
  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone,
                                      LogTopic("Pregel Conductor"), false);
  Utils::printResponses(requests);
  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

int Conductor::_finalizeWorkers() {
  double compEnd = TRI_microtime();

  bool store = _state == ExecutionState::DONE;
  store = store && _storeResults;
  if (_masterContext) {
    _masterContext->postApplication();
  }
  // stop monitoring shards
  RecoveryManager* mngr = PregelFeature::instance()->recoveryManager();
  if (mngr) {
    mngr->stopMonitoring(this);
  }

  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Finalizing workers";
  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.add(Utils::globalSuperstepKey, VPackValue(_globalSuperstep));
  b.add(Utils::storeResultsKey, VPackValue(store));
  b.close();
  int res = _sendToAllDBServers(Utils::finalizeExecutionPath, b);
  _endTimeSecs = TRI_microtime();  // offically done

  VPackBuilder debugOut;
  debugOut.openObject();
  debugOut.add("stats", VPackValue(VPackValueType::Object));
  _statistics.serializeValues(debugOut);
  debugOut.close();
  _aggregators->serializeValues(debugOut);
  debugOut.close();

  LOG_TOPIC(INFO, Logger::PREGEL) << "Done. We did " << _globalSuperstep
                                  << " rounds";
  LOG_TOPIC(DEBUG, Logger::PREGEL)
      << "Startup Time: " << _computationStartTimeSecs - _startTimeSecs << "s";
  LOG_TOPIC(DEBUG, Logger::PREGEL)
      << "Computation Time: " << compEnd - _computationStartTimeSecs << "s";
  LOG_TOPIC(DEBUG, Logger::PREGEL)
      << "Storage Time: " << TRI_microtime() - compEnd << "s";
  LOG_TOPIC(INFO, Logger::PREGEL) << "Overall: " << totalRuntimeSecs() << "s";
  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Stats: " << debugOut.toString();
  return res;
}

VPackBuilder Conductor::collectAQLResults() {
  if (_state != ExecutionState::DONE) {
    return VPackBuilder();
  }

  VPackBuilder b;
  b.openObject();
  b.add(Utils::executionNumberKey, VPackValue(_executionNumber));
  b.close();
  VPackBuilder messages;
  int res = _sendToAllDBServers(Utils::aqlResultsPath, b,
                                [&](VPackSlice const& payload) {
                                  if (payload.isArray()) {
                                    messages.add(payload);
                                  }
                                });
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return messages;
}

VPackBuilder Conductor::toVelocyPack() const {
  VPackBuilder result;
  result.openObject();
  result.add("state", VPackValue(pregel::ExecutionStateNames[_state]));
  result.add("gss", VPackValue(_globalSuperstep));
  result.add("totalRuntime", VPackValue(totalRuntimeSecs()));
  _aggregators->serializeValues(result);
  _statistics.serializeValues(result);
  if (_state != ExecutionState::RUNNING) {
    result.add("vertexCount", VPackValue(_totalVerticesCount));
    result.add("edgeCount", VPackValue(_totalEdgesCount));
  }
  result.close();
  return result;
}

int Conductor::_sendToAllDBServers(std::string const& path,
                                   VPackBuilder const& message) {
  return _sendToAllDBServers(path, message, std::function<void(VPackSlice)>());
}

int Conductor::_sendToAllDBServers(std::string const& path,
                                   VPackBuilder const& message,
                                   std::function<void(VPackSlice)> handle) {
  _respondedServers.clear();

  // to support the single server case, we handle it without optimizing it
  if (ServerState::instance()->isRunningInCluster() == false) {
    if (handle) {
      VPackBuilder response;
      PregelFeature::handleWorkerRequest(_vocbaseGuard.vocbase(), path,
                                         message.slice(), response);
      handle(response.slice());
    } else {
      TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
      rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
      scheduler->post([this, path, message] {
        VPackBuilder response;
        PregelFeature::handleWorkerRequest(_vocbaseGuard.vocbase(), path,
                                           message.slice(), response);
      });
    }
    return TRI_ERROR_NO_ERROR;
  }

  // cluster case
  std::shared_ptr<ClusterComm> cc = ClusterComm::instance();
  if (_dbServers.size() == 0) {
    LOG_TOPIC(WARN, Logger::PREGEL) << "No servers registered";
    return TRI_ERROR_FAILED;
  }
  std::string base =
      Utils::baseUrl(_vocbaseGuard.vocbase()->name(), Utils::workerPrefix);
  auto body = std::make_shared<std::string const>(message.toJson());
  std::vector<ClusterCommRequest> requests;
  for (auto const& server : _dbServers) {
    requests.emplace_back("server:" + server, rest::RequestType::POST,
                          base + path, body);
  }

  size_t nrDone = 0;
  size_t nrGood = cc->performRequests(requests, 5.0 * 60.0, nrDone,
                                      LogTopic("Pregel Conductor"), false);
  LOG_TOPIC(TRACE, Logger::PREGEL) << "Send " << path << " to " << nrDone
                                   << " servers";
  Utils::printResponses(requests);
  if (handle && nrGood == requests.size()) {
    for (ClusterCommRequest const& req : requests) {
      handle(req.result.answer->payload());
    }
  }
  return nrGood == requests.size() ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
}

void Conductor::_ensureUniqueResponse(VPackSlice body) {
  // check if this the only time we received this
  ServerID sender = body.get(Utils::senderKey).copyString();
  if (_respondedServers.find(sender) != _respondedServers.end()) {
    LOG_TOPIC(ERR, Logger::PREGEL) << "Received response already from "
                                   << sender;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CONFLICT);
  }
  _respondedServers.insert(sender);
}
