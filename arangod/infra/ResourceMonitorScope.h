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

#include "infra/ResourceMonitorRef.h"

namespace arangodb::infra {

namespace detail {

// The calling thread's monitor, or nullptr if it has none.
//
// Safe to dereference without touching a refcount, because the thread-local is
// non-null only while a ResourceMonitorScope holds a reference. That argument
// covers the current call only: to keep the pointer, take a ResourceMonitorRef.
//
// Returned by value; a reference would hand out write access.
auto currentResourceMonitor() noexcept -> ResourceMonitorBase*;

}  // namespace detail

// Installs a monitor as the calling thread's current monitor for the lifetime of
// the guard. The only way to change it: the thread-local has internal linkage,
// so a stray write cannot leave a thread charging allocations to a retired
// monitor.
class ResourceMonitorScope {
 public:
  explicit ResourceMonitorScope(ResourceMonitorRef monitor) noexcept;
  ~ResourceMonitorScope();

  ResourceMonitorScope(ResourceMonitorScope const&) = delete;
  ResourceMonitorScope& operator=(ResourceMonitorScope const&) = delete;
  ResourceMonitorScope(ResourceMonitorScope&&) = delete;
  ResourceMonitorScope& operator=(ResourceMonitorScope&&) = delete;

 private:
  ResourceMonitorRef _monitor;
  ResourceMonitorBase* _previous;
};

}  // namespace arangodb::infra
