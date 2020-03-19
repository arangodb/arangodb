// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_SLOT_H_
#define V8_OBJECTS_EMBEDDER_DATA_SLOT_H_

#include <utility>

#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/objects/slots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class EmbedderDataArray;
class JSObject;
class Object;

// An EmbedderDataSlot instance describes a kEmbedderDataSlotSize field ("slot")
// holding an embedder data which may contain raw aligned pointer or a tagged
// pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using respective load_XX() and
// store_XX() methods.
// Storing heap object through this slot may require triggering write barriers
// so this operation must be done via static store_tagged() methods.
class EmbedderDataSlot
    : public SlotBase<EmbedderDataSlot, Address, kTaggedSize> {
 public:
  EmbedderDataSlot() : SlotBase(kNullAddress) {}
  V8_INLINE EmbedderDataSlot(EmbedderDataArray array, int entry_index);
  V8_INLINE EmbedderDataSlot(JSObject object, int embedder_field_index);

#if defined(V8_TARGET_BIG_ENDIAN) && defined(V8_COMPRESS_POINTERS)
  static constexpr int kTaggedPayloadOffset = kTaggedSize;
#else
  static constexpr int kTaggedPayloadOffset = 0;
#endif

#ifdef V8_COMPRESS_POINTERS
  // The raw payload is located in the other tagged part of the full pointer.
  static constexpr int kRawPayloadOffset = kTaggedSize - kTaggedPayloadOffset;
#endif
  static constexpr int kRequiredPtrAlignment = kSmiTagSize;

  // Opaque type used for storing raw embedder data.
  using RawData = Address;

  V8_INLINE Object load_tagged() const;
  V8_INLINE void store_smi(Smi value);

  // Setting an arbitrary tagged value requires triggering a write barrier
  // which requires separate object and offset values, therefore these static
  // functions also has the target object parameter.
  static V8_INLINE void store_tagged(EmbedderDataArray array, int entry_index,
                                     Object value);
  static V8_INLINE void store_tagged(JSObject object, int embedder_field_index,
                                     Object value);

  // Tries reinterpret the value as an aligned pointer and sets *out_result to
  // the pointer-like value. Note, that some Smis could still look like an
  // aligned pointers.
  // Returns true on success.
  V8_INLINE bool ToAlignedPointer(void** out_result) const;

  // Returns true if the pointer was successfully stored or false it the pointer
  // was improperly aligned.
  V8_INLINE V8_WARN_UNUSED_RESULT bool store_aligned_pointer(void* ptr);

  V8_INLINE RawData load_raw(const DisallowHeapAllocation& no_gc) const;
  V8_INLINE void store_raw(RawData data, const DisallowHeapAllocation& no_gc);

 private:
  // Stores given value to the embedder data slot in a concurrent-marker
  // friendly manner (tagged part of the slot is written atomically).
  V8_INLINE void gc_safe_store(Address value);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_SLOT_H_
