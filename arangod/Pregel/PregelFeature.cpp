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
#include <unordered_set>
#include <variant>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberOfCores.h"
#include "Basics/ResultT.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "ExecutionNumber.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker/Worker.h"
#include "Pregel/WorkerConductorMessages.h"
#include "RestServer/DatabasePathFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "velocypack/Builder.h"

using namespace arangodb;
using namespace arangodb::options;
using namespace arangodb::pregel;

namespace {

// locations for Pregel's temporary files
std::unordered_set<std::string> const tempLocationTypes{
    {"temp-directory", "database-directory", "custom"}};

size_t availableCores() {
  return std::max<size_t>(1, NumberOfCores::getValue());
}

size_t defaultParallelism() {
  size_t procNum = std::min<size_t>(availableCores() / 4, 16);
  return procNum < 1 ? 1 : procNum;
}

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

}  // namespace

using namespace arangodb;

ResultT<ExecutionNumber> PregelFeature::startExecution(
    TRI_vocbase_t& vocbase, std::string algorithm,
    std::vector<std::string> const& vertexCollections,
    std::vector<std::string> const& edgeCollections,
    std::unordered_map<std::string, std::vector<std::string>> const&
        edgeCollectionRestrictions,
    VPackSlice const& params) {
  if (isStopping() || _softShutdownOngoing.load(std::memory_order_relaxed)) {
    return ResultT<ExecutionNumber>::error(TRI_ERROR_SHUTTING_DOWN,
                                           "pregel system not available");
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
        return ResultT<ExecutionNumber>::error(TRI_ERROR_FORBIDDEN);
      }
    }
    for (std::string const& ec : edgeCollections) {
      bool canWrite = exec.canUseCollection(ec, auth::Level::RW);
      bool canRead = exec.canUseCollection(ec, auth::Level::RO);
      if ((storeResults && !canWrite) || !canRead) {
        return ResultT<ExecutionNumber>::error(TRI_ERROR_FORBIDDEN);
      }
    }
  }

  for (std::string const& name : vertexCollections) {
    if (ss->isCoordinator()) {
      try {
        auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
        auto coll = ci.getCollection(vocbase.name(), name);

        if (coll->system()) {
          return ResultT<ExecutionNumber>::error(
              TRI_ERROR_BAD_PARAMETER,
              "Cannot use pregel on system collection");
        }

        if (coll->status() == TRI_VOC_COL_STATUS_DELETED || coll->deleted()) {
          return ResultT<ExecutionNumber>::error(
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
        }
      } catch (...) {
        return ResultT<ExecutionNumber>::error(
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
      }
    } else if (ss->getRole() == ServerState::ROLE_SINGLE) {
      auto coll = vocbase.lookupCollection(name);

      if (coll == nullptr || coll->status() == TRI_VOC_COL_STATUS_DELETED ||
          coll->deleted()) {
        return ResultT<ExecutionNumber>::error(
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
      }
    } else {
      return ResultT<ExecutionNumber>::error(TRI_ERROR_INTERNAL);
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
          return ResultT<ExecutionNumber>::error(
              TRI_ERROR_BAD_PARAMETER,
              "Cannot use pregel on system collection");
        }

        if (!coll->isSmart()) {
          std::vector<std::string> eKeys = coll->shardKeys();

          std::string shardKeyAttribute = "vertex";
          if (params.hasKey("shardKeyAttribute")) {
            shardKeyAttribute = params.get("shardKeyAttribute").copyString();
          }

          if (eKeys.size() != 1 || eKeys[0] != shardKeyAttribute) {
            return ResultT<ExecutionNumber>::error(
                TRI_ERROR_BAD_PARAMETER,
                "Edge collection needs to be sharded "
                "by shardKeyAttribute parameter ('" +
                    shardKeyAttribute +
                    "'), or use SmartGraphs. The current shardKey is: " +
                    (eKeys.empty() ? "undefined" : "'" + eKeys[0] + "'"));
          }
        }

        if (coll->status() == TRI_VOC_COL_STATUS_DELETED || coll->deleted()) {
          return ResultT<ExecutionNumber>::error(
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
        }

        // smart edge collections contain multiple actual collections
        std::vector<std::string> actual = coll->realNamesForRead();

        edgeColls.insert(edgeColls.end(), actual.begin(), actual.end());
      } catch (...) {
        return ResultT<ExecutionNumber>::error(
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
      }
    } else if (ss->getRole() == ServerState::ROLE_SINGLE) {
      auto coll = vocbase.lookupCollection(name);

      if (coll == nullptr || coll->deleted()) {
        return ResultT<ExecutionNumber>::error(
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, name);
      }
      std::vector<std::string> actual = coll->realNamesForRead();
      edgeColls.insert(edgeColls.end(), actual.begin(), actual.end());
    } else {
      return ResultT<ExecutionNumber>::error(TRI_ERROR_INTERNAL);
    }
  }

  auto en = createExecutionNumber();
  auto c = std::make_shared<pregel::Conductor>(
      en, vocbase, vertexCollections, edgeColls, edgeCollectionRestrictions,
      algorithm, params, *this);
  addConductor(std::move(c), en);
  TRI_ASSERT(conductor(en));
  conductor(en)->start();

  return ResultT<ExecutionNumber>::success(en);
}

ExecutionNumber PregelFeature::createExecutionNumber() {
  return ExecutionNumber(TRI_NewServerSpecificTick());
}

PregelFeature::PregelFeature(Server& server)
    : ArangodFeature{server, *this},
      _defaultParallelism(::defaultParallelism()),
      _minParallelism(1),
      _maxParallelism(::availableCores()),
      _tempLocationType("temp-directory"),
      _useMemoryMaps(true),
      _softShutdownOngoing(false),
      _metrics(std::make_shared<PregelMetrics>(
          server.getFeature<metrics::MetricsFeature>())) {
  static_assert(
      Server::isCreatedAfter<PregelFeature, metrics::MetricsFeature>());
  setOptional(true);
  startsAfter<DatabaseFeature>();
  startsAfter<application_features::V8FeaturePhase>();
}

PregelFeature::~PregelFeature() {
  TRI_ASSERT(_conductors.empty());
  TRI_ASSERT(_workers.empty());
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

void PregelFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("pregel", "Pregel jobs");

  options
      ->addOption(
          "--pregel.parallelism",
          "default parallelism to use in a Pregel job if none is specified",
          new SizeTParameter(&_defaultParallelism),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Dynamic,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--pregel.min-parallelism",
                  "minimum parallelism usable in a Pregel job",
                  new SizeTParameter(&_minParallelism),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--pregel.max-parallelism",
                  "maximum parallelism usable in a Pregel job",
                  new SizeTParameter(&_maxParallelism),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--pregel.memory-mapped-files",
                  "use memory mapped files for storing Pregel temporary data "
                  "(as opposed "
                  "to storing in RAM) if nothing is specifed in a Pregel job",
                  new BooleanParameter(&_useMemoryMaps),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--pregel.memory-mapped-files-location-type",
                  "location for Pregel's temporary files",
                  new DiscreteValuesParameter<StringParameter>(
                      &_tempLocationType, ::tempLocationTypes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--pregel.memory-mapped-files-custom-path",
                  "Custom path for Pregel's temporary files. Only used if "
                  "`--pregel.memory-mapped-files-location` is \"custom\".",
                  new StringParameter(&_tempLocationCustomPath),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);
}

void PregelFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_tempLocationCustomPath.empty() && _tempLocationType != "custom") {
    LOG_TOPIC("0dd1d", FATAL, Logger::PREGEL)
        << "invalid settings for Pregel's temporary files: if a custom path "
           "is "
           "provided, "
        << "`--pregel.memory-mapped-files-location-type` must have a value "
           "of "
           "'custom'";
    FATAL_ERROR_EXIT();
  } else if (_tempLocationCustomPath.empty() && _tempLocationType == "custom") {
    LOG_TOPIC("9b378", FATAL, Logger::PREGEL)
        << "invalid settings for Pregel's temporary files: if "
           "`--pregel.memory-mapped-files-location-type` is 'custom', a "
           "custom "
           "directory must be provided via "
           "`--pregel.memory-mapped-files-custom-path`";
    FATAL_ERROR_EXIT();
  }

  if (_minParallelism > _maxParallelism ||
      _defaultParallelism < _minParallelism ||
      _defaultParallelism > _maxParallelism || _minParallelism == 0 ||
      _maxParallelism == 0) {
    // parallelism values look somewhat odd in relation to each other. fix
    // them and issue a warning about it.
    _minParallelism = std::max<size_t>(1, _minParallelism);
    _maxParallelism = std::max<size_t>(_minParallelism, _maxParallelism);
    _defaultParallelism = std::clamp<size_t>(_defaultParallelism,
                                             _minParallelism, _maxParallelism);

    LOG_TOPIC("5a607", WARN, Logger::PREGEL)
        << "invalid values for Pregel paralellism values. adjusting them to: "
           "min: "
        << _minParallelism << ", max: " << _maxParallelism
        << ", default: " << _defaultParallelism;
  }

  TRI_ASSERT(::tempLocationTypes.contains(_tempLocationType));

  // these assertions should always hold
  TRI_ASSERT(_minParallelism > 0 && _minParallelism <= _maxParallelism);
  TRI_ASSERT(_defaultParallelism > 0 &&
             _defaultParallelism >= _minParallelism &&
             _defaultParallelism <= _maxParallelism);
}

void PregelFeature::start() {
  std::string tempDirectory = tempPath();

  if (!tempDirectory.empty()) {
    TRI_ASSERT(_tempLocationType == "custom" ||
               _tempLocationType == "database-directory");

    // if the target directory for temporary files does not yet exist, create
    // it on the fly! in case we want the temporary files to be created
    // underneath the database's data directory, create the directory once. if
    // a custom temporary directory was given, we can assume it to be
    // reasonably stable across restarts, so it is fine to create it. if we
    // want to store temporary files in the temporary directory, we should not
    // create it upon startup, simply because the temporary directory can
    // change with every instance start.
    if (!basics::FileUtils::isDirectory(tempDirectory)) {
      std::string systemErrorStr;
      long errorNo;

      auto res = TRI_CreateRecursiveDirectory(tempDirectory.c_str(), errorNo,
                                              systemErrorStr);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC("eb2da", FATAL, arangodb::Logger::PREGEL)
            << "unable to create directory for Pregel temporary files '"
            << tempDirectory << "': " << systemErrorStr;
        FATAL_ERROR_EXIT();
      }
    } else {
      // temp directory already existed at startup.
      // now, if it is underneath the database path, we own it and can
      // wipe its contents. if it is not underneath the database path,
      // we cannot assume ownership for the files in it and better leave
      // the files alone.
      if (_tempLocationType == "database-directory") {
        auto files = basics::FileUtils::listFiles(tempDirectory);
        for (auto const& f : files) {
          std::string fqn = basics::FileUtils::buildFilename(tempDirectory, f);

          LOG_TOPIC("876fd", INFO, Logger::PREGEL)
              << "removing Pregel temporary file '" << fqn << "' at startup";

          ErrorCode res = basics::FileUtils::remove(fqn);

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_TOPIC("cae59", INFO, Logger::PREGEL)
                << "unable to remove Pregel temporary file '" << fqn
                << "': " << TRI_last_error();
          }
        }
      }
    }
  }

  LOG_TOPIC("a0eb6", DEBUG, Logger::PREGEL)
      << "using Pregel default parallelism " << _defaultParallelism
      << " (min: " << _minParallelism << ", max: " << _maxParallelism << ")"
      << ", memory mapping: " << (_useMemoryMaps ? "on" : "off")
      << ", temp path: " << tempDirectory;

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

