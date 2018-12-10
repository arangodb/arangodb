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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "V8DealerFeature.h"

#include <regex>
#include <thread>

#include "Actions/actions.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/ConditionLocker.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/TimedAction.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomGenerator.h"
#include "Rest/Version.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/V8Context.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-user-functions.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

V8DealerFeature* V8DealerFeature::DEALER = nullptr;

namespace {
class V8GcThread : public Thread {
 public:
  explicit V8GcThread(V8DealerFeature* dealer)
      : Thread("V8GarbageCollector"),
        _dealer(dealer),
        _lastGcStamp(static_cast<uint64_t>(TRI_microtime())) {}

  ~V8GcThread() { shutdown(); }

 public:
  void run() override { _dealer->collectGarbage(); }

  double getLastGcStamp() {
    return static_cast<double>(_lastGcStamp.load(std::memory_order_acquire));
  }

  void updateGcStamp(double value) {
    _lastGcStamp.store(static_cast<uint64_t>(value), std::memory_order_release);
  }

 private:
  V8DealerFeature* _dealer;
  std::atomic<uint64_t> _lastGcStamp;
};
}

V8DealerFeature::V8DealerFeature(
    application_features::ApplicationServer& server
)
    : application_features::ApplicationFeature(server, "V8Dealer"),
      _gcFrequency(60.0),
      _gcInterval(2000),
      _maxContextAge(60.0),
      _copyInstallation(false),
      _nrMaxContexts(0),
      _nrMinContexts(0),
      _nrInflightContexts(0),
      _maxContextInvocations(0),
      _allowAdminExecute(false),
      _enableJS(true),
      _nextId(0),
      _stopping(false),
      _gcFinished(false),
      _dynamicContextCreationBlockers(0) {
  setOptional(true);
  startsAfter("ClusterPhase");

  startsAfter("Action");
  startsAfter("V8Platform");
}

void V8DealerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addOption(
      "--javascript.gc-frequency",
      "JavaScript time-based garbage collection frequency (each x seconds)",
      new DoubleParameter(&_gcFrequency),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--javascript.gc-interval",
      "JavaScript request-based garbage collection interval (each x requests)",
      new UInt64Parameter(&_gcInterval),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--javascript.app-path", "directory for Foxx applications",
                     new StringParameter(&_appPath));

  options->addOption(
      "--javascript.startup-directory",
      "path to the directory containing JavaScript startup scripts",
      new StringParameter(&_startupDirectory));

  options->addOption(
      "--javascript.module-directory",
      "additional paths containing JavaScript modules",
      new VectorParameter<StringParameter>(&_moduleDirectories),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
  
  options->addOption(
     "--javascript.copy-installation",
     "copy contents of 'javascript.startup-directory' on first start",
     new BooleanParameter(&_copyInstallation));
  
  options->addOption(
      "--javascript.v8-contexts",
      "maximum number of V8 contexts that are created for executing JavaScript actions",
      new UInt64Parameter(&_nrMaxContexts));

  options->addOption(
      "--javascript.v8-contexts-minimum",
      "minimum number of V8 contexts that keep available for executing JavaScript actions",
      new UInt64Parameter(&_nrMinContexts));

  options->addOption(
      "--javascript.v8-contexts-max-invocations",
      "maximum number of invocations for each V8 context before it is disposed",
      new UInt64Parameter(&_maxContextInvocations),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--javascript.v8-contexts-max-age",
      "maximum age for each V8 context (in seconds) before it is disposed",
      new DoubleParameter(&_maxContextAge),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--javascript.allow-admin-execute",
      "for testing purposes allow '_admin/execute', NEVER enable on production",
      new BooleanParameter(&_allowAdminExecute),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--javascript.enabled",
      "enable the V8 JavaScript engine",
      new BooleanParameter(&_enableJS),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

void V8DealerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  ProgramOptions::ProcessingResult const& result = options->processingResult();

  // DBServer and Agent don't need JS. Agent role handled in AgencyFeature
  if (ServerState::instance()->getRole() == ServerState::RoleEnum::ROLE_PRIMARY &&
      (!result.touched("console") || !*(options->get<BooleanParameter>("console")->ptr))) {
    // specifiying --console requires JavaScript, so we can only turn it off
    // if not specified
    _enableJS = false;
  }

  if (!_enableJS) {
    disable();
    application_features::ApplicationServer::disableFeatures({"V8Platform", "Action",
      "Script", "FoxxQueues", "Frontend"});
    return;
  }

  // check the startup path
  if (_startupDirectory.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8)
        << "no 'javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  // remove trailing / from path and set path
  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8) << "failed to get global context";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_startupDirectory, "javascript.startup-directory", true);
  ctx->normalizePath(_moduleDirectories, "javascript.module-directory", false);
  
  
  // try to append the current version name to the startup directory,
  // so instead of "/path/to/js" we will get "/path/to/js/3.4.0"
  std::string const versionAppendix = std::regex_replace(rest::Version::getServerVersion(), std::regex("-.*$"), "");
  std::string versionedPath = basics::FileUtils::buildFilename(_startupDirectory, versionAppendix);

  LOG_TOPIC(DEBUG, Logger::V8) << "checking for existence of version-specific startup-directory '" << versionedPath << "'";
  if (basics::FileUtils::isDirectory(versionedPath)) {
    // version-specific js path exists!
    _startupDirectory = versionedPath;
  }

  for (auto& it : _moduleDirectories) {
    versionedPath = basics::FileUtils::buildFilename(it, versionAppendix);

    LOG_TOPIC(DEBUG, Logger::V8) << "checking for existence of version-specific module-directory '" << versionedPath << "'";
    if (basics::FileUtils::isDirectory(versionedPath)) {
      // version-specific js path exists!
      it = versionedPath;
    }
  }

  // check whether app-path was specified
  if (_appPath.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8) << "no value has been specified for --javascript.app-path";
    FATAL_ERROR_EXIT();
  }

  // Tests if this path is either a directory (ok) or does not exist (we create it in ::start)
  // If it is something else this will throw an error.
  ctx->normalizePath(_appPath, "javascript.app-path", false);

  // use a minimum of 1 second for GC
  if (_gcFrequency < 1) {
    _gcFrequency = 1;
  }
}

