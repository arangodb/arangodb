// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_BOUNDED_PAGE_ALLOCATOR_H_
#define V8_BASE_BOUNDED_PAGE_ALLOCATOR_H_

#include "include/v8-platform.h"
#include "src/base/platform/mutex.h"
#include "src/base/region-allocator.h"

namespace v8 {
namespace base {

// This is a v8::PageAllocator implementation that allocates pages within the
// pre-reserved region of virtual space. This class requires the virtual space
// to be kept reserved during the lifetime of this object.
// The main application of bounded page allocator are
//  - V8 heap pointer compression which requires the whole V8 heap to be
//    allocated within a contiguous range of virtual address space,
//  - executable page allocation, which allows to use PC-relative 32-bit code
//    displacement on certain 64-bit platforms.
// Bounded page allocator uses other page allocator instance for doing actual
// page allocations.
// The implementation is thread-safe.
class V8_BASE_EXPORT BoundedPageAllocator : public v8::PageAllocator {
 public:
  typedef uintptr_t Address;

  BoundedPageAllocator(v8::PageAllocator* page_allocator, Address start,
                       size_t size, size_t allocate_page_size);
  ~BoundedPageAllocator() override = default;

  // These functions are not inlined to avoid https://crbug.com/v8/8275.
  Address begin() const;
  size_t size() const;

  // Returns true if given address is in the range controlled by the bounded
  // page allocator instance.
  bool contains(Address address) const {
    return region_allocator_.contains(address);
  }

  size_t AllocatePageSize() override { return allocate_page_size_; }

  size_t CommitPageSize() override { return commit_page_size_; }

  void SetRandomMmapSeed(int64_t seed) override {
    page_allocator_->SetRandomMmapSeed(seed);
  }

  void* GetRandomMmapAddr() override {
    return page_allocator_->GetRandomMmapAddr();
  }

  void* AllocatePages(void* address, size_t size, size_t alignment,
                      PageAllocator::Permission access) override;

  bool FreePages(void* address, size_t size) override;

  bool ReleasePages(void* address, size_t size, size_t new_size) override;

  bool SetPermissions(void* address, size_t size,
                      PageAllocator::Permission access) override;

 private:
  v8::base::Mutex mutex_;
  const size_t allocate_page_size_;
  const size_t commit_page_size_;
  v8::PageAllocator* const page_allocator_;
  v8::base::RegionAllocator region_allocator_;

  DISALLOW_COPY_AND_ASSIGN(BoundedPageAllocator);
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_BOUNDED_PAGE_ALLOCATOR_H_