std::string PregelFeature::tempPath() const {
  if (_tempLocationType == "database-directory") {
    auto& databasePathFeature = server().getFeature<DatabasePathFeature>();
    return databasePathFeature.subdirectoryName("pregel");
  }
  if (_tempLocationType == "custom") {
    TRI_ASSERT(!_tempLocationCustomPath.empty());
    return _tempLocationCustomPath;
  }

  TRI_ASSERT(_tempLocationType == "temp-directory");
  return "";
}

size_t PregelFeature::defaultParallelism() const noexcept {
  return _defaultParallelism;
}

size_t PregelFeature::minParallelism() const noexcept {
  return _minParallelism;
}

size_t PregelFeature::maxParallelism() const noexcept {
  return _maxParallelism;
}

bool PregelFeature::useMemoryMaps() const noexcept { return _useMemoryMaps; }

void PregelFeature::addConductor(std::shared_ptr<Conductor>&& c,
                                 ExecutionNumber executionNumber) {
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

std::shared_ptr<Conductor> PregelFeature::conductor(
    ExecutionNumber executionNumber) {
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
    auto executionNumber = c->executionNumber();

    _conductors.erase(executionNumber);
    _workers.erase(executionNumber);
  }
} catch (...) {
}

void PregelFeature::addWorker(std::shared_ptr<IWorker>&& w,
                              ExecutionNumber executionNumber) {
  if (worker(executionNumber)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Worker with this execution number already exists.");
  }
  if (isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  std::string user = ExecContext::current().user();
  MUTEX_LOCKER(guard, _mutex);
  _workers.try_emplace(executionNumber, std::move(user), std::move(w));
}

std::shared_ptr<IWorker> PregelFeature::worker(
    ExecutionNumber executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  auto it = _workers.find(executionNumber);
  return (it != _workers.end() && ::authorized(it->second.first))
             ? it->second.second
             : nullptr;
}

void PregelFeature::cleanupConductor(ExecutionNumber executionNumber) {
  MUTEX_LOCKER(guard, _mutex);
  _conductors.erase(executionNumber);
  _workers.erase(executionNumber);
}

void PregelFeature::cleanupWorker(ExecutionNumber executionNumber) {
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

  if (path != Utils::modernMessagingPath) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format("Conductor path not found: {}", path));
  }

  auto message = deserialize<ModernMessage>(body);
  auto c = conductor(message.executionNumber);
  if (!c && std::holds_alternative<CleanupFinished>(message.payload)) {
    // conductor not found, but potentially already garbage-collected
    return;
  }
  if (!c) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format("Conductor not found, invalid execution number: {}",
                    message.executionNumber));
  }
  auto response = c->process(message.payload);
  if (response.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format("Execution {}: {}: {}", message.executionNumber,
                    response.errorMessage(), body.toJson()));
  }
  return;
}

