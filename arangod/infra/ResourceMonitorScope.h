////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "infra/CurrentResourceMonitor.h"

#include <utility>

namespace arangodb::infra {

struct ResourceMonitorScope {
  explicit ResourceMonitorScope(ResourceMonitorRef monitor) noexcept
      : _previous(std::exchange(currentResourceMonitor(), std::move(monitor))) {
  }

  ~ResourceMonitorScope() { currentResourceMonitor() = std::move(_previous); }

  ResourceMonitorScope(ResourceMonitorScope const&) = delete;
  ResourceMonitorScope& operator=(ResourceMonitorScope const&) = delete;
  ResourceMonitorScope(ResourceMonitorScope&&) = delete;
  ResourceMonitorScope& operator=(ResourceMonitorScope&&) = delete;

 private:
  ResourceMonitorRef _previous;
};

}  // namespace arangodb::infra
