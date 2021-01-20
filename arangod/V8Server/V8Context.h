////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
  };

 public:
  static MethodType type(std::string const& type) {
    if (type == "reloadRouting") {
      return MethodType::RELOAD_ROUTING;
    }
    if (type == "reloadAql") {
      return MethodType::RELOAD_AQL;
    }

    return MethodType::UNKNOWN;
  }

  static std::string const name(MethodType type) {
    switch (type) {
      case MethodType::RELOAD_ROUTING:
        return "reloadRouting";
      case MethodType::RELOAD_AQL:
        return "reloadAql";
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
      case MethodType::UNKNOWN:
      default:
        return StaticStrings::Empty;
    }
  }

 public:
  static std::string const CodeReloadRouting;
  static std::string const CodeReloadAql;
};

class V8Context {
 public:
  V8Context(size_t id, v8::Isolate* isolate);

  size_t id() const { return _id; }
  bool isDefault() const { return _id == 0; }
  void assertLocked() const;
  double age() const;
  void lockAndEnter();
  void unlockAndExit();
  uint64_t invocations() const { return _invocations; }
  uint64_t invocationsSinceLastGc() const { return _invocationsSinceLastGc; }
  double acquired() const noexcept { return _acquired; }
  char const* description() const noexcept { return _description; }
  bool shouldBeRemoved(double maxAge, uint64_t maxInvocations) const;
  bool hasGlobalMethodsQueued();
  void setCleaned(double stamp);

  // sets acquisition description (char const* must stay valid forever) and
  // acquisition timestamp
  void setDescription(char const* description, double acquired) noexcept {
    _description = description;
    _acquired = acquired;
  }
  void clearDescription() noexcept { _description = "none"; }

  size_t const _id;
  v8::Persistent<v8::Context> _context;
  v8::Isolate* _isolate;
  v8::Locker* _locker;
  double const _creationStamp;
  /// @brief timestamp of when the context was last entered
  double _acquired;
  /// @brief description of what the context is doing. pointer must be valid 
  /// through the entire program lifetime
  char const* _description;
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

class V8ContextEntryGuard {
 public:
  explicit V8ContextEntryGuard(V8Context* context);
  V8ContextEntryGuard(V8ContextEntryGuard const&) = delete;
  V8ContextEntryGuard& operator=(V8ContextEntryGuard const&) = delete;
  ~V8ContextEntryGuard();

 private:
  V8Context* _context;
};

}  // namespace arangodb

#endif
