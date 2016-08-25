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

#include "ApplicationFeatures/V8PlatformFeature.h"
#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WorkMonitor.h"
#include "Basics/ArangoGlobalContext.h"
#include "Cluster/ServerState.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Dispatcher/DispatcherThread.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "V8Server/V8Context.h"
#include "V8Server/v8-actions.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/vocbase.h"

#include "3rdParty/valgrind/valgrind.h"

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
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "V8Dealer"),
      _gcFrequency(30.0),
      _gcInterval(1000),
      _nrContexts(0),
      _ok(false),
      _gcThread(nullptr),
      _stopping(false),
      _gcFinished(false),
      _nrAdditionalContexts(0),
      _minimumContexts(1),
      _forceNrContexts(0) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Action");
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("QueryRegistry");
  startsAfter("Random");
  startsAfter("Recovery");
  startsAfter("Scheduler");
  startsAfter("V8Platform");
  startsAfter("WorkMonitor");
}

void V8DealerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addOption(
      "--javascript.gc-frequency",
      "JavaScript time-based garbage collection frequency (each x seconds)",
      new DoubleParameter(&_gcFrequency));

  options->addOption(
      "--javascript.gc-interval",
      "JavaScript request-based garbage collection interval (each x requests)",
      new UInt64Parameter(&_gcInterval));

  options->addOption("--javascript.app-path", "directory for Foxx applications",
                     new StringParameter(&_appPath));

  options->addOption(
      "--javascript.startup-directory",
      "path to the directory containing JavaScript startup scripts",
      new StringParameter(&_startupPath));

  options->addOption(
      "--javascript.v8-contexts",
      "number of V8 contexts that are created for executing JavaScript actions",
      new UInt64Parameter(&_nrContexts));
}

void V8DealerFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // check the startup path
  if (_startupPath.empty()) {
    LOG(FATAL)
        << "no 'javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  // remove trailing / from path and set path
  _startupPath = StringUtils::rTrim(_startupPath, TRI_DIR_SEPARATOR_STR);

  auto ctx = ArangoGlobalContext::CONTEXT;
    
  if (ctx == nullptr) {
    LOG(ERR) << "failed to get global context.  ";
    FATAL_ERROR_EXIT();
  }
  
  ctx->getCheckPath(_startupPath, "javascript.startup-directory", true);
  
  _startupLoader.setDirectory(_startupPath);
  ServerState::instance()->setJavaScriptPath(_startupPath);

  // check whether app-path was specified
  if (_appPath.empty()) {
    LOG(FATAL) << "no value has been specified for --javascript.app-path.";
    FATAL_ERROR_EXIT();
  }

  ctx->getCheckPath(_appPath, "javascript.app-directory", true);
  
  // use a minimum of 1 second for GC
  if (_gcFrequency < 1) {
    _gcFrequency = 1;
  }
}

void V8DealerFeature::start() {
  // dump paths
  {
    std::vector<std::string> paths;

    paths.push_back(std::string("startup '" + _startupPath + "'"));

    if (!_appPath.empty()) {
      paths.push_back(std::string("application '" + _appPath + "'"));
    }

    LOG(INFO) << "JavaScript using " << StringUtils::join(paths, ", ");
  }

  // set singleton
  DEALER = this;

  // try to guess a suitable number of contexts
  if (0 == _nrContexts && 0 == _forceNrContexts) {
    DispatcherFeature* dispatcher =
        ApplicationServer::getFeature<DispatcherFeature>("Dispatcher");

    _nrContexts = dispatcher->concurrency();
  }

  // set a minimum of V8 contexts
  if (_nrContexts < _minimumContexts) {
    _nrContexts = _minimumContexts;
  }

  if (0 < _forceNrContexts) {
    _nrContexts = _forceNrContexts;
  } else if (0 < _nrAdditionalContexts) {
    _nrContexts += _nrAdditionalContexts;
  }

  defineDouble("V8_CONTEXTS", static_cast<double>(_nrContexts));

  // setup instances
  {
    CONDITION_LOCKER(guard, _contextCondition);
    _contexts.resize(static_cast<size_t>(_nrContexts), nullptr);

    _busyContexts.reserve(static_cast<size_t>(_nrContexts));
    _freeContexts.reserve(static_cast<size_t>(_nrContexts));
    _dirtyContexts.reserve(static_cast<size_t>(_nrContexts));
  }

  for (size_t i = 0; i < _nrContexts; ++i) {
    initializeContext(i);
  }

  applyContextUpdates();

  DatabaseFeature* database =
      ApplicationServer::getFeature<DatabaseFeature>("Database");

  loadJavascript(database->systemDatabase(), "server/initialize.js");

  startGarbageCollection();
}

