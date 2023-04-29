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

#include "Basics/Common.h"

#include "V8Server/GlobalContextMethods.h"

#include <atomic>
#include <mutex>
#include <string_view>

#include <v8.h>

namespace arangodb {
class V8Context {
 public:
  V8Context(size_t id, v8::Isolate* isolate);

  size_t id() const noexcept { return _id; }
  bool isDefault() const noexcept { return _id == 0; }
  void assertLocked() const;
  double age() const;
  void lockAndEnter();
  void unlockAndExit();
  uint64_t invocations() const noexcept {
    return _invocations.load(std::memory_order_relaxed);
  }
  double acquired() const noexcept { return _acquired; }
  std::string_view description() const noexcept { return _description; }
  bool shouldBeRemoved(double maxAge, uint64_t maxInvocations) const;
  bool hasGlobalMethodsQueued();
  void setCleaned(double stamp);

  // sets acquisition description (char const* must stay valid forever) and
  // acquisition timestamp
  void setDescription(std::string_view description, double acquired) noexcept {
    _description = description;
    _acquired = acquired;
  }
  void clearDescription() noexcept { _description = "none"; }

  void addGlobalContextMethod(GlobalContextMethods::MethodType type);
  void handleGlobalContextMethods();
  void handleCancellationCleanup();

  v8::Persistent<v8::Context> _context;
  v8::Isolate* const _isolate;
  double _lastGcStamp;
  uint64_t _invocationsSinceLastGc;
  bool _hasActiveExternals;

 private:
  size_t const _id;
  std::atomic_uint64_t _invocations;
  v8::Locker* _locker;
  /// @brief description of what the context is doing. pointer must be valid
  /// through the entire program lifetime
  std::string_view _description;
  /// @brief timestamp of when the context was last entered
  double _acquired;
  double const _creationStamp;

  std::mutex _globalMethodsLock;
  std::vector<GlobalContextMethods::MethodType> _globalMethods;
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
