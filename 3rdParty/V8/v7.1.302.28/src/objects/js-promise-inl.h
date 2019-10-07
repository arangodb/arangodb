// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PROMISE_INL_H_
#define V8_OBJECTS_JS_PROMISE_INL_H_

#include "src/objects/js-promise.h"

#include "src/objects-inl.h"  // Needed for write barriers
#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(JSPromise)

ACCESSORS(JSPromise, reactions_or_result, Object, kReactionsOrResultOffset)
SMI_ACCESSORS(JSPromise, flags, kFlagsOffset)
BOOL_ACCESSORS(JSPromise, flags, has_handler, kHasHandlerBit)
BOOL_ACCESSORS(JSPromise, flags, handled_hint, kHandledHintBit)

Object* JSPromise::result() const {
  DCHECK_NE(Promise::kPending, status());
  return reactions_or_result();
}

Object* JSPromise::reactions() const {
  DCHECK_EQ(Promise::kPending, status());
  return reactions_or_result();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PROMISE_INL_H_
