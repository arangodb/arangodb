////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "resource_manager.hpp"
#include "Metrics/Gauge.h"

namespace arangodb::iresearch {

struct ResourceManager final : metrics::Gauge<uint64_t>, irs::IResourceManager {
  using Value = uint64_t;
  using metrics::Gauge<uint64_t>::Gauge;

  bool Increase(size_t v) noexcept final {
    fetch_add(v);
    return true;
  }

  void Decrease(size_t v) noexcept final {
    [[maybe_unused]] auto const was = fetch_sub(v);
    TRI_ASSERT(v <= was);
  }
};

struct LimittedResourceManager final : metrics::Gauge<uint64_t>,
                                       irs::IResourceManager {
  using Value = uint64_t;
  using metrics::Gauge<uint64_t>::Gauge;

  bool Increase(size_t v) noexcept final {
    uint64_t current = load();
    do {
      if (limit < current + v) {
        return false;
      }
    } while (!compare_exchange_weak(current, current + v));
    return true;
  }

  void Decrease(size_t v) noexcept final {
    [[maybe_unused]] auto const was = fetch_sub(v);
    TRI_ASSERT(v <= was);
  }

  uint64_t limit{0};
};

}  // namespace arangodb::iresearch
