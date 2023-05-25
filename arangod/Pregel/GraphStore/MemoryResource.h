///////////////////////////////////////////////////////////////////////////////
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <experimental/memory_resource>
#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <atomic>

namespace arangodb::pregel {

struct MemoryResource : std::experimental::pmr::memory_resource {
  MemoryResource(std::experimental::pmr::memory_resource* base) : base(base) {}
  virtual void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    bytesAllocated += bytes;
    numberAllocations += 1;
    std::cout << "allocated: " << bytes << " bytes for a total of " << bytesAllocated << std::endl;
    return base->allocate(bytes, alignment);
  };
  virtual void do_deallocate(void* p, std::size_t bytes,
                             std::size_t alignment) override {
    bytesAllocated -= bytes;
    base->deallocate(p, bytes, alignment);
  };
  virtual bool do_is_equal(
      const std::experimental::pmr::memory_resource& other) const noexcept override {
    return false;
  };
  std::atomic<size_t> bytesAllocated{0};
  std::atomic<size_t> numberAllocations{0};
  std::experimental::pmr::memory_resource* base{nullptr};
};


}  // namespace arangodb::pregel
