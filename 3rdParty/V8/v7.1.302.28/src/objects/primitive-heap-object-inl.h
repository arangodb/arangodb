// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PRIMITIVE_HEAP_OBJECT_INL_H_
#define V8_OBJECTS_PRIMITIVE_HEAP_OBJECT_INL_H_

#include "src/objects/primitive-heap-object.h"

#include "src/objects/heap-object-inl.h"
#include "torque-generated/class-definitions-tq-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(PrimitiveHeapObject)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PRIMITIVE_HEAP_OBJECT_INL_H_
