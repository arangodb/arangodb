// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/api.h"
#include "src/debug/debug.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/macro-assembler.h"
#include "src/objects.h"
#include "test/cctest/heap/heap-utils.h"

using namespace v8::internal;

namespace {


template<typename HashMap>
static void TestHashMap(Handle<HashMap> table) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<JSObject> a = factory->NewJSArray(7);
  Handle<JSObject> b = factory->NewJSArray(11);
  table = HashMap::Put(table, a, b);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_EQ(table->Lookup(a), *b);
  // When the key does not exist in the map, Lookup returns the hole.
  CHECK_EQ(table->Lookup(b), CcTest::heap()->the_hole_value());

  // Keys still have to be valid after objects were moved.
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_EQ(table->Lookup(a), *b);
  CHECK_EQ(table->Lookup(b), CcTest::heap()->the_hole_value());

  // Keys that are overwritten should not change number of elements.
  table = HashMap::Put(table, a, factory->NewJSArray(13));
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK_NE(table->Lookup(a), *b);

  // Keys that have been removed are mapped to the hole.
  bool was_present = false;
  table = HashMap::Remove(table, a, &was_present);
  CHECK(was_present);
  CHECK_EQ(table->NumberOfElements(), 0);
  CHECK_EQ(table->Lookup(a), CcTest::heap()->the_hole_value());

  // Keys should map back to their respective values and also should get
  // an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    Handle<JSObject> value = factory->NewJSArray(11);
    table = HashMap::Put(table, key, value);
    CHECK_EQ(table->NumberOfElements(), i + 1);
    CHECK_NE(table->FindEntry(key), HashMap::kNotFound);
    CHECK_EQ(table->Lookup(key), *value);
    CHECK(JSReceiver::GetIdentityHash(isolate, key)->IsSmi());
  }

  // Keys never added to the map which already have an identity hash
  // code should not be found.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    CHECK(JSReceiver::GetOrCreateIdentityHash(isolate, key)->IsSmi());
    CHECK_EQ(table->FindEntry(key), HashMap::kNotFound);
    CHECK_EQ(table->Lookup(key), CcTest::heap()->the_hole_value());
    CHECK(JSReceiver::GetIdentityHash(isolate, key)->IsSmi());
  }

  // Keys that don't have an identity hash should not be found and also
  // should not get an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    CHECK_EQ(table->Lookup(key), CcTest::heap()->the_hole_value());
    Object* identity_hash = JSReceiver::GetIdentityHash(isolate, key);
    CHECK_EQ(CcTest::heap()->undefined_value(), identity_hash);
  }
}


TEST(HashMap) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  TestHashMap(ObjectHashTable::New(isolate, 23));
}

template <typename HashSet>
static void TestHashSet(Handle<HashSet> table) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<JSObject> a = factory->NewJSArray(7);
  Handle<JSObject> b = factory->NewJSArray(11);
  table = HashSet::Add(table, a);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK(table->Has(isolate, a));
  CHECK(!table->Has(isolate, b));

  // Keys still have to be valid after objects were moved.
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK(table->Has(isolate, a));
  CHECK(!table->Has(isolate, b));

  // Keys that are overwritten should not change number of elements.
  table = HashSet::Add(table, a);
  CHECK_EQ(table->NumberOfElements(), 1);
  CHECK(table->Has(isolate, a));
  CHECK(!table->Has(isolate, b));

  // Keys that have been removed are mapped to the hole.
  // TODO(cbruni): not implemented yet.
  // bool was_present = false;
  // table = HashSet::Remove(table, a, &was_present);
  // CHECK(was_present);
  // CHECK_EQ(table->NumberOfElements(), 0);
  // CHECK(!table->Has(a));
  // CHECK(!table->Has(b));

  // Keys should map back to their respective values and also should get
  // an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    table = HashSet::Add(table, key);
    CHECK_EQ(table->NumberOfElements(), i + 2);
    CHECK(table->Has(isolate, key));
    CHECK(JSReceiver::GetIdentityHash(isolate, key)->IsSmi());
  }

  // Keys never added to the map which already have an identity hash
  // code should not be found.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    CHECK(JSReceiver::GetOrCreateIdentityHash(isolate, key)->IsSmi());
    CHECK(!table->Has(isolate, key));
    CHECK(JSReceiver::GetIdentityHash(isolate, key)->IsSmi());
  }

  // Keys that don't have an identity hash should not be found and also
  // should not get an identity hash code generated.
  for (int i = 0; i < 100; i++) {
    Handle<JSReceiver> key = factory->NewJSArray(7);
    CHECK(!table->Has(isolate, key));
    Object* identity_hash = JSReceiver::GetIdentityHash(isolate, key);
    CHECK_EQ(CcTest::heap()->undefined_value(), identity_hash);
  }
}