void V8DealerFeature::unprepare() {
  shutdownContexts();

  // delete GC thread after all action threads have been stopped
  delete _gcThread;

  DEALER = nullptr;
}

bool V8DealerFeature::addGlobalContextMethod(std::string const& method) {
  bool result = true;

  for (size_t i = 0; i < _nrContexts; ++i) {
    try {
      if (!_contexts[i]->addGlobalContextMethod(method)) {
        result = false;
      }
    } catch (...) {
      result = false;
    }
  }

  return result;
}

void V8DealerFeature::collectGarbage() {
  V8GcThread* gc = dynamic_cast<V8GcThread*>(_gcThread);
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

  while (_stopping == 0) {
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
        if (context->_numExecutions < 50 && !context->_hasActiveExternals) {
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
      arangodb::CustomWorkStack custom("V8 GC", (uint64_t)context->_id);

      LOG(TRACE) << "collecting V8 garbage in context #" << context->_id
                 << ", numExecutions: " << context->_numExecutions
                 << ", hasActive: " << context->_hasActiveExternals
                 << ", wasDirty: " << wasDirty;
      bool hasActiveExternals = false;
      auto isolate = context->_isolate;
      TRI_ASSERT(context->_locker == nullptr);
      context->_locker = new v8::Locker(isolate);
      isolate->Enter();
      {
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

      isolate->Exit();
      delete context->_locker;
      context->_locker = nullptr;

      // update garbage collection statistics
      context->_hasActiveExternals = hasActiveExternals;
      context->_numExecutions = 0;
      context->_lastGcStamp = lastGc;

      {
        CONDITION_LOCKER(guard, _contextCondition);

        if (wasDirty) {
          _freeContexts.emplace_back(context);
        } else {
          _freeContexts.insert(_freeContexts.begin(), context);
        }
        guard.broadcast();
      }
    } else {
      useReducedWait = false;  // sanity
    }
  }

  _gcFinished = true;
}

void V8DealerFeature::loadJavascript(TRI_vocbase_t* vocbase,
                                     std::string const& file) {
  if (1 < _nrContexts) {
    std::vector<std::thread> threads;

    for (size_t i = 0; i < _nrContexts; ++i) {
      threads.push_back(std::thread(&V8DealerFeature::loadJavascriptFiles, this,
                                    vocbase, file, i));
    }

    for (size_t i = 0; i < _nrContexts; ++i) {
      threads[i].join();
    }
  } else {
    loadJavascriptFiles(vocbase, file, 0);
  }
}

void V8DealerFeature::startGarbageCollection() {
  TRI_ASSERT(_gcThread == nullptr);
  _gcThread = new V8GcThread(this);
  _gcThread->start();

  _gcFinished = false;
}

V8Context* V8DealerFeature::enterContext(TRI_vocbase_t* vocbase,
                                         bool allowUseDatabase,
                                         ssize_t forceContext) {
  TRI_ASSERT(vocbase != nullptr);

  if (_stopping) {
    return nullptr;
  }

  V8Context* context = nullptr;

  // this is for TESTING / DEBUGGING / INIT only
  if (forceContext != -1) {
    size_t id = static_cast<size_t>(forceContext);

    if (id >= _nrContexts) {
      LOG(ERR) << "internal error, not enough contexts";
      return nullptr;
    }

    while (!_stopping) {
      {
        CONDITION_LOCKER(guard, _contextCondition);

        if (_stopping) {
          break;
        }

        for (auto iter = _freeContexts.begin(); iter != _freeContexts.end();
             ++iter) {
          if ((*iter)->_id == id) {
            context = *iter;
            _freeContexts.erase(iter);
            _busyContexts.emplace(context);
            break;
          }
        }

        if (context != nullptr) {
          break;
        }

        for (auto iter = _dirtyContexts.begin(); iter != _dirtyContexts.end();
             ++iter) {
          if ((*iter)->_id == id) {
            context = *iter;
            _dirtyContexts.erase(iter);
            _busyContexts.emplace(context);
            break;
          }
        }

        if (context != nullptr) {
          break;
        }
      }

      LOG(DEBUG) << "waiting for V8 context " << id << " to become available";
      usleep(50 * 1000);
    }

    if (context == nullptr) {
      return nullptr;
    }
  }

  // look for a free context
  else {
    CONDITION_LOCKER(guard, _contextCondition);

    while (_freeContexts.empty() && !_stopping) {
      LOG(DEBUG) << "waiting for unused V8 context";

      if (!_dirtyContexts.empty()) {
        // we'll use a dirty context in this case
        V8Context* context = _dirtyContexts.back();
        _freeContexts.push_back(context);
        _dirtyContexts.pop_back();
        break;
      }

      auto currentThread = arangodb::rest::DispatcherThread::current();

      if (currentThread != nullptr) {
        currentThread->block();
      }

      guard.wait();

      if (currentThread != nullptr) {
        currentThread->unblock();
      }
    }

    // in case we are in the shutdown phase, do not enter a context!
    // the context might have been deleted by the shutdown
    if (_stopping) {
      return nullptr;
    }

    LOG(TRACE) << "found unused V8 context";
    TRI_ASSERT(!_freeContexts.empty());

    context = _freeContexts.back();
    TRI_ASSERT(context != nullptr);

    _freeContexts.pop_back();

    // should not fail because we reserved enough space beforehand
    _busyContexts.emplace(context);
  }

  // when we get here, we should have a context and an isolate
  TRI_ASSERT(context != nullptr);
  TRI_ASSERT(context->_isolate != nullptr);
  auto isolate = context->_isolate;

  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);

  isolate->Enter();
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

      vocbase->use();

      try {
        LOG(TRACE) << "entering V8 context " << context->_id;
        context->handleGlobalContextMethods();
      } catch (...) {
        // ignore errors here
      }
    }
  }

  return context;
}

