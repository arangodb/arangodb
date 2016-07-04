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

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/ConditionVariable.h"
#include "V8/JSLoader.h"

struct TRI_vocbase_t;

namespace arangodb {
class Thread;
class V8Context;

class V8DealerFeature final : public application_features::ApplicationFeature {
 public:
  static V8DealerFeature* DEALER;

 public:
  explicit V8DealerFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void unprepare() override final;

 private:
  double _gcFrequency;
  uint64_t _gcInterval;
  std::string _appPath;
  std::string _startupPath;
  uint64_t _nrContexts;

 public:
  JSLoader* startupLoader() { return &_startupLoader; };
  bool addGlobalContextMethod(std::string const&);
  void collectGarbage();

  void loadJavascript(TRI_vocbase_t*, std::string const&);
  void startGarbageCollection();

  V8Context* enterContext(TRI_vocbase_t*, bool useDatabase,
                          ssize_t forceContext = -1);
  void exitContext(V8Context*);

  void defineContextUpdate(
      std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)>,
      TRI_vocbase_t*);
  void applyContextUpdates();
  void setMinimumContexts(size_t nr) { _minimumContexts = nr; }
  void setNumberContexts(size_t nr) { _forceNrContexts = nr; }
  void increaseContexts() { ++_nrAdditionalContexts; }

  void shutdownContexts();

  void defineBoolean(std::string const& name, bool value) {
    _definedBooleans[name] = value;
  }

  void defineDouble(std::string const& name, double value) {
    _definedDoubles[name] = value;
  }

  std::string const& appPath() const { return _appPath; }

  void loadJavascriptFiles(TRI_vocbase_t*, std::string const&, size_t);

 private:
  V8Context* pickFreeContextForGc();
  void initializeContext(size_t);
  void shutdownV8Instance(V8Context*);

 private:
  std::atomic<bool> _ok;

  Thread* _gcThread;
  std::atomic<bool> _stopping;
  std::atomic<bool> _gcFinished;

  std::vector<V8Context*> _contexts;
  basics::ConditionVariable _contextCondition;
  std::vector<V8Context*> _freeContexts;
  std::vector<V8Context*> _dirtyContexts;
  std::unordered_set<V8Context*> _busyContexts;
  size_t _nrAdditionalContexts;
  size_t _minimumContexts;
  size_t _forceNrContexts;

  JSLoader _startupLoader;

  std::map<std::string, bool> _definedBooleans;
  std::map<std::string, double> _definedDoubles;
  std::map<std::string, std::string> _definedStrings;

  std::vector<std::pair<
      std::function<void(v8::Isolate*, v8::Handle<v8::Context>, size_t)>,
      TRI_vocbase_t*>> _contextUpdates;
};
}

#endif
