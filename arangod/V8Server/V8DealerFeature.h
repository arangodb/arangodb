////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/Result.h"
#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"
#include "Utils/DatabaseGuard.h"
#include "V8/JSLoader.h"
#include "V8Server/GlobalExecutorMethods.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
class JavaScriptSecurityContext;
class Thread;
class V8Executor;

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
  struct DetailedExecutorStatistics {
    size_t id;
    double tMax;
    size_t countOfTimes;
    size_t heapMax;
    size_t heapMin;
    size_t invocations;
  };

  static constexpr std::string_view name() noexcept { return "V8Dealer"; }

  V8DealerFeature(Server& server, metrics::MetricsFeature& metrics);

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
  double _maxExecutorAge;
  std::string _appPath;
  std::string _startupDirectory;
  std::string _nodeModulesDirectory;
  std::vector<std::string> _moduleDirectories;
  // maximum number of executors to create
  uint64_t _nrMaxExecutors;
  // minimum number of executors to keep
  uint64_t _nrMinExecutors;
  // number of executors currently in creation
  uint64_t _nrInflightExecutors;
  // maximum number of V8 context invocations
  uint64_t _maxExecutorInvocations;

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

  bool addGlobalExecutorMethod(GlobalExecutorMethods::MethodType type);
  void collectGarbage();

  /// @brief loads a JavaScript file in all executors, only called at startup.
  /// if the builder pointer is not nullptr, then
  /// the Javascript result(s) are returned as VPack in the builder,
  /// the builder is not cleared and thus should be empty before the call.
  void loadJavaScriptFileInAllExecutors(TRI_vocbase_t*, std::string const& file,
                                        velocypack::Builder* builder);

  /// @brief enter a V8 executor
  /// currently returns a nullptr if no executor can be acquired in time
  V8Executor* enterExecutor(TRI_vocbase_t*,
                            JavaScriptSecurityContext const& securityContext);
  void exitExecutor(V8Executor* executor);

  void setMinimumExecutors(size_t nr) {
    if (nr > _nrMinExecutors) {
      _nrMinExecutors = nr;
    }
  }

  uint64_t maximumExecutors() const noexcept { return _nrMaxExecutors; }

  void setMaximumExecutors(size_t nr) noexcept { _nrMaxExecutors = nr; }

  Statistics getCurrentExecutorStatistics();
  std::vector<DetailedExecutorStatistics> getCurrentExecutorDetails();

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
  std::unique_ptr<V8Executor> addExecutor();
  std::unique_ptr<V8Executor> buildExecutor(TRI_vocbase_t* vocbase, size_t id);
  V8Executor* pickFreeExecutorForGc();
  void shutdownExecutor(V8Executor* executor);
  void unblockDynamicExecutorCreation();
  void loadJavaScriptFileInternal(std::string const& file, V8Executor* executor,
                                  velocypack::Builder* builder);
  void loadJavaScriptFileInExecutor(TRI_vocbase_t*, std::string const& file,
                                    V8Executor* executor,
                                    velocypack::Builder* builder);
  void prepareLockedExecutor(TRI_vocbase_t*, V8Executor* executor,
                             JavaScriptSecurityContext const&);
  void exitExecutorInternal(V8Executor* executor);
  void cleanupLockedExecutor(V8Executor* executor);
  void applyExecutorUpdate(V8Executor* executor);
  void shutdownExecutors();

  std::atomic<uint64_t> _nextId;

  std::unique_ptr<Thread> _gcThread;
  std::atomic<bool> _stopping;
  std::atomic<bool> _gcFinished;

  basics::ConditionVariable _executorsCondition;
  std::vector<V8Executor*> _executors;
  std::vector<V8Executor*> _idleExecutors;
  std::vector<V8Executor*> _dirtyExecutors;
  std::unordered_set<V8Executor*> _busyExecutors;
  size_t _dynamicExecutorCreationBlockers;

  JSLoader _startupLoader;

  std::map<std::string, bool> _definedBooleans;
  std::map<std::string, double> _definedDoubles;
  std::map<std::string, std::string> _definedStrings;

  metrics::Counter& _executorsCreationTime;
  metrics::Counter& _executorsCreated;
  metrics::Counter& _executorsDestroyed;
  metrics::Counter& _executorsEntered;
  metrics::Counter& _executorsExited;
  metrics::Counter& _executorsEnterFailures;
};

/// @brief enters and exits an executor and provides an isolate
/// throws an exception when no executor can be provided
class V8ExecutorGuard {
 public:
  explicit V8ExecutorGuard(TRI_vocbase_t*, JavaScriptSecurityContext const&);
  V8ExecutorGuard(V8ExecutorGuard const&) = delete;
  V8ExecutorGuard& operator=(V8ExecutorGuard const&) = delete;
  ~V8ExecutorGuard();

  v8::Isolate* isolate() const noexcept { return _isolate; }
  Result runInContext(std::function<Result(v8::Isolate*)> const& cb,
                      bool executeGlobalMethods = true);

 private:
  TRI_vocbase_t* _vocbase;
  v8::Isolate* _isolate;
  V8Executor* _executor;
};

// enters and exits an executor and provides an isolate
// in case the passed in isolate is a nullptr
class V8ConditionalExecutorGuard {
 public:
  explicit V8ConditionalExecutorGuard(TRI_vocbase_t*,
                                      JavaScriptSecurityContext const&);
  V8ConditionalExecutorGuard(V8ConditionalExecutorGuard const&) = delete;
  V8ConditionalExecutorGuard& operator=(V8ConditionalExecutorGuard const&) =
      delete;
  ~V8ConditionalExecutorGuard();

  v8::Isolate* isolate() const noexcept { return _isolate; }
  Result runInContext(std::function<Result(v8::Isolate*)> const& cb,
                      bool executeGlobalMethods = true);

 private:
  TRI_vocbase_t* _vocbase;
  v8::Isolate* _isolate;
  V8Executor* _executor;
};

}  // namespace arangodb