void V8DealerFeature::prepare() {
  auto cluster = ApplicationServer::getFeature<ClusterFeature>("Cluster");
  TRI_ASSERT(cluster != nullptr);
  defineDouble("SYS_DEFAULT_REPLICATION_FACTOR_SYSTEM", cluster->systemReplicationFactor());
}

void V8DealerFeature::start() {
  if (_copyInstallation) {
    copyInstallationFiles(); // will exit process if it fails
  } else {
    // don't copy JS files on startup
    // now check if we have a js directory inside the database directory, and if it looks good
    auto dbPathFeature = application_features::ApplicationServer::getFeature<DatabasePathFeature>(DatabasePathFeature::name());
    const std::string dbJSPath = FileUtils::buildFilename(dbPathFeature->directory(), "js");
    const std::string checksumFile = FileUtils::buildFilename(dbJSPath, StaticStrings::checksumFileJs);
    const std::string serverPath = FileUtils::buildFilename(dbJSPath, "server");
    const std::string commonPath = FileUtils::buildFilename(dbJSPath, "common");
    if (FileUtils::isDirectory(dbJSPath) &&
        FileUtils::exists(checksumFile) &&
        FileUtils::isDirectory(serverPath) &&
        FileUtils::isDirectory(commonPath)) {
      // only load node modules from original startup path
      _nodeModulesDirectory = _startupDirectory;
      // js directory inside database directory looks good. now use it!
      _startupDirectory = dbJSPath;
    }
  }

  LOG_TOPIC(DEBUG, Logger::V8) << "effective startup-directory: " << _startupDirectory <<
                                  ", effective module-directories: " << _moduleDirectories << 
                                  ", node-modules-directory: " << _nodeModulesDirectory;
  
  _startupLoader.setDirectory(_startupDirectory);
  ServerState::instance()->setJavaScriptPath(_startupDirectory);
  
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

        int res = TRI_CreateRecursiveDirectory(_appPath.c_str(), errorNo,
                                               systemErrorStr);

        if (res == TRI_ERROR_NO_ERROR) {
          LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "created javascript.app-path directory '" << _appPath << "'";
        } else {
          LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "unable to create javascript.app-path directory '" << _appPath << "': " << systemErrorStr;
          FATAL_ERROR_EXIT();
        }
      }
    }

    LOG_TOPIC(INFO, arangodb::Logger::V8) << "JavaScript using " << StringUtils::join(paths, ", ");
  }

  // set singleton
  DEALER = this;

  if (_nrMinContexts < 1) {
    _nrMinContexts = 1;
  }

  // try to guess a suitable number of contexts
  if (0 == _nrMaxContexts) {
    SchedulerFeature* scheduler =
        ApplicationServer::getFeature<SchedulerFeature>("Scheduler");

    // automatic maximum number of contexts should not be below 16
    // this is because the number of cores may be too few for the cluster
    // startup to properly run through with all its parallel requests
    // and the potential need for multiple V8 contexts
    _nrMaxContexts = (std::max)(uint64_t(scheduler->concurrency()), uint64_t(16));
  }

  if (_nrMinContexts > _nrMaxContexts) {
    // max contexts must not be lower than min contexts
    _nrMaxContexts = _nrMinContexts;
  }

  LOG_TOPIC(DEBUG, Logger::V8) << "number of V8 contexts: min: " << _nrMinContexts << ", max: " << _nrMaxContexts;

  defineDouble("V8_CONTEXTS", static_cast<double>(_nrMaxContexts));
  defineBoolean("ALLOW_ADMIN_EXECUTE", _allowAdminExecute);

  // setup instances
  {
    CONDITION_LOCKER(guard, _contextCondition);
    _contexts.reserve(static_cast<size_t>(_nrMaxContexts));
    _busyContexts.reserve(static_cast<size_t>(_nrMaxContexts));
    _idleContexts.reserve(static_cast<size_t>(_nrMaxContexts));
    _dirtyContexts.reserve(static_cast<size_t>(_nrMaxContexts));

    for (size_t i = 0; i < _nrMinContexts; ++i) {
      guard.unlock(); // avoid lock order inversion in buildContext
      V8Context* context = buildContext(nextId());
      guard.lock();
      TRI_ASSERT(context != nullptr);
      try {
        _contexts.push_back(context);
      } catch (...) {
        delete context;
        throw;
      }
    }

    TRI_ASSERT(_contexts.size() > 0);
    TRI_ASSERT(_contexts.size() <= _nrMaxContexts);
    for (auto& context : _contexts) {
      // apply context update is only run on contexts that no other
      // threads can see (yet)
      guard.unlock();
      applyContextUpdate(context);
      guard.lock();
      _idleContexts.push_back(context);
    }
  }

  auto* sysDbFeature = arangodb::application_features::ApplicationServer::getFeature<
    arangodb::SystemDatabaseFeature
  >();
  auto database = sysDbFeature->use();

  loadJavaScriptFileInAllContexts(
    database.get(), "server/initialize.js", nullptr
  );
  startGarbageCollection();
}

