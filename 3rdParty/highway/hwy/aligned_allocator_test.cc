// Copyright 2020 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/aligned_allocator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  // malloc

#include <array>
#include <random>
#include <set>
#include <vector>

#include "gtest/gtest.h"

namespace {

// Sample object that keeps track on an external counter of how many times was
// the explicit constructor and destructor called.
template <size_t N>
class SampleObject {
 public:
  SampleObject() { data_[0] = 'a'; }
  explicit SampleObject(int* counter) : counter_(counter) {
    if (counter) (*counter)++;
    data_[0] = 'b';
  }

  ~SampleObject() {
    if (counter_) (*counter_)--;
  }

  static_assert(N > sizeof(int*), "SampleObject size too small.");
  int* counter_ = nullptr;
  char data_[N - sizeof(int*)];
};

class FakeAllocator {
 public:
  // static AllocPtr and FreePtr member to be used with the aligned
  // allocator. These functions calls the private non-static members.
  static void* StaticAlloc(void* opaque, size_t bytes) {
    return reinterpret_cast<FakeAllocator*>(opaque)->Alloc(bytes);
  }
  static void StaticFree(void* opaque, void* memory) {
    return reinterpret_cast<FakeAllocator*>(opaque)->Free(memory);
  }

  // Returns the number of pending allocations to be freed.
  size_t PendingAllocs() { return allocs_.size(); }

 private:
  void* Alloc(size_t bytes) {
    void* ret = malloc(bytes);
    allocs_.insert(ret);
    return ret;
  }
  void Free(void* memory) {
    if (!memory) return;
    EXPECT_NE(allocs_.end(), allocs_.find(memory));
    allocs_.erase(memory);
    free(memory);
  }

  std::set<void*> allocs_;
};

}  // namespace

namespace hwy {

class AlignedAllocatorTest : public testing::Test {};

TEST(AlignedAllocatorTest, FreeNullptr) {
  // Calling free with a nullptr is always ok.
  FreeAlignedBytes(/*aligned_pointer=*/nullptr, /*free_ptr=*/nullptr,
                   /*opaque_ptr=*/nullptr);
}

TEST(AlignedAllocatorTest, Log2) {
  EXPECT_EQ(0u, detail::ShiftCount(1));
  EXPECT_EQ(1u, detail::ShiftCount(2));
  EXPECT_EQ(3u, detail::ShiftCount(8));
}

// Allocator returns null when it detects overflow of items * sizeof(T).
TEST(AlignedAllocatorTest, Overflow) {
  constexpr size_t max = ~size_t(0);
  constexpr size_t msb = (max >> 1) + 1;
  using Size5 = std::array<uint8_t, 5>;
  using Size10 = std::array<uint8_t, 10>;
  EXPECT_EQ(nullptr,
            detail::AllocateAlignedItems<uint32_t>(max / 2, nullptr, nullptr));
  EXPECT_EQ(nullptr,
            detail::AllocateAlignedItems<uint32_t>(max / 3, nullptr, nullptr));
  EXPECT_EQ(nullptr,
            detail::AllocateAlignedItems<Size5>(max / 4, nullptr, nullptr));
  EXPECT_EQ(nullptr,
            detail::AllocateAlignedItems<uint16_t>(msb, nullptr, nullptr));
  EXPECT_EQ(nullptr,
            detail::AllocateAlignedItems<double>(msb + 1, nullptr, nullptr));
  EXPECT_EQ(nullptr,
            detail::AllocateAlignedItems<Size10>(msb / 4, nullptr, nullptr));
}

TEST(AlignedAllocatorTest, AllocDefaultPointers) {
  const size_t kSize = 7777;
  void* ptr = AllocateAlignedBytes(kSize, /*alloc_ptr=*/nullptr,
                                   /*opaque_ptr=*/nullptr);
  ASSERT_NE(nullptr, ptr);
  // Make sure the pointer is actually aligned.
  EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(ptr) % HWY_ALIGNMENT);
  char* p = static_cast<char*>(ptr);
  size_t ret = 0;
  for (size_t i = 0; i < kSize; i++) {
    // Performs a computation using p[] to prevent it being optimized away.
    p[i] = static_cast<char>(i & 0x7F);
    if (i) ret += static_cast<size_t>(p[i] * p[i - 1]);
  }
  EXPECT_NE(0U, ret);
  FreeAlignedBytes(ptr, /*free_ptr=*/nullptr, /*opaque_ptr=*/nullptr);
}

TEST(AlignedAllocatorTest, EmptyAlignedUniquePtr) {
  AlignedUniquePtr<SampleObject<32>> ptr(nullptr, AlignedDeleter());
  AlignedUniquePtr<SampleObject<32>[]> arr(nullptr, AlignedDeleter());
}

TEST(AlignedAllocatorTest, EmptyAlignedFreeUniquePtr) {
  AlignedFreeUniquePtr<SampleObject<32>> ptr(nullptr, AlignedFreer());
  AlignedFreeUniquePtr<SampleObject<32>[]> arr(nullptr, AlignedFreer());
}

TEST(AlignedAllocatorTest, CustomAlloc) {
  FakeAllocator fake_alloc;

  const size_t kSize = 7777;
  void* ptr =
      AllocateAlignedBytes(kSize, &FakeAllocator::StaticAlloc, &fake_alloc);
  ASSERT_NE(nullptr, ptr);
  // We should have only requested one alloc from the allocator.
  EXPECT_EQ(1U, fake_alloc.PendingAllocs());
  // Make sure the pointer is actually aligned.
  EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(ptr) % HWY_ALIGNMENT);
  FreeAlignedBytes(ptr, &FakeAllocator::StaticFree, &fake_alloc);
  EXPECT_EQ(0U, fake_alloc.PendingAllocs());
}

