////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <memory>

namespace arangodb {

struct ExclusiveBool;

struct ExclusiveBoolGuard {
  operator bool() const noexcept { return ptr != nullptr; }
  ~ExclusiveBoolGuard() { reset(); }
  void reset() noexcept;

  ExclusiveBoolGuard() = default;
  ExclusiveBoolGuard(ExclusiveBoolGuard const&) = delete;
  auto operator=(ExclusiveBoolGuard const&) -> ExclusiveBoolGuard& = delete;
  ExclusiveBoolGuard(ExclusiveBoolGuard&&) noexcept = default;
  auto operator=(ExclusiveBoolGuard&&) noexcept
      -> ExclusiveBoolGuard& = default;

 private:
  friend struct ExclusiveBool;
  explicit ExclusiveBoolGuard(ExclusiveBool* flag) : ptr(flag) {}
  struct noop {
    template<typename T>
    void operator()(T*) {}
  };
  std::unique_ptr<ExclusiveBool, noop> ptr;
};

struct ExclusiveBool {
  auto acquire() noexcept -> ExclusiveBoolGuard {
    if (not value.test_and_set()) {
      return ExclusiveBoolGuard{this};
    } else {
      return ExclusiveBoolGuard{};
    }
  }

  ExclusiveBool() { value.clear(); }
  ~ExclusiveBool() = default;  // TODO assert value == false

  ExclusiveBool(ExclusiveBool const&) = delete;
  auto operator=(ExclusiveBool const&) -> ExclusiveBool& = delete;
  ExclusiveBool(ExclusiveBool&&) noexcept = delete;
  auto operator=(ExclusiveBool&&) noexcept -> ExclusiveBool& = delete;

 private:
  friend struct ExclusiveBoolGuard;
  std::atomic_flag value;
};

inline void ExclusiveBoolGuard::reset() noexcept {
  if (ptr) {
    ptr->value.clear();
    ptr.reset();
  }
}

}  // namespace arangodb
