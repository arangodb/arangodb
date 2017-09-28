////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_V8_SERVER_V8_CONTEXT_H
#define ARANGOD_V8_SERVER_V8_CONTEXT_H 1

#include "Basics/Common.h"

#include <v8.h>

#include "Basics/Mutex.h"
#include "Basics/StaticStrings.h"

namespace arangodb {
class GlobalContextMethods {
 public:
  enum class MethodType {
    UNKNOWN = 0,
    RELOAD_ROUTING,
    RELOAD_AQL,
    COLLECT_GARBAGE,
    BOOTSTRAP_COORDINATOR,
    WARMUP_EXPORTS
  };

 public:
  static MethodType type(std::string const& type) {
    if (type == "reloadRouting") {
      return MethodType::RELOAD_ROUTING;
    }
    if (type == "reloadAql") {
      return MethodType::RELOAD_AQL;
    }
    if (type == "collectGarbage") {
      return MethodType::COLLECT_GARBAGE;
    }
    if (type == "bootstrapCoordinator") {
      return MethodType::BOOTSTRAP_COORDINATOR;
    }
    if (type == "warmupExports") {
      return MethodType::WARMUP_EXPORTS;
    }

    return MethodType::UNKNOWN;
  }

  static std::string const name(MethodType type) {
    switch (type) {
      case MethodType::RELOAD_ROUTING:
        return "reloadRouting";
      case MethodType::RELOAD_AQL:
        return "reloadAql";
      case MethodType::COLLECT_GARBAGE:
        return "collectGarbage";
      case MethodType::BOOTSTRAP_COORDINATOR:
        return "bootstrapCoordinator";
      case MethodType::WARMUP_EXPORTS:
        return "warmupExports";
      case MethodType::UNKNOWN:
      default:
        return "unknown";
    }
  }

  static std::string const& code(MethodType type) {
    switch (type) {
      case MethodType::RELOAD_ROUTING:
        return CodeReloadRouting;
      case MethodType::RELOAD_AQL:
        return CodeReloadAql;
      case MethodType::COLLECT_GARBAGE:
        return CodeCollectGarbage;
      case MethodType::BOOTSTRAP_COORDINATOR:
        return CodeBootstrapCoordinator;
      case MethodType::WARMUP_EXPORTS:
        return CodeWarmupExports;
      case MethodType::UNKNOWN:
      default:
        return StaticStrings::Empty;
    }
  }

 public:
  static std::string const CodeReloadRouting;
  static std::string const CodeReloadAql;
  static std::string const CodeCollectGarbage;
  static std::string const CodeBootstrapCoordinator;
  static std::string const CodeWarmupExports;
};

class V8Context {
 public:
  V8Context(size_t id, v8::Isolate* isolate);

  size_t id() const { return _id; }
  bool isDefault() const { return _id == 0; }
  bool isUsed() const { return _locker != nullptr; }
  double age() const;
  void lockAndEnter();
  void unlockAndExit();
  uint64_t invocations() const { return _invocations; }
  uint64_t invocationsSinceLastGc() const { return _invocationsSinceLastGc; }
  bool shouldBeRemoved(double maxAge, uint64_t maxInvocations) const;
  bool hasGlobalMethodsQueued();
  void setCleaned(double stamp);

  size_t const _id;

  v8::Persistent<v8::Context> _context;
  v8::Isolate* _isolate;
  v8::Locker* _locker;
  double const _creationStamp;
  double _lastGcStamp;
  uint64_t _invocations;
  uint64_t _invocationsSinceLastGc;
  bool _hasActiveExternals;

  Mutex _globalMethodsLock;
  std::vector<GlobalContextMethods::MethodType> _globalMethods;

 public:
  bool addGlobalContextMethod(std::string const&);
  void handleGlobalContextMethods();
  void handleCancelationCleanup();
};

class V8ContextGuard {
 public:
  explicit V8ContextGuard(V8Context* context);
  ~V8ContextGuard();

 private:
  V8Context* _context;
};

}

#endif
