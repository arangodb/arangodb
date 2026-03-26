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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <vector>

#include "shared.hpp"
#include "utils/managed_allocator.hpp"

namespace arangodb::iresearch
{
  class IResearchFeature;
};

namespace irs {

struct IResourceManager {
  static IResourceManager kNoop;
#ifdef IRESEARCH_DEBUG
  static IResourceManager kForbidden;
#endif

  IResourceManager() = default;
  virtual ~IResourceManager() = default;

  IResourceManager(const IResourceManager&) = delete;
  IResourceManager operator=(const IResourceManager&) = delete;

  virtual void Increase([[maybe_unused]] size_t v) {
    IRS_ASSERT(this != &kForbidden);
    IRS_ASSERT(v != 0);
  }

  virtual void Decrease([[maybe_unused]] size_t v) noexcept {
    IRS_ASSERT(this != &kForbidden);
    IRS_ASSERT(v != 0);
  }

  IRS_FORCE_INLINE void DecreaseChecked(size_t v) noexcept {
    if (v != 0) {
      Decrease(v);
    }
  }
};

struct ResourceManagementOptions {
  static ResourceManagementOptions kDefault;

  IResourceManager* transactions{&IResourceManager::kNoop};
  IResourceManager* readers{&IResourceManager::kNoop};
  IResourceManager* consolidations{&IResourceManager::kNoop};
  IResourceManager* file_descriptors{&IResourceManager::kNoop};
  IResourceManager* cached_columns{&IResourceManager::kNoop};
};

//  Memory manager for IResearch.
//  This is a singleton class
struct IResearchMemoryManager : public IResourceManager {
protected:
  IResearchMemoryManager() = default;

public:
  virtual ~IResearchMemoryManager() = default;

  virtual void Increase([[maybe_unused]] size_t value) override {

    IRS_ASSERT(this != &kForbidden);
    IRS_ASSERT(value >= 0);

    if (_memoryLimit == 0) {
      // since we have no limit, we can simply use fetch-add for the increment
      _current.fetch_add(value, std::memory_order_relaxed);
    } else {
      // we only want to perform the update if we don't exceed the limit!
      size_t cur = _current.load(std::memory_order_relaxed);
      size_t next;
      do {
        next = cur + value;
        if (IRS_UNLIKELY(next > _memoryLimit.load(std::memory_order_relaxed))) {
          throw std::bad_alloc();
        }
      } while (!_current.compare_exchange_weak(
        cur, next, std::memory_order_relaxed));
    }
  }

  virtual void Decrease([[maybe_unused]] size_t value) noexcept override {
    IRS_ASSERT(this != &kForbidden);
    IRS_ASSERT(value >= 0);
    _current.fetch_sub(value, std::memory_order_relaxed);
  }

  //  NOTE: IResearchFeature owns and manages this memory limit.
  //  That is why this method should only be used by IResearchFeature.
  virtual void SetMemoryLimit(size_t memoryLimit) {
    _memoryLimit.store(memoryLimit);
  }

  size_t getCurrentUsage() {
    return _current.load(std::memory_order_relaxed);
  }

private:
  //  This limit should be set exclusively by IResearchFeature.
  //  During IResearchFeature::validateOptions() this limit is set to a
  //  percentage of either the total available physical memory or the value
  //  of ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY envvar if specified.
  std::atomic<size_t> _memoryLimit = { 0 };
  std::atomic<size_t> _current = { 0 };

  //  Singleton
  static inline std::shared_ptr<IResearchMemoryManager> _instance;

public:
  static std::shared_ptr<IResearchMemoryManager> GetInstance() {
    if (!_instance.get())
      _instance.reset(new IResearchMemoryManager());

    return _instance;
  }
};

template<typename T>
struct ManagedTypedAllocator
  : ManagedAllocator<std::allocator<T>, IResourceManager> {
  using Base = ManagedAllocator<std::allocator<T>, IResourceManager>;
  explicit ManagedTypedAllocator()
    : Base(
#if !defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
        IResourceManager::kForbidden
#else
        *IResearchMemoryManager::GetInstance()
#endif
      ) {
  }
  using Base::Base;
};

template<typename T>
using ManagedVector = std::vector<T, ManagedTypedAllocator<T>>;

}  // namespace irs
