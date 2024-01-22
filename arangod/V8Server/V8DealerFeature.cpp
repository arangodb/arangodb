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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "V8DealerFeature.h"

#include <regex>
#include <thread>

#include "Actions/actions.h"
#include "Agency/v8-agency.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/FileUtils.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Cluster/v8-cluster.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FrontendFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utilities/NameValidator.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/V8PlatformFeature.h"
#include "V8/V8SecurityFeature.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8/v8-deadline.h"
#include "V8Server/FoxxFeature.h"
#include "V8Server/V8Executor.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-dispatcher.h"
#include "V8Server/v8-query.h"
#include "V8Server/v8-ttl.h"
#include "V8Server/v8-user-functions.h"
#include "V8Server/v8-user-structures.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/vocbase.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
class V8GcThread : public Thread {
 public:
  explicit V8GcThread(V8DealerFeature& dealer)
      : Thread(dealer.server(), "V8GarbageCollector"),
        _dealer(dealer),
        _lastGcStamp(static_cast<uint64_t>(TRI_microtime())) {}

  ~V8GcThread() { shutdown(); }

 public:
  void run() override { _dealer.collectGarbage(); }

  double getLastGcStamp() {
    return static_cast<double>(_lastGcStamp.load(std::memory_order_acquire));
  }

  void updateGcStamp(double value) {
    _lastGcStamp.store(static_cast<uint64_t>(value), std::memory_order_release);
  }

 private:
  V8DealerFeature& _dealer;
  std::atomic<uint64_t> _lastGcStamp;
};
}  // namespace

DECLARE_COUNTER(arangodb_v8_context_created_total, "V8 contexts created");
DECLARE_COUNTER(arangodb_v8_context_creation_time_msec_total,
                "Total time for creating V8 contexts [ms]");
DECLARE_COUNTER(arangodb_v8_context_destroyed_total, "V8 contexts destroyed");
DECLARE_COUNTER(arangodb_v8_context_enter_failures_total,
                "V8 context enter failures");
DECLARE_COUNTER(arangodb_v8_context_entered_total, "V8 context enter events");
DECLARE_COUNTER(arangodb_v8_context_exited_total, "V8 context exit events");

V8DealerFeature::V8DealerFeature(Server& server)
    : ArangodFeature{server, *this},
      _gcFrequency(60.0),
      _gcInterval(2000),
      _maxExecutorAge(60.0),
      _nrMaxExecutors(0),
      _nrMinExecutors(0),
      _nrInflightExecutors(0),
      _maxExecutorInvocations(0),
      _copyInstallation(false),
      _allowAdminExecute(false),
      _allowJavaScriptTransactions(true),
      _allowJavaScriptUdfs(true),
      _allowJavaScriptTasks(true),
      _enableJS(true),
      _nextId(0),
      _stopping(false),
      _gcFinished(false),
      _dynamicExecutorCreationBlockers(0),
      _executorsCreationTime(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_v8_context_creation_time_msec_total{})),
      _executorsCreated(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_v8_context_created_total{})),
      _executorsDestroyed(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_v8_context_destroyed_total{})),
      _executorsEntered(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_v8_context_entered_total{})),
      _executorsExited(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_v8_context_exited_total{})),
      _executorsEnterFailures(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_v8_context_enter_failures_total{})) {
  static_assert(
      Server::isCreatedAfter<V8DealerFeature, metrics::MetricsFeature>());

  setOptional(true);
  startsAfter<ClusterFeaturePhase>();

  startsAfter<ActionFeature>();
  startsAfter<V8PlatformFeature>();
  startsAfter<V8SecurityFeature>();
}