void V8DealerFeature::copyInstallationFiles() {
  if (!_enableJS && 
      (ServerState::instance()->isAgent() || ServerState::instance()->isDBServer())) {
    // skip expensive file-copying in case we are an agency or db server
    // these do not need JavaScript support
    return;
  }

  // get base path from DatabasePathFeature
  auto dbPathFeature = application_features::ApplicationServer::getFeature<DatabasePathFeature>();
  const std::string copyJSPath = FileUtils::buildFilename(dbPathFeature->directory(), "js");
  if (copyJSPath == _startupDirectory) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8)
    << "'javascript.startup-directory' cannot be inside 'database.directory'";
    FATAL_ERROR_EXIT();
  }
  TRI_ASSERT(!copyJSPath.empty());
  
  _nodeModulesDirectory = _startupDirectory;
  
  const std::string checksumFile = FileUtils::buildFilename(_startupDirectory, StaticStrings::checksumFileJs);
  const std::string copyChecksumFile = FileUtils::buildFilename(copyJSPath, StaticStrings::checksumFileJs);
  
  bool overwriteCopy = false;
  if (!FileUtils::exists(copyJSPath) ||
      !FileUtils::exists(checksumFile) ||
      !FileUtils::exists(copyChecksumFile)) {
    overwriteCopy = true;
  } else {
    try {
      overwriteCopy = (FileUtils::slurp(copyChecksumFile) != FileUtils::slurp(checksumFile));
    } catch(basics::Exception const& e) {
      LOG_TOPIC(ERR, Logger::V8) << "Error reading '" << StaticStrings::checksumFileJs <<
      "' from disk: " << e.what();
      overwriteCopy = true;
    }
  }
      
  if (overwriteCopy) {
    // sanity check before removing an existing directory:
    // check if for some reason we will be trying to remove the entire database directory...
    if (FileUtils::exists(FileUtils::buildFilename(copyJSPath, "ENGINE"))) {
      LOG_TOPIC(FATAL, Logger::V8) << "JS installation path '" << copyJSPath
        << "' seems to be invalid";
      FATAL_ERROR_EXIT();
    }
  
    LOG_TOPIC(DEBUG, Logger::V8) << "Copying JS installation files from '" << _startupDirectory << "' to '" << copyJSPath << "'";
    int res = TRI_ERROR_NO_ERROR;
    if (FileUtils::exists(copyJSPath)) {
      res = TRI_RemoveDirectory(copyJSPath.c_str());
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_TOPIC(FATAL, Logger::V8) << "Error cleaning JS installation path '" << copyJSPath
        << "': " << TRI_errno_string(res);
        FATAL_ERROR_EXIT();
      }
    }
    if (!FileUtils::createDirectory(copyJSPath, &res)) {
      LOG_TOPIC(FATAL, Logger::V8) << "Error creating JS installation path '" << copyJSPath
      << "': " << TRI_errno_string(res);
      FATAL_ERROR_EXIT();
    }

    // intentionally do not copy js/node/node_modules...
    // we avoid copying this directory because it contains 5000+ files at the moment,
    // and copying them one by one is darn slow at least on Windows...
    std::string const versionAppendix = std::regex_replace(rest::Version::getServerVersion(), std::regex("-.*$"), "");
    std::string const nodeModulesPath = FileUtils::buildFilename("js", "node", "node_modules");
    std::string const nodeModulesPathVersioned = basics::FileUtils::buildFilename("js", versionAppendix, "node", "node_modules");
    auto filter = [&nodeModulesPath, &nodeModulesPathVersioned](std::string const& filename) -> bool{
      if (filename.size() >= nodeModulesPath.size()) {
        std::string normalized = filename;
        FileUtils::normalizePath(normalized);
        TRI_ASSERT(filename.size() == normalized.size());
        if (normalized.substr(normalized.size() - nodeModulesPath.size(), nodeModulesPath.size()) == nodeModulesPath ||
            normalized.substr(normalized.size() - nodeModulesPathVersioned.size(), nodeModulesPathVersioned.size()) == nodeModulesPathVersioned) {
          // filter it out!
          return true;
        }
      }
      // let the file/directory pass through
      return false;
    };

    std::string error;
    if (!FileUtils::copyRecursive(_startupDirectory, copyJSPath, filter, error)) {
      LOG_TOPIC(FATAL, Logger::V8) << "Error copying JS installation files to '" << copyJSPath
        << "': " << error;
      FATAL_ERROR_EXIT();
    }
  }
  _startupDirectory = copyJSPath;
}