TEST(HashSet) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  TestHashSet(ObjectHashSet::New(isolate, 23));
}

class ObjectHashTableTest: public ObjectHashTable {
 public:
  void insert(int entry, int key, int value) {
    set(EntryToIndex(entry), Smi::FromInt(key));
    set(EntryToIndex(entry) + 1, Smi::FromInt(value));
  }

  int lookup(int key) {
    Handle<Object> key_obj(Smi::FromInt(key), GetIsolate());
    return Smi::cast(Lookup(key_obj))->value();
  }

  int capacity() {
    return Capacity();
  }
};


TEST(HashTableRehash) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(context->GetIsolate());
  // Test almost filled table.
  {
    Handle<ObjectHashTable> table = ObjectHashTable::New(isolate, 100);
    ObjectHashTableTest* t = reinterpret_cast<ObjectHashTableTest*>(*table);
    int capacity = t->capacity();
    for (int i = 0; i < capacity - 1; i++) {
      t->insert(i, i * i, i);
    }
    t->Rehash(handle(Smi::kZero, isolate));
    for (int i = 0; i < capacity - 1; i++) {
      CHECK_EQ(i, t->lookup(i * i));
    }
  }
  // Test half-filled table.
  {
    Handle<ObjectHashTable> table = ObjectHashTable::New(isolate, 100);
    ObjectHashTableTest* t = reinterpret_cast<ObjectHashTableTest*>(*table);
    int capacity = t->capacity();
    for (int i = 0; i < capacity / 2; i++) {
      t->insert(i, i * i, i);
    }
    t->Rehash(handle(Smi::kZero, isolate));
    for (int i = 0; i < capacity / 2; i++) {
      CHECK_EQ(i, t->lookup(i * i));
    }
  }
}


#ifdef DEBUG
template<class HashSet>
static void TestHashSetCausesGC(Handle<HashSet> table) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<JSObject> key = factory->NewJSArray(0);

  // Simulate a full heap so that generating an identity hash code
  // in subsequent calls will request GC.
  heap::SimulateFullSpace(CcTest::heap()->new_space());
  heap::SimulateFullSpace(CcTest::heap()->old_space());

  // Calling Contains() should not cause GC ever.
  int gc_count = isolate->heap()->gc_count();
  CHECK(!table->Contains(key));
  CHECK(gc_count == isolate->heap()->gc_count());

  // Calling Remove() will not cause GC in this case.
  bool was_present = false;
  table = HashSet::Remove(table, key, &was_present);
  CHECK(!was_present);
  CHECK(gc_count == isolate->heap()->gc_count());

  // Calling Add() should cause GC.
  table = HashSet::Add(table, key);
  CHECK(gc_count < isolate->heap()->gc_count());
}
#endif


#ifdef DEBUG
template<class HashMap>
static void TestHashMapCausesGC(Handle<HashMap> table) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<JSObject> key = factory->NewJSArray(0);

  // Simulate a full heap so that generating an identity hash code
  // in subsequent calls will request GC.
  heap::SimulateFullSpace(CcTest::heap()->new_space());
  heap::SimulateFullSpace(CcTest::heap()->old_space());

  // Calling Lookup() should not cause GC ever.
  CHECK(table->Lookup(key)->IsTheHole(isolate));

  // Calling Put() should request GC by returning a failure.
  int gc_count = isolate->heap()->gc_count();
  HashMap::Put(table, key, key);
  CHECK(gc_count < isolate->heap()->gc_count());
}


TEST(ObjectHashTableCausesGC) {
  i::FLAG_stress_compaction = false;
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  TestHashMapCausesGC(ObjectHashTable::New(isolate, 1));
}
#endif

TEST(SetRequiresCopyOnCapacityChange) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<NameDictionary> dict = NameDictionary::New(isolate, 0, TENURED);
  dict->SetRequiresCopyOnCapacityChange();
  Handle<Name> key = isolate->factory()->InternalizeString(
      v8::Utils::OpenHandle(*v8_str("key")));
  Handle<Object> value = handle(Smi::kZero, isolate);
  Handle<NameDictionary> new_dict =
      NameDictionary::Add(dict, key, value, PropertyDetails::Empty());
  CHECK_NE(*dict, *new_dict);
}

}  // namespace
