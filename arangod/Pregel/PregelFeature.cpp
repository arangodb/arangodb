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

#include "PregelFeature.h"

#include <atomic>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
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

namespace {
bool authorized(std::string const& user) {
  auto context = arangodb::ExecContext::CURRENT;
  if (context == nullptr || !arangodb::ExecContext::isAuthEnabled()) {
    return true;
  }

  if (context->isSuperuser()) {
    return true;
  }

  return (user == context->user());
}

bool authorized(
    std::pair<std::string, std::shared_ptr<arangodb::pregel::Conductor>> const&
        conductor) {
  return ::authorized(conductor.first);
}

bool authorized(
    std::pair<std::string, std::shared_ptr<arangodb::pregel::IWorker>> const&
        worker) {
  return ::authorized(worker.first);
}
}  // namespace

using namespace arangodb;
using namespace arangodb::pregel;

static PregelFeature* Instance = nullptr;

std::pair<Result, uint64_t> PregelFeature::startExecution(
    TRI_vocbase_t& vocbase, std::string algorithm,
    std::vector<std::string> const& vertexCollections,
    std::vector<std::string> const& edgeCollections, VPackSlice const& params) {
  if (nullptr == Instance) {
    return std::make_pair(
        Result{TRI_ERROR_INTERNAL, "pregel system not yet ready"}, 0);
  }
  ServerState* ss = ServerState::instance();

  // check the access rights to collections
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    TRI_ASSERT(params.isObject());
    VPackSlice storeSlice = params.get("store");
    bool storeResults = !storeSlice.isBool() || storeSlice.getBool();
    for (std::string const& vc : vertexCollections) {
      bool canWrite = exec->canUseCollection(vc, auth::Level::RW);
      bool canRead = exec->canUseCollection(vc, auth::Level::RO);
      if ((storeResults && !canWrite) || !canRead) {
        return std::make_pair(Result{TRI_ERROR_FORBIDDEN}, 0);
      }
    }
    for (std::string const& ec : edgeCollections) {
      bool canWrite = exec->canUseCollection(ec, auth::Level::RW);
      bool canRead = exec->canUseCollection(ec, auth::Level::RO);
      if ((storeResults && !canWrite) || !canRead) {
        return std::make_pair(Result{TRI_ERROR_FORBIDDEN}, 0);
      }
    }
  }

  for (std::string const& name : vertexCollections) {
    if (ss->isCoordinator()) {
      try {
        auto coll =
            ClusterInfo::instance()->getCollection(vocbase.name(), name);

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
        auto coll =
            ClusterInfo::instance()->getCollection(vocbase.name(), name);

        if (coll->system()) {
          return std::make_pair(
              Result{TRI_ERROR_BAD_PARAMETER,
                     "Cannot use pregel on system collection"},
              0);
        }

        if (!coll->isSmart()) {
          std::vector<std::string> eKeys = coll->shardKeys();
          if (eKeys.size() != 1 || eKeys[0] != "vertex") {
            return std::make_pair(Result{TRI_ERROR_BAD_PARAMETER,
                                         "Edge collection needs to be sharded "
                                         "after 'vertex', or use smart graphs"},
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

  uint64_t en = Instance->createExecutionNumber();
  auto c = std::make_unique<pregel::Conductor>(en, vocbase, vertexCollections,
                                               edgeColls, algorithm, params);
  Instance->addConductor(std::move(c), en);
  TRI_ASSERT(Instance->conductor(en));
  Instance->conductor(en)->start();

  return std::make_pair(Result{}, en);
}

uint64_t PregelFeature::createExecutionNumber() {
  return TRI_NewServerSpecificTick();
}

PregelFeature::PregelFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, "Pregel") {
  setOptional(true);
  startsAfter("V8Phase");
}

PregelFeature::~PregelFeature() {
  if (_recoveryManager) {
    _recoveryManager.reset();
  }
  cleanupAll();
}

PregelFeature* PregelFeature::instance() { return Instance; }

size_t PregelFeature::availableParallelism() {
  const size_t procNum = TRI_numberProcessors();
  return procNum < 1 ? 1 : procNum;
}

void PregelFeature::start() {
  Instance = this;
  if (ServerState::instance()->isAgent()) {
    return;
  }

  if (ServerState::instance()->isCoordinator()) {
    _recoveryManager.reset(new RecoveryManager());
  }
}

void PregelFeature::beginShutdown() {
  cleanupAll();
  Instance = nullptr;
}

void PregelFeature::addConductor(std::unique_ptr<Conductor>&& c,
                                 uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  std::string user = ExecContext::CURRENT ? ExecContext::CURRENT->user() : "";
  _conductors.emplace(
      executionNumber,
      std::make_pair(user, std::shared_ptr<Conductor>(c.get())));
  c.release();
}

std::shared_ptr<Conductor> PregelFeature::conductor(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _conductors.find(executionNumber);
  return (it != _conductors.end() && ::authorized(it->second))
             ? it->second.second
             : nullptr;
}

void PregelFeature::addWorker(std::unique_ptr<IWorker>&& w,
                              uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  std::string user = ExecContext::CURRENT ? ExecContext::CURRENT->user() : "";
  _workers.emplace(executionNumber,
                   std::make_pair(user, std::shared_ptr<IWorker>(w.get())));
  w.release();
}

std::shared_ptr<IWorker> PregelFeature::worker(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _workers.find(executionNumber);
  return (it != _workers.end() && ::authorized(it->second)) ? it->second.second
                                                            : nullptr;
}

void PregelFeature::cleanupConductor(uint64_t executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto cit = _conductors.find(executionNumber);
  if (cit != _conductors.end()) {
    _conductors.erase(executionNumber);
  }
}

void PregelFeature::cleanupWorker(uint64_t executionNumber) {
  // unmapping etc might need a few seconds
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestPriority::LOW, [this, executionNumber] {
    MUTEX_LOCKER(guard, _mutex);

    auto wit = _workers.find(executionNumber);
    if (wit != _workers.end()) {
      _workers.erase(executionNumber);
    }
    });
}

void PregelFeature::cleanupAll() {
  MUTEX_LOCKER(guard, _mutex);
  _conductors.clear();
  for (auto it : _workers) {
    it.second.second->cancelGlobalStep(VPackSlice());
  }
  std::this_thread::sleep_for(
      std::chrono::microseconds(1000 * 100));  // 100ms to send out cancel calls
  _workers.clear();
}

void PregelFeature::handleConductorRequest(std::string const& path,
                                           VPackSlice const& body,
                                           VPackBuilder& outBuilder) {
  if (SchedulerFeature::SCHEDULER->isStopping()) {
    return;  // shutdown ongoing
  }

  VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);
  if (!sExecutionNum.isInteger()) {
    LOG_TOPIC(ERR, Logger::PREGEL) << "Invalid execution number";
  }
  uint64_t exeNum = sExecutionNum.getUInt();
  std::shared_ptr<Conductor> co = Instance->conductor(exeNum);
  if (!co) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Conductor not found, invalid execution number");
  }

  if (path == Utils::finishedStartupPath) {
    co->finishedWorkerStartup(body);
  } else if (path == Utils::finishedWorkerStepPath) {
    outBuilder = co->finishedWorkerStep(body);
  } else if (path == Utils::finishedRecoveryPath) {
    co->finishedRecoveryStep(body);
  }
}

/*static*/ void PregelFeature::handleWorkerRequest(TRI_vocbase_t& vocbase,
                                                   std::string const& path,
                                                   VPackSlice const& body,
                                                   VPackBuilder& outBuilder) {
  if (SchedulerFeature::SCHEDULER->isStopping()) {
    return;  // shutdown ongoing
  }

  VPackSlice sExecutionNum = body.get(Utils::executionNumberKey);

  if (!sExecutionNum.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "Worker not found, invalid execution number");
  }

  uint64_t exeNum = sExecutionNum.getUInt();
  std::shared_ptr<IWorker> w = Instance->worker(exeNum);

  // create a new worker instance if necessary
  if (path == Utils::startExecutionPath) {
    if (w) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Worker with this execution number already exists.");
    }

    Instance->addWorker(AlgoRegistry::createWorker(vocbase, body), exeNum);
    Instance->worker(exeNum)->setupWorker();  // will call conductor

    return;
  } else if (path == Utils::startRecoveryPath) {
    if (!w) {
      Instance->addWorker(AlgoRegistry::createWorker(vocbase, body), exeNum);
    }

    Instance->worker(exeNum)->startRecovery(body);

    return;
  } else if (!w) {
    // any other call should have a working worker instance
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Handling " << path << "worker " << exeNum << " does not exist";
    THROW_ARANGO_EXCEPTION_FORMAT(
        TRI_ERROR_INTERNAL,
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
    w->finalizeExecution(body, [exeNum] {
      if (Instance != nullptr) {
        Instance->cleanupWorker(exeNum);
      }
    });
  } else if (path == Utils::continueRecoveryPath) {
    w->compensateStep(body);
  } else if (path == Utils::finalizeRecoveryPath) {
    w->finalizeRecovery(body);
  } else if (path == Utils::aqlResultsPath) {
    w->aqlResult(outBuilder);
  }
}
