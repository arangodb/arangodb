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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "PregelFeature.h"

#include <atomic>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Conductor.h"
#include "Pregel/Recovery.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::pregel;

namespace {
bool authorized(std::string const& user) {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }
  return (user == exec.user());
}

network::Headers buildHeaders() {
  auto auth = AuthenticationFeature::instance();

  network::Headers headers;
  if (auth != nullptr && auth->isActive()) {
    headers.try_emplace(StaticStrings::Authorization,
                        "bearer " + auth->tokenCache().jwtToken());
  }
  return headers;
}

/// @brief custom deleter for the PregelFeature.
/// it does nothing, i.e. doesn't delete it. This is because the
/// ApplicationServer is managing the PregelFeature, but we need a shared_ptr to
/// it here as well. The shared_ptr we are using here just tracks the refcount,
/// but doesn't manage the memory
struct NonDeleter {
  void operator()(arangodb::pregel::PregelFeature*) {}
};

}  // namespace

std::pair<Result, uint64_t> PregelFeature::startExecution(
    TRI_vocbase_t& vocbase, std::string algorithm,
    std::vector<std::string> const& vertexCollections,
    std::vector<std::string> const& edgeCollections,
    std::unordered_map<std::string, std::vector<std::string>> const&
        edgeCollectionRestrictions,
    VPackSlice const& params) {
  if (isStopping() || _softShutdownOngoing.load(std::memory_order_relaxed)) {
    return std::make_pair(
        Result{TRI_ERROR_SHUTTING_DOWN, "pregel system not available"}, 0);
  }

  ServerState* ss = ServerState::instance();

  // check the access rights to collections
  ExecContext const& exec = ExecContext::current();
  if (!exec.isSuperuser()) {
    TRI_ASSERT(params.isObject());
    VPackSlice storeSlice = params.get("store");
    bool storeResults = !storeSlice.isBool() || storeSlice.getBool();
    for (std::string const& vc : vertexCollections) {
      bool canWrite = exec.canUseCollection(vc, auth::Level::RW);
      bool canRead = exec.canUseCollection(vc, auth::Level::RO);
      if ((storeResults && !canWrite) || !canRead) {
        return std::make_pair(Result{TRI_ERROR_FORBIDDEN}, 0);
      }
    }
    for (std::string const& ec : edgeCollections) {
      bool canWrite = exec.canUseCollection(ec, auth::Level::RW);
      bool canRead = exec.canUseCollection(ec, auth::Level::RO);
      if ((storeResults && !canWrite) || !canRead) {
        return std::make_pair(Result{TRI_ERROR_FORBIDDEN}, 0);
      }
    }
  }

  for (std::string const& name : vertexCollections) {
    if (ss->isCoordinator()) {
      try {
        auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
        auto coll = ci.getCollection(vocbase.name(), name);

        if (coll->system()) {
          return std::make_pair(
              Result{TRI_ERROR_BAD_PARAMETER,
                     "Cannot use pregel on system collection"},
              0);
        }

        if (coll->status() == TRI_VOC_COL_STATUS_DELETED || coll->deleted()) {
          return std::make_pair(
              Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name}, 0);
        }
      } catch (...) {
        return std::make_pair(
            Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name}, 0);
      }
    } else if (ss->getRole() == ServerState::ROLE_SINGLE) {
      auto coll = vocbase.lookupCollection(name);

      if (coll == nullptr || coll->status() == TRI_VOC_COL_STATUS_DELETED ||
          coll->deleted()) {
        return std::make_pair(
            Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name}, 0);
      }
    } else {
      return std::make_pair(Result{TRI_ERROR_INTERNAL}, 0);
    }
  }

  std::vector<CollectionID> edgeColls;

  // load edge collection
  for (std::string const& name : edgeCollections) {
    if (ss->isCoordinator()) {
      try {
        auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
        auto coll = ci.getCollection(vocbase.name(), name);

        if (coll->system()) {
          return std::make_pair(
              Result{TRI_ERROR_BAD_PARAMETER,
                     "Cannot use pregel on system collection"},
              0);
        }

        if (!coll->isSmart()) {
          std::vector<std::string> eKeys = coll->shardKeys();

          std::string shardKeyAttribute = "vertex";
          if (params.hasKey("shardKeyAttribute")) {
            shardKeyAttribute = params.get("shardKeyAttribute").copyString();
          }

          if (eKeys.size() != 1 || eKeys[0] != shardKeyAttribute) {
            return std::make_pair(
                Result{TRI_ERROR_BAD_PARAMETER,
                       "Edge collection needs to be sharded "
                       "by shardKeyAttribute parameter ('" +
                           shardKeyAttribute +
                           "'), or use SmartGraphs. The current shardKey is: " +
                           (eKeys.empty() ? "undefined" : "'" + eKeys[0] + "'")

                },
                0);
          }
        }

        if (coll->status() == TRI_VOC_COL_STATUS_DELETED || coll->deleted()) {
          return std::make_pair(
              Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name}, 0);
        }

        // smart edge collections contain multiple actual collections
        std::vector<std::string> actual = coll->realNamesForRead();

        edgeColls.insert(edgeColls.end(), actual.begin(), actual.end());
      } catch (...) {
        return std::make_pair(
            Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name}, 0);
      }
    } else if (ss->getRole() == ServerState::ROLE_SINGLE) {
      auto coll = vocbase.lookupCollection(name);

      if (coll == nullptr || coll->deleted()) {
        return std::make_pair(
            Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name}, 0);
      }
      std::vector<std::string> actual = coll->realNamesForRead();
      edgeColls.insert(edgeColls.end(), actual.begin(), actual.end());
    } else {
      return std::make_pair(Result{TRI_ERROR_INTERNAL}, 0);
    }
  }

  uint64_t en = createExecutionNumber();
  auto c = std::make_shared<pregel::Conductor>(
      en, vocbase, vertexCollections, edgeColls, edgeCollectionRestrictions,
      algorithm, params, *this);
  addConductor(std::move(c), en);
  TRI_ASSERT(conductor(en));
  conductor(en)->start();

  return std::make_pair(Result{}, en);
}

