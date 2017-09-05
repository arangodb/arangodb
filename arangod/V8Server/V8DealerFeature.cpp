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

#include <thread>

#include "3rdParty/valgrind/valgrind.h"

#include "Actions/actions.h"
#include "ApplicationFeatures/MaxMapCountFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/ConditionLocker.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/TimedAction.h"
#include "Basics/WorkMonitor.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
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
#include "V8Server/v8-user-structures.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

V8DealerFeature* V8DealerFeature::DEALER = nullptr;

static thread_local bool alreadyLockedInThread = false;

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
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "V8Dealer"),
      _gcFrequency(30.0),
      _gcInterval(1000),
      _maxContextAge(60.0),
      _nrMaxContexts(0),
      _nrMinContexts(0),
      _nrInflightContexts(0),
      _maxContextInvocations(0),
      _allowAdminExecute(false),
      _ok(false),
      _nextId(0),
      _stopping(false),
      _gcFinished(false),
      _nrAdditionalContexts(0),
      _minimumContexts(1),
      _forceNrContexts(0),
      _contextsModificationBlockers(0) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Action");
  startsAfter("Authentication");
  startsAfter("Database");
  startsAfter("Random");
  startsAfter("Scheduler");
  startsAfter("V8Platform");
  startsAfter("WorkMonitor");
}

void V8DealerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption(
      "--javascript.gc-frequency",
      "JavaScript time-based garbage collection frequency (each x seconds)",
      new DoubleParameter(&_gcFrequency));

  options->addHiddenOption(
      "--javascript.gc-interval",
      "JavaScript request-based garbage collection interval (each x requests)",
      new UInt64Parameter(&_gcInterval));
  
  options->addOption("--javascript.app-path", "directory for Foxx applications",
                     new StringParameter(&_appPath));

  options->addOption(
      "--javascript.startup-directory",
      "path to the directory containing JavaScript startup scripts",
      new StringParameter(&_startupDirectory));

  options->addHiddenOption(
      "--javascript.module-directory",
      "additional paths containing JavaScript modules",
      new VectorParameter<StringParameter>(&_moduleDirectory));

  options->addOption(
      "--javascript.v8-contexts",
      "maximum number of V8 contexts that are created for executing JavaScript actions",
      new UInt64Parameter(&_nrMaxContexts));
  
  options->addOption(
      "--javascript.v8-contexts-minimum",
      "minimum number of V8 contexts that keep available for executing JavaScript actions",
      new UInt64Parameter(&_nrMinContexts));
  
  options->addHiddenOption(
      "--javascript.v8-contexts-max-invocations",
      "maximum number of invocations for each V8 context before it is disposed",
      new UInt64Parameter(&_maxContextInvocations));
  
  options->addHiddenOption(
      "--javascript.v8-contexts-max-age",
      "maximum age for each V8 context (in seconds) before it is disposed",
      new DoubleParameter(&_maxContextAge));

  options->addHiddenOption(
      "--javascript.allow-admin-execute",
      "for testing purposes allow '_admin/execute', NEVER enable on production",
      new BooleanParameter(&_allowAdminExecute));
}

void V8DealerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check the startup path
  if (_startupDirectory.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8)
        << "no 'javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  // remove trailing / from path and set path
  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::V8) << "failed to get global context";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_startupDirectory, "javascript.startup-directory", true);
  ctx->normalizePath(_moduleDirectory, "javascript.module-directory", false);

  _startupLoader.setDirectory(_startupDirectory);
  ServerState::instance()->setJavaScriptPath(_startupDirectory);

  // check whether app-path was specified
  if (_appPath.empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::V8) << "no value has been specified for --javascript.app-path";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(_appPath, "javascript.app-path", true);

  // use a minimum of 1 second for GC
  if (_gcFrequency < 1) {
    _gcFrequency = 1;
  }
}

