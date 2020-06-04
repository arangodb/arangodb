// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/handles/handles-inl.h"
#include "src/heap/heap.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using HeapTest = TestWithIsolate;
using HeapWithPointerCompressionTest = TestWithIsolateAndPointerCompression;

TEST(Heap, YoungGenerationSizeFromOldGenerationSize) {
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t KB = static_cast<size_t>(i::KB);
  const size_t pm = i::Heap::kPointerMultiplier;
  ASSERT_EQ(3 * 512u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(128u * pm * MB));
  ASSERT_EQ(3 * 2048u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(256u * pm * MB));
  ASSERT_EQ(3 * 4096u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(512u * pm * MB));
  ASSERT_EQ(3 * 8192u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(1024u * pm * MB));
}

TEST(Heap, GenerationSizesFromHeapSize) {
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t KB = static_cast<size_t>(i::KB);
  const size_t pm = i::Heap::kPointerMultiplier;
  size_t old, young;

  i::Heap::GenerationSizesFromHeapSize(1 * KB, &young, &old);
  ASSERT_EQ(0u, old);
  ASSERT_EQ(0u, young);

  i::Heap::GenerationSizesFromHeapSize(1 * KB + 3 * 512u * pm * KB, &young,
                                       &old);
  ASSERT_EQ(1 * KB, old);
  ASSERT_EQ(3 * 512u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(128 * pm * MB + 3 * 512 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(128u * pm * MB, old);
  ASSERT_EQ(3 * 512u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(256u * pm * MB + 3 * 2048 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(256u * pm * MB, old);
  ASSERT_EQ(3 * 2048u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(512u * pm * MB + 3 * 4096 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(512u * pm * MB, old);
  ASSERT_EQ(3 * 4096u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(1024u * pm * MB + 3 * 8192 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(1024u * pm * MB, old);
  ASSERT_EQ(3 * 8192u * pm * KB, young);
}

TEST(Heap, HeapSizeFromPhysicalMemory) {
  const size_t MB = static_cast<size_t>(i::MB);
  const size_t pm = i::Heap::kPointerMultiplier;

  // The expected value is old_generation_size + 3 * semi_space_size.
  ASSERT_EQ(128 * pm * MB + 3 * 512 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(0u));
  ASSERT_EQ(128 * pm * MB + 3 * 512 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(512u * MB));
  ASSERT_EQ(256 * pm * MB + 3 * 2048 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(1024u * MB));
  ASSERT_EQ(512 * pm * MB + 3 * 4096 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(2048u * MB));
  ASSERT_EQ(
      1024 * pm * MB + 3 * 8192 * pm * KB,
      i::Heap::HeapSizeFromPhysicalMemory(static_cast<uint64_t>(4096u) * MB));
  ASSERT_EQ(
      1024 * pm * MB + 3 * 8192 * pm * KB,
      i::Heap::HeapSizeFromPhysicalMemory(static_cast<uint64_t>(8192u) * MB));
}

TEST_F(HeapTest, ASLR) {
#if V8_TARGET_ARCH_X64
#if V8_OS_MACOSX
  Heap* heap = i_isolate()->heap();
  std::set<void*> hints;
  for (int i = 0; i < 1000; i++) {
    hints.insert(heap->GetRandomMmapAddr());
  }
  if (hints.size() == 1) {
    EXPECT_TRUE((*hints.begin()) == nullptr);
    EXPECT_TRUE(i::GetRandomMmapAddr() == nullptr);
  } else {
    // It is unlikely that 1000 random samples will collide to less then 500
    // values.
    EXPECT_GT(hints.size(), 500u);
    const uintptr_t kRegionMask = 0xFFFFFFFFu;
    void* first = *hints.begin();
    for (void* hint : hints) {
      uintptr_t diff = reinterpret_cast<uintptr_t>(first) ^
                       reinterpret_cast<uintptr_t>(hint);
      EXPECT_LE(diff, kRegionMask);
    }
  }
#endif  // V8_OS_MACOSX
#endif  // V8_TARGET_ARCH_X64
}

TEST_F(HeapTest, ExternalLimitDefault) {
  Heap* heap = i_isolate()->heap();
  EXPECT_EQ(kExternalAllocationSoftLimit,
            heap->isolate()->isolate_data()->external_memory_limit_);
}

TEST_F(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling) {
  v8_isolate()->AdjustAmountOfExternalAllocatedMemory(+10 * MB);
  v8_isolate()->AdjustAmountOfExternalAllocatedMemory(-10 * MB);
  Heap* heap = i_isolate()->heap();
  EXPECT_GE(heap->isolate()->isolate_data()->external_memory_limit_,
            kExternalAllocationSoftLimit);
}

#if V8_TARGET_ARCH_64_BIT
TEST_F(HeapWithPointerCompressionTest, HeapLayout) {
  // Produce some garbage.
  RunJS(
      "let ar = [];"
      "for (let i = 0; i < 100; i++) {"
      "  ar.push(Array(i));"
      "}"
      "ar.push(Array(32 * 1024 * 1024));");

  Address isolate_root = i_isolate()->isolate_root();
  EXPECT_TRUE(IsAligned(isolate_root, size_t{4} * GB));

  // Check that all memory chunks belong this region.
  base::AddressRegion heap_reservation(isolate_root, size_t{4} * GB);

  OldGenerationMemoryChunkIterator iter(i_isolate()->heap());
  for (;;) {
    MemoryChunk* chunk = iter.next();
    if (chunk == nullptr) break;

    Address address = chunk->address();
    size_t size = chunk->area_end() - address;
    EXPECT_TRUE(heap_reservation.contains(address, size));
  }
}
#endif  // V8_TARGET_ARCH_64_BIT

}  // namespace internal
}  // namespace v8
