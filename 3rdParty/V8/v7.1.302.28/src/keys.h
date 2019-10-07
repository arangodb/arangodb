// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_KEYS_H_
#define V8_KEYS_H_

#include "src/objects.h"
#include "src/objects/hash-table.h"
#include "src/objects/ordered-hash-table.h"

namespace v8 {
namespace internal {

class JSProxy;

enum AddKeyConversion { DO_NOT_CONVERT, CONVERT_TO_ARRAY_INDEX };

// This is a helper class for JSReceiver::GetKeys which collects and sorts keys.
// GetKeys needs to sort keys per prototype level, first showing the integer
// indices from elements then the strings from the properties. However, this
// does not apply to proxies which are in full control of how the keys are
// sorted.
//
// For performance reasons the KeyAccumulator internally separates integer keys
// in |elements_| into sorted lists per prototype level. String keys are
// collected in |string_properties_|, a single OrderedHashSet (similar for
// Symbols in |symbol_properties_|. To separate the keys per level later when
// assembling the final list, |levelLengths_| keeps track of the number of
// String and Symbol keys per level.
//
// Only unique keys are kept by the KeyAccumulator, strings are stored in a
// HashSet for inexpensive lookups. Integer keys are kept in sorted lists which
// are more compact and allow for reasonably fast includes check.
class KeyAccumulator final {
 public:
  KeyAccumulator(Isolate* isolate, KeyCollectionMode mode,
                 PropertyFilter filter)
      : isolate_(isolate), mode_(mode), filter_(filter) {}
  ~KeyAccumulator() = default;

  static MaybeHandle<FixedArray> GetKeys(
      Handle<JSReceiver> object, KeyCollectionMode mode, PropertyFilter filter,
      GetKeysConversion keys_conversion = GetKeysConversion::kKeepNumbers,
      bool is_for_in = false, bool skip_indices = false);

  Handle<FixedArray> GetKeys(
      GetKeysConversion convert = GetKeysConversion::kKeepNumbers);
  Maybe<bool> CollectKeys(Handle<JSReceiver> receiver,
                          Handle<JSReceiver> object);
  Maybe<bool> CollectOwnElementIndices(Handle<JSReceiver> receiver,
                                       Handle<JSObject> object);
  Maybe<bool> CollectOwnPropertyNames(Handle<JSReceiver> receiver,
                                      Handle<JSObject> object);
  Maybe<bool> CollectAccessCheckInterceptorKeys(
      Handle<AccessCheckInfo> access_check_info, Handle<JSReceiver> receiver,
      Handle<JSObject> object);

  // Might return directly the object's enum_cache, copy the result before using
  // as an elements backing store for a JSObject.
  // Does not throw for uninitialized exports in module namespace objects, so
  // this has to be checked separately.
  static Handle<FixedArray> GetOwnEnumPropertyKeys(Isolate* isolate,
                                                   Handle<JSObject> object);

  void AddKey(Object* key, AddKeyConversion convert = DO_NOT_CONVERT);
  void AddKey(Handle<Object> key, AddKeyConversion convert = DO_NOT_CONVERT);
  void AddKeys(Handle<FixedArray> array, AddKeyConversion convert);
  void AddKeys(Handle<JSObject> array_like, AddKeyConversion convert);

  // Jump to the next level, pushing the current |levelLength_| to
  // |levelLengths_| and adding a new list to |elements_|.
  Isolate* isolate() { return isolate_; }
  // Filter keys based on their property descriptors.
  PropertyFilter filter() { return filter_; }
  // The collection mode defines whether we collect the keys from the prototype
  // chain or only look at the receiver.
  KeyCollectionMode mode() { return mode_; }
  // In case of for-in loops we have to treat JSProxy keys differently and
  // deduplicate them. Additionally we convert JSProxy keys back to array
  // indices.
  void set_is_for_in(bool value) { is_for_in_ = value; }
  void set_skip_indices(bool value) { skip_indices_ = value; }
  // The last_non_empty_prototype is used to limit the prototypes for which
  // we have to keep track of non-enumerable keys that can shadow keys
  // repeated on the prototype chain.
  void set_last_non_empty_prototype(Handle<JSReceiver> object) {
    last_non_empty_prototype_ = object;
  }
  // Shadowing keys are used to filter keys. This happens when non-enumerable
  // keys appear again on the prototype chain.
  void AddShadowingKey(Object* key);
  void AddShadowingKey(Handle<Object> key);

 private:
  Maybe<bool> CollectOwnKeys(Handle<JSReceiver> receiver,
                             Handle<JSObject> object);
  Maybe<bool> CollectOwnJSProxyKeys(Handle<JSReceiver> receiver,
                                    Handle<JSProxy> proxy);
  Maybe<bool> CollectOwnJSProxyTargetKeys(Handle<JSProxy> proxy,
                                          Handle<JSReceiver> target);
  Maybe<bool> AddKeysFromJSProxy(Handle<JSProxy> proxy,
                                 Handle<FixedArray> keys);
  bool IsShadowed(Handle<Object> key);
  bool HasShadowingKeys();
  Handle<OrderedHashSet> keys();

  Isolate* isolate_;
  // keys_ is either an Handle<OrderedHashSet> or in the case of own JSProxy
  // keys a Handle<FixedArray>. The OrderedHashSet is in-place converted to the
  // result list, a FixedArray containing all collected keys.
  Handle<FixedArray> keys_;
  Handle<JSReceiver> last_non_empty_prototype_;
  Handle<ObjectHashSet> shadowing_keys_;
  KeyCollectionMode mode_;
  PropertyFilter filter_;
  bool is_for_in_ = false;
  bool skip_indices_ = false;
  // For all the keys on the first receiver adding a shadowing key we can skip
  // the shadow check.
  bool skip_shadow_check_ = true;

  DISALLOW_COPY_AND_ASSIGN(KeyAccumulator);
};

// The FastKeyAccumulator handles the cases where there are no elements on the
// prototype chain and forwords the complex/slow cases to the normal
// KeyAccumulator. This significantly speeds up the cases where the OWN_ONLY
// case where we do not have to walk the prototype chain.
class FastKeyAccumulator {
 public:
  FastKeyAccumulator(Isolate* isolate, Handle<JSReceiver> receiver,
                     KeyCollectionMode mode, PropertyFilter filter,
                     bool is_for_in = false, bool skip_indices = false)
      : isolate_(isolate),
        receiver_(receiver),
        mode_(mode),
        filter_(filter),
        is_for_in_(is_for_in),
        skip_indices_(skip_indices) {
    Prepare();
  }

  bool is_receiver_simple_enum() { return is_receiver_simple_enum_; }
  bool has_empty_prototype() { return has_empty_prototype_; }

  MaybeHandle<FixedArray> GetKeys(
      GetKeysConversion convert = GetKeysConversion::kKeepNumbers);

 private:
  void Prepare();
  MaybeHandle<FixedArray> GetKeysFast(GetKeysConversion convert);
  MaybeHandle<FixedArray> GetKeysSlow(GetKeysConversion convert);

  MaybeHandle<FixedArray> GetOwnKeysWithUninitializedEnumCache();

  Isolate* isolate_;
  Handle<JSReceiver> receiver_;
  Handle<JSReceiver> last_non_empty_prototype_;
  KeyCollectionMode mode_;
  PropertyFilter filter_;
  bool is_for_in_ = false;
  bool skip_indices_ = false;
  bool is_receiver_simple_enum_ = false;
  bool has_empty_prototype_ = false;

  DISALLOW_COPY_AND_ASSIGN(FastKeyAccumulator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_KEYS_H_
