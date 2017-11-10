// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MEMORY_H_
#define V8_MEMORY_H_

namespace v8 {
namespace internal {

// Memory provides an interface to 'raw' memory. It encapsulates the casts
// that typically are needed when incompatible pointer types are used.

class Memory {
 public:
  static uint8_t& uint8_at(Address addr) {
    return *reinterpret_cast<uint8_t*>(addr);
  }

  static uint16_t& uint16_at(Address addr)  {
    return *reinterpret_cast<uint16_t*>(addr);
  }

  static uint32_t& uint32_at(Address addr)  {
    return *reinterpret_cast<uint32_t*>(addr);
  }

  static int32_t& int32_at(Address addr)  {
    return *reinterpret_cast<int32_t*>(addr);
  }

  static uint64_t& uint64_at(Address addr)  {
    return *reinterpret_cast<uint64_t*>(addr);
  }

  static int& int_at(Address addr)  {
    return *reinterpret_cast<int*>(addr);
  }

  static unsigned& unsigned_at(Address addr) {
    return *reinterpret_cast<unsigned*>(addr);
  }

  static intptr_t& intptr_at(Address addr)  {
    return *reinterpret_cast<intptr_t*>(addr);
  }

  static uintptr_t& uintptr_at(Address addr) {
    return *reinterpret_cast<uintptr_t*>(addr);
  }

  static double& double_at(Address addr)  {
    return *reinterpret_cast<double*>(addr);
  }

  static Address& Address_at(Address addr)  {
    return *reinterpret_cast<Address*>(addr);
  }

  static Object*& Object_at(Address addr)  {
    return *reinterpret_cast<Object**>(addr);
  }

  static Handle<Object>& Object_Handle_at(Address addr)  {
    return *reinterpret_cast<Handle<Object>*>(addr);
  }

  static bool IsAddressInRange(Address base, Address address, uint32_t size) {
    uintptr_t numeric_base = reinterpret_cast<uintptr_t>(base);
    uintptr_t numeric_address = reinterpret_cast<uintptr_t>(address);
    return numeric_base <= numeric_address &&
           numeric_address < numeric_base + size;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_MEMORY_H_