void V8DealerFeature::start() {
  // dump paths
  {
    std::vector<std::string> paths;

    paths.push_back(std::string("startup '" + _startupDirectory + "'"));

    if (!_moduleDirectory.empty()) {
      paths.push_back(std::string(
          "module '" + StringUtils::join(_moduleDirectory, ";") + "'"));
    }

    if (!_appPath.empty()) {
      paths.push_back(std::string("application '" + _appPath + "'"));
    }

    LOG_TOPIC(INFO, arangodb::Logger::V8) << "JavaScript using " << StringUtils::join(paths, ", ");
  }

  // set singleton
  DEALER = this;

  // try to guess a suitable number of contexts
  if (0 == _nrMaxContexts && 0 == _forceNrContexts) {
    SchedulerFeature* scheduler =
        ApplicationServer::getFeature<SchedulerFeature>("Scheduler");

    _nrMaxContexts = scheduler->concurrency();
  }

  // set a minimum of V8 contexts
  if (_nrMaxContexts < _minimumContexts) {
    _nrMaxContexts = _minimumContexts;
  }

  if (0 < _forceNrContexts) {
    _nrMaxContexts = _forceNrContexts;
  } else if (0 < _nrAdditionalContexts) {
    _nrMaxContexts += _nrAdditionalContexts;
  }

  if (_nrMinContexts < 1 + _nrAdditionalContexts) {
    _nrMinContexts = 1 + _nrAdditionalContexts;
  }
  if (_nrMinContexts > _nrMaxContexts) {
    _nrMinContexts = _nrMaxContexts;
  }

  LOG_TOPIC(DEBUG, Logger::V8) << "number of V8 contexts: min: " << _nrMinContexts << ", max: " << _nrMaxContexts;

  defineDouble("V8_CONTEXTS", static_cast<double>(_nrMaxContexts));
  defineBoolean("ALLOW_ADMIN_EXECUTE", _allowAdminExecute);

  // setup instances
  {
    CONDITION_LOCKER(guard, _contextCondition);
    _contexts.reserve(static_cast<size_t>(_nrMaxContexts));
    _busyContexts.reserve(static_cast<size_t>(_nrMaxContexts));
    _freeContexts.reserve(static_cast<size_t>(_nrMaxContexts));
    _dirtyContexts.reserve(static_cast<size_t>(_nrMaxContexts));
  
    for (size_t i = 0; i < _nrMinContexts; ++i) {
      V8Context* context = buildContext(nextId());
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
      applyContextUpdate(context);
      _freeContexts.push_back(context);
    }
  }


  DatabaseFeature* database =
      ApplicationServer::getFeature<DatabaseFeature>("Database");

  loadJavaScriptFileInAllContexts(database->systemDatabase(), "server/initialize.js", nullptr);

  startGarbageCollection();
}

V8Context* V8DealerFeature::addContext() {
  V8Context* context = buildContext(nextId());

  try {
    // apply context update is only run on contexts that no other
    // threads can see (yet)
    applyContextUpdate(context);

    DatabaseFeature* database =
        ApplicationServer::getFeature<DatabaseFeature>("Database");

    // no other thread can use the context when we are here, as the
    // context has not been added to the global list of contexts yet
    loadJavaScriptFileInContext(database->systemDatabase(), "server/initialize.js", context, nullptr);
    return context; 
  } catch (...) {
    delete context;
    throw;
  }
}

