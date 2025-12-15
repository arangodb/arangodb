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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/Exceptions.h"

#include <vector>
#include "velocypack/Builder.h"

namespace arangodb {

template<typename T>
struct ThreadLocalLeaser {
  struct Lease {
    ~Lease() {
      if (_object != nullptr) {
        current.returnObject(std::move(_object));
      }
    }
    Lease(Lease&&) = default;
    auto operator=(Lease&&) -> Lease& = delete;

    friend struct ThreadLocalLeaser;

    auto get() noexcept -> T* { return _object.get(); }
    auto get() const noexcept -> T const* { return _object.get(); }
    auto operator->() noexcept -> T* { return get(); }
    auto operator->() const noexcept -> T const* { return get(); }
    auto operator*() noexcept -> T& { return *get(); }
    auto operator*() const noexcept -> T const& { return *get(); }

    // TODO: this is used precisely in one (dubious) place
    // Maybe we can remove that;
    auto release() -> std::unique_ptr<T> {
      return std::exchange(_object, nullptr);
    }
    auto acquire(std::unique_ptr<T>&& m) -> void { _object = std::move(m); }

   private:
    Lease(std::unique_ptr<T>&& leasee) : _object(std::move(leasee)) {
      TRI_ASSERT(_object != nullptr);
    };

    std::unique_ptr<T> _object;
  };

  static auto stashSize() noexcept -> size_t { return current._stash.size(); }
  static constexpr auto maxStashedPerThread = size_t{128};
  static auto lease() -> Lease { return current.doLease(); }
  static auto clear() -> void { current._stash.clear(); }
  friend struct Lease;

 private:
  ThreadLocalLeaser() = default;
  auto returnObject(std::unique_ptr<T>&& object) -> void {
    TRI_ASSERT(object != nullptr);
    if (_stash.size() < maxStashedPerThread) {
      _stash.push_back(std::move(object));
    }
  }
  auto doLease() -> Lease {
    if (!_stash.empty()) {
      TRI_ASSERT(_stash.back() != nullptr);
      auto leasee = std::move(_stash.back());
      leasee->clear();
      TRI_ASSERT(leasee != nullptr);
      _stash.pop_back();
      return Lease(std::move(leasee));
    } else {
      return Lease(std::make_unique<T>());
    }
  }

  static thread_local ThreadLocalLeaser current;
  std::vector<std::unique_ptr<T>> _stash;
};

template<typename T>
thread_local ThreadLocalLeaser<T> ThreadLocalLeaser<T>::current;

using ThreadLocalBuilderLeaser = ThreadLocalLeaser<velocypack::Builder>;
using ThreadLocalStringLeaser = ThreadLocalLeaser<std::string>;

}  // namespace arangodb