void V8DealerFeature::exitContext(V8Context* context) {
  LOG(TRACE) << "leaving V8 context " << context->_id;

  V8GcThread* gc = dynamic_cast<V8GcThread*>(_gcThread);
  double lastGc = (gc != nullptr) ? gc->getLastGcStamp() : -1.0;

  auto isolate = context->_isolate;
  TRI_ASSERT(isolate != nullptr);

  TRI_ASSERT(context->_locker->IsLocked(isolate));
  TRI_ASSERT(v8::Locker::IsLocked(isolate));

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
    ++context->_numExecutions;
    TRI_vocbase_t* vocbase = v8g->_vocbase;

    TRI_ASSERT(vocbase != nullptr);
    // release last recently used vocbase
    vocbase->release();

    // check for cancelation requests
    canceled = v8g->_canceled;
    v8g->_canceled = false;
  }

  // check if we need to execute global context methods
  bool runGlobal = false;

  {
    MUTEX_LOCKER(mutexLocker, context->_globalMethodsLock);
    runGlobal = !context->_globalMethods.empty();
  }

  // exit the context
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

      context->handleGlobalContextMethods();
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

  isolate->Exit();

  delete context->_locker;
  context->_locker = nullptr;

  TRI_ASSERT(!v8::Locker::IsLocked(isolate));

  LOG(TRACE) << "returned dirty V8 context";

  if (gc != nullptr) {
    // default is false
    bool performGarbageCollection = false;

    // postpone garbage collection for standard contexts
    if (context->_lastGcStamp + _gcFrequency < lastGc) {
      LOG(TRACE) << "V8 context has reached GC timeout threshold and will be "
                    "scheduled for GC";
      performGarbageCollection = true;
    } else if (context->_numExecutions >= _gcInterval) {
      LOG(TRACE)
          << "V8 context has reached maximum number of requests and will "
             "be scheduled for GC";
      performGarbageCollection = true;
    }

    CONDITION_LOCKER(guard, _contextCondition);

    if (performGarbageCollection && !_freeContexts.empty()) {
      // only add the context to the dirty list if there is at least one other
      // free context
      _dirtyContexts.emplace_back(context);
    } else {
      _freeContexts.emplace_back(context);
    }

    _busyContexts.erase(context);

    guard.broadcast();
  } else {
    CONDITION_LOCKER(guard, _contextCondition);

    _busyContexts.erase(context);
    _freeContexts.emplace_back(context);

    guard.broadcast();
  }
}