void PregelFeature::handleWorkerRequest(TRI_vocbase_t& vocbase,
                                        std::string const& path,
                                        VPackSlice const& body,
                                        VPackBuilder& outBuilder) {
  if (path != Utils::modernMessagingPath) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format("Worker path not found: {}", path));
  }

  auto message = deserialize<ModernMessage>(body);
  if (std::holds_alternative<LoadGraph>(message.payload)) {
    addWorker(AlgoRegistry::createWorker(
                  vocbase, std::get<LoadGraph>(message.payload).details.slice(),
                  *this),
              message.executionNumber);
  }
  auto w = worker(message.executionNumber);
  if (std::holds_alternative<StartCleanup>(message.payload)) {
    if (isStopping()) {
      return;  // shutdown ongoing
    }
    if (!w) {
      // except this is a cleanup call, and cleanup has already happened
      // because of garbage collection
      auto response = ModernMessage{.executionNumber = message.executionNumber,
                                    .payload = {CleanupStarted{}}};
      serialize(outBuilder, response);
      return;
    }
  }
  if (!w) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format(
            "Handling request {} but worker for execution {} does not exist",
            body.toJson(), message.executionNumber));
  }
  auto response = w->process(message.payload).get();
  if (response.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format("Execution {}: {}: {}", message.executionNumber,
                    response.errorMessage(), body.toJson()));
  }
  serialize(outBuilder, response.get());
  return;
}

uint64_t PregelFeature::numberOfActiveConductors() const {
  MUTEX_LOCKER(guard, _mutex);
  uint64_t nr{0};
  for (auto const& p : _conductors) {
    auto const& c = p.second.conductor;
    if (c->state->isRunning()) {
      ++nr;
      LOG_TOPIC("41564", WARN, Logger::PREGEL)
          << fmt::format("Conductor for executionNumber {} is in state {}.",
                         c->_executionNumber, c->state->name());
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
