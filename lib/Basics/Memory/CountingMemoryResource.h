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
/// @author Aditya Mukhopadhyay
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MemoryTypes.h"
#include "Basics/ResourceUsage.h"

namespace arangodb {

struct CountingMemoryResource : memory_resource_t {
  CountingMemoryResource(memory_resource* base,
                         ResourceMonitor& resourceMonitor)
      : base(base), _resourceMonitor(resourceMonitor) {}

 private:
  void* do_allocate(size_t bytes, size_t alignment) override {
      _resourceMonitor.increaseMemoryUsage(bytes);
      ScopeGuard guard{[&] {
        _resourceMonitor.decreaseMemoryUsage(bytes); 
      }};
      void* mem = base->allocate(bytes, alignment);
      guard.cancel();
      return mem;
  }

  void do_deallocate(void* p, size_t bytes, size_t alignment) override {
    base->deallocate(p, bytes, alignment);
    _resourceMonitor.decreaseMemoryUsage(bytes);
  }

  [[nodiscard]] bool do_is_equal(
      memory_resource const&) const noexcept override {
    return false;
  }

  memory_resource_t* base;

  /// @brief current resources and limits used by query
  ResourceMonitor& _resourceMonitor;
};
}  // namespace arangodb