uint64_t PregelFeature::createExecutionNumber() {
  return TRI_NewServerSpecificTick();
}

PregelFeature::PregelFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, "Pregel"),
      _softShutdownOngoing(false) {
  setOptional(true);
  startsAfter<application_features::V8FeaturePhase>();
}

PregelFeature::~PregelFeature() {
  TRI_ASSERT(_conductors.empty());
  TRI_ASSERT(_workers.empty());
}

size_t PregelFeature::availableParallelism() {
  const size_t procNum = NumberOfCores::getValue();
  return procNum < 1 ? 1 : procNum;
}

void PregelFeature::scheduleGarbageCollection() {
  if (isStopping()) {
    return;
  }

  // GC will be run every 20 seconds
  std::chrono::seconds offset = std::chrono::seconds(20);

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  auto handle = scheduler->queueDelayed(RequestLane::INTERNAL_LOW, offset,
                                        [this](bool canceled) {
                                          if (!canceled) {
                                            garbageCollectConductors();
                                            scheduleGarbageCollection();
                                          }
                                        });

  MUTEX_LOCKER(guard, _mutex);
  _gcHandle = std::move(handle);
}

void PregelFeature::start() {
  if (ServerState::instance()->isCoordinator()) {
    auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
    _recoveryManager = std::make_unique<RecoveryManager>(ci);
    _recoveryManagerPtr.store(_recoveryManager.get(),
                              std::memory_order_release);
  }

  if (!ServerState::instance()->isAgent()) {
    scheduleGarbageCollection();
  }
}

void PregelFeature::beginShutdown() {
  TRI_ASSERT(isStopping());

  MUTEX_LOCKER(guard, _mutex);
  _gcHandle.reset();

  // cancel all conductors and workers
  for (auto& it : _conductors) {
    it.second.conductor->cancel();
  }
  for (auto it : _workers) {
    it.second.second->cancelGlobalStep(VPackSlice());
  }
}

void PregelFeature::unprepare() {
  garbageCollectConductors();

  MUTEX_LOCKER(guard, _mutex);
  decltype(_conductors) cs = std::move(_conductors);
  decltype(_workers) ws = std::move(_workers);
  guard.unlock();

  // all pending tasks should have been finished by now, and all references
  // to conductors and workers been dropped!
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (auto& it : cs) {
    TRI_ASSERT(it.second.conductor.use_count() == 1);
  }

  for (auto it : _workers) {
    TRI_ASSERT(it.second.second.use_count() == 1);
  }
#endif
}

bool PregelFeature::isStopping() const noexcept {
  return server().isStopping();
}

