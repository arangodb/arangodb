////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include <velocypack/SharedSlice.h>

#include <atomic>
#include <unordered_set>

#include "Actor/DistributedActorPID.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Graph/GraphManager.h"
#include "Inspection/VPackWithErrorT.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Actor.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/Conductor/Messages.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/GraphStore/GraphSerdeConfig.h"
#include "Pregel/GraphStore/GraphSerdeConfigBuilder.h"
#include "Pregel/GraphStore/GraphSourceToGraphByCollectionsResolver.h"
#include "Pregel/StatusActor.h"
#include "Pregel/PregelOptions.h"
#include "Pregel/ResultActor.h"
#include "Pregel/ResultMessages.h"
#include "Pregel/SpawnActor.h"
#include "Pregel/MetricsActor.h"
#include "Pregel/StatusWriter/CollectionStatusWriter.h"
#include "Pregel/StatusWriter/StatusEntry.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/Worker.h"
#include "Rest/CommonDefines.h"
#include "RestServer/DatabasePathFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "Metrics/MetricsFeature.h"

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

}  // namespace

PregelScheduler::PregelScheduler(Scheduler* scheduler) : _scheduler{scheduler} {
  TRI_ASSERT(_scheduler != nullptr);
}

void PregelScheduler::queue(actor::LazyWorker&& worker) {
  _scheduler->queue(RequestLane::INTERNAL_LOW, std::move(worker));
}

void PregelScheduler::delay(std::chrono::seconds delay,
                            std::function<void(bool)>&& fn) {
  std::ignore = _scheduler->queueDelayed("pregel-actors",
                                         RequestLane::INTERNAL_LOW, delay, fn);
}

auto PregelRunUser::authorized(ExecContext const& userContext) const -> bool {
  if (userContext.isSuperuser()) {
    return true;
  }
  return name == userContext.user();
}

Result PregelFeature::persistExecution(TRI_vocbase_t& vocbase,
                                       ExecutionNumber en) {
  statuswriter::CollectionStatusWriter cWriter{vocbase, en};

  // TODO: Here we should also write the Coordinator's ServerID into the
  // collection
  auto serialized = inspection::serializeWithErrorT(PregelCollectionEntry{
      .serverId = ServerState::instance()->getId(), .executionNumber = en});

  if (!serialized.ok()) {
    return Result{TRI_ERROR_INTERNAL, serialized.error().error()};
  }

  auto storeResult = cWriter.createResult(serialized.get().slice());
  if (storeResult.ok()) {
    LOG_TOPIC("a63f1", INFO, Logger::PREGEL) << fmt::format(
        "[ExecutionNumber {}] Created pregel execution entry in {}", en,
        StaticStrings::PregelCollection);
    return {};
  } else {
    LOG_TOPIC("063f2", WARN, Logger::PREGEL) << fmt::format(
        "[ExecutionNumber {}] Failed to create pregel execution entry in {}, "
        "message {}",
        en, StaticStrings::PregelCollection, storeResult.errorMessage());
    return Result{TRI_ERROR_INTERNAL, storeResult.errorMessage()};
  }
}

