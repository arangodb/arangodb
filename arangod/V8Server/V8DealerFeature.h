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

#ifndef APPLICATION_FEATURES_V8_DEALER_FEATURE_H
#define APPLICATION_FEATURES_V8_DEALER_FEATURE_H 1

#include <atomic>

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/ConditionVariable.h"
#include "RestServer/MetricsFeature.h"
#include "V8/JSLoader.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

struct TRI_vocbase_t;

namespace arangodb {
class JavaScriptSecurityContext;
class Thread;
class V8Context;

class V8DealerFeature final : public application_features::ApplicationFeature {
 public:
  static V8DealerFeature* DEALER;

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

  explicit V8DealerFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

 private:
  double _gcFrequency;
  uint64_t _gcInterval;
  double _maxContextAge;
  std::string _appPath;
  std::string _startupDirectory;
  std::string _nodeModulesDirectory;
  std::vector<std::string> _moduleDirectories;
  bool _copyInstallation;
  uint64_t _nrMaxContexts;          // maximum number of contexts to create
  uint64_t _nrMinContexts;          // minimum number of contexts to keep
  uint64_t _nrInflightContexts;     // number of contexts currently in creation
  uint64_t _maxContextInvocations;  // maximum number of V8 context invocations
  bool _allowAdminExecute;
  bool _enableJS;

 public:
  bool allowAdminExecute() const { return _allowAdminExecute; }

  bool addGlobalContextMethod(std::string const&);
  void collectGarbage();

  // In the following two, if the builder pointer is not nullptr, then
  // the Javascript result(s) are returned as VPack in the builder,
  // the builder is not cleared and thus should be empty before the call.
  void loadJavaScriptFileInAllContexts(TRI_vocbase_t*, std::string const& file,
                                       VPackBuilder* builder);

  /// @brief enter a V8 context
  /// currently returns a nullptr if no context can be acquired in time
  V8Context* enterContext(TRI_vocbase_t*, JavaScriptSecurityContext const& securityContext);
  void exitContext(V8Context*);

  void defineContextUpdate(std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)>,
                           TRI_vocbase_t*);

  void setMinimumContexts(size_t nr) {
    if (nr > _nrMinContexts) {
      _nrMinContexts = nr;
    }
  }

  uint64_t maximumContexts() const { return _nrMaxContexts; }

  void setMaximumContexts(size_t nr) { _nrMaxContexts = nr; }

  Statistics getCurrentContextNumbers();
  std::vector<DetailedContextStatistics> getCurrentContextDetails();

  void defineBoolean(std::string const& name, bool value) {
    _definedBooleans[name] = value;
  }

  void defineDouble(std::string const& name, double value) {
    _definedDoubles[name] = value;
  }

  std::string const& appPath() const { return _appPath; }

 private:
  uint64_t nextId() { return _nextId++; }
  void copyInstallationFiles();
  void startGarbageCollection();
  V8Context* addContext();
  V8Context* buildContext(size_t id);
  V8Context* pickFreeContextForGc();
  void shutdownContext(V8Context* context);
  void unblockDynamicContextCreation();
  void loadJavaScriptFileInternal(std::string const& file, V8Context* context,
                                  VPackBuilder* builder);
  bool loadJavaScriptFileInContext(TRI_vocbase_t*, std::string const& file,
                                   V8Context* context, VPackBuilder* builder);
  void prepareLockedContext(TRI_vocbase_t*, V8Context*, JavaScriptSecurityContext const&);
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

  std::vector<std::pair<std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)>, TRI_vocbase_t*>> _contextUpdates;

  Counter& _contextsCreated;
  Counter& _contextsDestroyed;
  Counter& _contextsEntered;
  Counter& _contextsExited;
  Counter& _contextsEnterFailures;
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
  v8::Isolate* _isolate;
  V8Context* _context;
};

// enters and exits a context and provides an isolate
// in case the passed in isolate is a nullptr
class V8ConditionalContextGuard {
 public:
  explicit V8ConditionalContextGuard(Result&, v8::Isolate*&, TRI_vocbase_t*, JavaScriptSecurityContext const&);
  V8ConditionalContextGuard(V8ConditionalContextGuard const&) = delete;
  V8ConditionalContextGuard& operator=(V8ConditionalContextGuard const&) = delete;
  ~V8ConditionalContextGuard();

 private:
  v8::Isolate*& _isolate;
  V8Context* _context;
  bool _active;
};

}  // namespace arangodb

#endif
