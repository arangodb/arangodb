// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8MEMORY_H_
#define V8_V8MEMORY_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

// Memory provides an interface to 'raw' memory. It encapsulates the casts
// that typically are needed when incompatible pointer types are used.
// Note that this class currently relies on undefined behaviour. There is a
// proposal (http://wg21.link/p0593r2) to make it defined behaviour though.
template <class T>
T& Memory(Address addr) {
  return *reinterpret_cast<T*>(addr);
}
template <class T>
T& Memory(byte* addr) {
  return Memory<T>(reinterpret_cast<Address>(addr));
}

template <typename V>
static inline V ReadUnalignedValue(Address p) {
  ASSERT_TRIVIALLY_COPYABLE(V);
#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM)
  return *reinterpret_cast<const V*>(p);
#else   // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
  V r;
  memmove(&r, reinterpret_cast<void*>(p), sizeof(V));
  return r;
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
}

template <typename V>
static inline void WriteUnalignedValue(Address p, V value) {
  ASSERT_TRIVIALLY_COPYABLE(V);
#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM)
  *(reinterpret_cast<V*>(p)) = value;
#else   // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
  memmove(reinterpret_cast<void*>(p), &value, sizeof(V));
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_ARM
}

static inline double ReadFloatValue(Address p) {
  return ReadUnalignedValue<float>(p);
}

static inline double ReadDoubleValue(Address p) {
  return ReadUnalignedValue<double>(p);
}

static inline void WriteDoubleValue(Address p, double value) {
  WriteUnalignedValue(p, value);
}

static inline uint16_t ReadUnalignedUInt16(Address p) {
  return ReadUnalignedValue<uint16_t>(p);
}

static inline void WriteUnalignedUInt16(Address p, uint16_t value) {
  WriteUnalignedValue(p, value);
}

static inline uint32_t ReadUnalignedUInt32(Address p) {
  return ReadUnalignedValue<uint32_t>(p);
}

static inline void WriteUnalignedUInt32(Address p, uint32_t value) {
  WriteUnalignedValue(p, value);
}

template <typename V>
static inline V ReadLittleEndianValue(Address p) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  return ReadUnalignedValue<V>(p);
#elif defined(V8_TARGET_BIG_ENDIAN)
  V ret{};
  const byte* src = reinterpret_cast<const byte*>(p);
  byte* dst = reinterpret_cast<byte*>(&ret);
  for (size_t i = 0; i < sizeof(V); i++) {
    dst[i] = src[sizeof(V) - i - 1];
  }
  return ret;
#endif  // V8_TARGET_LITTLE_ENDIAN
}

template <typename V>
static inline void WriteLittleEndianValue(Address p, V value) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  WriteUnalignedValue<V>(p, value);
#elif defined(V8_TARGET_BIG_ENDIAN)
  byte* src = reinterpret_cast<byte*>(&value);
  byte* dst = reinterpret_cast<byte*>(p);
  for (size_t i = 0; i < sizeof(V); i++) {
    dst[i] = src[sizeof(V) - i - 1];
  }
#endif  // V8_TARGET_LITTLE_ENDIAN
}

template <typename V>
static inline V ReadLittleEndianValue(V* p) {
  return ReadLittleEndianValue<V>(reinterpret_cast<Address>(p));
}

template <typename V>
static inline void WriteLittleEndianValue(V* p, V value) {
  WriteLittleEndianValue<V>(reinterpret_cast<Address>(p), value);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_V8MEMORY_H_
