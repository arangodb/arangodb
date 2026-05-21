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

#ifndef HIGHWAY_HWY_ALIGNED_ALLOCATOR_H_
#define HIGHWAY_HWY_ALIGNED_ALLOCATOR_H_

// Memory allocator with support for alignment and offsets.

#include <memory>
#include <utility>

#include "hwy/base.h"

namespace hwy {

// Minimum alignment of allocated memory for use in HWY_ASSUME_ALIGNED, which
// requires a literal. This matches typical L1 cache line sizes, which prevents
// false sharing.
#define HWY_ALIGNMENT 64

// Pointers to functions equivalent to malloc/free with an opaque void* passed
// to them.
using AllocPtr = void* (*)(void* opaque, size_t bytes);
using FreePtr = void (*)(void* opaque, void* memory);

// Returns null or a pointer to at least `payload_size` (which can be zero)
// bytes of newly allocated memory, aligned to the larger of HWY_ALIGNMENT and
// the vector size. Calls `alloc` with the passed `opaque` pointer to obtain
// memory or malloc() if it is null.
HWY_DLLEXPORT void* AllocateAlignedBytes(size_t payload_size,
                                         AllocPtr alloc_ptr, void* opaque_ptr);

// Frees all memory. No effect if `aligned_pointer` == nullptr, otherwise it
// must have been returned from a previous call to `AllocateAlignedBytes`.
// Calls `free_ptr` with the passed `opaque_ptr` pointer to free the memory; if
// `free_ptr` function is null, uses the default free().
HWY_DLLEXPORT void FreeAlignedBytes(const void* aligned_pointer,
                                    FreePtr free_ptr, void* opaque_ptr);

// Class that deletes the aligned pointer passed to operator() calling the
// destructor before freeing the pointer. This is equivalent to the
// std::default_delete but for aligned objects. For a similar deleter equivalent
// to free() for aligned memory see AlignedFreer().
class AlignedDeleter {
 public:
  AlignedDeleter() : free_(nullptr), opaque_ptr_(nullptr) {}
  AlignedDeleter(FreePtr free_ptr, void* opaque_ptr)
      : free_(free_ptr), opaque_ptr_(opaque_ptr) {}

  template <typename T>
  void operator()(T* aligned_pointer) const {
    return DeleteAlignedArray(aligned_pointer, free_, opaque_ptr_,
                              TypedArrayDeleter<T>);
  }

 private:
  template <typename T>
  static void TypedArrayDeleter(void* ptr, size_t size_in_bytes) {
    size_t elems = size_in_bytes / sizeof(T);
    for (size_t i = 0; i < elems; i++) {
      // Explicitly call the destructor on each element.
      (static_cast<T*>(ptr) + i)->~T();
    }
  }

  // Function prototype that calls the destructor for each element in a typed
  // array. TypeArrayDeleter<T> would match this prototype.
  using ArrayDeleter = void (*)(void* t_ptr, size_t t_size);

  HWY_DLLEXPORT static void DeleteAlignedArray(void* aligned_pointer,
                                               FreePtr free_ptr,
                                               void* opaque_ptr,
                                               ArrayDeleter deleter);

