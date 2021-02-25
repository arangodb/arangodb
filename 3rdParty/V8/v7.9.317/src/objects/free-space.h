// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FREE_SPACE_H_
#define V8_OBJECTS_FREE_SPACE_H_

#include "src/objects/heap-object.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// FreeSpace are fixed-size free memory blocks used by the heap and GC.
// They look like heap objects (are heap object tagged and have a map) so that
// the heap remains iterable.  They have a size and a next pointer.
// The next pointer is the raw address of the next FreeSpace object (or NULL)
// in the free list.
class FreeSpace : public HeapObject {
 public:
  // [size]: size of the free space including the header.
  inline int size() const;
  inline void set_size(int value);

  inline int relaxed_read_size() const;
  inline void relaxed_write_size(int value);

  inline int Size();

  // Accessors for the next field.
  inline FreeSpace next();
  inline void set_next(FreeSpace next);

  inline static FreeSpace cast(HeapObject obj);
  inline static FreeSpace unchecked_cast(const Object obj);

  // Dispatched behavior.
  DECL_PRINTER(FreeSpace)
  DECL_VERIFIER(FreeSpace)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_FREE_SPACE_FIELDS)

 private:
  inline bool IsValid();

  OBJECT_CONSTRUCTORS(FreeSpace, HeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FREE_SPACE_H_