void V8DealerFeature::unprepare() {
  // turn off memory allocation failures before going into v8 code 
  TRI_DisallowMemoryFailures();

  shutdownContexts();

  // delete GC thread after all action threads have been stopped
  _gcThread.reset();

  DEALER = nullptr;

  // turn on memory allocation failures again
  TRI_AllowMemoryFailures();
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

  if (alreadyLockedInThread) {
    // the condition lock has already been acquired in this thread
    // we cannot detect this easily here without a thread-local variable
    // as we may be called from a JavaScript callback here
    return cb();
  } else {
    // the condition lock has not been acquired in this thread, so
    // we are responsible for locking properly!
    CONDITION_LOCKER(guard, _contextCondition);
    return cb();
  }
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

  // turn off memory allocation failures before going into v8 code 
  TRI_DisallowMemoryFailures();

  while (_stopping == 0) {
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

        if (preferFree && !_freeContexts.empty()) {
          context = pickFreeContextForGc();
        }

        if (context == nullptr && !_dirtyContexts.empty()) {
          context = _dirtyContexts.back();
          _dirtyContexts.pop_back();
          if (context->invocationsSinceLastGc() < 50 && !context->_hasActiveExternals) {
            // don't collect this one yet. it doesn't have externals, so there
            // is no urge for garbage collection
            _freeContexts.emplace_back(context);
            context = nullptr;
          } else {
            wasDirty = true;
          }
        }

        if (context == nullptr && !preferFree && !gotSignal &&
            !_freeContexts.empty()) {
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
        arangodb::CustomWorkStack custom("V8 GC", (uint64_t)context->id());

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
          V8ContextGuard contextGuard(context);

          v8::HandleScope scope(isolate);

          auto localContext =
              v8::Local<v8::Context>::New(isolate, context->_context);

          localContext->Enter();
          {
            v8::Context::Scope contextScope(localContext);

            TRI_ASSERT(context->_locker->IsLocked(isolate));
            TRI_ASSERT(v8::Locker::IsLocked(isolate));

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
              _contextsModificationBlockers == 0) {
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
              _freeContexts.emplace_back(context);
            } else {
              _freeContexts.insert(_freeContexts.begin(), context);
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
  
  // turn on memory allocation failures again
  TRI_AllowMemoryFailures();

  _gcFinished = true;
}
  
void V8DealerFeature::unblockContextsModification() {
  CONDITION_LOCKER(guard, _contextCondition);
    
  TRI_ASSERT(_contextsModificationBlockers > 0);
  --_contextsModificationBlockers;
}

void V8DealerFeature::loadJavaScriptFileInAllContexts(TRI_vocbase_t* vocbase,
    std::string const& file, VPackBuilder* builder) {
    
  alreadyLockedInThread = true;
  TRI_DEFER(alreadyLockedInThread = false);
  
  if (builder != nullptr) {
    builder->openArray();
  }
 
  std::vector<V8Context*> contexts; 
  {
    CONDITION_LOCKER(guard, _contextCondition);
    
    // block the addition or removal of contexts
    ++_contextsModificationBlockers;
  
    // copy the list of contexts into a local variable
    contexts = _contexts;
  }

  TRI_DEFER(unblockContextsModification());

  // now safely scan the local copy of the contexts  
  for (auto& context : contexts) {
    CONDITION_LOCKER(guard, _contextCondition);

    while (context->isUsed()) {
      // we must not enter the context if another thread is also using it...
      guard.wait(10000);
    }

    TRI_ASSERT(!context->isUsed());
    loadJavaScriptFileInContext(vocbase, file, context, builder);
    TRI_ASSERT(!context->isUsed());
  }

  if (builder != nullptr) {
    builder->close();
  }
}

void V8DealerFeature::loadJavaScriptFileInDefaultContext(TRI_vocbase_t* vocbase,
    std::string const& file, VPackBuilder* builder) {
  // find context with id 0
    
  alreadyLockedInThread = true;
  TRI_DEFER(alreadyLockedInThread = false);

  // enter context #0
  V8Context* context = enterContext(vocbase, true, 0);
  
  if (context == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "could not acquire default V8 context");
  }

  TRI_DEFER(exitContext(context));
  
  try {
    loadJavaScriptFileInternal(file, context, builder);
  } catch (...) {
    LOG_TOPIC(WARN, Logger::V8) << "caught exception while executing JavaScript file '" << file << "' in context #" << context->id();
    throw;
  }
}

void V8DealerFeature::startGarbageCollection() {
  TRI_ASSERT(_gcThread == nullptr);
  _gcThread.reset(new V8GcThread(this));
  _gcThread->start();

  _gcFinished = false;
}

void V8DealerFeature::enterContextInternal(TRI_vocbase_t* vocbase,
                                           V8Context* context,
                                           bool allowUseDatabase) {
  context->lockAndEnter();
  enterLockedContext(vocbase, context, allowUseDatabase);
}

void V8DealerFeature::enterLockedContext(TRI_vocbase_t* vocbase,
                                         V8Context* context,
                                         bool allowUseDatabase) {
  TRI_ASSERT(vocbase != nullptr);

  // when we get here, we should have a context and an isolate
  TRI_ASSERT(context != nullptr);
  TRI_ASSERT(context->_isolate != nullptr);
  TRI_ASSERT(context->isUsed());

  auto isolate = context->_isolate;

  // turn off memory allocation failures before going into v8 code 
  TRI_DisallowMemoryFailures();

  {
    v8::HandleScope scope(isolate);
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      TRI_ASSERT(context->_locker->IsLocked(isolate));
      TRI_ASSERT(v8::Locker::IsLocked(isolate));
      TRI_GET_GLOBALS();

      // initialize the context data
      v8g->_query = nullptr;
      v8g->_vocbase = vocbase;
      v8g->_allowUseDatabase = allowUseDatabase;

      try {
        LOG_TOPIC(TRACE, arangodb::Logger::V8) << "entering V8 context " << context->id();
        context->handleGlobalContextMethods();
      } catch (...) {
        // ignore errors here
      }
    }
  }
}

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
  if (forceContext != -1) {
    size_t id = static_cast<size_t>(forceContext);

    while (!_stopping) {
      {
        CONDITION_LOCKER(guard, _contextCondition);

        if (_stopping) {
          break;
        }

        for (auto it = _freeContexts.begin(); it != _freeContexts.end(); ++it) {
          if ((*it)->id() == id) {
            context = (*it);
            _freeContexts.erase(it);
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
          context->lockAndEnter();
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
          LOG_TOPIC(WARN, arangodb::Logger::V8) << "specified context #" << id << " not found";
          return nullptr;
        }
      }

      LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "waiting for V8 context " << id << " to become available";
      usleep(50 * 1000);
    }

    if (context == nullptr) {
      vocbase->release();
      return nullptr;
    }
  }

  // look for a free context
  else {
    CONDITION_LOCKER(guard, _contextCondition);

    while (_freeContexts.empty() && !_stopping) {
      TRI_ASSERT(guard.isLocked());

      LOG_TOPIC(TRACE, arangodb::Logger::V8) << "waiting for unused V8 context";

      if (!_dirtyContexts.empty()) {
        // we'll use a dirty context in this case
        V8Context* context = _dirtyContexts.back();
        _freeContexts.push_back(context);
        _dirtyContexts.pop_back();
        break;
      }

      if (_contexts.size() + _nrInflightContexts < _nrMaxContexts &&
          _contextsModificationBlockers == 0 && 
          !MaxMapCountFeature::isNearMaxMappings()) {
  
        ++_nrInflightContexts;

        TRI_ASSERT(guard.isLocked());
        guard.unlock();

        try {
          LOG_TOPIC(DEBUG, Logger::V8) << "creating additional V8 context";
          context = addContext();
        } catch (...) {
          guard.lock();

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

        try {
          _freeContexts.push_back(context);
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

      {
        JobGuard jobGuard(SchedulerFeature::SCHEDULER);
        jobGuard.block();
        
        TRI_ASSERT(guard.isLocked());
        guard.wait(100000);
      }

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

    LOG_TOPIC(TRACE, arangodb::Logger::V8) << "found unused V8 context";
    TRI_ASSERT(!_freeContexts.empty());

    context = _freeContexts.back();
    TRI_ASSERT(context != nullptr);

    _freeContexts.pop_back();

    // should not fail because we reserved enough space beforehand
    _busyContexts.emplace(context);

    context->lockAndEnter();
  }
  
  TRI_ASSERT(context != nullptr);
  TRI_ASSERT(context->isUsed());

  enterLockedContext(vocbase, context, allowUseDatabase);
  return context;
}

void V8DealerFeature::exitContextInternal(V8Context* context) {
  try {
    exitLockedContext(context);
    context->unlockAndExit();
  } catch (...) {
    // make sure the context will be exited
    context->unlockAndExit();
    throw;
  }
}

void V8DealerFeature::exitLockedContext(V8Context* context) {
  TRI_ASSERT(context != nullptr);

  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "leaving V8 context " << context->id();

  auto isolate = context->_isolate;
  TRI_ASSERT(isolate != nullptr);
  TRI_ASSERT(context->_locker != nullptr);
  TRI_ASSERT(context->_locker->IsLocked(isolate));
  TRI_ASSERT(v8::Locker::IsLocked(isolate));
  TRI_ASSERT(context->isUsed());

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
      TRI_ASSERT(context->_locker->IsLocked(isolate));
      TRI_ASSERT(v8::Locker::IsLocked(isolate));

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
  exitLockedContext(context);

  V8GcThread* gc = static_cast<V8GcThread*>(_gcThread.get());
  
  if (gc != nullptr) {
    // default is false
    bool performGarbageCollection = false;

    // postpone garbage collection for standard contexts
    double lastGc = gc->getLastGcStamp();
    if (context->_lastGcStamp + _gcFrequency < lastGc) {
      LOG_TOPIC(TRACE, arangodb::Logger::V8) << "V8 context has reached GC timeout threshold and will be "
                    "scheduled for GC";
      performGarbageCollection = true;
    } else if (context->invocationsSinceLastGc() >= _gcInterval) {
      LOG_TOPIC(TRACE, arangodb::Logger::V8)
          << "V8 context has reached maximum number of requests and will "
             "be scheduled for GC";
      performGarbageCollection = true;
    }

    CONDITION_LOCKER(guard, _contextCondition);
    context->unlockAndExit();

    if (performGarbageCollection && !_freeContexts.empty()) {
      // only add the context to the dirty list if there is at least one other
      // free context

      // note that re-adding the context here should not fail as we reserved
      // enough room for all contexts during startup
      _dirtyContexts.emplace_back(context);
    } else {
      // note that re-adding the context here should not fail as we reserved
      // enough room for all contexts during startup
      _freeContexts.emplace_back(context);
    }

    _busyContexts.erase(context);

    guard.broadcast();
  } else {
    CONDITION_LOCKER(guard, _contextCondition);
    
    context->unlockAndExit();

    _busyContexts.erase(context);
    // note that re-adding the context here should not fail as we reserved
    // enough room for all contexts during startup
    _freeContexts.emplace_back(context);

    guard.broadcast();
  }
  
  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "returned dirty V8 context";
  
  // turn on memory allocation failures again
  TRI_AllowMemoryFailures();
}

void V8DealerFeature::defineContextUpdate(
    std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)> func,
    TRI_vocbase_t* vocbase) {
  _contextUpdates.emplace_back(func, vocbase);
}

// apply context update is only run on contexts that no other
// threads can see (yet)
void V8DealerFeature::applyContextUpdate(V8Context* context) {
  for (auto& p : _contextUpdates) {
    auto vocbase = p.second;

    if (vocbase == nullptr) {
      vocbase = DatabaseFeature::DATABASE->systemDatabase();
    }
  
    if (!vocbase->use()) {
      // oops
      continue;
    }

    enterContextInternal(vocbase, context, true);
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
      LOG_TOPIC(WARN, arangodb::Logger::V8) << "sending termination signal to V8 context";
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
    LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "waiting for GC Thread to finish action";
    _gcThread->beginShutdown();

    // wait until garbage collector thread is done
    while (!_gcFinished) {
      usleep(10000);
    }

    LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "commanding GC Thread to terminate";
  }

  // shutdown all instances
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (auto& context : _contexts) {
      shutdownContext(context);
    }

    _contexts.clear();
  }

  LOG_TOPIC(DEBUG, arangodb::Logger::V8) << "V8 contexts are shut down";
}

V8Context* V8DealerFeature::pickFreeContextForGc() {
  int const n = (int)_freeContexts.size();

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
    if (_freeContexts[i]->invocationsSinceLastGc() < 50 &&
        !_freeContexts[i]->_hasActiveExternals) {
      continue;
    }

    // compare last GC stamp
    if (pickedContextNr == -1 ||
        _freeContexts[i]->_lastGcStamp <=
            _freeContexts[pickedContextNr]->_lastGcStamp) {
      pickedContextNr = i;
    }
  }

  // we now have the context to clean up in pickedContextNr

  if (pickedContextNr == -1) {
    // no context found
    return nullptr;
  }

  // this is the context to clean up
  V8Context* context = _freeContexts[pickedContextNr];
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
      _freeContexts[i] = _freeContexts[i + 1];
    }
  }
  _freeContexts.pop_back();

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
    V8ContextGuard contextGuard(context.get());

    v8::HandleScope handleScope(isolate);

    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

    v8::Persistent<v8::Context> persistentContext;
    persistentContext.Reset(isolate, v8::Context::New(isolate, 0, global));
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
      globalObj->Set(TRI_V8_ASCII_STRING("GLOBAL"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("global"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("root"), globalObj);

      std::string modules = "";
      std::string sep = "";

      std::vector<std::string> directories;
      directories.insert(directories.end(), _moduleDirectory.begin(),
                         _moduleDirectory.end());
      directories.emplace_back(_startupDirectory);

      for (auto directory : directories) {
        modules += sep;
        sep = ";";

        modules += FileUtils::buildFilename(directory, "server/modules") + sep +
                   FileUtils::buildFilename(directory, "common/modules") + sep +
                   FileUtils::buildFilename(directory, "node");
      }

      TRI_InitV8UserStructures(isolate, localContext);
      TRI_InitV8Buffer(isolate, localContext);
      TRI_InitV8Utils(isolate, localContext, _startupDirectory, modules);
      TRI_InitV8DebugUtils(isolate, localContext, _startupDirectory, modules);
      TRI_InitV8Shell(isolate, localContext);

      {
        v8::HandleScope scope(isolate);

        TRI_AddGlobalVariableVocbase(isolate,
                                     TRI_V8_ASCII_STRING("APP_PATH"),
                                     TRI_V8_STD_STRING(_appPath));

        for (auto j : _definedBooleans) {
          localContext->Global()->ForceSet(TRI_V8_STD_STRING(j.first),
                                           v8::Boolean::New(isolate, j.second),
                                           v8::ReadOnly);
        }

        for (auto j : _definedDoubles) {
          localContext->Global()->ForceSet(TRI_V8_STD_STRING(j.first),
                                           v8::Number::New(isolate, j.second),
                                           v8::ReadOnly);
        }

        for (auto j : _definedStrings) {
          localContext->Global()->ForceSet(TRI_V8_STD_STRING(j.first),
                                           TRI_V8_STD_STRING(j.second),
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

  enterContextInternal(vocbase, context, true);

  try {
    loadJavaScriptFileInternal(file, context, builder);
  } catch (...) {
    LOG_TOPIC(WARN, Logger::V8) << "caught exception while executing JavaScript file '" << file << "' in context #" << context->id();
    exitContextInternal(context);
    throw;
  }
  
  exitContextInternal(context);
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
  
  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "loaded Javascript file '" << file << "' for V8 context #" << context->id();
}

void V8DealerFeature::shutdownContext(V8Context* context) {
  TRI_ASSERT(context != nullptr);
  LOG_TOPIC(TRACE, arangodb::Logger::V8) << "shutting down V8 context #" << context->id();

  auto isolate = context->_isolate;
  {
    // this guard will lock and enter the isolate
    // and automatically exit and unlock it when it runs out of scope
    V8ContextGuard contextGuard(context);

    v8::HandleScope scope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      TRI_VisitActions([&isolate](TRI_action_t* action) {
        action->visit(isolate);
      });

      double availableTime = 30.0;

      if (RUNNING_ON_VALGRIND) {
        // running under Valgrind
        availableTime *= 10;
        int tries = 0;

        while (tries++ < 10 &&
               TRI_RunGarbageCollectionV8(isolate, availableTime)) {
          if (tries > 3) {
            LOG_TOPIC(WARN, arangodb::Logger::V8) << "waiting for garbage v8 collection to end";
          }
        }
      } else {
        TRI_RunGarbageCollectionV8(isolate, availableTime);
      }

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