  FreePtr free_;
  void* opaque_ptr_;
};

// Unique pointer to T with custom aligned deleter. This can be a single
// element U or an array of element if T is a U[]. The custom aligned deleter
// will call the destructor on U or each element of a U[] in the array case.
template <typename T>
using AlignedUniquePtr = std::unique_ptr<T, AlignedDeleter>;

// Aligned memory equivalent of make_unique<T> using the custom allocators
// alloc/free with the passed `opaque` pointer. This function calls the
// constructor with the passed Args... and calls the destructor of the object
// when the AlignedUniquePtr is destroyed.
template <typename T, typename... Args>
AlignedUniquePtr<T> MakeUniqueAlignedWithAlloc(AllocPtr alloc, FreePtr free,
                                               void* opaque, Args&&... args) {
  T* ptr = static_cast<T*>(AllocateAlignedBytes(sizeof(T), alloc, opaque));
  return AlignedUniquePtr<T>(new (ptr) T(std::forward<Args>(args)...),
                             AlignedDeleter(free, opaque));
}

// Similar to MakeUniqueAlignedWithAlloc but using the default alloc/free
// functions.
template <typename T, typename... Args>
AlignedUniquePtr<T> MakeUniqueAligned(Args&&... args) {
  T* ptr = static_cast<T*>(AllocateAlignedBytes(
      sizeof(T), /*alloc_ptr=*/nullptr, /*opaque_ptr=*/nullptr));
  return AlignedUniquePtr<T>(new (ptr) T(std::forward<Args>(args)...),
                             AlignedDeleter());
}

// Helpers for array allocators (avoids overflow)
namespace detail {

// Returns x such that 1u << x == n (if n is a power of two).
static inline constexpr size_t ShiftCount(size_t n) {
  return (n <= 1) ? 0 : 1 + ShiftCount(n / 2);
}

template <typename T>
T* AllocateAlignedItems(size_t items, AllocPtr alloc_ptr, void* opaque_ptr) {
  constexpr size_t size = sizeof(T);

  constexpr bool is_pow2 = (size & (size - 1)) == 0;
  constexpr size_t bits = ShiftCount(size);
  static_assert(!is_pow2 || (1ull << bits) == size, "ShiftCount is incorrect");

  const size_t bytes = is_pow2 ? items << bits : items * size;
  const size_t check = is_pow2 ? bytes >> bits : bytes / size;
  if (check != items) {
    return nullptr;  // overflowed
  }
  return static_cast<T*>(AllocateAlignedBytes(bytes, alloc_ptr, opaque_ptr));
}

}  // namespace detail

// Aligned memory equivalent of make_unique<T[]> for array types using the
// custom allocators alloc/free. This function calls the constructor with the
// passed Args... on every created item. The destructor of each element will be
// called when the AlignedUniquePtr is destroyed.
template <typename T, typename... Args>
AlignedUniquePtr<T[]> MakeUniqueAlignedArrayWithAlloc(
    size_t items, AllocPtr alloc, FreePtr free, void* opaque, Args&&... args) {
  T* ptr = detail::AllocateAlignedItems<T>(items, alloc, opaque);
  if (ptr != nullptr) {
    for (size_t i = 0; i < items; i++) {
      new (ptr + i) T(std::forward<Args>(args)...);
    }
  }
  return AlignedUniquePtr<T[]>(ptr, AlignedDeleter(free, opaque));
}

template <typename T, typename... Args>
AlignedUniquePtr<T[]> MakeUniqueAlignedArray(size_t items, Args&&... args) {
  return MakeUniqueAlignedArrayWithAlloc<T, Args...>(
      items, nullptr, nullptr, nullptr, std::forward<Args>(args)...);
}

// Custom deleter for std::unique_ptr equivalent to using free() as a deleter
// but for aligned memory.
class AlignedFreer {
 public:
  // Pass address of this to ctor to skip deleting externally-owned memory.
  static void DoNothing(void* /*opaque*/, void* /*aligned_pointer*/) {}

  AlignedFreer() : free_(nullptr), opaque_ptr_(nullptr) {}
  AlignedFreer(FreePtr free_ptr, void* opaque_ptr)
      : free_(free_ptr), opaque_ptr_(opaque_ptr) {}

  template <typename T>
  void operator()(T* aligned_pointer) const {
    // TODO(deymo): assert that we are using a POD type T.
    FreeAlignedBytes(aligned_pointer, free_, opaque_ptr_);
  }

 private:
  FreePtr free_;
  void* opaque_ptr_;
};

// Unique pointer to single POD, or (if T is U[]) an array of POD. For non POD
// data use AlignedUniquePtr.
template <typename T>
using AlignedFreeUniquePtr = std::unique_ptr<T, AlignedFreer>;

// Allocate an aligned and uninitialized array of POD values as a unique_ptr.
// Upon destruction of the unique_ptr the aligned array will be freed.
template <typename T>
AlignedFreeUniquePtr<T[]> AllocateAligned(const size_t items, AllocPtr alloc,
                                          FreePtr free, void* opaque) {
  return AlignedFreeUniquePtr<T[]>(
      detail::AllocateAlignedItems<T>(items, alloc, opaque),
      AlignedFreer(free, opaque));
}

// Same as previous AllocateAligned(), using default allocate/free functions.
template <typename T>
AlignedFreeUniquePtr<T[]> AllocateAligned(const size_t items) {
  return AllocateAligned<T>(items, nullptr, nullptr, nullptr);
}

}  // namespace hwy
#endif  // HIGHWAY_HWY_ALIGNED_ALLOCATOR_H_