V8Context* V8DealerFeature::addContext() {
  if (application_features::ApplicationServer::isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  V8Context* context = buildContext(nextId());

  try {
    // apply context update is only run on contexts that no other
    // threads can see (yet)
    applyContextUpdate(context);

    auto* sysDbFeature = arangodb::application_features::ApplicationServer::getFeature<
      arangodb::SystemDatabaseFeature
    >();
    auto database = sysDbFeature->use();

    TRI_ASSERT(database != nullptr);

    // no other thread can use the context when we are here, as the
    // context has not been added to the global list of contexts yet
    loadJavaScriptFileInContext(
      database.get(), "server/initialize.js", context, nullptr
    );

    return context;
  } catch (...) {
    delete context;
    throw;
  }
}

void V8DealerFeature::unprepare() {
  shutdownContexts();

  // delete GC thread after all action threads have been stopped
  _gcThread.reset();

  DEALER = nullptr;
}

bool V8DealerFeature::addGlobalContextMethod(std::string const& method) {
  auto cb = [this, &method]() -> bool {
    bool result = true;
    for (auto& context : _contexts) {
      try {
        if (!context->addGlobalContextMethod(method)) {
          result = false;
        }
      } catch (...) {
        result = false;
      }
    }
    return result;
  };

  CONDITION_LOCKER(guard, _contextCondition);
  return cb();
}

void V8DealerFeature::collectGarbage() {
  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());
  TRI_ASSERT(gc != nullptr);

  // this flag will be set to true if we timed out waiting for a GC signal
  // if set to true, the next cycle will use a reduced wait time so the GC
  // can be performed more early for all dirty contexts. The flag is set
  // to false again once all contexts have been cleaned up and there is nothing
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
      V8Context* context = nullptr;
      bool wasDirty = false;

      {
        bool gotSignal = false;
        preferFree = !preferFree;
        CONDITION_LOCKER(guard, _contextCondition);

        if (_dirtyContexts.empty()) {
          uint64_t waitTime = useReducedWait ? reducedWaitTime : regularWaitTime;

          // we'll wait for a signal or a timeout
          gotSignal = guard.wait(waitTime);
        }

        if (preferFree && !_idleContexts.empty()) {
          context = pickFreeContextForGc();
        }

        if (context == nullptr && !_dirtyContexts.empty()) {
          context = _dirtyContexts.back();
          _dirtyContexts.pop_back();
          if (context->invocationsSinceLastGc() < 50 && !context->_hasActiveExternals) {
            // don't collect this one yet. it doesn't have externals, so there
            // is no urge for garbage collection
            _idleContexts.emplace_back(context);
            context = nullptr;
          } else {
            wasDirty = true;
          }
        }

        if (context == nullptr && !preferFree && !gotSignal &&
            !_idleContexts.empty()) {
          // we timed out waiting for a signal, so we have idle time that we can
          // spend on running the GC pro-actively
          // We'll pick one of the free contexts and clean it up
          context = pickFreeContextForGc();
        }

        // there is no context to clean up, probably they all have been cleaned up
        // already. increase the wait time so we don't cycle too much in the GC
        // loop
        // and waste CPU unnecessary
        useReducedWait = (context != nullptr);
      }

      // update last gc time
      double lastGc = TRI_microtime();
      gc->updateGcStamp(lastGc);

      if (context != nullptr) {

        LOG_TOPIC(TRACE, arangodb::Logger::V8) << "collecting V8 garbage in context #" << context->id()
                  << ", invocations total: " << context->invocations()
                  << ", invocations since last gc: " << context->invocationsSinceLastGc()
                  << ", hasActive: " << context->_hasActiveExternals
                  << ", wasDirty: " << wasDirty;
        bool hasActiveExternals = false;
        auto isolate = context->_isolate;
        {
          // this guard will lock and enter the isolate
          // and automatically exit and unlock it when it runs out of scope
          V8ContextEntryGuard contextGuard(context);

          v8::HandleScope scope(isolate);

          auto localContext =
              v8::Local<v8::Context>::New(isolate, context->_context);

          localContext->Enter();
          {
            v8::Context::Scope contextScope(localContext);

            context->assertLocked();

            TRI_GET_GLOBALS();
            TRI_RunGarbageCollectionV8(isolate, 1.0);
            hasActiveExternals = v8g->hasActiveExternals();
          }
          localContext->Exit();
        }

        // update garbage collection statistics
        context->_hasActiveExternals = hasActiveExternals;
        context->setCleaned(lastGc);

        {
          CONDITION_LOCKER(guard, _contextCondition);

          if (_contexts.size() > _nrMinContexts &&
              !context->isDefault() &&
              context->shouldBeRemoved(_maxContextAge, _maxContextInvocations) &&
              _dynamicContextCreationBlockers == 0) {
            // remove the extra context as it is not needed anymore
            _contexts.erase(std::remove_if(_contexts.begin(), _contexts.end(), [&context](V8Context* c) {
              return (c->id() == context->id());
            }));

            LOG_TOPIC(DEBUG, Logger::V8) << "removed superfluous V8 context #" << context->id() << ", number of contexts is now: " << _contexts.size();

            guard.unlock();
            shutdownContext(context);
          } else {
            // put it back into the free list
            if (wasDirty) {
              _idleContexts.emplace_back(context);
            } else {
              _idleContexts.insert(_idleContexts.begin(), context);
            }
            guard.broadcast();
          }
        }
      } else {
        useReducedWait = true;  // sanity
      }
    } catch (...) {
      // simply ignore errors here
      useReducedWait = false;
    }
  }

  _gcFinished = true;
}

void V8DealerFeature::unblockDynamicContextCreation() {
  CONDITION_LOCKER(guard, _contextCondition);

  TRI_ASSERT(_dynamicContextCreationBlockers > 0);
  --_dynamicContextCreationBlockers;
}

void V8DealerFeature::loadJavaScriptFileInAllContexts(TRI_vocbase_t* vocbase,
    std::string const& file, VPackBuilder* builder) {

  if (builder != nullptr) {
    builder->openArray();
  }

  std::vector<V8Context*> contexts;
  {
    CONDITION_LOCKER(guard, _contextCondition);

    while (_nrInflightContexts > 0) {
      // wait until all pending context creation requests have been satisified
      guard.wait(10000);
    }

    // copy the list of contexts into a local variable
    contexts = _contexts;
    // block the addition or removal of contexts
    ++_dynamicContextCreationBlockers;
  }

  TRI_DEFER(unblockDynamicContextCreation());

  LOG_TOPIC(TRACE, Logger::V8) << "loading JavaScript file '" << file << "' in all (" << contexts.size() << ") V8 context";

  // now safely scan the local copy of the contexts
  for (auto& context : contexts) {
    CONDITION_LOCKER(guard, _contextCondition);

    while (_busyContexts.find(context) != _busyContexts.end()) {
      // we must not enter the context if another thread is also using it...
      guard.wait(10000);
    }

    auto it = std::find(_dirtyContexts.begin(), _dirtyContexts.end(), context);
    if (it != _dirtyContexts.end()) {
      // context is in _dirtyContexts
      // remove it from there
      _dirtyContexts.erase(it);

      guard.unlock();
      try {
        loadJavaScriptFileInContext(vocbase, file, context, builder);
      } catch (...) {
        guard.lock();
        _dirtyContexts.push_back(context);
        throw;
      }
      // and re-insert it after we are done
      guard.lock();
      _dirtyContexts.push_back(context);
    } else {
      // if the context is neither busy nor dirty, it must be idle
      auto it = std::find(_idleContexts.begin(), _idleContexts.end(), context);
      if (it != _idleContexts.end()) {
        // remove it from there
        _idleContexts.erase(it);

        guard.unlock();
        try {
          loadJavaScriptFileInContext(vocbase, file, context, builder);
        } catch (...) {
          guard.lock();
          _idleContexts.push_back(context);
          throw;
        }
        // and re-insert it after we are done
        guard.lock();
        _idleContexts.push_back(context);
      } else {
        LOG_TOPIC(WARN, Logger::V8) << "v8 context #" << context->id() << " has disappeared";
      }
    }
  }

  if (builder != nullptr) {
    builder->close();
  }
}

