// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_HEAP_UTILS_H_
#define HEAP_HEAP_UTILS_H_

#include "src/heap/heap.h"

namespace v8 {
namespace internal {
namespace heap {

void SealCurrentObjects(Heap* heap);

int FixedArrayLenFromSize(int size);

// Fill a page with fixed arrays leaving remainder behind. The function does
// not create additional fillers and assumes that the space has just been
// sealed.
std::vector<Handle<FixedArray>> FillOldSpacePageWithFixedArrays(Heap* heap,
                                                                int remainder);

std::vector<Handle<FixedArray>> CreatePadding(
    Heap* heap, int padding_size, PretenureFlag tenure,
    int object_size = kMaxRegularHeapObjectSize);

void AllocateAllButNBytes(
    v8::internal::NewSpace* space, int extra_bytes,
    std::vector<Handle<FixedArray>>* out_handles = nullptr);

void FillCurrentPage(v8::internal::NewSpace* space,
                     std::vector<Handle<FixedArray>>* out_handles = nullptr);

// Helper function that simulates a full new-space in the heap.
bool FillUpOnePage(v8::internal::NewSpace* space,
                   std::vector<Handle<FixedArray>>* out_handles = nullptr);

void SimulateFullSpace(v8::internal::NewSpace* space,
                       std::vector<Handle<FixedArray>>* out_handles = nullptr);

// Helper function that simulates many incremental marking steps until
// marking is completed.
void SimulateIncrementalMarking(i::Heap* heap, bool force_completion = true);

// Helper function that simulates a full old-space in the heap.
void SimulateFullSpace(v8::internal::PagedSpace* space);

void AbandonCurrentlyFreeMemory(PagedSpace* space);

void GcAndSweep(Heap* heap, AllocationSpace space);

void ForceEvacuationCandidate(Page* page);

}  // namespace heap
}  // namespace internal
}  // namespace v8

#endif  // HEAP_HEAP_UTILS_H_
