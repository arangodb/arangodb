// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_ALLOCATOR_INL_H_
#define V8_HEAP_LOCAL_ALLOCATOR_INL_H_

#include "src/heap/local-allocator.h"

#include "src/heap/spaces-inl.h"

namespace v8 {
namespace internal {

AllocationResult LocalAllocator::Allocate(AllocationSpace space,
                                          int object_size,
                                          AllocationAlignment alignment) {
  switch (space) {
    case NEW_SPACE:
      return AllocateInNewSpace(object_size, alignment);
    case OLD_SPACE:
      return compaction_spaces_.Get(OLD_SPACE)->AllocateRaw(object_size,
                                                            alignment);
    case CODE_SPACE:
      return compaction_spaces_.Get(CODE_SPACE)
          ->AllocateRaw(object_size, alignment);
    default:
      UNREACHABLE();
      break;
  }
}

void LocalAllocator::FreeLast(AllocationSpace space, HeapObject* object,
                              int object_size) {
  switch (space) {
    case NEW_SPACE:
      FreeLastInNewSpace(object, object_size);
      return;
    case OLD_SPACE:
      FreeLastInOldSpace(object, object_size);
      return;
    default:
      // Only new and old space supported.
      UNREACHABLE();
      break;
  }
}

void LocalAllocator::FreeLastInNewSpace(HeapObject* object, int object_size) {
  if (!new_space_lab_.TryFreeLast(object, object_size)) {
    // We couldn't free the last object so we have to write a proper filler.
    heap_->CreateFillerObjectAt(object->address(), object_size,
                                ClearRecordedSlots::kNo);
  }
}

void LocalAllocator::FreeLastInOldSpace(HeapObject* object, int object_size) {
  if (!compaction_spaces_.Get(OLD_SPACE)->TryFreeLast(object, object_size)) {
    // We couldn't free the last object so we have to write a proper filler.
    heap_->CreateFillerObjectAt(object->address(), object_size,
                                ClearRecordedSlots::kNo);
  }
}

AllocationResult LocalAllocator::AllocateInLAB(int object_size,
                                               AllocationAlignment alignment) {
  AllocationResult allocation;
  if (!new_space_lab_.IsValid() && !NewLocalAllocationBuffer()) {
    return AllocationResult::Retry(OLD_SPACE);
  }
  allocation = new_space_lab_.AllocateRawAligned(object_size, alignment);
  if (allocation.IsRetry()) {
    if (!NewLocalAllocationBuffer()) {
      return AllocationResult::Retry(OLD_SPACE);
    } else {
      allocation = new_space_lab_.AllocateRawAligned(object_size, alignment);
      CHECK(!allocation.IsRetry());
    }
  }
  return allocation;
}

bool LocalAllocator::NewLocalAllocationBuffer() {
  if (lab_allocation_will_fail_) return false;
  LocalAllocationBuffer saved_lab_ = new_space_lab_;
  AllocationResult result =
      new_space_->AllocateRawSynchronized(kLabSize, kWordAligned);
  new_space_lab_ = LocalAllocationBuffer::FromResult(heap_, result, kLabSize);
  if (new_space_lab_.IsValid()) {
    new_space_lab_.TryMerge(&saved_lab_);
    return true;
  }
  new_space_lab_ = saved_lab_;
  lab_allocation_will_fail_ = true;
  return false;
}

AllocationResult LocalAllocator::AllocateInNewSpace(
    int object_size, AllocationAlignment alignment) {
  if (object_size > kMaxLabObjectSize) {
    return new_space_->AllocateRawSynchronized(object_size, alignment);
  }
  return AllocateInLAB(object_size, alignment);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_ALLOCATOR_INL_H_