void V8DealerFeature::startGarbageCollection() {
  TRI_ASSERT(_gcThread == nullptr);
  _gcThread.reset(new V8GcThread(this));
  _gcThread->start();

  _gcFinished = false;
}

void V8DealerFeature::prepareLockedContext(TRI_vocbase_t* vocbase,
                                           V8Context* context,
                                           bool allowUseDatabase) {
  TRI_ASSERT(vocbase != nullptr);

  // when we get here, we should have a context and an isolate
  context->assertLocked();

  auto isolate = context->_isolate;

  {
    v8::HandleScope scope(isolate);
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      context->assertLocked();
      TRI_GET_GLOBALS();

      // initialize the context data
      v8g->_query = nullptr;
      v8g->_vocbase = vocbase;
      v8g->_allowUseDatabase = allowUseDatabase;

      try {
        LOG_TOPIC(TRACE, arangodb::Logger::V8) << "entering V8 context #" << context->id();
        context->handleGlobalContextMethods();
      } catch (...) {
        // ignore errors here
      }
    }
  }
}

/// @brief forceContext == -1 means that any free context may be
/// picked, or a new one will be created if we have not exceeded
/// the maximum number of contexts
/// forceContext == -2 means that any free context may be picked,
/// or a new one will be created if we have not exceeded or exactly
/// reached the maximum number of contexts. this can be used to
/// force the creation of another context for high priority tasks
/// forceContext >= 0 means picking the context with that exact id
V8Context* V8DealerFeature::enterContext(TRI_vocbase_t* vocbase,
                                         bool allowUseDatabase,
                                         ssize_t forceContext) {
  TRI_ASSERT(vocbase != nullptr);

  if (_stopping) {
    return nullptr;
  }

  if (!vocbase->use()) {
    return nullptr;
  }

  TimedAction exitWhenNoContext([](double waitTime) {
    LOG_TOPIC(WARN, arangodb::Logger::V8) << "giving up waiting for unused V8 context after " << Logger::FIXED(waitTime) << " s";
  }, 60);


  V8Context* context = nullptr;

  // this is for TESTING / DEBUGGING / INIT only
  if (forceContext >= 0) {
    size_t id = static_cast<size_t>(forceContext);

    while (!_stopping) {
      {
        CONDITION_LOCKER(guard, _contextCondition);

        if (_stopping) {
          break;
        }

        for (auto it = _idleContexts.begin(); it != _idleContexts.end(); ++it) {
          if ((*it)->id() == id) {
            context = (*it);
            _idleContexts.erase(it);
            _busyContexts.emplace(context);
            break;
          }
        }

        if (context == nullptr) {
          // still not found
          for (auto it = _dirtyContexts.begin(); it != _dirtyContexts.end(); ++it) {
            if ((*it)->id() == id) {
              context = (*it);
              _dirtyContexts.erase(it);
              _busyContexts.emplace(context);
              break;
            }
          }
        }

        if (context != nullptr) {
          // found the context
          TRI_ASSERT(guard.isLocked());
          break;
        }

        // check if such context exists at all
        bool found = false;
        for (auto& it : _contexts) {
          if (it->id() == id) {
            found = true;
            break;
          }
        }

        if (!found) {
          vocbase->release();
          LOG_TOPIC(WARN, arangodb::Logger::V8) << "specified V8 context #" << id << " not found";
          return nullptr;
        }
      }

      LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "waiting for V8 context #" << id << " to become available";
      std::this_thread::sleep_for(std::chrono::microseconds(50 * 1000));
    }

    if (context == nullptr) {
      vocbase->release();
      return nullptr;
    }
  }

  // look for a free context
  else {
    CONDITION_LOCKER(guard, _contextCondition);

    while (_idleContexts.empty() && !_stopping) {
      TRI_ASSERT(guard.isLocked());

      LOG_TOPIC(TRACE, arangodb::Logger::V8) << "waiting for unused V8 context";

      if (!_dirtyContexts.empty()) {
        // we'll use a dirty context in this case
        _idleContexts.push_back(_dirtyContexts.back());
        _dirtyContexts.pop_back();
        break;
      }

      bool const contextLimitNotExceeded = (_contexts.size() + _nrInflightContexts < _nrMaxContexts);

      if (contextLimitNotExceeded &&
          _dynamicContextCreationBlockers == 0) {
        ++_nrInflightContexts;

        TRI_ASSERT(guard.isLocked());
        guard.unlock();

        try {
          LOG_TOPIC(DEBUG, Logger::V8) << "creating additional V8 context";
          context = addContext();
        } catch (...) {
          guard.lock();

          // clean up state
          --_nrInflightContexts;
          throw;
        }

        // must re-lock
        TRI_ASSERT(!guard.isLocked());
        guard.lock();

        --_nrInflightContexts;
        try {
          _contexts.push_back(context);
        } catch (...) {
          // oops
          delete context;
          context = nullptr;
          continue;
        }

        TRI_ASSERT(guard.isLocked());
        try {
          _idleContexts.push_back(context);
          LOG_TOPIC(DEBUG, Logger::V8) << "created additional V8 context #" << context->id() << ", number of contexts is now " << _contexts.size();
        } catch (...) {
          TRI_ASSERT(!_contexts.empty());
          _contexts.pop_back();
          TRI_ASSERT(context != nullptr);
          delete context;
        }

        guard.broadcast();
        continue;
      }

      TRI_ASSERT(guard.isLocked());
      guard.wait(100000);

      if (exitWhenNoContext.tick()) {
        vocbase->release();
        return nullptr;
      }
    }

    TRI_ASSERT(guard.isLocked());

    // in case we are in the shutdown phase, do not enter a context!
    // the context might have been deleted by the shutdown
    if (_stopping) {
      vocbase->release();
      return nullptr;
    }

    TRI_ASSERT(!_idleContexts.empty());

    context = _idleContexts.back();
    LOG_TOPIC(TRACE, arangodb::Logger::V8) << "found unused V8 context #" << context->id();
    TRI_ASSERT(context != nullptr);

    _idleContexts.pop_back();

    // should not fail because we reserved enough space beforehand
    _busyContexts.emplace(context);
  }

  TRI_ASSERT(context != nullptr);
  context->lockAndEnter();
  context->assertLocked();

  prepareLockedContext(vocbase, context, allowUseDatabase);
  return context;
}