TEST(AlignedAllocatorTest, MakeUniqueAlignedDefaultConstructor) {
  {
    auto ptr = MakeUniqueAligned<SampleObject<24>>();
    // Default constructor sets the data_[0] to 'a'.
    EXPECT_EQ('a', ptr->data_[0]);
    EXPECT_EQ(nullptr, ptr->counter_);
  }
}

TEST(AlignedAllocatorTest, MakeUniqueAligned) {
  int counter = 0;
  {
    // Creates the object, initializes it with the explicit constructor and
    // returns an unique_ptr to it.
    auto ptr = MakeUniqueAligned<SampleObject<24>>(&counter);
    EXPECT_EQ(1, counter);
    // Custom constructor sets the data_[0] to 'b'.
    EXPECT_EQ('b', ptr->data_[0]);
  }
  EXPECT_EQ(0, counter);
}

TEST(AlignedAllocatorTest, MakeUniqueAlignedArray) {
  int counter = 0;
  {
    // Creates the array of objects and initializes them with the explicit
    // constructor.
    auto arr = MakeUniqueAlignedArray<SampleObject<24>>(7, &counter);
    EXPECT_EQ(7, counter);
    for (size_t i = 0; i < 7; i++) {
      // Custom constructor sets the data_[0] to 'b'.
      EXPECT_EQ('b', arr[i].data_[0]) << "Where i = " << i;
    }
  }
  EXPECT_EQ(0, counter);
}

TEST(AlignedAllocatorTest, AllocSingleInt) {
  auto ptr = AllocateAligned<uint32_t>(1);
  ASSERT_NE(nullptr, ptr.get());
  EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(ptr.get()) % HWY_ALIGNMENT);
  // Force delete of the unique_ptr now to check that it doesn't crash.
  ptr.reset(nullptr);
  EXPECT_EQ(nullptr, ptr.get());
}

TEST(AlignedAllocatorTest, AllocMultipleInt) {
  const size_t kSize = 7777;
  auto ptr = AllocateAligned<uint32_t>(kSize);
  ASSERT_NE(nullptr, ptr.get());
  EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(ptr.get()) % HWY_ALIGNMENT);
  // ptr[i] is actually (*ptr.get())[i] which will use the operator[] of the
  // underlying type chosen by AllocateAligned() for the std::unique_ptr.
  EXPECT_EQ(&(ptr[0]) + 1, &(ptr[1]));

  size_t ret = 0;
  for (size_t i = 0; i < kSize; i++) {
    // Performs a computation using ptr[] to prevent it being optimized away.
    ptr[i] = static_cast<uint32_t>(i);
    if (i) ret += ptr[i] * ptr[i - 1];
  }
  EXPECT_NE(0U, ret);
}

TEST(AlignedAllocatorTest, AllocateAlignedObjectWithoutDestructor) {
  int counter = 0;
  {
    // This doesn't call the constructor.
    auto obj = AllocateAligned<SampleObject<24>>(1);
    obj[0].counter_ = &counter;
  }
  // Destroying the unique_ptr shouldn't have called the destructor of the
  // SampleObject<24>.
  EXPECT_EQ(0, counter);
}

TEST(AlignedAllocatorTest, MakeUniqueAlignedArrayWithCustomAlloc) {
  FakeAllocator fake_alloc;
  int counter = 0;
  {
    // Creates the array of objects and initializes them with the explicit
    // constructor.
    auto arr = MakeUniqueAlignedArrayWithAlloc<SampleObject<24>>(
        7, FakeAllocator::StaticAlloc, FakeAllocator::StaticFree, &fake_alloc,
        &counter);
    ASSERT_NE(nullptr, arr.get());
    // An array should still only call a single allocation.
    EXPECT_EQ(1u, fake_alloc.PendingAllocs());
    EXPECT_EQ(7, counter);
    for (size_t i = 0; i < 7; i++) {
      // Custom constructor sets the data_[0] to 'b'.
      EXPECT_EQ('b', arr[i].data_[0]) << "Where i = " << i;
    }
  }
  EXPECT_EQ(0, counter);
  EXPECT_EQ(0u, fake_alloc.PendingAllocs());
}

TEST(AlignedAllocatorTest, DefaultInit) {
  // The test is whether this compiles. Default-init is useful for output params
  // and per-thread storage.
  std::vector<AlignedUniquePtr<int[]>> ptrs;
  std::vector<AlignedFreeUniquePtr<double[]>> free_ptrs;
  ptrs.resize(128);
  free_ptrs.resize(128);
  // The following is to prevent elision of the pointers.
  std::mt19937 rng(129);  // Emscripten lacks random_device.
  std::uniform_int_distribution<size_t> dist(0, 127);
  ptrs[dist(rng)] = MakeUniqueAlignedArray<int>(123);
  free_ptrs[dist(rng)] = AllocateAligned<double>(456);
  // "Use" pointer without resorting to printf. 0 == 0. Can't shift by 64.
  const auto addr1 = reinterpret_cast<uintptr_t>(ptrs[dist(rng)].get());
  const auto addr2 = reinterpret_cast<uintptr_t>(free_ptrs[dist(rng)].get());
  constexpr size_t kBits = sizeof(uintptr_t) * 8;
  EXPECT_EQ((addr1 >> (kBits - 1)) >> (kBits - 1),
            (addr2 >> (kBits - 1)) >> (kBits - 1));
}

}  // namespace hwy
