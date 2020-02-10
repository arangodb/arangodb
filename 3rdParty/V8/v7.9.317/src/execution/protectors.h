// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_PROTECTORS_H_
#define V8_EXECUTION_PROTECTORS_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Protectors : public AllStatic {
 public:
  static const int kProtectorValid = 1;
  static const int kProtectorInvalid = 0;

#define DECLARED_PROTECTORS_ON_NATIVE_CONTEXT(V) \
  V(RegExpSpeciesLookupChainProtector, regexp_species_protector)

#define DECLARED_PROTECTORS_ON_ISOLATE(V)                                     \
  V(ArrayBufferDetaching, ArrayBufferDetachingProtector,                      \
    array_buffer_detaching_protector)                                         \
  V(ArrayConstructor, ArrayConstructorProtector, array_constructor_protector) \
  V(ArrayIteratorLookupChain, ArrayIteratorProtector,                         \
    array_iterator_protector)                                                 \
  V(ArraySpeciesLookupChain, ArraySpeciesProtector, array_species_protector)  \
  V(IsConcatSpreadableLookupChain, IsConcatSpreadableProtector,               \
    is_concat_spreadable_protector)                                           \
  V(NoElements, NoElementsProtector, no_elements_protector)                   \
                                                                              \
  /* The MapIterator protector protects the original iteration behaviors   */ \
  /* of Map.prototype.keys(), Map.prototype.values(), and                  */ \
  /* Set.prototype.entries(). It does not protect the original iteration   */ \
  /* behavior of Map.prototype[Symbol.iterator]().                         */ \
  /* The protector is invalidated when:                                    */ \
  /* * The 'next' property is set on an object where the property holder   */ \
  /*   is the %MapIteratorPrototype% (e.g. because the object is that very */ \
  /*   prototype).                                                         */ \
  /* * The 'Symbol.iterator' property is set on an object where the        */ \
  /*   property holder is the %IteratorPrototype%. Note that this also     */ \
  /*   invalidates the SetIterator protector (see below).                  */ \
  V(MapIteratorLookupChain, MapIteratorProtector, map_iterator_protector)     \
  V(PromiseHook, PromiseHookProtector, promise_hook_protector)                \
  V(PromiseThenLookupChain, PromiseThenProtector, promise_then_protector)     \
  V(PromiseResolveLookupChain, PromiseResolveProtector,                       \
    promise_resolve_protector)                                                \
  V(PromiseSpeciesLookupChain, PromiseSpeciesProtector,                       \
    promise_species_protector)                                                \
                                                                              \
  /* The SetIterator protector protects the original iteration behavior of */ \
  /* Set.prototype.keys(), Set.prototype.values(),                         */ \
  /* Set.prototype.entries(), and Set.prototype[Symbol.iterator](). The    */ \
  /* protector is invalidated when:                                        */ \
  /* * The 'next' property is set on an object where the property holder   */ \
  /*   is the %SetIteratorPrototype% (e.g. because the object is that very */ \
  /*   prototype).                                                         */ \
  /* * The 'Symbol.iterator' property is set on an object where the        */ \
  /*   property holder is the %SetPrototype% OR %IteratorPrototype%. This  */ \
  /*   means that setting Symbol.iterator on a MapIterator object can also */ \
  /*   invalidate the SetIterator protector, and vice versa, setting       */ \
  /*   Symbol.iterator on a SetIterator object can also invalidate the     */ \
  /*   MapIterator. This is an over-approximation for the sake of          */ \
  /*   simplicity.                                                         */ \
  V(SetIteratorLookupChain, SetIteratorProtector, set_iterator_protector)     \
                                                                              \
  /* The StringIteratorProtector protects the original string iteration    */ \
  /* behavior for primitive strings. As long as the                        */ \
  /* StringIteratorProtector is valid, iterating over a primitive string   */ \
  /* is guaranteed to be unobservable from user code and can thus be cut   */ \
  /* short. More specifically, the protector gets invalidated as soon as   */ \
  /* either String.prototype[Symbol.iterator] or                           */ \
  /* String.prototype[Symbol.iterator]().next is modified. This guarantee  */ \
  /* does not apply to string objects (as opposed to primitives), since    */ \
  /* they could define their own Symbol.iterator.                          */ \
  /* String.prototype itself does not need to be protected, since it is    */ \
  /* non-configurable and non-writable.                                    */ \
  V(StringIteratorLookupChain, StringIteratorProtector,                       \
    string_iterator_protector)                                                \
  V(StringLengthOverflowLookupChain, StringLengthProtector,                   \
    string_length_protector)                                                  \
  V(TypedArraySpeciesLookupChain, TypedArraySpeciesProtector,                 \
    typed_array_species_protector)

#define DECLARE_PROTECTOR_ON_NATIVE_CONTEXT(name, unused_cell) \
  V8_EXPORT_PRIVATE static inline bool Is##name##Intact(       \
      Handle<NativeContext> native_context);                   \
  V8_EXPORT_PRIVATE static void Invalidate##name(              \
      Isolate* isolate, Handle<NativeContext> native_context);

  DECLARED_PROTECTORS_ON_NATIVE_CONTEXT(DECLARE_PROTECTOR_ON_NATIVE_CONTEXT)
#undef DECLARE_PROTECTOR_ON_NATIVE_CONTEXT

#define DECLARE_PROTECTOR_ON_ISOLATE(name, unused_root_index, unused_cell) \
  V8_EXPORT_PRIVATE static inline bool Is##name##Intact(Isolate* isolate); \
  V8_EXPORT_PRIVATE static void Invalidate##name(Isolate* isolate);

  DECLARED_PROTECTORS_ON_ISOLATE(DECLARE_PROTECTOR_ON_ISOLATE)
#undef DECLARE_PROTECTOR_ON_ISOLATE
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_PROTECTORS_H_
