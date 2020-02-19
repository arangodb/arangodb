// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_MEMCOPY_H_
#define V8_UTILS_MEMCOPY_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

using Address = uintptr_t;

// ----------------------------------------------------------------------------
// Generated memcpy/memmove for ia32, arm, and mips.

void init_memcopy_functions();

#if defined(V8_TARGET_ARCH_IA32)
// Limit below which the extra overhead of the MemCopy function is likely
// to outweigh the benefits of faster copying.
const size_t kMinComplexMemCopy = 64;

// Copy memory area. No restrictions.
V8_EXPORT_PRIVATE void MemMove(void* dest, const void* src, size_t size);
using MemMoveFunction = void (*)(void* dest, const void* src, size_t size);

// Keep the distinction of "move" vs. "copy" for the benefit of other
// architectures.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  MemMove(dest, src, size);
}
#elif defined(V8_HOST_ARCH_ARM)
using MemCopyUint8Function = void (*)(uint8_t* dest, const uint8_t* src,
                                      size_t size);
V8_EXPORT_PRIVATE extern MemCopyUint8Function memcopy_uint8_function;
V8_INLINE void MemCopyUint8Wrapper(uint8_t* dest, const uint8_t* src,
                                   size_t chars) {
  memcpy(dest, src, chars);
}
// For values < 16, the assembler function is slower than the inlined C code.
const size_t kMinComplexMemCopy = 16;
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                            reinterpret_cast<const uint8_t*>(src), size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}

// For values < 12, the assembler function is slower than the inlined C code.
const int kMinComplexConvertMemCopy = 12;
#elif defined(V8_HOST_ARCH_MIPS)
using MemCopyUint8Function = void (*)(uint8_t* dest, const uint8_t* src,
                                      size_t size);
V8_EXPORT_PRIVATE extern MemCopyUint8Function memcopy_uint8_function;
V8_INLINE void MemCopyUint8Wrapper(uint8_t* dest, const uint8_t* src,
                                   size_t chars) {
  memcpy(dest, src, chars);
}
// For values < 16, the assembler function is slower than the inlined C code.
const size_t kMinComplexMemCopy = 16;
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                            reinterpret_cast<const uint8_t*>(src), size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}
#else
// Copy memory area to disjoint memory area.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  memcpy(dest, src, size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}
const size_t kMinComplexMemCopy = 8;
#endif  // V8_TARGET_ARCH_IA32

// Copies words from |src| to |dst|. The data spans must not overlap.
// |src| and |dst| must be TWord-size aligned.
template <size_t kBlockCopyLimit, typename T>
inline void CopyImpl(T* dst_ptr, const T* src_ptr, size_t count) {
  constexpr int kTWordSize = sizeof(T);
#ifdef DEBUG
  Address dst = reinterpret_cast<Address>(dst_ptr);
  Address src = reinterpret_cast<Address>(src_ptr);
  DCHECK(IsAligned(dst, kTWordSize));
  DCHECK(IsAligned(src, kTWordSize));
  DCHECK(((src <= dst) && ((src + count * kTWordSize) <= dst)) ||
         ((dst <= src) && ((dst + count * kTWordSize) <= src)));
#endif
  if (count == 0) return;

  // Use block copying MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  if (count < kBlockCopyLimit) {
    do {
      count--;
      *dst_ptr++ = *src_ptr++;
    } while (count > 0);
  } else {
    MemCopy(dst_ptr, src_ptr, count * kTWordSize);
  }
}

// Copies kSystemPointerSize-sized words from |src| to |dst|. The data spans
// must not overlap. |src| and |dst| must be kSystemPointerSize-aligned.
inline void CopyWords(Address dst, const Address src, size_t num_words) {
  static const size_t kBlockCopyLimit = 16;
  CopyImpl<kBlockCopyLimit>(reinterpret_cast<Address*>(dst),
                            reinterpret_cast<const Address*>(src), num_words);
}

// Copies data from |src| to |dst|.  The data spans must not overlap.
template <typename T>
inline void CopyBytes(T* dst, const T* src, size_t num_bytes) {
  STATIC_ASSERT(sizeof(T) == 1);
  if (num_bytes == 0) return;
  CopyImpl<kMinComplexMemCopy>(dst, src, num_bytes);
}

inline void MemsetInt32(int32_t* dest, int32_t value, size_t counter) {
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
#define STOS "stosl"
#endif

#if defined(MEMORY_SANITIZER)
  // MemorySanitizer does not understand inline assembly.
#undef STOS
#endif

#if defined(__GNUC__) && defined(STOS)
  asm volatile(
      "cld;"
      "rep ; " STOS
      : "+&c"(counter), "+&D"(dest)
      : "a"(value)
      : "memory", "cc");
#else
  for (size_t i = 0; i < counter; i++) {
    dest[i] = value;
  }
#endif

#undef STOS
}

inline void MemsetPointer(Address* dest, Address value, size_t counter) {
#if V8_HOST_ARCH_IA32
#define STOS "stosl"
#elif V8_HOST_ARCH_X64
#define STOS "stosq"
#endif

#if defined(MEMORY_SANITIZER)
  // MemorySanitizer does not understand inline assembly.
#undef STOS
#endif

#if defined(__GNUC__) && defined(STOS)
  asm volatile(
      "cld;"
      "rep ; " STOS
      : "+&c"(counter), "+&D"(dest)
      : "a"(value)
      : "memory", "cc");
#else
  for (size_t i = 0; i < counter; i++) {
    dest[i] = value;
  }
#endif

#undef STOS
}

template <typename T, typename U>
inline void MemsetPointer(T** dest, U* value, size_t counter) {
#ifdef DEBUG
  T* a = nullptr;
  U* b = nullptr;
  a = b;  // Fake assignment to check assignability.
  USE(a);
#endif  // DEBUG
  MemsetPointer(reinterpret_cast<Address*>(dest),
                reinterpret_cast<Address>(value), counter);
}

// Copy from 8bit/16bit chars to 8bit/16bit chars. Values are zero-extended if
// needed. Ranges are not allowed to overlap.
// The separate declaration is needed for the V8_NONNULL, which is not allowed
// on a definition.
template <typename SrcType, typename DstType>
void CopyChars(DstType* dst, const SrcType* src, size_t count) V8_NONNULL(1, 2);

template <typename SrcType, typename DstType>
void CopyChars(DstType* dst, const SrcType* src, size_t count) {
  STATIC_ASSERT(std::is_integral<SrcType>::value);
  STATIC_ASSERT(std::is_integral<DstType>::value);

#ifdef DEBUG
  // Check for no overlap, otherwise {std::copy_n} cannot be used.
  Address src_start = reinterpret_cast<Address>(src);
  Address src_end = src_start + count * sizeof(SrcType);
  Address dst_start = reinterpret_cast<Address>(dst);
  Address dst_end = dst_start + count * sizeof(DstType);
  DCHECK(src_end <= dst_start || dst_end <= src_start);
#endif

  using SrcTypeUnsigned = typename std::make_unsigned<SrcType>::type;
  using DstTypeUnsigned = typename std::make_unsigned<DstType>::type;
  std::copy_n(reinterpret_cast<const SrcTypeUnsigned*>(src), count,
              reinterpret_cast<DstTypeUnsigned*>(dst));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_MEMCOPY_H_