void V8DealerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "JavaScript engine and execution");

  options
      ->addOption(
          "--javascript.gc-frequency",
          "Time-based garbage collection frequency for JavaScript objects "
          "(each x seconds).",
          new DoubleParameter(&_gcFrequency),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(This option is useful to have the garbage
collection still work in periods with no or little numbers of requests.)");

  options->addOption(
      "--javascript.gc-interval",
      "Request-based garbage collection interval for JavaScript objects "
      "(each x requests).",
      new UInt64Parameter(&_gcInterval),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options->addOption("--javascript.app-path",
                     "The directory for Foxx applications.",
                     new StringParameter(&_appPath),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.startup-directory",
      "A path to the directory containing the JavaScript startup scripts.",
      new StringParameter(&_startupDirectory),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption("--javascript.module-directory",
                     "Additional paths containing JavaScript modules.",
                     new VectorParameter<StringParameter>(&_moduleDirectories),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle,
                         arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--javascript.copy-installation",
          "Copy the contents of `javascript.startup-directory` on first start.",
          new BooleanParameter(&_copyInstallation),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(This option is intended to be useful for rolling
upgrades. If you set it to `true`, you can upgrade the underlying ArangoDB
packages without influencing the running _arangod_ instance.

Setting this value does only make sense if you use ArangoDB outside of a
container solution, like Docker or Kubernetes.)");

  options
      ->addOption("--javascript.v8-contexts",
                  "The maximum number of V8 contexts that are created for "
                  "executing JavaScript actions.",
                  new UInt64Parameter(&_nrMaxExecutors),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(More contexts allow executing more JavaScript
actions in parallel, provided that there are also enough threads available.
Note that each V8 context uses a substantial amount of memory and requires
periodic CPU processing time for garbage collection.

This option configures the maximum number of V8 contexts that can be used in
parallel. On server start, only as many V8 contexts are created as are
configured by the `--javascript.v8-contexts-minimum` option. The actual number
of available V8 contexts may vary between `--javascript.v8-contexts-minimum`
and `--javascript.v8-contexts` at runtime. When there are unused V8 contexts
that linger around, the server's garbage collector thread automatically deletes
them.)");

  options
      ->addOption("--javascript.v8-contexts-minimum",
                  "The minimum number of V8 contexts to keep available for "
                  "executing JavaScript actions.",
                  new UInt64Parameter(&_nrMinExecutors),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The actual number of V8 contexts never drops below
this value, but it may go up as high as specified by the
`--javascript.v8-contexts` option.

When there are unused V8 contexts that linger around and the number of V8
contexts is greater than `--javascript.v8-contexts-minimum`, the server's
garbage collector thread automatically deletes them.)");

  options->addOption(
      "--javascript.v8-contexts-max-invocations",
      "The maximum number of invocations for each V8 context before it is "
      "disposed (0 = unlimited).",
      new UInt64Parameter(&_maxExecutorInvocations),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options
      ->addOption("--javascript.v8-contexts-max-age",
                  "The maximum age for each V8 context (in seconds) before it "
                  "is disposed.",
                  new DoubleParameter(&_maxExecutorAge),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If both `--javascript.v8-contexts-max-invocations`
and `--javascript.v8-contexts-max-age` are set, then the context is destroyed
when either of the specified threshold values is reached.)");

  options
      ->addOption("--javascript.allow-admin-execute",
                  "For testing purposes, allow `/_admin/execute`. Never enable "
                  "this option "
                  "in production!",
                  new BooleanParameter(&_allowAdminExecute),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to control whether
user-defined JavaScript code is allowed to be executed on the server by sending
HTTP requests to the `/_admin/execute` API endpoint with an authenticated user
account.

The default value is `false`, which disables the execution of user-defined
code. This is also the recommended setting for production. In test environments,
it may be convenient to turn the option on in order to send arbitrary setup
or teardown commands for execution on the server.)");

  options
      ->addOption("--javascript.transactions",
                  "Enable JavaScript transactions.",
                  new BooleanParameter(&_allowJavaScriptTransactions),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  options
      ->addOption(
          "--javascript.user-defined-functions",
          "Enable JavaScript user-defined functions (UDFs) in AQL queries.",
          new BooleanParameter(&_allowJavaScriptUdfs),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31004);

  options
      ->addOption("--javascript.tasks", "Enable JavaScript tasks.",
                  new BooleanParameter(&_allowJavaScriptTasks),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  options
      ->addOption("--javascript.enabled", "Enable the V8 JavaScript engine.",
                  new BooleanParameter(&_enableJS),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(By default, the V8 engine is enabled on single
servers and Coordinators. It is disabled by default on Agents and DB-Servers.

It is possible to turn the V8 engine off also on the latter instance types to 
reduce the footprint of ArangoDB. Turning the V8 engine off on single servers or
Coordinators will automatically render certain functionality unavailable or
dysfunctional. The affected functionality includes JavaScript transactions, Foxx, 
AQL user-defined functions, the built-in web interface and some server APIs.)");
}

void V8DealerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();

  // a bit of duck typing here to check if we are an agent.
  // the problem is that the server role may be still unclear in this early
  // phase, so we are also looking for startup options that identify an agent
  bool const isAgent =
      (ServerState::instance()->getRole() ==
       ServerState::RoleEnum::ROLE_AGENT) ||
      (result.touched("agency.activate") &&
       *(options->get<BooleanParameter>("agency.activate")->ptr));

  // DBServer and Agent don't need JS. Agent role handled in AgencyFeature
  if (!javascriptRequestedViaOptions(options) &&
      (isAgent || ServerState::instance()->getRole() ==
                      ServerState::RoleEnum::ROLE_DBSERVER)) {
    // specifying --console requires JavaScript, so we can only turn it off
    // if not requested
    _enableJS = false;
  }

  if (!_enableJS) {
    disable();

    server().disableFeatures(
        std::array{Server::id<V8PlatformFeature>(), Server::id<ActionFeature>(),
                   Server::id<ScriptFeature>(), Server::id<FoxxFeature>(),
                   Server::id<FrontendFeature>()});
    return;
  }

  // check the startup path
  if (_startupDirectory.empty()) {
    LOG_TOPIC("6330a", FATAL, arangodb::Logger::V8)
        << "no 'javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  // remove trailing / from path and set path
  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC("ae845", FATAL, arangodb::Logger::V8)
        << "failed to get global context";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_startupDirectory, "javascript.startup-directory", true);
  ctx->normalizePath(_moduleDirectories, "javascript.module-directory", false);

  // check whether app-path was specified
  if (_appPath.empty()) {
    LOG_TOPIC("a161b", FATAL, arangodb::Logger::V8)
        << "no value has been specified for --javascript.app-path";
    FATAL_ERROR_EXIT();
  }

  // Tests if this path is either a directory (ok) or does not exist (we create
  // it in ::start) If it is something else this will throw an error.
  ctx->normalizePath(_appPath, "javascript.app-path", false);

  // use a minimum of 1 second for GC
  if (_gcFrequency < 1) {
    _gcFrequency = 1;
  }
}

void V8DealerFeature::prepare() {
  auto& cluster = server().getFeature<ClusterFeature>();
  defineDouble("SYS_DEFAULT_REPLICATION_FACTOR_SYSTEM",
               cluster.systemReplicationFactor());
}

void V8DealerFeature::start() {
  TRI_ASSERT(_enableJS);
  TRI_ASSERT(isEnabled());

  if (_copyInstallation) {
    copyInstallationFiles();  // will exit process if it fails
  } else {
    // don't copy JS files on startup
    // now check if we have a js directory inside the database directory, and if
    // it looks good
    auto& dbPathFeature = server().getFeature<DatabasePathFeature>();
    std::string const dbJSPath =
        FileUtils::buildFilename(dbPathFeature.directory(), "js");
    std::string const checksumFile =
        FileUtils::buildFilename(dbJSPath, StaticStrings::checksumFileJs);
    std::string const serverPath = FileUtils::buildFilename(dbJSPath, "server");
    std::string const commonPath = FileUtils::buildFilename(dbJSPath, "common");
    std::string const nodeModulesPath =
        FileUtils::buildFilename(dbJSPath, "node", "node_modules");
    if (FileUtils::isDirectory(dbJSPath) && FileUtils::exists(checksumFile) &&
        FileUtils::isDirectory(serverPath) &&
        FileUtils::isDirectory(commonPath)) {
      // js directory inside database directory looks good. now use it!
      _startupDirectory = dbJSPath;
      // older versions didn't copy node_modules. so check if it exists inside
      // the database directory or not.
      if (FileUtils::isDirectory(nodeModulesPath)) {
        _nodeModulesDirectory = nodeModulesPath;
      } else {
        _nodeModulesDirectory = _startupDirectory;
      }
    }
  }

  LOG_TOPIC("77c97", DEBUG, Logger::V8)
      << "effective startup-directory: " << _startupDirectory
      << ", effective module-directories: " << _moduleDirectories
      << ", node-modules-directory: " << _nodeModulesDirectory;

  // add all paths to allowlists
  V8SecurityFeature& v8security = server().getFeature<V8SecurityFeature>();
  TRI_ASSERT(!_startupDirectory.empty());
  v8security.addToInternalAllowList(_startupDirectory, FSAccessType::READ);

  if (!_nodeModulesDirectory.empty()) {
    v8security.addToInternalAllowList(_nodeModulesDirectory,
                                      FSAccessType::READ);
  }
  for (auto const& it : _moduleDirectories) {
    if (!it.empty()) {
      v8security.addToInternalAllowList(it, FSAccessType::READ);
    }
  }

  TRI_ASSERT(!_appPath.empty());
  v8security.addToInternalAllowList(_appPath, FSAccessType::READ);
  v8security.addToInternalAllowList(_appPath, FSAccessType::WRITE);
  v8security.dumpAccessLists();

  _startupLoader.setDirectory(_startupDirectory);

  // dump paths
  {
    std::vector<std::string> paths;

    paths.push_back(std::string("startup '" + _startupDirectory + "'"));

    if (!_moduleDirectories.empty()) {
      paths.push_back(std::string(
          "module '" + StringUtils::join(_moduleDirectories, ";") + "'"));
    }

    if (!_appPath.empty()) {
      paths.push_back(std::string("application '" + _appPath + "'"));

      // create app directory if it does not exist
      if (!basics::FileUtils::isDirectory(_appPath)) {
        std::string systemErrorStr;
        long errorNo;

        auto res = TRI_CreateRecursiveDirectory(_appPath.c_str(), errorNo,
                                                systemErrorStr);

        if (res == TRI_ERROR_NO_ERROR) {
          LOG_TOPIC("86aa0", INFO, arangodb::Logger::FIXME)
              << "created javascript.app-path directory '" << _appPath << "'";
        } else {
          LOG_TOPIC("2d23f", FATAL, arangodb::Logger::FIXME)
              << "unable to create javascript.app-path directory '" << _appPath
              << "': " << systemErrorStr;
          FATAL_ERROR_EXIT();
        }
      }
    }

    LOG_TOPIC("86632", INFO, arangodb::Logger::V8)
        << "JavaScript using " << StringUtils::join(paths, ", ");
  }

  if (_nrMinExecutors < 1) {
    _nrMinExecutors = 1;
  }

  // try to guess a suitable number of executors
  if (0 == _nrMaxExecutors) {
    // use 7/8 of the available scheduler threads as the default number
    // of available V8 executors. only 7/8 are used to leave some headroom
    // for important maintenance tasks.
    // automatic maximum number of executors should not be below 8
    // this is because the number of cores may be too few for the cluster
    // startup to properly run through with all its parallel requests
    // and the potential need for multiple V8 executors.
    auto& sf = server().getFeature<SchedulerFeature>();
    _nrMaxExecutors = std::max(sf.maximalThreads() * 7 / 8, uint64_t(8));
  }

  if (_nrMinExecutors > _nrMaxExecutors) {
    // max executors must not be lower than min executors
    _nrMaxExecutors = _nrMinExecutors;
  }

  LOG_TOPIC("09e14", DEBUG, Logger::V8)
      << "number of V8 executors: min: " << _nrMinExecutors
      << ", max: " << _nrMaxExecutors;

  defineDouble("V8_CONTEXTS", static_cast<double>(_nrMaxExecutors));

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  // setup instances
  {
    std::unique_lock guard{_executorsCondition.mutex};
    _executors.reserve(static_cast<size_t>(_nrMaxExecutors));
    _busyExecutors.reserve(static_cast<size_t>(_nrMaxExecutors));
    _idleExecutors.reserve(static_cast<size_t>(_nrMaxExecutors));
    _dirtyExecutors.reserve(static_cast<size_t>(_nrMaxExecutors));

    for (size_t i = 0; i < _nrMinExecutors; ++i) {
      guard.unlock();  // avoid lock order inversion in buildExecutor

      // use vocbase here and hand ownership to executor
      auto vocbase = databaseFeature.useDatabase(StaticStrings::SystemDatabase);
      TRI_ASSERT(vocbase != nullptr);

      auto executor = buildExecutor(vocbase.get(), nextId());
      TRI_ASSERT(executor != nullptr);
      vocbase.release();

      guard.lock();
      // push_back will not fail as we reserved enough memory before
      _executors.push_back(executor.release());
      ++_executorsCreated;
    }

    TRI_ASSERT(_executors.size() > 0);
    TRI_ASSERT(_executors.size() <= _nrMaxExecutors);
    for (auto& executor : _executors) {
      _idleExecutors.push_back(executor);
    }
  }

  auto& sysDbFeature = server().getFeature<arangodb::SystemDatabaseFeature>();
  auto database = sysDbFeature.use();

  loadJavaScriptFileInAllExecutors(database.get(), "server/initialize.js",
                                   nullptr);
  startGarbageCollection();
}

void V8DealerFeature::copyInstallationFiles() {
  if (!_enableJS && (ServerState::instance()->isAgent() ||
                     ServerState::instance()->isDBServer())) {
    // skip expensive file-copying in case we are an agency or db server
    // these do not need JavaScript support
    return;
  }

  // get base path from DatabasePathFeature
  auto& dbPathFeature = server().getFeature<DatabasePathFeature>();
  std::string const copyJSPath =
      FileUtils::buildFilename(dbPathFeature.directory(), "js");
  if (copyJSPath == _startupDirectory) {
    LOG_TOPIC("89fe2", FATAL, arangodb::Logger::V8)
        << "'javascript.startup-directory' cannot be inside "
           "'database.directory'";
    FATAL_ERROR_EXIT();
  }
  TRI_ASSERT(!copyJSPath.empty());

  _nodeModulesDirectory = _startupDirectory;

  std::string const checksumFile = FileUtils::buildFilename(
      _startupDirectory, StaticStrings::checksumFileJs);
  std::string const copyChecksumFile =
      FileUtils::buildFilename(copyJSPath, StaticStrings::checksumFileJs);

  bool overwriteCopy = false;
  if (!FileUtils::exists(copyJSPath) || !FileUtils::exists(checksumFile) ||
      !FileUtils::exists(copyChecksumFile)) {
    overwriteCopy = true;
  } else {
    try {
      overwriteCopy = (StringUtils::trim(FileUtils::slurp(copyChecksumFile)) !=
                       StringUtils::trim(FileUtils::slurp(checksumFile)));
    } catch (basics::Exception const& e) {
      LOG_TOPIC("efa47", ERR, Logger::V8)
          << "Error reading '" << StaticStrings::checksumFileJs
          << "' from disk: " << e.what();
      overwriteCopy = true;
    }
  }

  if (overwriteCopy) {
    // basic security checks before removing an existing directory:
    // check if for some reason we will be trying to remove the entire database
    // directory...
    if (FileUtils::exists(FileUtils::buildFilename(copyJSPath, "ENGINE"))) {
      LOG_TOPIC("214d1", FATAL, Logger::V8)
          << "JS installation path '" << copyJSPath << "' seems to be invalid";
      FATAL_ERROR_EXIT();
    }

    LOG_TOPIC("dd1c0", INFO, Logger::V8)
        << "Copying JS installation files from '" << _startupDirectory
        << "' to '" << copyJSPath << "'";
    auto res = TRI_ERROR_NO_ERROR;
    if (FileUtils::exists(copyJSPath)) {
      res = TRI_RemoveDirectory(copyJSPath.c_str());
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC("1a20d", FATAL, Logger::V8)
            << "Error cleaning JS installation path '" << copyJSPath
            << "': " << TRI_errno_string(res);
        FATAL_ERROR_EXIT();
      }
    }
    if (!FileUtils::createDirectory(copyJSPath, &res)) {
      LOG_TOPIC("b8c79", FATAL, Logger::V8)
          << "Error creating JS installation path '" << copyJSPath
          << "': " << TRI_errno_string(res);
      FATAL_ERROR_EXIT();
    }

    // intentionally do not copy js/node/node_modules/estlint!
    // we avoid copying this directory because it contains 5000+ files at the
    // moment, and copying them one by one is slow. In addition, eslint is not
    // needed in release builds
    std::string const versionAppendix = std::regex_replace(
        rest::Version::getServerVersion(), std::regex("-.*$"), "");
    std::string const uiNodeModulesPath =
        FileUtils::buildFilename("js", "apps", "system", "_admin", "aardvark",
                                 "APP", "react", "node_modules");

    // .bin directories could be harmful, and .map files are large and
    // unnecessary
    std::string const binDirectory =
        std::string(TRI_DIR_SEPARATOR_STR) + ".bin" + TRI_DIR_SEPARATOR_STR;

    size_t copied = 0;

    auto filter = [&uiNodeModulesPath, &binDirectory,
                   &copied](std::string const& filename) -> bool {
      if (filename.ends_with(".map")) {
        // filename ends with ".map". filter it out!
        return true;
      }
      if (filename.find(binDirectory) != std::string::npos) {
        // don't copy files in .bin
        return true;
      }

      std::string normalized = filename;
      FileUtils::normalizePath(normalized);
      if (normalized.ends_with(uiNodeModulesPath)) {
        // filter it out!
        return true;
      }

      // let the file/directory pass through
      ++copied;
      return false;
    };

    double start = TRI_microtime();

    std::string error;
    if (!FileUtils::copyRecursive(_startupDirectory, copyJSPath, filter,
                                  error)) {
      LOG_TOPIC("45261", FATAL, Logger::V8)
          << "Error copying JS installation files to '" << copyJSPath
          << "': " << error;
      FATAL_ERROR_EXIT();
    }

    // attempt to copy enterprise JS files too.
    // only required for developer installations, not packages
    std::string const enterpriseJs = basics::FileUtils::buildFilename(
        _startupDirectory, "..", "enterprise", "js");

    if (FileUtils::isDirectory(enterpriseJs)) {
      std::function<bool(std::string const&)> const passAllFilter =
          [](std::string const&) { return false; };
      if (!FileUtils::copyRecursive(enterpriseJs, copyJSPath, passAllFilter,
                                    error)) {
        LOG_TOPIC("ae9d3", WARN, Logger::V8)
            << "Error copying enterprise JS installation files to '"
            << copyJSPath << "': " << error;
      }
    }

    LOG_TOPIC("38e1e", INFO, Logger::V8)
        << "copying " << copied << " JS installation file(s) took "
        << Logger::FIXED(TRI_microtime() - start, 6) << "s";
  }

  // finally switch over the paths
  _startupDirectory = copyJSPath;
  _nodeModulesDirectory =
      basics::FileUtils::buildFilename(copyJSPath, "node", "node_modules");
}

std::unique_ptr<V8Executor> V8DealerFeature::addExecutor() {
  if (server().isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  // use vocbase here and hand ownership to executor
  auto vocbase = databaseFeature.useDatabase(StaticStrings::SystemDatabase);
  TRI_ASSERT(vocbase != nullptr);

  // vocbase will be released when the executor is garbage collected
  auto executor = buildExecutor(vocbase.get(), nextId());
  TRI_ASSERT(executor != nullptr);

  auto& sysDbFeature = server().getFeature<arangodb::SystemDatabaseFeature>();
  auto database = sysDbFeature.use();
  TRI_ASSERT(database != nullptr);

  // no other thread can use the executor when we are here, as the
  // executor has not been added to the global list of executors yet
  loadJavaScriptFileInExecutor(database.get(), "server/initialize.js",
                               executor.get(), nullptr);

  ++_executorsCreated;
  vocbase.release();
  return executor;
}

void V8DealerFeature::unprepare() {
  shutdownExecutors();

  // delete GC thread after all action threads have been stopped
  _gcThread.reset();
}

/// @brief return either the name of the database to be used as a folder name,
/// or its id if its name contains special characters and is not fully supported
/// in every OS
[[nodiscard]] static std::string_view getDatabaseDirName(std::string_view name,
                                                         std::string_view id) {
  bool const isOldStyleName =
      DatabaseNameValidator::validateName(
          /*allowSystem=*/true, /*extendedNames=*/false, name)
          .ok();
  return (isOldStyleName || id.empty()) ? name : id;
}

void V8DealerFeature::verifyAppPaths() {
  if (!_appPath.empty() && !TRI_IsDirectory(_appPath.c_str())) {
    long systemError;
    std::string errorMessage;
    auto res = TRI_CreateRecursiveDirectory(_appPath.c_str(), systemError,
                                            errorMessage);

    if (res == TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("1bf74", INFO, Logger::FIXME)
          << "created --javascript.app-path directory '" << _appPath << "'";
    } else {
      LOG_TOPIC("52bd5", ERR, Logger::FIXME)
          << "unable to create --javascript.app-path directory '" << _appPath
          << "': " << errorMessage;
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  // create subdirectory js/apps/_db if not yet present
  auto r = createBaseApplicationDirectory(_appPath, "_db");

  if (r != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("610c7", ERR, Logger::FIXME)
        << "unable to initialize databases: " << TRI_errno_string(r);
    THROW_ARANGO_EXCEPTION(r);
  }
}

ErrorCode V8DealerFeature::createDatabase(std::string_view name,
                                          std::string_view id,
                                          bool removeExisting) {
  // create app directory for database if it does not exist
  std::string const dirName{getDatabaseDirName(name, id)};
  return createApplicationDirectory(dirName, _appPath, removeExisting);
}

void V8DealerFeature::cleanupDatabase(TRI_vocbase_t& database) {
  if (_appPath.empty()) {
    return;
  }
  std::string const dirName{
      getDatabaseDirName(database.name(), std::to_string(database.id()))};
  std::string const path = basics::FileUtils::buildFilename(
      basics::FileUtils::buildFilename(_appPath, "_db"), dirName);

  if (TRI_IsDirectory(path.c_str())) {
    LOG_TOPIC("041b1", TRACE, arangodb::Logger::FIXME)
        << "removing app directory '" << path << "' of database '"
        << database.name() << "'";

    TRI_RemoveDirectory(path.c_str());
  }
}

ErrorCode V8DealerFeature::createApplicationDirectory(
    std::string const& name, std::string const& basePath, bool removeExisting) {
  if (basePath.empty()) {
    return TRI_ERROR_NO_ERROR;
  }

  std::string const path = basics::FileUtils::buildFilename(
      basics::FileUtils::buildFilename(basePath, "_db"), name);

  if (TRI_IsDirectory(path.c_str())) {
    // directory already exists
    // this can happen if a database is dropped and quickly recreated
    if (!removeExisting) {
      return TRI_ERROR_NO_ERROR;
    }

    if (!basics::FileUtils::listFiles(path).empty()) {
      LOG_TOPIC("56fc7", INFO, arangodb::Logger::FIXME)
          << "forcefully removing existing application directory '" << path
          << "' for database '" << name << "'";
      // removing is best effort. if it does not succeed, we can still
      // go on creating the it
      TRI_RemoveDirectory(path.c_str());
    }
  }

  // directory does not yet exist - this should be the standard case
  long systemError = 0;
  std::string errorMessage;
  auto r =
      TRI_CreateRecursiveDirectory(path.c_str(), systemError, errorMessage);

  if (r == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("6745a", TRACE, arangodb::Logger::FIXME)
        << "created application directory '" << path << "' for database '"
        << name << "'";
  } else if (r == TRI_ERROR_FILE_EXISTS) {
    LOG_TOPIC("2a78e", INFO, arangodb::Logger::FIXME)
        << "unable to create application directory '" << path
        << "' for database '" << name << "': " << errorMessage;
    r = TRI_ERROR_NO_ERROR;
  } else {
    LOG_TOPIC("36682", ERR, arangodb::Logger::FIXME)
        << "unable to create application directory '" << path
        << "' for database '" << name << "': " << errorMessage;
  }

  return r;
}

ErrorCode V8DealerFeature::createBaseApplicationDirectory(
    std::string const& appPath, std::string const& type) {
  auto const path = basics::FileUtils::buildFilename(appPath, type);
  if (TRI_IsDirectory(path.c_str())) {
    return TRI_ERROR_NO_ERROR;
  }
  std::string errorMessage;
  long systemError = 0;
  auto r = TRI_CreateDirectory(path.c_str(), systemError, errorMessage);
  if (r == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("e6460", INFO, Logger::FIXME)
        << "created base application directory '" << path << "'";
  } else if ((r != TRI_ERROR_FILE_EXISTS) || (!TRI_IsDirectory(path.c_str()))) {
    LOG_TOPIC("5a0b4", ERR, Logger::FIXME)
        << "unable to create base application directory " << errorMessage;
  } else {
    LOG_TOPIC("0a25f", INFO, Logger::FIXME)
        << "someone else created base application directory '" << path << "'";
    r = TRI_ERROR_NO_ERROR;
  }
  return r;
}

bool V8DealerFeature::addGlobalExecutorMethod(
    GlobalExecutorMethods::MethodType type) {
  bool result = true;

  std::lock_guard guard{_executorsCondition.mutex};

  for (auto& executor : _executors) {
    try {
      executor->addGlobalExecutorMethod(type);
    } catch (...) {
      result = false;
    }
  }
  return result;
}

void V8DealerFeature::collectGarbage() {
  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());
  TRI_ASSERT(gc != nullptr);

  // this flag will be set to true if we timed out waiting for a GC signal
  // if set to true, the next cycle will use a reduced wait time so the GC
  // can be performed more early for all dirty executors. The flag is set
  // to false again once all executors have been cleaned up and there is nothing
  // more to do
  bool useReducedWait = false;
  bool preferFree = false;

  // the time we'll wait for a signal
  uint64_t const regularWaitTime =
      static_cast<uint64_t>(_gcFrequency * 1000.0 * 1000.0);

  // the time we'll wait for a signal when the previous wait timed out
  uint64_t const reducedWaitTime =
      static_cast<uint64_t>(_gcFrequency * 1000.0 * 200.0);

  while (!_stopping) {
    try {
      V8Executor* executor = nullptr;
      bool wasDirty = false;

      {
        bool gotSignal = false;
        preferFree = !preferFree;
        std::unique_lock guard{_executorsCondition.mutex};

        if (_dirtyExecutors.empty()) {
          uint64_t waitTime =
              useReducedWait ? reducedWaitTime : regularWaitTime;

          // we'll wait for a signal or a timeout
          gotSignal = _executorsCondition.cv.wait_for(
                          guard, std::chrono::microseconds{waitTime}) ==
                      std::cv_status::no_timeout;
        }

        if (preferFree && !_idleExecutors.empty()) {
          executor = pickFreeExecutorForGc();
        }

        if (executor == nullptr && !_dirtyExecutors.empty()) {
          executor = _dirtyExecutors.back();
          _dirtyExecutors.pop_back();
          if (executor->invocationsSinceLastGc() < 50 &&
              !executor->hasActiveExternals()) {
            // don't collect this one yet. it doesn't have externals, so there
            // is no urge for garbage collection
            _idleExecutors.emplace_back(executor);
            executor = nullptr;
          } else {
            wasDirty = true;
          }
        }

        if (executor == nullptr && !preferFree && !gotSignal &&
            !_idleExecutors.empty()) {
          // we timed out waiting for a signal, so we have idle time that we can
          // spend on running the GC pro-actively
          // We'll pick one of the free executors and clean it up
          executor = pickFreeExecutorForGc();
        }

        // there is no executor to clean up, probably they all have been cleaned
        // up already. increase the wait time so we don't cycle too much in the
        // GC loop and waste CPU unnecessary
        useReducedWait = (executor != nullptr);
      }

      // update last gc time
      double lastGc = TRI_microtime();
      gc->updateGcStamp(lastGc);

      if (executor != nullptr) {
        LOG_TOPIC("6bb08", TRACE, arangodb::Logger::V8)
            << "collecting V8 garbage in executor #" << executor->id()
            << ", invocations total: " << executor->invocations()
            << ", invocations since last gc: "
            << executor->invocationsSinceLastGc()
            << ", hasActive: " << executor->hasActiveExternals()
            << ", wasDirty: " << wasDirty;
        bool hasActiveExternals = false;
        {
          executor->lockAndEnter();
          auto guard =
              scopeGuard([&]() noexcept { executor->unlockAndExit(); });

          executor->runInContext(
              [&](v8::Isolate* isolate) -> Result {
                v8::HandleScope scope(isolate);

                TRI_GET_GLOBALS();
                v8g->_inForcedCollect = true;
                TRI_RunGarbageCollectionV8(isolate, 1.0);
                v8g->_inForcedCollect = false;
                hasActiveExternals = v8g->hasActiveExternals();

                return {};
              },
              /*executeGlobalMethods*/ false);
        }

        // update garbage collection statistics
        executor->setHasActiveExternals(hasActiveExternals);
        executor->setCleaned(lastGc);

        {
          std::unique_lock guard{_executorsCondition.mutex};

          if (_executors.size() > _nrMinExecutors && !executor->isDefault() &&
              executor->shouldBeRemoved(_maxExecutorAge,
                                        _maxExecutorInvocations) &&
              _dynamicExecutorCreationBlockers == 0) {
            // remove the extra context as it is not needed anymore
            _executors.erase(std::remove_if(_executors.begin(),
                                            _executors.end(),
                                            [&executor](V8Executor* e) {
                                              return e->id() == executor->id();
                                            }));

            LOG_TOPIC("0a995", DEBUG, Logger::V8)
                << "removed superfluous V8 executor #" << executor->id()
                << ", number of executors is now: " << _executors.size();

            guard.unlock();
            shutdownExecutor(executor);
          } else {
            // put it back into the free list
            if (wasDirty) {
              _idleExecutors.emplace_back(executor);
            } else {
              _idleExecutors.insert(_idleExecutors.begin(), executor);
            }
            _executorsCondition.cv.notify_all();
          }
        }
      } else {
        useReducedWait = true;
      }
    } catch (...) {
      // simply ignore errors here
      useReducedWait = false;
    }
  }

  _gcFinished = true;
}

void V8DealerFeature::unblockDynamicExecutorCreation() {
  std::lock_guard guard{_executorsCondition.mutex};

  TRI_ASSERT(_dynamicExecutorCreationBlockers > 0);
  --_dynamicExecutorCreationBlockers;
}

/// @brief loads a JavaScript file in all executors, only called at startup
void V8DealerFeature::loadJavaScriptFileInAllExecutors(TRI_vocbase_t* vocbase,
                                                       std::string const& file,
                                                       VPackBuilder* builder) {
  if (builder != nullptr) {
    builder->openArray();
  }

  std::vector<V8Executor*> executors;
  {
    std::unique_lock guard{_executorsCondition.mutex};

    while (_nrInflightExecutors > 0) {
      // wait until all pending executors creation requests have been satisified
      _executorsCondition.cv.wait_for(guard, std::chrono::milliseconds{10});
    }

    // copy the list of executors into a local variable
    executors = _executors;
    // block the addition or removal of executors
    ++_dynamicExecutorCreationBlockers;
  }

  auto sg = arangodb::scopeGuard(
      [&]() noexcept { unblockDynamicExecutorCreation(); });

  LOG_TOPIC("1364d", TRACE, Logger::V8)
      << "loading JavaScript file '" << file << "' in all (" << executors.size()
      << ") V8 executors";

  // now safely scan the local copy of the executors
  for (auto& executor : executors) {
    std::unique_lock guard{_executorsCondition.mutex};

    while (_busyExecutors.contains(executor)) {
      // we must not enter the executor if another thread is also using it...
      _executorsCondition.cv.wait_for(guard, std::chrono::milliseconds{10});
    }

    auto it =
        std::find(_dirtyExecutors.begin(), _dirtyExecutors.end(), executor);
    if (it != _dirtyExecutors.end()) {
      // executor is in _dirtyExecutors
      // remove it from there
      _dirtyExecutors.erase(it);

      guard.unlock();
      try {
        loadJavaScriptFileInExecutor(vocbase, file, executor, builder);
      } catch (...) {
        guard.lock();
        _dirtyExecutors.push_back(executor);
        throw;
      }
      // and re-insert it after we are done
      guard.lock();
      _dirtyExecutors.push_back(executor);
    } else {
      // if the executor is neither busy nor dirty, it must be idle
      auto it =
          std::find(_idleExecutors.begin(), _idleExecutors.end(), executor);
      if (it != _idleExecutors.end()) {
        // remove it from there
        _idleExecutors.erase(it);

        guard.unlock();
        try {
          loadJavaScriptFileInExecutor(vocbase, file, executor, builder);
        } catch (...) {
          guard.lock();
          _idleExecutors.push_back(executor);
          throw;
        }
        // and re-insert it after we are done
        guard.lock();
        _idleExecutors.push_back(executor);
      } else {
        LOG_TOPIC("d3a7f", WARN, Logger::V8)
            << "v8 executor #" << executor->id() << " has disappeared";
      }
    }
  }

  if (builder != nullptr) {
    builder->close();
  }
}

void V8DealerFeature::startGarbageCollection() {
  TRI_ASSERT(_gcThread == nullptr);
  _gcThread = std::make_unique<V8GcThread>(*this);
  _gcThread->start();

  _gcFinished = false;
}

void V8DealerFeature::prepareLockedExecutor(
    TRI_vocbase_t* vocbase, V8Executor* executor,
    JavaScriptSecurityContext const& securityContext) {
  TRI_ASSERT(vocbase != nullptr);

  v8::Isolate* isolate = executor->isolate();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));

  // reset the isolate data
  v8g->_expressionContext = nullptr;
  v8g->_vocbase = vocbase;
  v8g->_securityContext = securityContext;
  v8g->_currentRequest.Reset();
  v8g->_currentResponse.Reset();

  LOG_TOPIC("94226", TRACE, arangodb::Logger::V8)
      << "entering V8 context #" << executor->id();

  executor->runInContext([&](v8::Isolate* isolate) -> Result { return {}; },
                         /*executeGlobalMethods*/ true);
}

/// @brief enter a V8 executor
/// currently returns a nullptr if no executor can be acquired in time
V8Executor* V8DealerFeature::enterExecutor(
    TRI_vocbase_t* vocbase, JavaScriptSecurityContext const& securityContext) {
  TRI_ASSERT(vocbase != nullptr);

  if (_stopping) {
    return nullptr;
  }

  if (!vocbase->use()) {
    return nullptr;
  }

  double const startTime = TRI_microtime();
  TRI_ASSERT(v8::Isolate::TryGetCurrent() == nullptr);
  V8Executor* executor = nullptr;

  // look for a free executor
  {
    std::unique_lock guard{_executorsCondition.mutex};

    while (_idleExecutors.empty() && !_stopping) {
      TRI_ASSERT(guard.owns_lock());

      LOG_TOPIC("619ab", TRACE, arangodb::Logger::V8)
          << "waiting for unused V8 executor";

      if (!_dirtyExecutors.empty()) {
        // we'll use a dirty executor in this case
        _idleExecutors.push_back(_dirtyExecutors.back());
        _dirtyExecutors.pop_back();
        break;
      }

      bool const executorsLimitNotExceeded =
          (_executors.size() + _nrInflightExecutors < _nrMaxExecutors);

      if (executorsLimitNotExceeded && _dynamicExecutorCreationBlockers == 0) {
        ++_nrInflightExecutors;

        TRI_ASSERT(guard.owns_lock());
        guard.unlock();

        try {
          LOG_TOPIC("973d7", DEBUG, Logger::V8)
              << "creating additional V8 executor";
          executor = addExecutor().release();
        } catch (...) {
          guard.lock();

          // clean up state
          --_nrInflightExecutors;
          throw;
        }

        // must re-lock
        TRI_ASSERT(!guard.owns_lock());
        guard.lock();

        --_nrInflightExecutors;
        try {
          _executors.push_back(executor);
        } catch (...) {
          // oops
          delete executor;
          executor = nullptr;
          ++_executorsDestroyed;
          continue;
        }

        TRI_ASSERT(guard.owns_lock());
        try {
          _idleExecutors.push_back(executor);
          LOG_TOPIC("25f94", DEBUG, Logger::V8)
              << "created additional V8 executor #" << executor->id()
              << ", number of executors is now " << _executors.size();
        } catch (...) {
          TRI_ASSERT(!_executors.empty());
          _executors.pop_back();
          TRI_ASSERT(executor != nullptr);
          delete executor;
          ++_executorsDestroyed;
        }

        _executorsCondition.cv.notify_all();
        continue;
      }

      TRI_ASSERT(guard.owns_lock());

      constexpr double maxWaitTime = 60.0;
      double const now = TRI_microtime();
      if (now - startTime >= maxWaitTime) {
        vocbase->release();

        ++_executorsEnterFailures;

        LOG_TOPIC("e1807", WARN, arangodb::Logger::V8)
            << "giving up waiting for unused V8 executors for '"
            << securityContext.typeName() << "' operation after "
            << Logger::FIXED(maxWaitTime) << " s - "
            << "executors: " << _executors.size() << "/" << _nrMaxExecutors
            << ", idle: " << _idleExecutors.size()
            << ", busy: " << _busyExecutors.size()
            << ", dirty: " << _dirtyExecutors.size()
            << ", in flight: " << _nrInflightExecutors
            << " - executor overview following...";

        size_t i = 0;
        for (auto const& it : _executors) {
          ++i;
          LOG_TOPIC("74439", WARN, arangodb::Logger::V8)
              << "- executor #" << it->id() << " (" << i << "/"
              << _executors.size() << ")"
              << ": acquired: " << Logger::FIXED(now - it->acquired())
              << " s ago"
              << ", performing '" << it->description() << "' operation";
        }
        return nullptr;
      }

      _executorsCondition.cv.wait_for(guard, std::chrono::milliseconds{100});
    }

    TRI_ASSERT(guard.owns_lock());

    // in case we are in the shutdown phase, do not enter an executor.
    // the executor might have been deleted by the shutdown
    if (_stopping) {
      vocbase->release();
      return nullptr;
    }

    TRI_ASSERT(!_idleExecutors.empty());

    executor = _idleExecutors.back();
    TRI_ASSERT(executor != nullptr);
    LOG_TOPIC("bbe93", TRACE, arangodb::Logger::V8)
        << "found unused V8 executor #" << executor->id();

    _idleExecutors.pop_back();

    // should not fail because we reserved enough space beforehand
    _busyExecutors.emplace(executor);

    executor->setDescription(securityContext.typeName(), TRI_microtime());
  }

  executor->lockAndEnter();

  prepareLockedExecutor(vocbase, executor, securityContext);
  ++_executorsEntered;

  return executor;
}

void V8DealerFeature::exitExecutorInternal(V8Executor* executor) {
  auto sg = arangodb::scopeGuard([&]() noexcept { executor->unlockAndExit(); });
  cleanupLockedExecutor(executor);
}

void V8DealerFeature::cleanupLockedExecutor(V8Executor* executor) {
  TRI_ASSERT(executor != nullptr);

  LOG_TOPIC("e1c52", TRACE, arangodb::Logger::V8)
      << "leaving V8 executor #" << executor->id();

  v8::Isolate* isolate = executor->isolate();

  if (V8PlatformFeature::isOutOfMemory(isolate)) {
    executor->runInContext([&](v8::Isolate* isolate) -> Result {
      v8::HandleScope scope(isolate);

      TRI_GET_GLOBALS();

      v8g->_inForcedCollect = true;
      TRI_RunGarbageCollectionV8(isolate, 0.1);
      v8g->_inForcedCollect = false;

      // needs to be reset after the garbage collection
      V8PlatformFeature::resetOutOfMemory(isolate);

      return {};
    });
  }

  // update data for later garbage collection
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
  executor->setHasActiveExternals(v8g->hasActiveExternals());
  TRI_vocbase_t* vocbase = v8g->_vocbase;

  TRI_ASSERT(vocbase != nullptr);
  // release last recently used vocbase
  vocbase->release();

  // check for cancelation requests
  bool canceled = v8g->_canceled;
  v8g->_canceled = false;

  // if the execution was canceled, we need to cleanup
  if (canceled) {
    executor->handleCancellationCleanup();
  }

  // reset the executor data; gc should be able to run without it
  v8g->_expressionContext = nullptr;
  v8g->_vocbase = nullptr;
  v8g->_securityContext.reset();
  v8g->_currentRequest.Reset();
  v8g->_currentResponse.Reset();
}

void V8DealerFeature::exitExecutor(V8Executor* executor) {
  cleanupLockedExecutor(executor);

  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());

  if (gc != nullptr) {
    // default is no garbage collection
    bool performGarbageCollection = false;
    bool forceGarbageCollection = false;

    // postpone garbage collection for standard executors
    double lastGc = gc->getLastGcStamp();
    if (executor->lastGcStamp() + _gcFrequency < lastGc) {
      performGarbageCollection = true;
      if (executor->lastGcStamp() + 30 * _gcFrequency < lastGc) {
        // force the GC, so that it happens eventually
        forceGarbageCollection = true;
        LOG_TOPIC("f543a", TRACE, arangodb::Logger::V8)
            << "V8 executor #" << executor->id()
            << " has reached GC timeout threshold and will be forced into GC";
      } else {
        LOG_TOPIC("f3526", TRACE, arangodb::Logger::V8)
            << "V8 executor #" << executor->id()
            << " has reached GC timeout threshold and will be scheduled for GC";
      }
    } else if (executor->invocationsSinceLastGc() >= _gcInterval) {
      LOG_TOPIC("c6441", TRACE, arangodb::Logger::V8)
          << "V8 executor #" << executor->id()
          << " has reached maximum number of requests and will "
             "be scheduled for GC";
      performGarbageCollection = true;
    }

    executor->unlockAndExit();
    std::lock_guard guard{_executorsCondition.mutex};

    executor->clearDescription();

    if (performGarbageCollection &&
        (forceGarbageCollection || !_idleExecutors.empty())) {
      // only add the executor to the dirty list if there is at least one other
      // free executor

      // note that re-adding the executors here should not fail as we reserved
      // enough room for all executors during startup
      _dirtyExecutors.emplace_back(executor);
    } else {
      // note that re-adding the executor here should not fail as we reserved
      // enough room for all executors during startup
      _idleExecutors.emplace_back(executor);
    }

    _busyExecutors.erase(executor);

    LOG_TOPIC("fc763", TRACE, arangodb::Logger::V8)
        << "returned dirty V8 executor #" << executor->id();
    _executorsCondition.cv.notify_all();
  } else {
    executor->unlockAndExit();
    std::lock_guard guard{_executorsCondition.mutex};

    executor->clearDescription();

    _busyExecutors.erase(executor);
    // note that re-adding the executor here should not fail as we reserved
    // enough room for all executors during startup
    _idleExecutors.emplace_back(executor);

    LOG_TOPIC("82410", TRACE, arangodb::Logger::V8)
        << "returned dirty V8 executor #" << executor->id()
        << " back into free";
    _executorsCondition.cv.notify_all();
  }

  ++_executorsExited;
}

void V8DealerFeature::shutdownExecutors() {
  _stopping = true;

  // wait for all executors to finish
  {
    std::unique_lock guard{_executorsCondition.mutex};
    _executorsCondition.cv.notify_all();

    for (size_t n = 0; n < 10 * 5; ++n) {
      if (_busyExecutors.empty()) {
        LOG_TOPIC("36259", DEBUG, arangodb::Logger::V8)
            << "no busy V8 executors";
        break;
      }

      LOG_TOPIC("ea785", DEBUG, arangodb::Logger::V8)
          << "waiting for busy V8 executors (" << _busyExecutors.size()
          << ") to finish ";

      _executorsCondition.cv.wait_for(guard, std::chrono::milliseconds{100});
    }
  }

  // send all busy executors a terminate signal
  {
    std::lock_guard guard{_executorsCondition.mutex};

    for (auto& it : _busyExecutors) {
      LOG_TOPIC("e907b", WARN, arangodb::Logger::V8)
          << "sending termination signal to V8 executor #" << it->id();
      it->isolate()->TerminateExecution();
    }
  }

  // wait no more than one minute
  {
    std::unique_lock guard{_executorsCondition.mutex};

    for (size_t n = 0; n < 10 * 60; ++n) {
      if (_busyExecutors.empty()) {
        break;
      }

      _executorsCondition.cv.wait_for(guard, std::chrono::milliseconds{100});
    }
  }

  if (!_busyExecutors.empty()) {
    LOG_TOPIC("4b09f", FATAL, arangodb::Logger::V8)
        << "cannot shutdown V8 executors";
    FATAL_ERROR_EXIT();
  }

  // stop GC thread
  if (_gcThread != nullptr) {
    LOG_TOPIC("c6543", DEBUG, arangodb::Logger::V8)
        << "waiting for V8 GC thread to finish action";
    _gcThread->beginShutdown();

    // wait until garbage collector thread is done
    while (!_gcFinished) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_TOPIC("ea409", DEBUG, arangodb::Logger::V8)
        << "commanding V8 GC thread to terminate";
  }

  // shutdown all instances
  {
    std::vector<V8Executor*> executors;
    {
      std::lock_guard guard{_executorsCondition.mutex};
      executors = _executors;
      _executors.clear();
    }

    for (auto& executor : executors) {
      shutdownExecutor(executor);
    }
  }

  LOG_TOPIC("7cdb2", DEBUG, arangodb::Logger::V8)
      << "V8 executors are shut down";
}

V8Executor* V8DealerFeature::pickFreeExecutorForGc() {
  int const n = static_cast<int>(_idleExecutors.size());

  if (n == 0) {
    // this is easy...
    return nullptr;
  }

  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());
  TRI_ASSERT(gc != nullptr);

  // we got more than 1 executor to clean up, pick the one with the "oldest" GC
  // stamp
  int pickedExecutorNr =
      -1;  // index of executor with lowest GC stamp, -1 means "none"

  for (int i = n - 1; i > 0; --i) {
    // check if there's actually anything to clean up in the executor
    if (_idleExecutors[i]->invocationsSinceLastGc() < 50 &&
        !_idleExecutors[i]->hasActiveExternals()) {
      continue;
    }

    // compare last GC stamp
    if (pickedExecutorNr == -1 ||
        _idleExecutors[i]->lastGcStamp() <=
            _idleExecutors[pickedExecutorNr]->lastGcStamp()) {
      pickedExecutorNr = i;
    }
  }

  // we now have the executor to clean up in pickedExecutorNr

  if (pickedExecutorNr == -1) {
    // no executor found
    return nullptr;
  }

  // this is the executor to clean up
  V8Executor* executor = _idleExecutors[pickedExecutorNr];
  TRI_ASSERT(executor != nullptr);

  // now compare its last GC timestamp with the last global GC stamp
  if (executor->lastGcStamp() + _gcFrequency >= gc->getLastGcStamp()) {
    // no need yet to clean up the executor
    return nullptr;
  }

  // we'll pop the executor from the vector. the executor might be at
  // any position in the vector so we need to move the other elements
  // around
  if (n > 1) {
    for (int i = pickedExecutorNr; i < n - 1; ++i) {
      _idleExecutors[i] = _idleExecutors[i + 1];
    }
  }
  _idleExecutors.pop_back();

  return executor;
}

std::unique_ptr<V8Executor> V8DealerFeature::buildExecutor(
    TRI_vocbase_t* vocbase, size_t id) {
  double const start = TRI_microtime();

  V8PlatformFeature& v8platform = server().getFeature<V8PlatformFeature>();

  // create isolate
  v8::Isolate* isolate = v8platform.createIsolate();
  TRI_ASSERT(isolate != nullptr);

  std::unique_ptr<V8Executor> executor;

  try {
    // pass isolate to a new executor
    executor =
        std::make_unique<V8Executor>(id, isolate, [&](V8Executor& executor) {
          executor.runInContext(
              [&](v8::Isolate* isolate) -> Result {
                v8::HandleScope scope(isolate);

                v8::Handle<v8::Context> context = isolate->GetCurrentContext();

                auto* v8g = CreateV8Globals(server(), isolate, id);

                v8::Handle<v8::Object> globalObj = context->Global();
                globalObj
                    ->Set(context, TRI_V8_ASCII_STRING(isolate, "GLOBAL"),
                          globalObj)
                    .FromMaybe(false);
                globalObj
                    ->Set(context, TRI_V8_ASCII_STRING(isolate, "global"),
                          globalObj)
                    .FromMaybe(false);
                globalObj
                    ->Set(context, TRI_V8_ASCII_STRING(isolate, "root"),
                          globalObj)
                    .FromMaybe(false);

                std::string modules;
                std::string sep;

                std::vector<std::string> directories;
                directories.insert(directories.end(),
                                   _moduleDirectories.begin(),
                                   _moduleDirectories.end());
                directories.emplace_back(_startupDirectory);
                if (!_nodeModulesDirectory.empty() &&
                    _nodeModulesDirectory != _startupDirectory) {
                  directories.emplace_back(_nodeModulesDirectory);
                }

                for (auto const& directory : directories) {
                  modules += sep;
                  sep = ";";

                  modules +=
                      FileUtils::buildFilename(directory, "server/modules") +
                      sep +
                      FileUtils::buildFilename(directory, "common/modules") +
                      sep + FileUtils::buildFilename(directory, "node");
                }

                TRI_InitV8UserFunctions(isolate, context);
                TRI_InitV8UserStructures(isolate, context);
                TRI_InitV8Buffer(isolate);
                TRI_InitV8Utils(isolate, context, _startupDirectory, modules);
                TRI_InitV8ServerUtils(isolate);
                TRI_InitV8Shell(isolate);
                TRI_InitV8Ttl(isolate);

                {
                  v8::HandleScope scope(isolate);

                  TRI_AddGlobalVariableVocbase(
                      isolate, TRI_V8_ASCII_STRING(isolate, "APP_PATH"),
                      TRI_V8_STD_STRING(isolate, _appPath));

                  for (auto const& j : _definedBooleans) {
                    context->Global()
                        ->DefineOwnProperty(TRI_IGETC,
                                            TRI_V8_STD_STRING(isolate, j.first),
                                            v8::Boolean::New(isolate, j.second),
                                            v8::ReadOnly)
                        .FromMaybe(false);  // Ignore it...
                  }

                  for (auto const& j : _definedDoubles) {
                    context->Global()
                        ->DefineOwnProperty(
                            TRI_IGETC, TRI_V8_STD_STRING(isolate, j.first),
                            v8::Number::New(isolate, j.second), v8::ReadOnly)
                        .FromMaybe(false);  // Ignore it...
                  }

                  for (auto const& j : _definedStrings) {
                    context->Global()
                        ->DefineOwnProperty(
                            TRI_IGETC, TRI_V8_STD_STRING(isolate, j.first),
                            TRI_V8_STD_STRING(isolate, j.second),
                            v8::ReadOnly)
                        .FromMaybe(false);  // Ignore it...
                  }
                }

                auto queryRegistry = QueryRegistryFeature::registry();
                TRI_ASSERT(queryRegistry != nullptr);

                JavaScriptSecurityContext old(v8g->_securityContext);
                v8g->_securityContext =
                    JavaScriptSecurityContext::createInternalContext();

                TRI_InitV8VocBridge(isolate, context, queryRegistry, *vocbase,
                                    id);
                TRI_InitV8Queries(isolate, context);
                TRI_InitV8Cluster(isolate, context);
                TRI_InitV8Agency(isolate, context);
                TRI_InitV8Dispatcher(isolate, context);
                TRI_InitV8Actions(isolate);

                // restore old security settings
                v8g->_securityContext = old;

                return {};
              },
              /*executeGlobalMethods*/ true);
        });

  } catch (...) {
    LOG_TOPIC("35586", WARN, Logger::V8)
        << "caught exception during context initialization";
    v8platform.disposeIsolate(isolate);
    throw;
  }

  double const now = TRI_microtime();

  LOG_TOPIC("83428", TRACE, arangodb::Logger::V8)
      << "initialized V8 executor #" << id << " in "
      << Logger::FIXED(now - start, 6) << " s";

  // add executor creation time to global metrics
  _executorsCreationTime += static_cast<uint64_t>(1000 * (now - start));

  return executor;
}

V8DealerFeature::Statistics V8DealerFeature::getCurrentExecutorStatistics() {
  std::lock_guard guard{_executorsCondition.mutex};

  return {_executors.size(),     _busyExecutors.size(), _dirtyExecutors.size(),
          _idleExecutors.size(), _nrMaxExecutors,       _nrMinExecutors};
}

std::vector<V8DealerFeature::DetailedExecutorStatistics>
V8DealerFeature::getCurrentExecutorDetails() {
  std::vector<V8DealerFeature::DetailedExecutorStatistics> result;
  {
    std::lock_guard guard{_executorsCondition.mutex};
    result.reserve(_executors.size());
    for (auto it : _executors) {
      v8::Isolate* isolate = it->isolate();
      TRI_GET_GLOBALS();
      result.push_back(DetailedExecutorStatistics{
          v8g->_id, v8g->_lastMaxTime, v8g->_countOfTimes, v8g->_heapMax,
          v8g->_heapLow, it->invocations()});
    }
  }
  return result;
}

void V8DealerFeature::loadJavaScriptFileInExecutor(
    TRI_vocbase_t* vocbase, std::string const& file, V8Executor* executor,
    velocypack::Builder* builder) {
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(executor != nullptr);

  if (_stopping) {
    return;
  }

  if (!vocbase->use()) {
    return;
  }

  JavaScriptSecurityContext securityContext =
      JavaScriptSecurityContext::createInternalContext();

  executor->lockAndEnter();

  TRI_ASSERT(!executor->isolate()->InContext());
  prepareLockedExecutor(vocbase, executor, securityContext);
  auto sg =
      arangodb::scopeGuard([&]() noexcept { exitExecutorInternal(executor); });

  try {
    loadJavaScriptFileInternal(file, executor, builder);
  } catch (...) {
    LOG_TOPIC("e099e", WARN, Logger::V8)
        << "caught exception while executing JavaScript file '" << file
        << "' in executor #" << executor->id();
    throw;
  }
}

void V8DealerFeature::loadJavaScriptFileInternal(std::string const& file,
                                                 V8Executor* executor,
                                                 velocypack::Builder* builder) {
  double start = TRI_microtime();

  executor->runInContext([&](v8::Isolate* isolate) -> Result {
    v8::HandleScope scope(isolate);

    switch (_startupLoader.loadScript(isolate, file, builder)) {
      case JSLoader::eSuccess:
        LOG_TOPIC("29e73", TRACE, arangodb::Logger::V8)
            << "loaded JavaScript file '" << file << "'";
        break;
      case JSLoader::eFailLoad:
        LOG_TOPIC("0f13b", FATAL, arangodb::Logger::V8)
            << "cannot load JavaScript file '" << file << "'";
        FATAL_ERROR_EXIT();
      case JSLoader::eFailExecute:
        LOG_TOPIC("69ac3", FATAL, arangodb::Logger::V8)
            << "error during execution of JavaScript file '" << file << "'";
        FATAL_ERROR_EXIT();
    }

    return {};
  });

  LOG_TOPIC("53bbb", TRACE, arangodb::Logger::V8)
      << "loaded JavaScript file '" << file << "' for V8 executor #"
      << executor->id()
      << ", took: " << Logger::FIXED(TRI_microtime() - start, 6) << "s";
}

void V8DealerFeature::shutdownExecutor(V8Executor* executor) {
  TRI_ASSERT(executor != nullptr);
  LOG_TOPIC("7946e", TRACE, arangodb::Logger::V8)
      << "shutting down V8 executor #" << executor->id();

  v8::Isolate* isolate = executor->isolate();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
  {
    executor->lockAndEnter();
    auto guard = scopeGuard([&]() noexcept { executor->unlockAndExit(); });

    // note: executeGlobalMethods must be false here to prevent
    // shutdown errors.
    executor->runInContext(
        [&](v8::Isolate* isolate) -> Result {
          v8::HandleScope scope(isolate);

          TRI_VisitActions(
              [&isolate](TRI_action_t* action) { action->visit(isolate); });

          v8g->_inForcedCollect = true;
          TRI_RunGarbageCollectionV8(isolate, 30.0);
          v8g->_inForcedCollect = false;

          return {};
        },
        /*executeGlobalMethods*/ false);
  }

  delete v8g;

  server().getFeature<V8PlatformFeature>().disposeIsolate(isolate);

  LOG_TOPIC("34c28", TRACE, arangodb::Logger::V8)
      << "shut down V8 executor #" << executor->id();

  delete executor;
  ++_executorsDestroyed;
}

bool V8DealerFeature::javascriptRequestedViaOptions(
    std::shared_ptr<ProgramOptions> const& options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();

  if (result.touched("console") &&
      *(options->get<BooleanParameter>("console")->ptr)) {
    // --console
    return true;
  }
  if (result.touched("javascript.enabled") &&
      *(options->get<BooleanParameter>("javascript.enabled")->ptr)) {
    // --javascript.enabled
    return true;
  }
  return false;
}

V8ExecutorGuard::V8ExecutorGuard(
    TRI_vocbase_t* vocbase, JavaScriptSecurityContext const& securityContext)
    : _vocbase(vocbase), _isolate(nullptr), _executor(nullptr) {
  _executor = vocbase->server().getFeature<V8DealerFeature>().enterExecutor(
      vocbase, securityContext);
  if (_executor == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT,
                                   "unable to acquire V8 executor in time");
  }
  _isolate = _executor->isolate();
}

V8ExecutorGuard::~V8ExecutorGuard() {
  if (_executor) {
    try {
      _vocbase->server().getFeature<V8DealerFeature>().exitExecutor(_executor);
    } catch (...) {
    }
  }
}

Result V8ExecutorGuard::runInContext(
    std::function<Result(v8::Isolate*)> const& cb, bool executeGlobalMethods) {
  TRI_ASSERT(_executor != nullptr);
  return _executor->runInContext(cb, executeGlobalMethods);
}

V8ConditionalExecutorGuard::V8ConditionalExecutorGuard(
    TRI_vocbase_t* vocbase, JavaScriptSecurityContext const& securityContext)
    : _vocbase(vocbase),
      _isolate(v8::Isolate::TryGetCurrent()),
      _executor(nullptr) {
  TRI_ASSERT(vocbase != nullptr);
  if (_isolate == nullptr) {
    _executor = vocbase->server().getFeature<V8DealerFeature>().enterExecutor(
        vocbase, securityContext);
    if (_executor != nullptr) {
      _isolate = _executor->isolate();
    }
    TRI_ASSERT((_isolate == nullptr) == (_executor == nullptr));
  }
}

V8ConditionalExecutorGuard::~V8ConditionalExecutorGuard() {
  if (_executor != nullptr) {
    try {
      _vocbase->server().getFeature<V8DealerFeature>().exitExecutor(_executor);
    } catch (...) {
    }
  }
}

Result V8ConditionalExecutorGuard::runInContext(
    std::function<Result(v8::Isolate*)> const& cb, bool executeGlobalMethods) {
  TRI_ASSERT(_isolate != nullptr);

  Result res;
  if (_executor != nullptr) {
    res = _executor->runInContext(cb, executeGlobalMethods);
  } else {
    v8::HandleScope scope(_isolate);

    v8::Handle<v8::Context> context = _isolate->GetCurrentContext();
    TRI_ASSERT(!context.IsEmpty());
    {
      v8::Context::Scope contextScope(context);
      TRI_ASSERT(_isolate->InContext());

      res = cb(_isolate);
    }
  }
  return res;
}
