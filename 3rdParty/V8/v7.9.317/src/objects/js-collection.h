// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLECTION_H_
#define V8_OBJECTS_JS_COLLECTION_H_

#include "src/objects/js-collection-iterator.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class OrderedHashSet;
class OrderedHashMap;

class JSCollection
    : public TorqueGeneratedJSCollection<JSCollection, JSObject> {
 public:
  static const int kAddFunctionDescriptorIndex = 3;

  TQ_OBJECT_CONSTRUCTORS(JSCollection)
};

// The JSSet describes EcmaScript Harmony sets
class JSSet : public TorqueGeneratedJSSet<JSSet, JSCollection> {
 public:
  static void Initialize(Handle<JSSet> set, Isolate* isolate);
  static void Clear(Isolate* isolate, Handle<JSSet> set);

  // Dispatched behavior.
  DECL_PRINTER(JSSet)
  DECL_VERIFIER(JSSet)

  TQ_OBJECT_CONSTRUCTORS(JSSet)
};

class JSSetIterator
    : public OrderedHashTableIterator<JSSetIterator, OrderedHashSet> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSSetIterator)
  DECL_VERIFIER(JSSetIterator)

  DECL_CAST(JSSetIterator)

  OBJECT_CONSTRUCTORS(JSSetIterator,
                      OrderedHashTableIterator<JSSetIterator, OrderedHashSet>);
};

// The JSMap describes EcmaScript Harmony maps
class JSMap : public TorqueGeneratedJSMap<JSMap, JSCollection> {
 public:
  static void Initialize(Handle<JSMap> map, Isolate* isolate);
  static void Clear(Isolate* isolate, Handle<JSMap> map);

  // Dispatched behavior.
  DECL_PRINTER(JSMap)
  DECL_VERIFIER(JSMap)

  TQ_OBJECT_CONSTRUCTORS(JSMap)
};

class JSMapIterator
    : public OrderedHashTableIterator<JSMapIterator, OrderedHashMap> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSMapIterator)
  DECL_VERIFIER(JSMapIterator)

  DECL_CAST(JSMapIterator)

  // Returns the current value of the iterator. This should only be called when
  // |HasMore| returns true.
  inline Object CurrentValue();

  OBJECT_CONSTRUCTORS(JSMapIterator,
                      OrderedHashTableIterator<JSMapIterator, OrderedHashMap>);
};

// Base class for both JSWeakMap and JSWeakSet
class JSWeakCollection
    : public TorqueGeneratedJSWeakCollection<JSWeakCollection, JSObject> {
 public:
  static void Initialize(Handle<JSWeakCollection> collection, Isolate* isolate);
  V8_EXPORT_PRIVATE static void Set(Handle<JSWeakCollection> collection,
                                    Handle<Object> key, Handle<Object> value,
                                    int32_t hash);
  static bool Delete(Handle<JSWeakCollection> collection, Handle<Object> key,
                     int32_t hash);
  static Handle<JSArray> GetEntries(Handle<JSWeakCollection> holder,
                                    int max_entries);

  static const int kAddFunctionDescriptorIndex = 3;

  // Iterates the function object according to the visiting policy.
  class BodyDescriptorImpl;

  // Visit the whole object.
  using BodyDescriptor = BodyDescriptorImpl;

  static const int kSizeOfAllWeakCollections = kHeaderSize;

  TQ_OBJECT_CONSTRUCTORS(JSWeakCollection)
};

// The JSWeakMap describes EcmaScript Harmony weak maps
class JSWeakMap : public TorqueGeneratedJSWeakMap<JSWeakMap, JSWeakCollection> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSWeakMap)
  DECL_VERIFIER(JSWeakMap)

  STATIC_ASSERT(kSize == kSizeOfAllWeakCollections);
  TQ_OBJECT_CONSTRUCTORS(JSWeakMap)
};

// The JSWeakSet describes EcmaScript Harmony weak sets
class JSWeakSet : public TorqueGeneratedJSWeakSet<JSWeakSet, JSWeakCollection> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(JSWeakSet)
  DECL_VERIFIER(JSWeakSet)

  STATIC_ASSERT(kSize == kSizeOfAllWeakCollections);
  TQ_OBJECT_CONSTRUCTORS(JSWeakSet)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_H_