void V8DealerFeature::defineContextUpdate(
    std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)> func,
    TRI_vocbase_t* vocbase) {
  _contextUpdates.emplace_back(func, vocbase);
}

void V8DealerFeature::applyContextUpdates() {
  for (size_t i = 0; i < _nrContexts; ++i) {
    for (auto& p : _contextUpdates) {
      auto vocbase = p.second;

      if (vocbase == nullptr) {
        vocbase = DatabaseFeature::DATABASE->systemDatabase();
      }

      V8Context* context = V8DealerFeature::DEALER->enterContext(
          vocbase, true, static_cast<ssize_t>(i));

      if (context == nullptr) {
        LOG(FATAL) << "could not updated V8 context #" << i;
        FATAL_ERROR_EXIT();
      }

      {
        v8::HandleScope scope(context->_isolate);
        auto localContext =
            v8::Local<v8::Context>::New(context->_isolate, context->_context);
        localContext->Enter();

        {
          v8::Context::Scope contextScope(localContext);
          p.first(context->_isolate, localContext, i);
        }

        localContext->Exit();
      }

      V8DealerFeature::DEALER->exitContext(context);

      LOG(TRACE) << "updated V8 context #" << i;
    }
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
        LOG(DEBUG) << "no busy V8 contexts";
        break;
      }

      LOG(DEBUG) << "waiting for busy V8 contexts (" << _busyContexts.size()
                 << ") to finish ";

      guard.wait(100 * 1000);
    }
  }

  // send all busy contexts a terminate signal
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (auto& it : _busyContexts) {
      LOG(WARN) << "sending termination signal to V8 context";
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
    LOG(FATAL) << "cannot shutdown V8 contexts";
    FATAL_ERROR_EXIT();
  }

  // stop GC thread
  if (_gcThread != nullptr) {
    LOG(DEBUG) << "Waiting for GC Thread to finish action";
    _gcThread->beginShutdown();

    // wait until garbage collector thread is done
    while (!_gcFinished) {
      usleep(10000);
    }

    LOG(DEBUG) << "Commanding GC Thread to terminate";
  }

  // shutdown all instances
  {
    CONDITION_LOCKER(guard, _contextCondition);

    for (auto context : _contexts) {
      shutdownV8Instance(context);
    }

    _contexts.clear();
  }

  LOG(DEBUG) << "V8 contexts are shut down";
}