void V8DealerFeature::exitContextInternal(V8Context* context) {
  TRI_DEFER(context->unlockAndExit());
  cleanupLockedContext(context);
}

void V8DealerFeature::cleanupLockedContext(V8Context* context) {
  TRI_ASSERT(context != nullptr);

  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "leaving V8 context #" << context->id();

  auto isolate = context->_isolate;
  TRI_ASSERT(isolate != nullptr);
  context->assertLocked();

  bool canceled = false;

  if (V8PlatformFeature::isOutOfMemory(isolate)) {
    static double const availableTime = 300.0;

    v8::HandleScope scope(isolate);
    {
      auto localContext =
          v8::Local<v8::Context>::New(isolate, context->_context);
      localContext->Enter();

      {
        v8::Context::Scope contextScope(localContext);
        TRI_RunGarbageCollectionV8(isolate, availableTime);
      }

      // needs to be reset after the garbage collection
      V8PlatformFeature::resetOutOfMemory(isolate);

      localContext->Exit();
    }
  }

  // update data for later garbage collection
  {
    TRI_GET_GLOBALS();
    context->_hasActiveExternals = v8g->hasActiveExternals();
    TRI_vocbase_t* vocbase = v8g->_vocbase;

    TRI_ASSERT(vocbase != nullptr);
    // release last recently used vocbase
    vocbase->release();

    // check for cancelation requests
    canceled = v8g->_canceled;
    v8g->_canceled = false;
  }

  // check if we need to execute global context methods
  bool const runGlobal = context->hasGlobalMethodsQueued();

  {
    v8::HandleScope scope(isolate);

    // if the execution was canceled, we need to cleanup
    if (canceled) {
      context->handleCancelationCleanup();
    }

    // run global context methods
    if (runGlobal) {
      context->assertLocked();

      try {
        context->handleGlobalContextMethods();
      } catch (...) {
        // ignore errors here
      }
    }

    TRI_GET_GLOBALS();

    // reset the context data; gc should be able to run without it
    v8g->_query = nullptr;
    v8g->_vocbase = nullptr;
    v8g->_allowUseDatabase = false;

    // now really exit
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Exit();
  }
}

void V8DealerFeature::exitContext(V8Context* context) {
  cleanupLockedContext(context);

  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());

  if (gc != nullptr) {
    // default is no garbage collection
    bool performGarbageCollection = false;
    bool forceGarbageCollection = false;

    // postpone garbage collection for standard contexts
    double lastGc = gc->getLastGcStamp();
    if (context->_lastGcStamp + _gcFrequency < lastGc) {
      performGarbageCollection = true;
      if (context->_lastGcStamp + 30 * _gcFrequency < lastGc) {
        // force the GC, so that it happens eventually
        forceGarbageCollection = true;
        LOG_TOPIC(TRACE, arangodb::Logger::V8) << "V8 context #" << context->id()
                  << " has reached GC timeout threshold and will be forced into GC";
      } else {
        LOG_TOPIC(TRACE, arangodb::Logger::V8) << "V8 context #" << context->id()
                  << " has reached GC timeout threshold and will be scheduled for GC";
      }
    } else if (context->invocationsSinceLastGc() >= _gcInterval) {
      LOG_TOPIC(TRACE, arangodb::Logger::V8)
          << "V8 context #" << context->id() << " has reached maximum number of requests and will "
             "be scheduled for GC";
      performGarbageCollection = true;
    }

    context->unlockAndExit();
    CONDITION_LOCKER(guard, _contextCondition);

    if (performGarbageCollection && (forceGarbageCollection || !_idleContexts.empty())) {
      // only add the context to the dirty list if there is at least one other
      // free context

      // note that re-adding the context here should not fail as we reserved
      // enough room for all contexts during startup
      _dirtyContexts.emplace_back(context);
    } else {
      // note that re-adding the context here should not fail as we reserved
      // enough room for all contexts during startup
      _idleContexts.emplace_back(context);
    }

    _busyContexts.erase(context);

    LOG_TOPIC(TRACE, arangodb::Logger::V8) << "returned dirty V8 context #" << context->id();
    guard.broadcast();
  } else {
    context->unlockAndExit();
    CONDITION_LOCKER(guard, _contextCondition);

    _busyContexts.erase(context);
    // note that re-adding the context here should not fail as we reserved
    // enough room for all contexts during startup
    _idleContexts.emplace_back(context);

    LOG_TOPIC(TRACE, arangodb::Logger::V8) << "returned dirty V8 context #" << context->id() << " back into free";
    guard.broadcast();
  }
}

void V8DealerFeature::defineContextUpdate(
    std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)> func,
    TRI_vocbase_t* vocbase) {
  _contextUpdates.emplace_back(func, vocbase);
}

// apply context update is only run on contexts that no other
// threads can see (yet)
void V8DealerFeature::applyContextUpdate(V8Context* context) {
  auto* sysDbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();

  for (auto& p : _contextUpdates) {
    auto vocbase = p.second;

    if (vocbase == nullptr) {
      vocbase = sysDbFeature ? sysDbFeature->use().get() : nullptr;

      if (!vocbase) {
        continue;
      }
    }

    if (!vocbase->use()) {
      // oops
      continue;
    }

    context->lockAndEnter();
    prepareLockedContext(vocbase, context, true);
    TRI_DEFER(exitContextInternal(context));

    {
      v8::HandleScope scope(context->_isolate);
      auto localContext =
          v8::Local<v8::Context>::New(context->_isolate, context->_context);
      localContext->Enter();

      {
        v8::Context::Scope contextScope(localContext);
        p.first(context->_isolate, localContext, context->id());
      }

      localContext->Exit();
    }

    LOG_TOPIC(TRACE, arangodb::Logger::V8) << "updated V8 context #" << context->id();
  }
}