void PregelFeature::addConductor(std::shared_ptr<Conductor>&& c,
                                 uint64_t executionNumber) {
  if (isStopping() || _softShutdownOngoing.load(std::memory_order_relaxed)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  std::string user = ExecContext::current().user();
  MUTEX_LOCKER(guard, _mutex);
  _conductors.try_emplace(
      executionNumber,
      ConductorEntry{std::move(user), std::chrono::steady_clock::time_point{},
                     std::move(c)});
}

std::shared_ptr<Conductor> PregelFeature::conductor(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _conductors.find(executionNumber);
  return (it != _conductors.end() && ::authorized(it->second.user))
             ? it->second.conductor
             : nullptr;
}

void PregelFeature::garbageCollectConductors() try {
  // iterate over all conductors and remove the ones which can be
  // garbage-collected
  std::vector<std::shared_ptr<Conductor>> conductors;

  // copy out shared-ptrs of Conductors under the mutex
  {
    MUTEX_LOCKER(guard, _mutex);
    for (auto const& it : _conductors) {
      if (it.second.conductor->canBeGarbageCollected()) {
        if (conductors.empty()) {
          conductors.reserve(8);
        }
        conductors.emplace_back(it.second.conductor);
      }
    }
  }

  // cancel and kill conductors without holding the mutex
  // permanently
  for (auto& c : conductors) {
    c->cancel();
  }

  MUTEX_LOCKER(guard, _mutex);
  for (auto& c : conductors) {
    uint64_t executionNumber = c->executionNumber();

    _conductors.erase(executionNumber);
    _workers.erase(executionNumber);
  }
} catch (...) {
}

void PregelFeature::addWorker(std::shared_ptr<IWorker>&& w,
                              uint64_t executionNumber) {
  if (isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  std::string user = ExecContext::current().user();
  MUTEX_LOCKER(guard, _mutex);
  _workers.try_emplace(executionNumber, std::move(user), std::move(w));
}

std::shared_ptr<IWorker> PregelFeature::worker(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _workers.find(executionNumber);
  return (it != _workers.end() && ::authorized(it->second.first))
             ? it->second.second
             : nullptr;
}

void PregelFeature::cleanupConductor(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  _conductors.erase(executionNumber);
  _workers.erase(executionNumber);
}

void PregelFeature::cleanupWorker(uint64_t executionNumber) {
  // unmapping etc might need a few seconds
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestLane::INTERNAL_LOW, [this, executionNumber] {
    MUTEX_LOCKER(guard, _mutex);
    _workers.erase(executionNumber);
  });
}

void PregelFeature::handleConductorRequest(TRI_vocbase_t& vocbase,
                                           std::string const& path,
                                           VPackSlice const& body,
                                           VPackBuilder& outBuilder) {
  if (isStopping()) {
    return;  // shutdown ongoing
  }

  VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);
  if (!sExecutionNum.isInteger() && !sExecutionNum.isString()) {
    LOG_TOPIC("8410a", ERR, Logger::PREGEL) << "Invalid execution number";
  }
  uint64_t exeNum = 0;
  if (sExecutionNum.isInteger()) {
    exeNum = sExecutionNum.getUInt();
  } else if (sExecutionNum.isString()) {
    exeNum = basics::StringUtils::uint64(sExecutionNum.copyString());
  }
  std::shared_ptr<Conductor> co = conductor(exeNum);
  if (!co) {
    if (path == Utils::finishedWorkerFinalizationPath) {
      // conductor not found, but potentially already garbage-collected
      return;
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        "Conductor not found, invalid execution number: " +
            std::to_string(exeNum));
  }

  if (path == Utils::finishedStartupPath) {
    co->finishedWorkerStartup(body);
  } else if (path == Utils::finishedWorkerStepPath) {
    outBuilder = co->finishedWorkerStep(body);
  } else if (path == Utils::finishedWorkerFinalizationPath) {
    co->finishedWorkerFinalize(body);
  } else if (path == Utils::finishedRecoveryPath) {
    co->finishedRecoveryStep(body);
  }
}