ResultT<ExecutionNumber> PregelFeature::startExecution(TRI_vocbase_t& vocbase,
                                                       PregelOptions options) {
  if (isStopping() || _softShutdownOngoing.load(std::memory_order_relaxed)) {
    return Result{TRI_ERROR_SHUTTING_DOWN, "pregel system not available"};
  }

  auto storeSlice = options.userParameters.slice().get("store");
  auto wantToStoreResults = !storeSlice.isBool() || storeSlice.getBool();

  auto shardKeyAttribute =
      options.userParameters.slice().hasKey("shardKeyAttribute")
          ? options.userParameters.slice().get("shardKeyAttribute").copyString()
          : "vertex";

  auto maxSuperstep = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      options.userParameters.slice(), Utils::maxGSS, 500);
  // If the user sets maxNumIterations, we just do as many supersteps
  // as necessary to please the algorithm
  if (options.userParameters.slice().hasKey(Utils::maxNumIterations)) {
    maxSuperstep = std::numeric_limits<uint64_t>::max();
  }

  auto parallelismVar = parallelism(options.userParameters.slice());
  auto ttl = TTL{.duration = std::chrono::seconds(
                     basics::VelocyPackHelper::getNumericValue(
                         options.userParameters.slice(), "ttl", 600))};

  auto algorithmName = std::move(options.algorithm);
  std::transform(algorithmName.begin(), algorithmName.end(),
                 algorithmName.begin(), ::tolower);

  /* resolve the graph input parameters to a struct that
   * contains the collection names for vertices and edges
   * and the positive list of restrictions of vertex collections
   * to edge collections
   */
  auto maybeGraphByCollections = resolveGraphSourceToGraphByCollections(
      vocbase, options.graphSource, shardKeyAttribute);
  if (!maybeGraphByCollections.ok()) {
    return maybeGraphByCollections.error();
  }
  auto graphByCollections = maybeGraphByCollections.get();

  // check the access rights to collections (yes, really)
  ExecContext const& exec = ExecContext::current();
  auto permissionsGranted =
      checkUserPermissions(exec, graphByCollections, wantToStoreResults);
  if (!permissionsGranted.ok()) {
    return permissionsGranted;
  }

  auto maybeGraphSerdeConfig =
      buildGraphSerdeConfig(vocbase, graphByCollections);
  if (!maybeGraphSerdeConfig.ok()) {
    return maybeGraphSerdeConfig.error();
  }
  auto graphSerdeConfig = maybeGraphSerdeConfig.get();

  auto en = createExecutionNumber();

  auto persistResult = persistExecution(vocbase, en);
  if (!persistResult.ok()) {
    return persistResult;
  }

  auto executionSpecifications = ExecutionSpecifications{
      .executionNumber = en,
      .algorithm = std::move(algorithmName),
      .graphSerdeConfig = std::move(graphSerdeConfig),
      .maxSuperstep = maxSuperstep,
      .storeResults = wantToStoreResults,
      .ttl = ttl,
      .parallelism = parallelismVar,
      .userParameters = std::move(options.userParameters)};

  if (options.useActors) {
    auto server = ServerState::instance()->getId();
    auto statusStart = message::StatusMessages{message::StatusStart{
        .state = "Execution Started",
        .id = executionSpecifications.executionNumber,
        .user = ExecContext::current().user(),
        .database = vocbase.name(),
        .algorithm = executionSpecifications.algorithm,
        .ttl = executionSpecifications.ttl,
        .parallelism = executionSpecifications.parallelism}};
    auto statusActorPID = _actorRuntime->spawn<StatusActor>(
        std::make_unique<StatusState>(vocbase), std::move(statusStart));

    auto metricsActorPID = _actorRuntime->spawn<MetricsActor>(
        std::make_unique<MetricsState>(_metrics),
        metrics::message::MetricsStart{});

    auto resultState = std::make_unique<ResultState>(ttl);
    auto resultData = resultState->data;
    auto resultActorPID = _actorRuntime->spawn<ResultActor>(
        std::move(resultState),
        message::ResultMessages{message::ResultStart{}});

    auto spawnActor = _actorRuntime->spawn<SpawnActor>(
        std::make_unique<SpawnState>(vocbase, resultActorPID),
        message::SpawnMessages{message::SpawnStart{}});
    auto algorithm = AlgoRegistry::createAlgorithmNew(
        executionSpecifications.algorithm,
        executionSpecifications.userParameters.slice());
    if (not algorithm.has_value()) {
      return Result{TRI_ERROR_BAD_PARAMETER,
                    fmt::format("Unsupported Algorithm: {}",
                                executionSpecifications.algorithm)};
    }
    auto conductorActorPID = _actorRuntime->spawn<conductor::ConductorActor>(
        std::make_unique<conductor::ConductorState>(
            std::move(algorithm.value()), executionSpecifications,
            std::move(spawnActor), std::move(resultActorPID),
            std::move(statusActorPID), std::move(metricsActorPID)),
        conductor::message::ConductorStart{});

    _pregelRuns.doUnderLock([&](auto& actors) {
      actors.emplace(
          en, PregelRun{PregelRunUser(ExecContext::current().user()),
                        PregelRunActors{.resultActor = resultActorPID,
                                        .results = resultData,
                                        .conductor = conductorActorPID}});
    });

    return en;
  } else {
    auto c = std::make_shared<pregel::Conductor>(
        executionSpecifications, ExecContext::current().user(), vocbase, *this);
    addConductor(std::move(c), en);
    TRI_ASSERT(conductor(en));
    conductor(en)->start();
    return en;
  }
}