void V8DealerFeature::shutdownContexts() {
  _stopping = true;

  // wait for all contexts to finish
  {
    CONDITION_LOCKER(guard, _contextCondition);
    guard.broadcast();

    for (size_t n = 0; n < 10 * 5; ++n) {
      if (_busyContexts.empty()) {
        LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "no busy V8 contexts";
        break;
      }

      LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "waiting for busy V8 contexts (" << _busyContexts.size()
                 << ") to finish ";

      guard.wait(100 * 1000);
    }
  }

  // send all busy contexts a terminate signal
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (auto& it : _busyContexts) {
      LOG_TOPIC(WARN, arangodb::Logger::V8) << "sending termination signal to V8 context #" << it->id();
      v8::V8::TerminateExecution(it->_isolate);
    }
  }

  // wait for one minute
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (size_t n = 0; n < 10 * 60; ++n) {
      if (_busyContexts.empty()) {
        break;
      }

      guard.wait(100000);
    }
  }

  if (!_busyContexts.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8) << "cannot shutdown V8 contexts";
    FATAL_ERROR_EXIT();
  }

  // stop GC thread
  if (_gcThread != nullptr) {
    LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "waiting for V8 GC thread to finish action";
    _gcThread->beginShutdown();

    // wait until garbage collector thread is done
    while (!_gcFinished) {
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }

    LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "commanding V8 GC thread to terminate";
  }

  // shutdown all instances
  {
    std::vector<V8Context*> contexts;
    {
      CONDITION_LOCKER(guard, _contextCondition);
      contexts = _contexts;
      _contexts.clear();
    }

    for (auto& context : contexts) {
      shutdownContext(context);
    }
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "V8 contexts are shut down";
}

V8Context* V8DealerFeature::pickFreeContextForGc() {
  int const n = static_cast<int>(_idleContexts.size());

  if (n == 0) {
    // this is easy...
    return nullptr;
  }

  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());
  TRI_ASSERT(gc != nullptr);

  // we got more than 1 context to clean up, pick the one with the "oldest" GC
  // stamp
  int pickedContextNr =
      -1;  // index of context with lowest GC stamp, -1 means "none"

  for (int i = n - 1; i > 0; --i) {
    // check if there's actually anything to clean up in the context
    if (_idleContexts[i]->invocationsSinceLastGc() < 50 &&
        !_idleContexts[i]->_hasActiveExternals) {
      continue;
    }

    // compare last GC stamp
    if (pickedContextNr == -1 ||
        _idleContexts[i]->_lastGcStamp <=
            _idleContexts[pickedContextNr]->_lastGcStamp) {
      pickedContextNr = i;
    }
  }

  // we now have the context to clean up in pickedContextNr

  if (pickedContextNr == -1) {
    // no context found
    return nullptr;
  }

  // this is the context to clean up
  V8Context* context = _idleContexts[pickedContextNr];
  TRI_ASSERT(context != nullptr);

  // now compare its last GC timestamp with the last global GC stamp
  if (context->_lastGcStamp + _gcFrequency >= gc->getLastGcStamp()) {
    // no need yet to clean up the context
    return nullptr;
  }

  // we'll pop the context from the vector. the context might be at
  // any position in the vector so we need to move the other elements
  // around
  if (n > 1) {
    for (int i = pickedContextNr; i < n - 1; ++i) {
      _idleContexts[i] = _idleContexts[i + 1];
    }
  }
  _idleContexts.pop_back();

  return context;
}

