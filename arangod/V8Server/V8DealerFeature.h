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

#pragma once

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include <atomic>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/ConditionVariable.h"
#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"
#include "Utils/DatabaseGuard.h"
#include "V8/JSLoader.h"
#include "V8Server/GlobalContextMethods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
class JavaScriptSecurityContext;
class Thread;
class V8Context;

class V8DealerFeature final : public ArangodFeature {
 public:
  struct Statistics {
    size_t available;
    size_t busy;
    size_t dirty;
    size_t free;
    size_t max;
    size_t min;
  };
  struct DetailedContextStatistics {
    size_t id;
    double tMax;
    size_t countOfTimes;
    size_t heapMax;
    size_t heapMin;
    size_t invocations;
  };

  static constexpr std::string_view name() noexcept { return "V8Dealer"; }

  explicit V8DealerFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void prepare() final;
  void start() final;
  void unprepare() final;

  void verifyAppPaths();
  ErrorCode createDatabase(std::string_view name, std::string_view id,
                           bool removeExisting);
  void cleanupDatabase(TRI_vocbase_t& database);

 private:
  ErrorCode createApplicationDirectory(std::string const& name,
                                       std::string const& basePath,
                                       bool removeExisting);
  ErrorCode createBaseApplicationDirectory(std::string const& appPath,
                                           std::string const& type);

  double _gcFrequency;
  uint64_t _gcInterval;
  double _maxContextAge;
  std::string _appPath;
  std::string _startupDirectory;
  std::string _nodeModulesDirectory;
  std::vector<std::string> _moduleDirectories;
  // maximum number of contexts to create
  uint64_t _nrMaxContexts;
  // minimum number of contexts to keep
  uint64_t _nrMinContexts;
  // number of contexts currently in creation
  uint64_t _nrInflightContexts;
  // maximum number of V8 context invocations
  uint64_t _maxContextInvocations;

  // copy JavaScript files into database directory on startup
  bool _copyInstallation;
  // enable /_admin/execute API
  bool _allowAdminExecute;
  // allow JavaScript transactions?
  bool _allowJavaScriptTransactions;
  // allow JavaScript user-defined functions?
  bool _allowJavaScriptUdfs;
  // allow JavaScript tasks (tasks module)?
  bool _allowJavaScriptTasks;
  // enable JavaScript globally
  bool _enableJS;

 public:
  bool allowAdminExecute() const noexcept { return _allowAdminExecute; }
  bool allowJavaScriptTransactions() const noexcept {
    return _allowJavaScriptTransactions;
  }
  bool allowJavaScriptUdfs() const noexcept { return _allowJavaScriptUdfs; }
  bool allowJavaScriptTasks() const noexcept { return _allowJavaScriptTasks; }

  bool addGlobalContextMethod(GlobalContextMethods::MethodType type);
  void collectGarbage();

  /// @brief loads a JavaScript file in all contexts, only called at startup.
  /// if the builder pointer is not nullptr, then
  /// the Javascript result(s) are returned as VPack in the builder,
  /// the builder is not cleared and thus should be empty before the call.
  void loadJavaScriptFileInAllContexts(TRI_vocbase_t*, std::string const& file,
                                       VPackBuilder* builder);

  /// @brief enter a V8 context
  /// currently returns a nullptr if no context can be acquired in time
  V8Context* enterContext(TRI_vocbase_t*,
                          JavaScriptSecurityContext const& securityContext);
  void exitContext(V8Context*);

  void setMinimumContexts(size_t nr) {
    if (nr > _nrMinContexts) {
      _nrMinContexts = nr;
    }
  }

  uint64_t maximumContexts() const noexcept { return _nrMaxContexts; }

  void setMaximumContexts(size_t nr) noexcept { _nrMaxContexts = nr; }

  Statistics getCurrentContextNumbers();
  std::vector<DetailedContextStatistics> getCurrentContextDetails();

  void defineBoolean(std::string const& name, bool value) {
    _definedBooleans[name] = value;
  }

  void defineDouble(std::string const& name, double value) {
    _definedDoubles[name] = value;
  }

  std::string const& appPath() const { return _appPath; }

  static bool javascriptRequestedViaOptions(
      std::shared_ptr<options::ProgramOptions> const& options);

 private:
  uint64_t nextId() { return _nextId++; }
  void copyInstallationFiles();
  void startGarbageCollection();
  std::unique_ptr<V8Context> addContext();
  std::unique_ptr<V8Context> buildContext(TRI_vocbase_t* vocbase, size_t id);
  V8Context* pickFreeContextForGc();
  void shutdownContext(V8Context* context);
  void unblockDynamicContextCreation();
  void loadJavaScriptFileInternal(std::string const& file, V8Context* context,
                                  VPackBuilder* builder);
  bool loadJavaScriptFileInContext(TRI_vocbase_t*, std::string const& file,
                                   V8Context* context, VPackBuilder* builder);
  void prepareLockedContext(TRI_vocbase_t*, V8Context*,
                            JavaScriptSecurityContext const&);
  void exitContextInternal(V8Context*);
  void cleanupLockedContext(V8Context*);
  void applyContextUpdate(V8Context* context);
  void shutdownContexts();

  std::atomic<uint64_t> _nextId;

  std::unique_ptr<Thread> _gcThread;
  std::atomic<bool> _stopping;
  std::atomic<bool> _gcFinished;

  basics::ConditionVariable _contextCondition;
  std::vector<V8Context*> _contexts;
  std::vector<V8Context*> _idleContexts;
  std::vector<V8Context*> _dirtyContexts;
  std::unordered_set<V8Context*> _busyContexts;
  size_t _dynamicContextCreationBlockers;

  JSLoader _startupLoader;

  std::map<std::string, bool> _definedBooleans;
  std::map<std::string, double> _definedDoubles;
  std::map<std::string, std::string> _definedStrings;

  metrics::Counter& _contextsCreationTime;
  metrics::Counter& _contextsCreated;
  metrics::Counter& _contextsDestroyed;
  metrics::Counter& _contextsEntered;
  metrics::Counter& _contextsExited;
  metrics::Counter& _contextsEnterFailures;
};

/// @brief enters and exits a context and provides an isolate
/// throws an exception when no context can be provided
class V8ContextGuard {
 public:
  explicit V8ContextGuard(TRI_vocbase_t*, JavaScriptSecurityContext const&);
  V8ContextGuard(V8ContextGuard const&) = delete;
  V8ContextGuard& operator=(V8ContextGuard const&) = delete;
  ~V8ContextGuard();

  v8::Isolate* isolate() const { return _isolate; }
  V8Context* context() const { return _context; }

 private:
  TRI_vocbase_t* _vocbase;
  v8::Isolate* _isolate;
  V8Context* _context;
};

// enters and exits a context and provides an isolate
// in case the passed in isolate is a nullptr
class V8ConditionalContextGuard {
 public:
  explicit V8ConditionalContextGuard(Result&, v8::Isolate*&, TRI_vocbase_t*,
                                     JavaScriptSecurityContext const&);
  V8ConditionalContextGuard(V8ConditionalContextGuard const&) = delete;
  V8ConditionalContextGuard& operator=(V8ConditionalContextGuard const&) =
      delete;
  ~V8ConditionalContextGuard();

 private:
  TRI_vocbase_t* _vocbase;
  v8::Isolate*& _isolate;
  V8Context* _context;
  bool _active;
};

}  // namespace arangodb