void PregelFeature::handleWorkerRequest(TRI_vocbase_t& vocbase,
                                        std::string const& path,
                                        VPackSlice const& body,
                                        VPackBuilder& outBuilder) {
  if (isStopping() && path != Utils::finalizeExecutionPath) {
    return;  // shutdown ongoing
  }

  VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);

  if (!sExecutionNum.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Worker not found, invalid execution number");
  }

  uint64_t exeNum = sExecutionNum.getUInt();

  std::shared_ptr<IWorker> w = worker(exeNum);

  // create a new worker instance if necessary
  if (path == Utils::startExecutionPath) {
    if (w) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Worker with this execution number already exists.");
    }

    addWorker(AlgoRegistry::createWorker(vocbase, body, *this), exeNum);
    worker(exeNum)->setupWorker();  // will call conductor

    return;
  } else if (path == Utils::startRecoveryPath) {
    if (!w) {
      addWorker(AlgoRegistry::createWorker(vocbase, body, *this), exeNum);
    }

    worker(exeNum)->startRecovery(body);
    return;
  } else if (!w) {
    // any other call should have a working worker instance
    if (path == Utils::finalizeExecutionPath) {
      // except this is a cleanup call, and cleanup has already happened
      // because of garbage collection
      return;
    }
    LOG_TOPIC("41788", WARN, Logger::PREGEL)
        << "Handling " << path << ", worker " << exeNum << " does not exist";
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_CURSOR_NOT_FOUND,
        "Handling request %s, but worker %lld does not exist.", path.c_str(),
        exeNum);
  }

  if (path == Utils::prepareGSSPath) {
    w->prepareGlobalStep(body, outBuilder);
  } else if (path == Utils::startGSSPath) {
    w->startGlobalStep(body);
  } else if (path == Utils::messagesPath) {
    w->receivedMessages(body);
  } else if (path == Utils::cancelGSSPath) {
    w->cancelGlobalStep(body);
  } else if (path == Utils::finalizeExecutionPath) {
    w->finalizeExecution(body, [this, exeNum]() { cleanupWorker(exeNum); });
  } else if (path == Utils::continueRecoveryPath) {
    w->compensateStep(body);
  } else if (path == Utils::finalizeRecoveryPath) {
    w->finalizeRecovery(body);
  } else if (path == Utils::aqlResultsPath) {
    bool withId = false;
    if (body.isObject()) {
      VPackSlice slice = body.get("withId");
      withId = slice.isBoolean() && slice.getBool();
    }
    w->aqlResult(outBuilder, withId);
  }
}

uint64_t PregelFeature::numberOfActiveConductors() const {
  MUTEX_LOCKER(guard, _mutex);
  uint64_t nr{0};
  for (auto const& p : _conductors) {
    std::shared_ptr<Conductor> const& c = p.second.conductor;
    if (c->_state == ExecutionState::DEFAULT ||
        c->_state == ExecutionState::RUNNING ||
        c->_state == ExecutionState::STORING) {
      ++nr;
    }
  }
  return nr;
}

Result PregelFeature::toVelocyPack(TRI_vocbase_t& vocbase,
                                   arangodb::velocypack::Builder& result,
                                   bool allDatabases, bool fanout) const {
  std::vector<std::shared_ptr<Conductor>> conductors;

  // make a copy of all conductor shared-ptrs under the mutex
  {
    MUTEX_LOCKER(guard, _mutex);
    conductors.reserve(_conductors.size());

    for (auto const& p : _conductors) {
      auto const& ce = p.second;
      if (!::authorized(ce.user)) {
        continue;
      }

      conductors.emplace_back(ce.conductor);
    }
  }

  // release lock, and now velocypackify all conductors
  result.openArray();
  for (auto const& c : conductors) {
    c->toVelocyPack(result);
  }

  Result res;

  if (ServerState::instance()->isCoordinator() && fanout) {
    // coordinator case, fan out to other coordinators!
    NetworkFeature const& nf = vocbase.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;

    network::RequestOptions options;
    options.timeout = network::Timeout(30.0);
    options.database = vocbase.name();
    options.param("local", "true");
    options.param("all", allDatabases ? "true" : "false");

    std::string const url = "/_api/control_pregel";

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      auto f = network::sendRequestRetry(
          pool, "server:" + coordinator, fuerte::RestVerb::Get, url,
          VPackBuffer<uint8_t>{}, options, ::buildHeaders());
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        auto& resp = it.get();
        res.reset(resp.combinedResult());
        if (res.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
          // it is expected in a multi-coordinator setup that a coordinator is
          // not aware of a database that was created very recently.
          res.reset();
        }
        if (res.fail()) {
          break;
        }
        auto slice = resp.slice();
        // copy results from other coordinators
        if (slice.isArray()) {
          for (auto const& entry : VPackArrayIterator(slice)) {
            result.add(entry);
          }
        }
      }
    }
  }

  result.close();

  return res;
}
