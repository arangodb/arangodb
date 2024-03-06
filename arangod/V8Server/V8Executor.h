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

#include "Basics/Common.h"

#include "Basics/Result.h"
#include "V8Server/GlobalExecutorMethods.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>

#include <v8.h>

namespace arangodb {
class V8Executor {
 public:
  V8Executor(size_t id, v8::Isolate* isolate,
             std::function<void(V8Executor&)> const& cb);

  v8::Isolate* isolate() const noexcept { return _isolate; }
  size_t id() const noexcept { return _id; }
  bool isDefault() const noexcept { return _id == 0; }
  double age() const;
  void lockAndEnter();
  void unlockAndExit();
  uint64_t invocations() const noexcept {
    return _invocations.load(std::memory_order_relaxed);
  }
  double acquired() const noexcept { return _acquired; }
  std::string_view description() const noexcept { return _description; }
  bool shouldBeRemoved(double maxAge, uint64_t maxInvocations) const;
  void setCleaned(double stamp);
  void runCodeInContext(std::string_view code,
                        std::string_view codeDescription);
  Result runInContext(std::function<Result(v8::Isolate*)> const& cb,
                      bool executeGlobalMethods = true);

  // sets acquisition description (std::string_view data must stay valid
  // forever) and acquisition timestamp
  void setDescription(std::string_view description, double acquired) noexcept {
    _description = description;
    _acquired = acquired;
  }
  void clearDescription() noexcept { _description = "none"; }

  bool hasActiveExternals() const noexcept { return _hasActiveExternals; }
  void setHasActiveExternals(bool value) noexcept {
    _hasActiveExternals = value;
  }
  uint64_t invocationsSinceLastGc() const noexcept {
    return _invocationsSinceLastGc;
  }
  double lastGcStamp() const noexcept { return _lastGcStamp; }

  void addGlobalExecutorMethod(GlobalExecutorMethods::MethodType type);
  void handleCancellationCleanup();

 private:
  v8::Isolate* const _isolate;
  size_t const _id;
  double const _creationStamp;

  std::atomic_uint64_t _invocations;
  std::unique_ptr<v8::Locker> _locker;

  v8::Persistent<v8::Context> _context;
  double _lastGcStamp;
  uint64_t _invocationsSinceLastGc;

  /// @brief description of what the executor is doing. pointer must be valid
  /// through the entire program lifetime
  std::string_view _description;
  /// @brief timestamp of when the executor was last entered
  double _acquired;
  bool _hasActiveExternals;
  bool _isInIsolate;

  std::mutex _globalMethodsLock;
  std::vector<GlobalExecutorMethods::MethodType> _globalMethods;
};

}  // namespace arangodb