ExecutionNumber PregelFeature::createExecutionNumber() {
  return ExecutionNumber{TRI_NewServerSpecificTick()};
}

PregelFeature::PregelFeature(Server& server)
    : ArangodFeature{server, *this},
      _defaultParallelism(::defaultParallelism()),
      _minParallelism(1),
      _maxParallelism(::availableCores()),
      _softShutdownOngoing(false),
      _metrics(std::make_shared<PregelMetrics>(
          server.getFeature<arangodb::metrics::MetricsFeature>())),
      _actorRuntime(nullptr) {
  static_assert(Server::isCreatedAfter<PregelFeature,
                                       arangodb::metrics::MetricsFeature>());

  setOptional(true);
  startsAfter<DatabaseFeature>();
#ifdef USE_V8
  startsAfter<application_features::V8FeaturePhase>();
#else
  startsAfter<application_features::ClusterFeaturePhase>();
#endif
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
  auto handle = scheduler->queueDelayed("pregel-gc", RequestLane::INTERNAL_LOW,
                                        offset, [this](bool canceled) {
                                          if (!canceled) {
                                            garbageCollectConductors();
                                            garbageCollectActors();
                                            scheduleGarbageCollection();
                                          }
                                        });

  std::lock_guard guard{_mutex};
  _gcHandle = std::move(handle);
}

void PregelFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("pregel", "Pregel jobs");

  options
      ->addOption("--pregel.parallelism",
                  "The default parallelism to use in a Pregel job if none is "
                  "specified.",
                  new SizeTParameter(&_defaultParallelism),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(R"(The default parallelism for a Pregel job is only
used if you start a job without setting the `parallelism` attribute.

Defaults to the number of available cores divided by 4. The result is limited to
a value between 1 and 16.)");

  options
      ->addOption("--pregel.min-parallelism",
                  "The minimum parallelism usable in a Pregel job.",
                  new SizeTParameter(&_minParallelism),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(R"(Increasing the value of this option forces each
Pregel job to run with at least this level of parallelism. In a cluster
deployment, the limit applies per DB-Server.)");

  options
      ->addOption("--pregel.max-parallelism",
                  "The maximum parallelism usable in a Pregel job.",
                  new SizeTParameter(&_maxParallelism),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(R"(This option effectively limits the parallelism of
each Pregel job to the specified value. In a cluster deployment, the limit
applies per DB-Server.

Defaults to the number of available cores.)");

  options
      ->addObsoleteOption(
          "--pregel.memory-mapped-files",
          "Whether to use memory mapped files for storing Pregel "
          "temporary data (as opposed to storing it in RAM) by default.",
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(R"(If set to `true`, Pregel jobs store their
temporary data in disk-backed memory-mapped files. If set to `false`, the
temporary data of Pregel jobs is buffered in main memory.

Memory-mapped files are used by default. This has the advantage of a lower RAM
utilization, which reduces the likelihood of out-of-memory situations. However,
storing the files on disk requires a certain disk capacity, so that instead of
running out of RAM, it is possible to run out of a disk space. Make sure to use
a suitable storage location.

You can override this option for each Pregel job by setting the `useMemoryMaps`
attribute of the job.)");

  options
      ->addObsoleteOption("--pregel.memory-mapped-files-location-type",
                          "The location for Pregel's temporary files.",
                          arangodb::options::makeFlags(
                              arangodb::options::Flags::DefaultNoComponents,
                              arangodb::options::Flags::OnDBServer,
                              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(R"(You can configure the location for the
memory-mapped files written by Pregel with this option. This option is only
meaningful if you use memory-mapped files.

The option can have one of the following values:

- `temp-directory`: store memory-mapped files in the temporary directory, as
  configured via `--temp.path`. If `--temp.path` is not set, the system's
  temporary directory is used.
- `database-directory`: store memory-mapped files in a separate directory
  underneath the database directory.
- `custom`: use a custom directory location for memory-mapped files. You can set
  the location via the `--pregel.memory-mapped-files-custom-path` option.

The default location for Pregel's memory-mapped files is the temporary directory
(`--temp.path`), which may not provide enough capacity for larger Pregel jobs.
It may be more sensible to configure a custom directory for memory-mapped files
and provide the necessary disk space there (`custom`).
Such custom directory can be mounted on ephemeral storage, as the files are only
needed temporarily. If a custom directory location is used, you need to specify
the actual location via the `--pregel.memory-mapped-files-custom-path`
parameter.

You can also use a subdirectory of the database directory as the storage
location for the memory-mapped files (`--database.directory`). The database
directory often provides a lot of disk space capacity, but when Pregel's
temporary files are stored in there too, it has to provide enough capacity to
store both the regular database data and the Pregel files.)");

  options
      ->addObsoleteOption(
          "--pregel.memory-mapped-files-custom-path",
          "Custom path for Pregel's temporary files. Only used if "
          "`--pregel.memory-mapped-files-location` is \"custom\".",
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(R"(If you use this option, you need to specify the
storage directory location as an absolute path.)");
}

void PregelFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
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

  // these assertions should always hold
  TRI_ASSERT(_minParallelism > 0 && _minParallelism <= _maxParallelism);
  TRI_ASSERT(_defaultParallelism > 0 &&
             _defaultParallelism >= _minParallelism &&
             _defaultParallelism <= _maxParallelism);
}

void PregelFeature::start() {
  LOG_TOPIC("a0eb6", DEBUG, Logger::PREGEL)
      << "using Pregel default parallelism " << _defaultParallelism
      << " (min: " << _minParallelism << ", max: " << _maxParallelism << ")";

  if (!ServerState::instance()->isAgent()) {
    scheduleGarbageCollection();
  }

  // TODO needs to go here for now because server feature has not startd in
  // pregel feature constructor
  _actorRuntime = std::make_shared<actor::DistributedRuntime>(
      ServerState::instance()->getId(), "PregelFeature",
      std::make_shared<PregelScheduler>(SchedulerFeature::SCHEDULER),
      std::make_shared<ArangoExternalDispatcher>(
          "/_api/pregel/actor", server().getFeature<NetworkFeature>().pool(),
          network::Timeout{5.0 * 60}));
}

void PregelFeature::beginShutdown() {
  // Copy the _conductors and _workers maps here, because in the case of a
  // single server there is a lock order inversion.  This is because the
  // conductor code directly calls back into the feature while holding the
  // _callbackMutex.  At the same time there is code that calls into the feature
  // trying to acquire _mutex while holding _callbackMutex.
  //
  // This code will go away as soon as the actor code is used.
  std::unique_lock guard{_mutex};
  _gcHandle.reset();

  for (auto& it : _conductors) {
    it.second.conductor->_shutdown = true;
  }

  auto cs = _conductors;
  auto ws = _workers;
  guard.unlock();

  // cancel all conductors and workers
  for (auto& it : cs) {
    try {
      it.second.conductor->cancel();
    } catch (std::exception const& ex) {
      // if an exception happens here, log it, but continue with
      // the garbage collection. this is important, so that we
      // can eventually get rid of some leftover conductors.
      LOG_TOPIC("aaa06", INFO, Logger::PREGEL)
          << "unable to cancel conductor during shutdown: " << ex.what();
    }
  }
  for (auto it : ws) {
    it.second.second->cancelGlobalStep(VPackSlice());
  }
}

void PregelFeature::stop() {
  // garbage collect conductors here, because it may be too
  // late for garbage collection during unprepare().
  // during unprepare() we are not allowed to post further items
  // onto the scheduler anymore, but the garbage collection
  // can post onto the scheduler.
  garbageCollectConductors();
}

void PregelFeature::unprepare() {
  // TODO: this may trigger an assertion failure in maintainer
  // mode, because it is not allowed to post to the scheduler
  // during unprepare() anymore. we are working around this by
  // trying to garbage-collection in the stop() phase already.
  garbageCollectConductors();

  std::unique_lock guard{_mutex};
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

size_t PregelFeature::defaultParallelism() const noexcept {
  return _defaultParallelism;
}

size_t PregelFeature::minParallelism() const noexcept {
  return _minParallelism;
}

size_t PregelFeature::maxParallelism() const noexcept {
  return _maxParallelism;
}

size_t PregelFeature::parallelism(VPackSlice params) const noexcept {
  size_t parallelism = defaultParallelism();
  if (params.isObject()) {
    // then update parallelism value from user config
    if (VPackSlice parallel = params.get(Utils::parallelismKey);
        parallel.isInteger()) {
      // limit parallelism to configured bounds
      parallelism = std::clamp(parallel.getNumber<size_t>(), minParallelism(),
                               maxParallelism());
    }
  }
  return parallelism;
}

void PregelFeature::addConductor(std::shared_ptr<Conductor>&& c,
                                 ExecutionNumber executionNumber) {
  if (isStopping() || _softShutdownOngoing.load(std::memory_order_relaxed)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  std::string user = c->_user;
  std::lock_guard guard{_mutex};
  _conductors.try_emplace(
      executionNumber,
      ConductorEntry{std::move(user), std::chrono::steady_clock::time_point{},
                     std::move(c)});
}

std::shared_ptr<Conductor> PregelFeature::conductor(
    ExecutionNumber executionNumber) {
  std::lock_guard guard{_mutex};
  auto it = _conductors.find(executionNumber);
  return (it != _conductors.end() && ::authorized(it->second.user))
             ? it->second.conductor
             : nullptr;
}

void PregelFeature::garbageCollectActors() {
  // clean up map
  _pregelRuns.doUnderLock([this](auto& items) {
    std::erase_if(items, [this](auto& item) {
      auto const& [_, run] = item;
      auto actors = run.getActorsInternally();
      return not _actorRuntime->contains(actors.resultActor.id) &&
             (actors.conductor == std::nullopt ||
              not _actorRuntime->contains(actors.conductor.value().id));
    });
  });
}

void PregelFeature::garbageCollectConductors() try {
  // iterate over all conductors and remove the ones which can be
  // garbage-collected
  std::vector<std::shared_ptr<Conductor>> conductors;

  // copy out shared-ptrs of Conductors under the mutex
  {
    std::lock_guard guard{_mutex};
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
    try {
      c->cancel();
    } catch (std::exception const& ex) {
      // if an exception happens here, log it, but continue with
      // the garbage collection. this is important, so that we
      // can eventually get rid of some leftover conductors.
      LOG_TOPIC("517bb", INFO, Logger::PREGEL)
          << "Unable to cancel conductor for garbage-collection: " << ex.what();
    }
  }

  std::lock_guard guard{_mutex};
  for (auto& c : conductors) {
    ExecutionNumber executionNumber = c->executionNumber();

    _conductors.erase(executionNumber);
    _workers.erase(executionNumber);
  }
} catch (...) {
}

void PregelFeature::addWorker(std::shared_ptr<IWorker>&& w,
                              ExecutionNumber executionNumber) {
  if (isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  std::string user = ExecContext::current().user();
  std::lock_guard guard{_mutex};
  _workers.try_emplace(executionNumber, std::move(user), std::move(w));
}

std::shared_ptr<IWorker> PregelFeature::worker(
    ExecutionNumber executionNumber) {
  std::lock_guard guard{_mutex};
  auto it = _workers.find(executionNumber);
  return (it != _workers.end() && ::authorized(it->second.first))
             ? it->second.second
             : nullptr;
}

ResultT<PregelResults> PregelFeature::getResults(ExecutionNumber execNr) {
  return _pregelRuns.doUnderLock(
      [&execNr](auto const& items) -> ResultT<PregelResults> {
        auto item = items.find(execNr);
        if (item == items.end()) {
          return Result{
              TRI_ERROR_HTTP_NOT_FOUND,
              fmt::format("Cannot locate results for pregel run {}.", execNr)};
        }
        auto const& [_, run] = *item;
        if (auto actors = run.getActorsFromUser(ExecContext::current());
            actors != std::nullopt) {
          auto results = actors.value().results->get();
          if (!results.has_value()) {
            return Result{
                TRI_ERROR_INTERNAL,
                fmt::format("Pregel results for run {} are not yet available.",
                            execNr)};
          }
          return results.value();
        }
        return Result{TRI_ERROR_HTTP_UNAUTHORIZED, "User is not authorized."};
      });
}

void PregelFeature::cleanupConductor(ExecutionNumber executionNumber) {
  std::lock_guard guard{_mutex};
  _conductors.erase(executionNumber);
  _workers.erase(executionNumber);
}

void PregelFeature::cleanupWorker(ExecutionNumber executionNumber) {
  // unmapping etc. might need a few seconds
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestLane::INTERNAL_LOW, [this, executionNumber] {
    std::lock_guard guard{_mutex};
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
  auto exeNum = ExecutionNumber{0};
  if (sExecutionNum.isInteger()) {
    exeNum = ExecutionNumber{sExecutionNum.getUInt()};
  } else if (sExecutionNum.isString()) {
    exeNum = ExecutionNumber{
        basics::StringUtils::uint64(sExecutionNum.copyString())};
  }
  std::shared_ptr<Conductor> co = conductor(exeNum);
  if (!co) {
    if (path == Utils::finishedWorkerFinalizationPath) {
      // conductor not found, but potentially already garbage-collected
      return;
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_CURSOR_NOT_FOUND,
        fmt::format("Conductor not found, invalid execution number: {}",
                    exeNum));
  }

  if (path == Utils::statusUpdatePath) {
    auto message = inspection::deserializeWithErrorT<StatusUpdated>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize StatusUpdated message: {}",
                      message.error().error()));
    }
    co->workerStatusUpdate(std::move(message.get()));
  } else if (path == Utils::finishedStartupPath) {
    auto message = inspection::deserializeWithErrorT<GraphLoaded>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize GraphLoaded message: {}",
                      message.error().error()));
    }
    co->finishedWorkerStartup(message.get());
  } else if (path == Utils::finishedWorkerStepPath) {
    auto message = inspection::deserializeWithErrorT<GlobalSuperStepFinished>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize GlobalSuperStepFinished message: {}",
                      message.error().error()));
    }
    co->finishedWorkerStep(message.get());
  } else if (path == Utils::finishedWorkerFinalizationPath) {
    auto message = inspection::deserializeWithErrorT<Finished>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize Finished message: {}",
                      message.error().error()));
    }
    co->finishedWorkerFinalize(message.get());
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

  auto exeNum = ExecutionNumber{sExecutionNum.getUInt()};

  std::shared_ptr<IWorker> w = worker(exeNum);

  // create a new worker instance if necessary
  if (path == Utils::startExecutionPath) {
    if (w) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "Worker with this execution number already exists.");
    }
    auto createWorker =
        inspection::deserializeWithErrorT<worker::message::CreateWorker>(
            velocypack::SharedSlice({}, body));
    if (!createWorker.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize CreateWorker message: {}",
                      createWorker.error().error()));
    }

    addWorker(AlgoRegistry::createWorker(vocbase, createWorker.get(), *this),
              exeNum);
    worker(exeNum)->setupWorker();  // will call conductor

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
    auto message = inspection::deserializeWithErrorT<PrepareGlobalSuperStep>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize PrepareGlobalSuperStep message: {}",
                      message.error().error()));
    }
    auto prepared = w->prepareGlobalStep(message.get());
    auto response = inspection::serializeWithErrorT(prepared);
    if (!response.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot serialize GlobalSuperStepPrepared message: {}",
                      message.error().error()));
    }
    outBuilder.add(response.get().slice());
  } else if (path == Utils::startGSSPath) {
    auto message = inspection::deserializeWithErrorT<RunGlobalSuperStep>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize RunGlobalSuperStep message: {}",
                      message.error().error()));
    }
    w->startGlobalStep(message.get());
  } else if (path == Utils::messagesPath) {
    auto message =
        inspection::deserializeWithErrorT<worker::message::PregelMessage>(
            velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize PregelMessage message: {}",
                      message.error().error()));
    }
    w->receivedMessages(message.get());
  } else if (path == Utils::finalizeExecutionPath) {
    auto message = inspection::deserializeWithErrorT<FinalizeExecution>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize FinalizeExecution message: {}",
                      message.error().error()));
    }
    w->finalizeExecution(message.get(),
                         [this, exeNum]() { cleanupWorker(exeNum); });
  } else if (path == Utils::aqlResultsPath) {
    auto message = inspection::deserializeWithErrorT<CollectPregelResults>(
        velocypack::SharedSlice({}, body));
    if (!message.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot deserialize CollectPregelResults message: {}",
                      message.error().error()));
    }
    auto results = w->aqlResult(message.get().withId);
    auto response = inspection::serializeWithErrorT(results);
    if (!response.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          fmt::format("Cannot serialize PregelResults message: {}",
                      response.error().error()));
    }
    outBuilder.add(response.get().slice());
  }
}

