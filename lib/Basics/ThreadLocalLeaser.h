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

namespace {
constexpr auto max_leasees_per_thread = size_t{128};
}

namespace arangodb {

template<typename T>
struct ThreadLocalLeaser {
  struct Lease {
    ~Lease() {
      // put builder on builders vector unless capacity is reached
      if (_leasee != nullptr) {
        current.returnLeasee(std::move(_leasee));
      }
    }
    Lease(Lease&&) = default;
    auto operator=(Lease&&) -> Lease& = default;

    friend struct ThreadLocalLeaser;

    auto leasee() const -> T* { return _leasee.get(); }

    // TODO: this is used precisely in one (dubious) place
    // Maybe we can remove that use and these functions.
    auto release() -> std::unique_ptr<T> {
      return std::exchange(_leasee, nullptr);
    }
    auto acquire(std::unique_ptr<T>&& m) -> void { _leasee = std::move(m); }

   private:
    Lease(std::unique_ptr<T>&& leasee) : _leasee(std::move(leasee)) {
      TRI_ASSERT(_leasee != nullptr);
    };

    std::unique_ptr<T> _leasee;
  };

  auto lease() -> Lease {
    if (!_leasees.empty()) {
      TRI_ASSERT(_leasees.back() != nullptr);
      auto leasee = std::move(_leasees.back());
      leasee->clear();
      TRI_ASSERT(leasee != nullptr);
      _leasees.pop_back();
      return Lease(std::move(leasee));
    } else {
      return Lease(std::make_unique<T>());
    }
  }

  static thread_local ThreadLocalLeaser current;

  friend struct Lease;

 private:
  auto returnLeasee(std::unique_ptr<T>&& leasee) -> void {
    TRI_ASSERT(leasee != nullptr);
    if (_leasees.size() < max_leasees_per_thread) {
      _leasees.push_back(std::move(leasee));
    }
  }

  std::vector<std::unique_ptr<T>> _leasees;
};

template<typename T>
thread_local ThreadLocalLeaser<T> ThreadLocalLeaser<T>::current;

using ThreadLocalBuilderLeaser = ThreadLocalLeaser<velocypack::Builder>;
using ThreadLocalStringLeaser = ThreadLocalLeaser<std::string>;

}  // namespace arangodb