V8Context* V8DealerFeature::buildContext(size_t id) {
  V8PlatformFeature* v8platform =
      application_features::ApplicationServer::getFeature<V8PlatformFeature>(
          "V8Platform");
  TRI_ASSERT(v8platform != nullptr);

  // create isolate
  v8::Isolate* isolate = v8platform->createIsolate();
  TRI_ASSERT(isolate != nullptr);

  // pass isolate to a new context
  auto context = std::make_unique<V8Context>(id, isolate);

  try {
    // this guard will lock and enter the isolate
    // and automatically exit and unlock it when it runs out of scope
    V8ContextEntryGuard contextGuard(context.get());

    v8::HandleScope scope(isolate);

    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

    v8::Persistent<v8::Context> persistentContext;
    persistentContext.Reset(isolate, v8::Context::New(isolate, nullptr, global));
    auto localContext = v8::Local<v8::Context>::New(isolate, persistentContext);

    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      TRI_CreateV8Globals(isolate);
      context->_context.Reset(context->_isolate, localContext);

      if (context->_context.IsEmpty()) {
        LOG_TOPIC(FATAL, arangodb::Logger::V8) << "cannot initialize V8 engine";
        FATAL_ERROR_EXIT();
      }

      v8::Handle<v8::Object> globalObj = localContext->Global();
      globalObj->Set(TRI_V8_ASCII_STRING(isolate, "GLOBAL"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING(isolate, "global"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING(isolate, "root"), globalObj);

      std::string modules = "";
      std::string sep = "";

      std::vector<std::string> directories;
      directories.insert(directories.end(), _moduleDirectories.begin(),
                         _moduleDirectories.end());
      directories.emplace_back(_startupDirectory);
      if (!_nodeModulesDirectory.empty() && _nodeModulesDirectory != _startupDirectory) {
        directories.emplace_back(_nodeModulesDirectory);
      }

      for (auto const& directory : directories) {
        modules += sep;
        sep = ";";

        modules += FileUtils::buildFilename(directory, "server/modules") + sep +
                   FileUtils::buildFilename(directory, "common/modules") + sep +
                   FileUtils::buildFilename(directory, "node");
      }
      TRI_InitV8UserFunctions(isolate, localContext);
      TRI_InitV8UserStructures(isolate, localContext);
      TRI_InitV8Buffer(isolate);
      TRI_InitV8Utils(isolate, localContext, _startupDirectory, modules);
      TRI_InitV8DebugUtils(isolate, localContext);
      TRI_InitV8Shell(isolate);

      {
        v8::HandleScope scope(isolate);

        TRI_AddGlobalVariableVocbase(isolate,
                                     TRI_V8_ASCII_STRING(isolate, "APP_PATH"),
                                     TRI_V8_STD_STRING(isolate, _appPath));

        for (auto j : _definedBooleans) {
          localContext->Global()->ForceSet(TRI_V8_STD_STRING(isolate, j.first),
                                           v8::Boolean::New(isolate, j.second),
                                           v8::ReadOnly);
        }

        for (auto j : _definedDoubles) {
          localContext->Global()->ForceSet(TRI_V8_STD_STRING(isolate, j.first),
                                           v8::Number::New(isolate, j.second),
                                           v8::ReadOnly);
        }

        for (auto const& j : _definedStrings) {
          localContext->Global()->ForceSet(TRI_V8_STD_STRING(isolate, j.first),
                                           TRI_V8_STD_STRING(isolate, j.second),
                                           v8::ReadOnly);
        }
      }
    }

    // and return from the context
    localContext->Exit();
  } catch (...) {
    LOG_TOPIC(WARN, Logger::V8) << "caught exception during context initialization";
    v8platform->disposeIsolate(isolate);
    throw;
  }

  // some random delay value to add as an initial garbage collection offset
  // this avoids collecting all contexts at the very same time
  double const randomWait =
      static_cast<double>(RandomGenerator::interval(0, 60));

  // initialize garbage collection for context
  context->_hasActiveExternals = true;
  context->_lastGcStamp = TRI_microtime() + randomWait;

  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "initialized V8 context #" << id;

  return context.release();
}

V8DealerFeature::Statistics V8DealerFeature::getCurrentContextNumbers() {
  CONDITION_LOCKER(guard, _contextCondition);

  return {
    _contexts.size(),
    _busyContexts.size(),
    _dirtyContexts.size(),
    _idleContexts.size(),
    _nrMaxContexts
  };
}

bool V8DealerFeature::loadJavaScriptFileInContext(TRI_vocbase_t* vocbase,
    std::string const& file, V8Context* context,
    VPackBuilder* builder) {

  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(context != nullptr);

  if (_stopping) {
    return false;
  }

  if (!vocbase->use()) {
    return false;
  }

  context->lockAndEnter();
  prepareLockedContext(vocbase, context, true);
  TRI_DEFER(exitContextInternal(context));

  try {
    loadJavaScriptFileInternal(file, context, builder);
  } catch (...) {
    LOG_TOPIC(WARN, Logger::V8) << "caught exception while executing JavaScript file '" << file << "' in context #" << context->id();
    throw;
  }

  return true;
}

void V8DealerFeature::loadJavaScriptFileInternal(std::string const& file, V8Context* context, VPackBuilder* builder) {
  v8::HandleScope scope(context->_isolate);
  auto localContext =
      v8::Local<v8::Context>::New(context->_isolate, context->_context);
  localContext->Enter();

  {
    v8::Context::Scope contextScope(localContext);

    switch (
        _startupLoader.loadScript(context->_isolate, localContext, file, builder)) {
      case JSLoader::eSuccess:
        LOG_TOPIC(TRACE, arangodb::Logger::V8) << "loaded JavaScript file '" << file << "'";
        break;
      case JSLoader::eFailLoad:
        LOG_TOPIC(FATAL, arangodb::Logger::V8) << "cannot load JavaScript file '" << file << "'";
        FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG_TOPIC(FATAL, arangodb::Logger::V8) << "error during execution of JavaScript file '" << file
                    << "'";
        FATAL_ERROR_EXIT();
        break;
    }
  }

  localContext->Exit();

  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "loaded Javascript file '" << file << "' for V8 context #" << context->id();
}

void V8DealerFeature::shutdownContext(V8Context* context) {
  TRI_ASSERT(context != nullptr);
  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "shutting down V8 context #" << context->id();

  auto isolate = context->_isolate;
  {
    // this guard will lock and enter the isolate
    // and automatically exit and unlock it when it runs out of scope
    V8ContextEntryGuard contextGuard(context);

    v8::HandleScope scope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      TRI_VisitActions([&isolate](TRI_action_t* action) {
        action->visit(isolate);
      });

      TRI_RunGarbageCollectionV8(isolate, 30.0);

      TRI_GET_GLOBALS();

      if (v8g != nullptr) {
        if (v8g->_transactionContext != nullptr) {
          delete static_cast<transaction::V8Context*>(v8g->_transactionContext);
          v8g->_transactionContext = nullptr;
        }
        delete v8g;
      }
    }

    localContext->Exit();
  }

  context->_context.Reset();

  application_features::ApplicationServer::getFeature<V8PlatformFeature>(
          "V8Platform")->disposeIsolate(isolate);

  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "closed V8 context #" << context->id();

  delete context;
}

V8ContextDealerGuard::V8ContextDealerGuard(Result& res, v8::Isolate*& isolate, TRI_vocbase_t* vocbase, bool allowModification)
  : _isolate(isolate)
  , _context(nullptr)
  , _active(isolate ? false : true)
{
  if (_active) {
    if(!vocbase){
      res.reset(TRI_ERROR_INTERNAL, "V8ContextDealerGuard - no vocbase provided");
      return;
    }
    _context = V8DealerFeature::DEALER->enterContext(vocbase, allowModification);
    if (!_context) {
      res.reset(TRI_ERROR_INTERNAL, "V8ContextDealerGuard - could not acquire context");
      return;
    }
    isolate = _context->_isolate;
  }
}

V8ContextDealerGuard::~V8ContextDealerGuard() {
  if (_active && _context) {
    try {
      V8DealerFeature::DEALER->exitContext(_context);
    }
    catch (...) {}
    _isolate = nullptr;
  }
}