uint64_t PregelFeature::numberOfActiveConductors() const {
  std::lock_guard guard{_mutex};
  uint64_t nr{0};
  for (auto const& p : _conductors) {
    std::shared_ptr<Conductor> const& c = p.second.conductor;
    if (c->_state == ExecutionState::DEFAULT ||
        c->_state == ExecutionState::LOADING ||
        c->_state == ExecutionState::RUNNING ||
        c->_state == ExecutionState::STORING) {
      ++nr;
      LOG_TOPIC("41564", WARN, Logger::PREGEL)
          << fmt::format("Conductor for executionNumber {} is in state {}.",
                         c->executionNumber(), ExecutionStateNames[c->_state]);
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
    std::lock_guard guard{_mutex};
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
          VPackBuffer<uint8_t>{}, options, network::addAuthorizationHeader({}));
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

auto PregelFeature::cancel(ExecutionNumber executionNumber) -> Result {
  auto c = conductor(executionNumber);
  if (c != nullptr) {
    c->cancel();
    return Result{};
  }

  // pregel can still have ran with actors, then the result actor would exist
  return _pregelRuns.doUnderLock([&executionNumber,
                                  this](auto const& items) -> Result {
    auto item = items.find(executionNumber);
    if (item == items.end()) {
      // TODO GOROD-1634 if historic collection has executionNumber entry,
      // return Result{} or a different error message
      return Result{TRI_ERROR_CURSOR_NOT_FOUND, "Execution number is invalid"};
    }
    auto const& [_, run] = *item;
    if (auto actors = run.getActorsFromUser(ExecContext::current());
        actors != std::nullopt) {
      auto resultActor = actors.value().resultActor;
      if (_actorRuntime->contains(resultActor.id)) {
        _actorRuntime->dispatch<pregel::message::ResultMessages>(
            resultActor, resultActor, pregel::message::CleanupResults{});
      }
      auto conductor = actors.value().conductor;
      if (conductor != std::nullopt &&
          _actorRuntime->contains(conductor.value().id)) {
        _actorRuntime->dispatch<pregel::conductor::message::ConductorMessages>(
            conductor.value(), conductor.value(),
            pregel::conductor::message::Cancel{});
      }
      return Result{};
    }
    return Result{TRI_ERROR_HTTP_UNAUTHORIZED, "User is not authorized."};
  });
}