V8Context* V8DealerFeature::pickFreeContextForGc() {
  int const n = (int)_freeContexts.size();

  if (n == 0) {
    // this is easy...
    return nullptr;
  }

  V8GcThread* gc = dynamic_cast<V8GcThread*>(_gcThread);
  TRI_ASSERT(gc != nullptr);

  // we got more than 1 context to clean up, pick the one with the "oldest" GC
  // stamp
  int pickedContextNr =
      -1;  // index of context with lowest GC stamp, -1 means "none"

  for (int i = n - 1; i > 0; --i) {
    // check if there's actually anything to clean up in the context
    if (_freeContexts[i]->_numExecutions < 50 &&
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

void V8DealerFeature::initializeContext(size_t i) {
  CONDITION_LOCKER(guard, _contextCondition);

  V8PlatformFeature* v8platform =
      application_features::ApplicationServer::getFeature<V8PlatformFeature>(
          "V8Platform");
  TRI_ASSERT(v8platform != nullptr);

  v8::Isolate* isolate = v8platform->createIsolate();
  V8Context* context = _contexts[i] = new V8Context();

  TRI_ASSERT(context->_locker == nullptr);

  // enter a new isolate
  context->_id = i;
  context->_isolate = isolate;
  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);
  context->_isolate->Enter();

  // create the context
  {
    v8::HandleScope handle_scope(isolate);

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
        LOG(FATAL) << "cannot initialize V8 engine";
        FATAL_ERROR_EXIT();
      }

      v8::Handle<v8::Object> globalObj = localContext->Global();
      globalObj->Set(TRI_V8_ASCII_STRING("GLOBAL"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("global"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("root"), globalObj);

      std::string modulesPath = _startupPath + TRI_DIR_SEPARATOR_STR +
                                "server" + TRI_DIR_SEPARATOR_STR + "modules;" +
                                _startupPath + TRI_DIR_SEPARATOR_STR +
                                "common" + TRI_DIR_SEPARATOR_STR + "modules;" +
                                _startupPath + TRI_DIR_SEPARATOR_STR + "node";

      TRI_InitV8UserStructures(isolate, localContext);
      TRI_InitV8Buffer(isolate, localContext);
      TRI_InitV8Utils(isolate, localContext, _startupPath, modulesPath);
      TRI_InitV8DebugUtils(isolate, localContext, _startupPath, modulesPath);
      TRI_InitV8Shell(isolate, localContext);

      {
        v8::HandleScope scope(isolate);

        TRI_AddGlobalVariableVocbase(isolate, localContext,
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
  }

  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  // some random delay value to add as an initial garbage collection offset
  // this avoids collecting all contexts at the very same time
  double const randomWait =
      static_cast<double>(RandomGenerator::interval(0, 14));

  // initialize garbage collection for context
  context->_numExecutions = 0;
  context->_hasActiveExternals = true;
  context->_lastGcStamp = TRI_microtime() + randomWait;

  LOG(TRACE) << "initialized V8 context #" << i;

  _freeContexts.emplace_back(context);
}

void V8DealerFeature::loadJavascriptFiles(TRI_vocbase_t* vocbase,
                                          std::string const& file, size_t i) {
  V8Context* context = V8DealerFeature::DEALER->enterContext(
      vocbase, true, static_cast<ssize_t>(i));

  if (context == nullptr) {
    LOG(FATAL) << "could not load JavaScript files in context #" << i;
    FATAL_ERROR_EXIT();
  }

  {
    v8::HandleScope scope(context->_isolate);
    auto localContext =
        v8::Local<v8::Context>::New(context->_isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);

      switch (
          _startupLoader.loadScript(context->_isolate, localContext, file)) {
        case JSLoader::eSuccess:
          LOG(TRACE) << "loaded JavaScript file '" << file << "'";
          break;
        case JSLoader::eFailLoad:
          LOG(FATAL) << "cannot load JavaScript file '" << file << "'";
          FATAL_ERROR_EXIT();
          break;
        case JSLoader::eFailExecute:
          LOG(FATAL) << "error during execution of JavaScript file '" << file
                     << "'";
          FATAL_ERROR_EXIT();
          break;
      }
    }

    localContext->Exit();
  }
  V8DealerFeature::DEALER->exitContext(context);

  LOG(TRACE) << "loaded Javascript files for V8 context #" << i;
}

void V8DealerFeature::shutdownV8Instance(V8Context* context) {
  LOG(TRACE) << "shutting down V8 context #" << context->_id;

  auto isolate = context->_isolate;
  isolate->Enter();
  TRI_ASSERT(context->_locker == nullptr);
  context->_locker = new v8::Locker(isolate);
  {
    v8::HandleScope scope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();

    {
      v8::Context::Scope contextScope(localContext);
      double availableTime = 30.0;

      if (RUNNING_ON_VALGRIND) {
        // running under Valgrind
        availableTime *= 10;
        int tries = 0;

        while (tries++ < 10 &&
               TRI_RunGarbageCollectionV8(isolate, availableTime)) {
          if (tries > 3) {
            LOG(WARN) << "waiting for garbage v8 collection to end";
          }
        }
      } else {
        TRI_RunGarbageCollectionV8(isolate, availableTime);
      }

      TRI_GET_GLOBALS();

      if (v8g != nullptr) {
        if (v8g->_transactionContext != nullptr) {
          delete static_cast<V8TransactionContext*>(v8g->_transactionContext);
          v8g->_transactionContext = nullptr;
        }
        delete v8g;
      }
    }

    localContext->Exit();
  }
  context->_context.Reset();

  isolate->Exit();
  delete context->_locker;
  context->_locker = nullptr;

  isolate->Dispose();

  LOG(TRACE) << "closed V8 context #" << context->_id;

  delete context;
}
