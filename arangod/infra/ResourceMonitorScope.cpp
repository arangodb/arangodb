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

#include "ResourceMonitorScope.h"

#include <utility>

namespace arangodb::infra {
namespace {

// Internal linkage confines every write to this file. Must stay trivially
// destructible and constant-initialized: a thread-local with a destructor
// registers a __cxa_thread_atexit handler, and glibc allocates that handler's
// node -- re-entering malloc, which reads this thread-local, until the stack is
// gone. The ref lives in ResourceMonitorScope for that reason.
constinit thread_local ResourceMonitorBase* tlsCurrentMonitor = nullptr;

}  // namespace

auto detail::currentResourceMonitor() noexcept -> ResourceMonitorBase* {
  return tlsCurrentMonitor;
}

ResourceMonitorScope::ResourceMonitorScope(ResourceMonitorRef monitor) noexcept
    : _monitor(std::move(monitor)),
      _previous(std::exchange(tlsCurrentMonitor, _monitor.get())) {}

ResourceMonitorScope::~ResourceMonitorScope() {
  tlsCurrentMonitor = _previous;
}

}  // namespace arangodb::infra
