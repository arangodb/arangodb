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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource_manager.hpp"
#include "Basics/ResourceUsage.h"
#include "Metrics/Gauge.h"

namespace arangodb::iresearch {

struct ResourceManager final : metrics::Gauge<uint64_t>, irs::IResourceManager {
  using Value = uint64_t;
  using metrics::Gauge<uint64_t>::Gauge;

  void Increase(size_t v) noexcept final {
    fetch_add(v);  //
  }

  void Decrease(size_t v) noexcept final {
    [[maybe_unused]] auto const was = fetch_sub(v);
    TRI_ASSERT(v <= was);
  }
};

struct LimitedResourceManager final : metrics::Gauge<uint64_t>,
                                      irs::IResourceManager {
  using Value = uint64_t;
  using metrics::Gauge<uint64_t>::Gauge;

  void Increase(size_t v) final {
    uint64_t current = load();
    do {
      if (limit < current + v) {
        throw std::bad_alloc{};
      }
    } while (!compare_exchange_weak(current, current + v));
  }

  void Decrease(size_t v) noexcept final {
    [[maybe_unused]] auto const was = fetch_sub(v);
    TRI_ASSERT(v <= was);
  }

  uint64_t limit{0};
};

struct MonitorManager final : irs::IResourceManager {
  explicit MonitorManager(ResourceMonitor& monitor) noexcept
      : _monitor{monitor} {}

  void Increase(size_t bytes) final {
    _monitor.increaseMemoryUsage(bytes);  //
  }

  void Decrease(size_t bytes) noexcept final {
    _monitor.decreaseMemoryUsage(bytes);
  }

  ResourceMonitor& _monitor;
};

}  // namespace arangodb::iresearch
