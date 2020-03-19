// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-objects.h"

#include "src/api/api-arguments-inl.h"
#include "src/codegen/compiler.h"
#include "src/date/date.h"
#include "src/execution/arguments.h"
#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/heap-inl.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/elements.h"
#include "src/objects/field-type.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/layout-descriptor.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-generator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-promise.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segment-iterator.h"
#include "src/objects/js-segmenter.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-weak-refs.h"
#include "src/objects/map-inl.h"
#include "src/objects/module.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/property.h"
#include "src/objects/prototype-info.h"
#include "src/objects/prototype.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/transitions.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-stream.h"
#include "src/utils/ostreams.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

// static
Maybe<bool> JSReceiver::HasProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::HasProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                    it->GetName());
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return Nothing<bool>();
        if (result.FromJust() != ABSENT) return Just(true);
        break;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it->HasAccess()) break;
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
        if (result.IsNothing()) return Nothing<bool>();
        return Just(result.FromJust() != ABSENT);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TypedArray out-of-bounds access.
        return Just(false);
      case LookupIterator::ACCESSOR:
      case LookupIterator::DATA:
        return Just(true);
    }
  }
  return Just(false);
}

// static
Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       Handle<Name> name) {
  if (object->IsJSModuleNamespace()) {
    PropertyDescriptor desc;
    return JSReceiver::GetOwnPropertyDescriptor(object->GetIsolate(), object,
                                                name, &desc);
  }

  if (object->IsJSObject()) {  // Shortcut.
    LookupIterator it = LookupIterator::PropertyOrElement(
        object->GetIsolate(), object, name, object, LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

Handle<Object> JSReceiver::GetDataProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        // Support calling this method without an active context, but refuse
        // access to access-checked objects in that case.
        if (!it->isolate()->context().is_null() && it->HasAccess()) continue;
        V8_FALLTHROUGH;
      case LookupIterator::JSPROXY:
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::ACCESSOR:
        // TODO(verwaest): For now this doesn't call into AccessorInfo, since
        // clients don't need it. Update once relevant.
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }
  return it->isolate()->factory()->undefined_value();
}

// static
Maybe<bool> JSReceiver::HasInPrototypeChain(Isolate* isolate,
                                            Handle<JSReceiver> object,
                                            Handle<Object> proto) {
  PrototypeIterator iter(isolate, object, kStartAtReceiver);
  while (true) {
    if (!iter.AdvanceFollowingProxies()) return Nothing<bool>();
    if (iter.IsAtEnd()) return Just(false);
    if (PrototypeIterator::GetCurrent(iter).is_identical_to(proto)) {
      return Just(true);
    }
  }
}

namespace {

bool HasExcludedProperty(
    const ScopedVector<Handle<Object>>* excluded_properties,
    Handle<Object> search_element) {
  // TODO(gsathya): Change this to be a hashtable.
  for (int i = 0; i < excluded_properties->length(); i++) {
    if (search_element->SameValue(*excluded_properties->at(i))) {
      return true;
    }
  }

  return false;
}

V8_WARN_UNUSED_RESULT Maybe<bool> FastAssign(
    Handle<JSReceiver> target, Handle<Object> source,
    const ScopedVector<Handle<Object>>* excluded_properties, bool use_set) {
  // Non-empty strings are the only non-JSReceivers that need to be handled
  // explicitly by Object.assign.
  if (!source->IsJSReceiver()) {
    return Just(!source->IsString() || String::cast(*source).length() == 0);
  }

  Isolate* isolate = target->GetIsolate();

  // If the target is deprecated, the object will be updated on first store. If
  // the source for that store equals the target, this will invalidate the
  // cached representation of the source. Preventively upgrade the target.
  // Do this on each iteration since any property load could cause deprecation.
  if (target->map().is_deprecated()) {
    JSObject::MigrateInstance(isolate, Handle<JSObject>::cast(target));
  }

  Handle<Map> map(JSReceiver::cast(*source).map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> from = Handle<JSObject>::cast(source);
  if (from->elements() != ReadOnlyRoots(isolate).empty_fixed_array()) {
    return Just(false);
  }

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);

  bool stable = true;

  for (InternalIndex i : map->IterateOwnDescriptors()) {
    HandleScope inner_scope(isolate);

    Handle<Name> next_key(descriptors->GetKey(i), isolate);
    Handle<Object> prop_value;
    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      DCHECK_EQ(from->map(), *map);
      DCHECK_EQ(*descriptors, map->instance_descriptors());

      PropertyDetails details = descriptors->GetDetails(i);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetStrongValue(i), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex index = FieldIndex::ForPropertyIndex(
              *map, details.field_index(), representation);
          prop_value = JSObject::FastPropertyAt(from, representation, index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value,
            JSReceiver::GetProperty(isolate, from, next_key), Nothing<bool>());
        stable = from->map() == *map;
        *descriptors.location() = map->instance_descriptors().ptr();
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(from, next_key, from,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (use_set) {
      LookupIterator it(target, next_key, target);
      Maybe<bool> result =
          Object::SetProperty(&it, prop_value, StoreOrigin::kNamed,
                              Just(ShouldThrow::kThrowOnError));
      if (result.IsNothing()) return result;
      if (stable) {
        stable = from->map() == *map;
        *descriptors.location() = map->instance_descriptors().ptr();
      }
    } else {
      if (excluded_properties != nullptr &&
          HasExcludedProperty(excluded_properties, next_key)) {
        continue;
      }

      // 4a ii 2. Perform ? CreateDataProperty(target, nextKey, propValue).
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, target, next_key, &success, LookupIterator::OWN);
      CHECK(success);
      CHECK(JSObject::CreateDataProperty(&it, prop_value, Just(kThrowOnError))
                .FromJust());
    }
  }

  return Just(true);
}
}  // namespace

// static
Maybe<bool> JSReceiver::SetOrCopyDataProperties(
    Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
    const ScopedVector<Handle<Object>>* excluded_properties, bool use_set) {
  Maybe<bool> fast_assign =
      FastAssign(target, source, excluded_properties, use_set);
  if (fast_assign.IsNothing()) return Nothing<bool>();
  if (fast_assign.FromJust()) return Just(true);

  Handle<JSReceiver> from = Object::ToObject(isolate, source).ToHandleChecked();
  // 3b. Let keys be ? from.[[OwnPropertyKeys]]().
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(from, KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                              GetKeysConversion::kKeepNumbers),
      Nothing<bool>());

  // 4. Repeat for each element nextKey of keys in List order,
  for (int j = 0; j < keys->length(); ++j) {
    Handle<Object> next_key(keys->get(j), isolate);
    // 4a i. Let desc be ? from.[[GetOwnProperty]](nextKey).
    PropertyDescriptor desc;
    Maybe<bool> found =
        JSReceiver::GetOwnPropertyDescriptor(isolate, from, next_key, &desc);
    if (found.IsNothing()) return Nothing<bool>();
    // 4a ii. If desc is not undefined and desc.[[Enumerable]] is true, then
    if (found.FromJust() && desc.enumerable()) {
      // 4a ii 1. Let propValue be ? Get(from, nextKey).
      Handle<Object> prop_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value,
          Runtime::GetObjectProperty(isolate, from, next_key), Nothing<bool>());

      if (use_set) {
        // 4c ii 2. Let status be ? Set(to, nextKey, propValue, true).
        Handle<Object> status;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, status,
            Runtime::SetObjectProperty(isolate, target, next_key, prop_value,
                                       StoreOrigin::kMaybeKeyed,
                                       Just(ShouldThrow::kThrowOnError)),
            Nothing<bool>());
      } else {
        if (excluded_properties != nullptr &&
            HasExcludedProperty(excluded_properties, next_key)) {
          continue;
        }

        // 4a ii 2. Perform ! CreateDataProperty(target, nextKey, propValue).
        bool success;
        LookupIterator it = LookupIterator::PropertyOrElement(
            isolate, target, next_key, &success, LookupIterator::OWN);
        CHECK(success);
        CHECK(JSObject::CreateDataProperty(&it, prop_value, Just(kThrowOnError))
                  .FromJust());
      }
    }
  }

  return Just(true);
}

String JSReceiver::class_name() {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  if (IsFunction()) return roots.Function_string();
  if (IsJSArgumentsObject()) return roots.Arguments_string();
  if (IsJSArray()) return roots.Array_string();
  if (IsJSArrayBuffer()) {
    if (JSArrayBuffer::cast(*this).is_shared()) {
      return roots.SharedArrayBuffer_string();
    }
    return roots.ArrayBuffer_string();
  }
  if (IsJSArrayIterator()) return roots.ArrayIterator_string();
  if (IsJSDate()) return roots.Date_string();
  if (IsJSError()) return roots.Error_string();
  if (IsJSGeneratorObject()) return roots.Generator_string();
  if (IsJSMap()) return roots.Map_string();
  if (IsJSMapIterator()) return roots.MapIterator_string();
  if (IsJSProxy()) {
    return map().is_callable() ? roots.Function_string()
                               : roots.Object_string();
  }
  if (IsJSRegExp()) return roots.RegExp_string();
  if (IsJSSet()) return roots.Set_string();
  if (IsJSSetIterator()) return roots.SetIterator_string();
  if (IsJSTypedArray()) {
#define SWITCH_KIND(Type, type, TYPE, ctype)      \
  if (map().elements_kind() == TYPE##_ELEMENTS) { \
    return roots.Type##Array_string();            \
  }
    TYPED_ARRAYS(SWITCH_KIND)
#undef SWITCH_KIND
  }
  if (IsJSPrimitiveWrapper()) {
    Object value = JSPrimitiveWrapper::cast(*this).value();
    if (value.IsBoolean()) return roots.Boolean_string();
    if (value.IsString()) return roots.String_string();
    if (value.IsNumber()) return roots.Number_string();
    if (value.IsBigInt()) return roots.BigInt_string();
    if (value.IsSymbol()) return roots.Symbol_string();
    if (value.IsScript()) return roots.Script_string();
    UNREACHABLE();
  }
  if (IsJSWeakMap()) return roots.WeakMap_string();
  if (IsJSWeakSet()) return roots.WeakSet_string();
  if (IsJSGlobalProxy()) return roots.global_string();

  Object maybe_constructor = map().GetConstructor();
  if (maybe_constructor.IsJSFunction()) {
    JSFunction constructor = JSFunction::cast(maybe_constructor);
    if (constructor.shared().IsApiFunction()) {
      maybe_constructor = constructor.shared().get_api_func_data();
    }
  }

  if (maybe_constructor.IsFunctionTemplateInfo()) {
    FunctionTemplateInfo info = FunctionTemplateInfo::cast(maybe_constructor);
    if (info.class_name().IsString()) return String::cast(info.class_name());
  }

  return roots.Object_string();
}

namespace {
std::pair<MaybeHandle<JSFunction>, Handle<String>> GetConstructorHelper(
    Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();

  // If the object was instantiated simply with base == new.target, the
  // constructor on the map provides the most accurate name.
  // Don't provide the info for prototypes, since their constructors are
  // reclaimed and replaced by Object in OptimizeAsPrototype.
  if (!receiver->IsJSProxy() && receiver->map().new_target_is_base() &&
      !receiver->map().is_prototype_map()) {
    Object maybe_constructor = receiver->map().GetConstructor();
    if (maybe_constructor.IsJSFunction()) {
      JSFunction constructor = JSFunction::cast(maybe_constructor);
      String name = constructor.shared().DebugName();
      if (name.length() != 0 &&
          !name.Equals(ReadOnlyRoots(isolate).Object_string())) {
        return std::make_pair(handle(constructor, isolate),
                              handle(name, isolate));
      }
    } else if (maybe_constructor.IsFunctionTemplateInfo()) {
      FunctionTemplateInfo info = FunctionTemplateInfo::cast(maybe_constructor);
      if (info.class_name().IsString()) {
        return std::make_pair(MaybeHandle<JSFunction>(),
                              handle(String::cast(info.class_name()), isolate));
      }
    }
  }

  Handle<Object> maybe_tag = JSReceiver::GetDataProperty(
      receiver, isolate->factory()->to_string_tag_symbol());
  if (maybe_tag->IsString())
    return std::make_pair(MaybeHandle<JSFunction>(),
                          Handle<String>::cast(maybe_tag));

  PrototypeIterator iter(isolate, receiver);
  if (iter.IsAtEnd()) {
    return std::make_pair(MaybeHandle<JSFunction>(),
                          handle(receiver->class_name(), isolate));
  }

  Handle<JSReceiver> start = PrototypeIterator::GetCurrent<JSReceiver>(iter);
  LookupIterator it(receiver, isolate->factory()->constructor_string(), start,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Handle<Object> maybe_constructor = JSReceiver::GetDataProperty(&it);
  if (maybe_constructor->IsJSFunction()) {
    JSFunction constructor = JSFunction::cast(*maybe_constructor);
    String name = constructor.shared().DebugName();

    if (name.length() != 0 &&
        !name.Equals(ReadOnlyRoots(isolate).Object_string())) {
      return std::make_pair(handle(constructor, isolate),
                            handle(name, isolate));
    }
  }

  return std::make_pair(MaybeHandle<JSFunction>(),
                        handle(receiver->class_name(), isolate));
}
}  // anonymous namespace

// static
MaybeHandle<JSFunction> JSReceiver::GetConstructor(
    Handle<JSReceiver> receiver) {
  return GetConstructorHelper(receiver).first;
}

// static
Handle<String> JSReceiver::GetConstructorName(Handle<JSReceiver> receiver) {
  return GetConstructorHelper(receiver).second;
}

Handle<NativeContext> JSReceiver::GetCreationContext() {
  JSReceiver receiver = *this;
  // Externals are JSObjects with null as a constructor.
  DCHECK(!receiver.IsExternal(GetIsolate()));
  Object constructor = receiver.map().GetConstructor();
  JSFunction function;
  if (constructor.IsJSFunction()) {
    function = JSFunction::cast(constructor);
  } else if (constructor.IsFunctionTemplateInfo()) {
    // Remote objects don't have a creation context.
    return Handle<NativeContext>::null();
  } else if (receiver.IsJSGeneratorObject()) {
    function = JSGeneratorObject::cast(receiver).function();
  } else {
    // Functions have null as a constructor,
    // but any JSFunction knows its context immediately.
    CHECK(receiver.IsJSFunction());
    function = JSFunction::cast(receiver);
  }

  return function.has_context()
             ? Handle<NativeContext>(function.context().native_context(),
                                     receiver.GetIsolate())
             : Handle<NativeContext>::null();
}

// static
MaybeHandle<NativeContext> JSReceiver::GetFunctionRealm(
    Handle<JSReceiver> receiver) {
  if (receiver->IsJSProxy()) {
    return JSProxy::GetFunctionRealm(Handle<JSProxy>::cast(receiver));
  }

  if (receiver->IsJSFunction()) {
    return JSFunction::GetFunctionRealm(Handle<JSFunction>::cast(receiver));
  }

  if (receiver->IsJSBoundFunction()) {
    return JSBoundFunction::GetFunctionRealm(
        Handle<JSBoundFunction>::cast(receiver));
  }

  return JSObject::GetFunctionRealm(Handle<JSObject>::cast(receiver));
}

// static
MaybeHandle<NativeContext> JSReceiver::GetContextForMicrotask(
    Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();
  while (receiver->IsJSBoundFunction() || receiver->IsJSProxy()) {
    if (receiver->IsJSBoundFunction()) {
      receiver = handle(
          Handle<JSBoundFunction>::cast(receiver)->bound_target_function(),
          isolate);
    } else {
      DCHECK(receiver->IsJSProxy());
      Handle<Object> target(Handle<JSProxy>::cast(receiver)->target(), isolate);
      if (!target->IsJSReceiver()) return MaybeHandle<NativeContext>();
      receiver = Handle<JSReceiver>::cast(target);
    }
  }

  if (!receiver->IsJSFunction()) return MaybeHandle<NativeContext>();
  return handle(Handle<JSFunction>::cast(receiver)->native_context(), isolate);
}

Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::GetPropertyAttributes(it);
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return result;
        if (result.FromJust() != ABSENT) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        return JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(ABSENT);
      case LookupIterator::ACCESSOR:
        if (it->GetHolder<Object>()->IsJSModuleNamespace()) {
          return JSModuleNamespace::GetPropertyAttributes(it);
        } else {
          return Just(it->property_attributes());
        }
      case LookupIterator::DATA:
        return Just(it->property_attributes());
    }
  }
  return Just(ABSENT);
}

namespace {

Object SetHashAndUpdateProperties(HeapObject properties, int hash) {
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);
  DCHECK(PropertyArray::HashField::is_valid(hash));

  ReadOnlyRoots roots = properties.GetReadOnlyRoots();
  if (properties == roots.empty_fixed_array() ||
      properties == roots.empty_property_array() ||
      properties == roots.empty_property_dictionary()) {
    return Smi::FromInt(hash);
  }

  if (properties.IsPropertyArray()) {
    PropertyArray::cast(properties).SetHash(hash);
    DCHECK_LT(0, PropertyArray::cast(properties).length());
    return properties;
  }

  if (properties.IsGlobalDictionary()) {
    GlobalDictionary::cast(properties).SetHash(hash);
    return properties;
  }

  DCHECK(properties.IsNameDictionary());
  NameDictionary::cast(properties).SetHash(hash);
  return properties;
}

int GetIdentityHashHelper(JSReceiver object) {
  DisallowHeapAllocation no_gc;
  Object properties = object.raw_properties_or_hash();
  if (properties.IsSmi()) {
    return Smi::ToInt(properties);
  }

  if (properties.IsPropertyArray()) {
    return PropertyArray::cast(properties).Hash();
  }

  if (properties.IsNameDictionary()) {
    return NameDictionary::cast(properties).Hash();
  }

  if (properties.IsGlobalDictionary()) {
    return GlobalDictionary::cast(properties).Hash();
  }

#ifdef DEBUG
  ReadOnlyRoots roots = object.GetReadOnlyRoots();
  DCHECK(properties == roots.empty_fixed_array() ||
         properties == roots.empty_property_dictionary());
#endif

  return PropertyArray::kNoHashSentinel;
}
}  // namespace

void JSReceiver::SetIdentityHash(int hash) {
  DisallowHeapAllocation no_gc;
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);
  DCHECK(PropertyArray::HashField::is_valid(hash));

  HeapObject existing_properties = HeapObject::cast(raw_properties_or_hash());
  Object new_properties = SetHashAndUpdateProperties(existing_properties, hash);
  set_raw_properties_or_hash(new_properties);
}

void JSReceiver::SetProperties(HeapObject properties) {
  DCHECK_IMPLIES(properties.IsPropertyArray() &&
                     PropertyArray::cast(properties).length() == 0,
                 properties == GetReadOnlyRoots().empty_property_array());
  DisallowHeapAllocation no_gc;
  int hash = GetIdentityHashHelper(*this);
  Object new_properties = properties;

  // TODO(cbruni): Make GetIdentityHashHelper return a bool so that we
  // don't have to manually compare against kNoHashSentinel.
  if (hash != PropertyArray::kNoHashSentinel) {
    new_properties = SetHashAndUpdateProperties(properties, hash);
  }

  set_raw_properties_or_hash(new_properties);
}

Object JSReceiver::GetIdentityHash() {
  DisallowHeapAllocation no_gc;

  int hash = GetIdentityHashHelper(*this);
  if (hash == PropertyArray::kNoHashSentinel) {
    return GetReadOnlyRoots().undefined_value();
  }

  return Smi::FromInt(hash);
}

// static
Smi JSReceiver::CreateIdentityHash(Isolate* isolate, JSReceiver key) {
  DisallowHeapAllocation no_gc;
  int hash = isolate->GenerateIdentityHash(PropertyArray::HashField::kMax);
  DCHECK_NE(PropertyArray::kNoHashSentinel, hash);

  key.SetIdentityHash(hash);
  return Smi::FromInt(hash);
}

Smi JSReceiver::GetOrCreateIdentityHash(Isolate* isolate) {
  DisallowHeapAllocation no_gc;

  int hash = GetIdentityHashHelper(*this);
  if (hash != PropertyArray::kNoHashSentinel) {
    return Smi::FromInt(hash);
  }

  return JSReceiver::CreateIdentityHash(isolate, *this);
}

void JSReceiver::DeleteNormalizedProperty(Handle<JSReceiver> object,
                                          int entry) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();

  if (object->IsJSGlobalObject()) {
    // If we have a global object, invalidate the cell and swap in a new one.
    Handle<GlobalDictionary> dictionary(
        JSGlobalObject::cast(*object).global_dictionary(), isolate);
    DCHECK_NE(GlobalDictionary::kNotFound, entry);

    auto cell = PropertyCell::InvalidateEntry(isolate, dictionary, entry);
    cell->set_value(ReadOnlyRoots(isolate).the_hole_value());
    cell->set_property_details(
        PropertyDetails::Empty(PropertyCellType::kUninitialized));
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);
    DCHECK_NE(NameDictionary::kNotFound, entry);

    dictionary = NameDictionary::DeleteEntry(isolate, dictionary, entry);
    object->SetProperties(*dictionary);
  }
  if (object->map().is_prototype_map()) {
    // Invalidate prototype validity cell as this may invalidate transitioning
    // store IC handlers.
    JSObject::InvalidatePrototypeChains(object->map());
  }
}

Maybe<bool> JSReceiver::DeleteProperty(LookupIterator* it,
                                       LanguageMode language_mode) {
  it->UpdateProtector();

  Isolate* isolate = it->isolate();

  if (it->state() == LookupIterator::JSPROXY) {
    return JSProxy::DeletePropertyOrElement(it->GetHolder<JSProxy>(),
                                            it->GetName(), language_mode);
  }

  if (it->GetReceiver()->IsJSProxy()) {
    if (it->state() != LookupIterator::NOT_FOUND) {
      DCHECK_EQ(LookupIterator::DATA, it->state());
      DCHECK(it->name()->IsPrivate());
      it->Delete();
    }
    return Just(true);
  }
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
        RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
        return Just(false);
      case LookupIterator::INTERCEPTOR: {
        ShouldThrow should_throw =
            is_sloppy(language_mode) ? kDontThrow : kThrowOnError;
        Maybe<bool> result =
            JSObject::DeletePropertyWithInterceptor(it, should_throw);
        // An exception was thrown in the interceptor. Propagate.
        if (isolate->has_pending_exception()) return Nothing<bool>();
        // Delete with interceptor succeeded. Return result.
        // TODO(neis): In strict mode, we should probably throw if the
        // interceptor returns false.
        if (result.IsJust()) return result;
        break;
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(true);
      case LookupIterator::DATA:
      case LookupIterator::ACCESSOR: {
        if (!it->IsConfigurable()) {
          // Fail if the property is not configurable.
          if (is_strict(language_mode)) {
            isolate->Throw(*isolate->factory()->NewTypeError(
                MessageTemplate::kStrictDeleteProperty, it->GetName(),
                receiver));
            return Nothing<bool>();
          }
          return Just(false);
        }

        it->Delete();

        return Just(true);
      }
    }
  }

  return Just(true);
}

Maybe<bool> JSReceiver::DeleteElement(Handle<JSReceiver> object, uint32_t index,
                                      LanguageMode language_mode) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

Maybe<bool> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
                                       Handle<Name> name,
                                       LanguageMode language_mode) {
  LookupIterator it(object, name, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

Maybe<bool> JSReceiver::DeletePropertyOrElement(Handle<JSReceiver> object,
                                                Handle<Name> name,
                                                LanguageMode language_mode) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}

// ES6 19.1.2.4
// static
Object JSReceiver::DefineProperty(Isolate* isolate, Handle<Object> object,
                                  Handle<Object> key,
                                  Handle<Object> attributes) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperty");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name));
  }
  // 2. Let key be ToPropertyKey(P).
  // 3. ReturnIfAbrupt(key).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key,
                                     Object::ToPropertyKey(isolate, key));
  // 4. Let desc be ToPropertyDescriptor(Attributes).
  // 5. ReturnIfAbrupt(desc).
  PropertyDescriptor desc;
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, attributes, &desc)) {
    return ReadOnlyRoots(isolate).exception();
  }
  // 6. Let success be DefinePropertyOrThrow(O,key, desc).
  Maybe<bool> success =
      DefineOwnProperty(isolate, Handle<JSReceiver>::cast(object), key, &desc,
                        Just(kThrowOnError));
  // 7. ReturnIfAbrupt(success).
  MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
  CHECK(success.FromJust());
  // 8. Return O.
  return *object;
}

// ES6 19.1.2.3.1
// static
MaybeHandle<Object> JSReceiver::DefineProperties(Isolate* isolate,
                                                 Handle<Object> object,
                                                 Handle<Object> properties) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperties");
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name),
                    Object);
  }
  // 2. Let props be ToObject(Properties).
  // 3. ReturnIfAbrupt(props).
  Handle<JSReceiver> props;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, props,
                             Object::ToObject(isolate, properties), Object);

  // 4. Let keys be props.[[OwnPropertyKeys]]().
  // 5. ReturnIfAbrupt(keys).
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, keys,
      KeyAccumulator::GetKeys(props, KeyCollectionMode::kOwnOnly,
                              ALL_PROPERTIES),
      Object);
  // 6. Let descriptors be an empty List.
  int capacity = keys->length();
  std::vector<PropertyDescriptor> descriptors(capacity);
  size_t descriptors_index = 0;
  // 7. Repeat for each element nextKey of keys in List order,
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> next_key(keys->get(i), isolate);
    // 7a. Let propDesc be props.[[GetOwnProperty]](nextKey).
    // 7b. ReturnIfAbrupt(propDesc).
    bool success = false;
    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate, props, next_key, &success, LookupIterator::OWN);
    DCHECK(success);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    if (maybe.IsNothing()) return MaybeHandle<Object>();
    PropertyAttributes attrs = maybe.FromJust();
    // 7c. If propDesc is not undefined and propDesc.[[Enumerable]] is true:
    if (attrs == ABSENT) continue;
    if (attrs & DONT_ENUM) continue;
    // 7c i. Let descObj be Get(props, nextKey).
    // 7c ii. ReturnIfAbrupt(descObj).
    Handle<Object> desc_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, desc_obj, Object::GetProperty(&it),
                               Object);
    // 7c iii. Let desc be ToPropertyDescriptor(descObj).
    success = PropertyDescriptor::ToPropertyDescriptor(
        isolate, desc_obj, &descriptors[descriptors_index]);
    // 7c iv. ReturnIfAbrupt(desc).
    if (!success) return MaybeHandle<Object>();
    // 7c v. Append the pair (a two element List) consisting of nextKey and
    //       desc to the end of descriptors.
    descriptors[descriptors_index].set_name(next_key);
    descriptors_index++;
  }
  // 8. For each pair from descriptors in list order,
  for (size_t i = 0; i < descriptors_index; ++i) {
    PropertyDescriptor* desc = &descriptors[i];
    // 8a. Let P be the first element of pair.
    // 8b. Let desc be the second element of pair.
    // 8c. Let status be DefinePropertyOrThrow(O, P, desc).
    Maybe<bool> status =
        DefineOwnProperty(isolate, Handle<JSReceiver>::cast(object),
                          desc->name(), desc, Just(kThrowOnError));
    // 8d. ReturnIfAbrupt(status).
    if (status.IsNothing()) return MaybeHandle<Object>();
    CHECK(status.FromJust());
  }
  // 9. Return o.
  return object;
}

// static
Maybe<bool> JSReceiver::DefineOwnProperty(Isolate* isolate,
                                          Handle<JSReceiver> object,
                                          Handle<Object> key,
                                          PropertyDescriptor* desc,
                                          Maybe<ShouldThrow> should_throw) {
  if (object->IsJSArray()) {
    return JSArray::DefineOwnProperty(isolate, Handle<JSArray>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSProxy()) {
    return JSProxy::DefineOwnProperty(isolate, Handle<JSProxy>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSTypedArray()) {
    return JSTypedArray::DefineOwnProperty(
        isolate, Handle<JSTypedArray>::cast(object), key, desc, should_throw);
  }

  // OrdinaryDefineOwnProperty, by virtue of calling
  // DefineOwnPropertyIgnoreAttributes, can handle arguments
  // (ES#sec-arguments-exotic-objects-defineownproperty-p-desc).
  return OrdinaryDefineOwnProperty(isolate, Handle<JSObject>::cast(object), key,
                                   desc, should_throw);
}

// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(
    Isolate* isolate, Handle<JSObject> object, Handle<Object> key,
    PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw) {
  bool success = false;
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey...
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success, LookupIterator::OWN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.

  // Deal with access checks first.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(it.GetHolder<JSObject>());
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      return Just(true);
    }
    it.Next();
  }

  return OrdinaryDefineOwnProperty(&it, desc, should_throw);
}

namespace {

MaybeHandle<Object> GetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor, bool* done) {
  *done = false;
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->getter().IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver), Object);
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(kDontThrow));

  if (it->IsElement()) {
    result = args.CallIndexedGetter(interceptor, it->index());
  } else {
    result = args.CallNamedGetter(interceptor, it->name());
  }

  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (result.is_null()) return isolate->factory()->undefined_value();
  *done = true;
  // Rebox handle before return
  return handle(*result, isolate);
}

Maybe<PropertyAttributes> GetPropertyAttributesWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);
  HandleScope scope(isolate);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  DCHECK_IMPLIES(!it->IsElement() && it->name()->IsSymbol(),
                 interceptor->can_intercept_symbols());
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<PropertyAttributes>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(kDontThrow));
  if (!interceptor->query().IsUndefined(isolate)) {
    Handle<Object> result;
    if (it->IsElement()) {
      result = args.CallIndexedQuery(interceptor, it->index());
    } else {
      result = args.CallNamedQuery(interceptor, it->name());
    }
    if (!result.is_null()) {
      int32_t value;
      CHECK(result->ToInt32(&value));
      return Just(static_cast<PropertyAttributes>(value));
    }
  } else if (!interceptor->getter().IsUndefined(isolate)) {
    // TODO(verwaest): Use GetPropertyWithInterceptor?
    Handle<Object> result;
    if (it->IsElement()) {
      result = args.CallIndexedGetter(interceptor, it->index());
    } else {
      result = args.CallNamedGetter(interceptor, it->name());
    }
    if (!result.is_null()) return Just(DONT_ENUM);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}

Maybe<bool> SetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    Maybe<ShouldThrow> should_throw, Handle<Object> value) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->setter().IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  if (it->IsElement()) {
    // TODO(neis): In the future, we may want to actually return the
    // interceptor's result, which then should be a boolean.
    result = !args.CallIndexedSetter(interceptor, it->index(), value).is_null();
  } else {
    result = !args.CallNamedSetter(interceptor, it->name(), value).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

Maybe<bool> DefinePropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    Maybe<ShouldThrow> should_throw, PropertyDescriptor* desc) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->definer().IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  std::unique_ptr<v8::PropertyDescriptor> descriptor(
      new v8::PropertyDescriptor());
  if (PropertyDescriptor::IsAccessorDescriptor(desc)) {
    descriptor.reset(new v8::PropertyDescriptor(
        v8::Utils::ToLocal(desc->get()), v8::Utils::ToLocal(desc->set())));
  } else if (PropertyDescriptor::IsDataDescriptor(desc)) {
    if (desc->has_writable()) {
      descriptor.reset(new v8::PropertyDescriptor(
          v8::Utils::ToLocal(desc->value()), desc->writable()));
    } else {
      descriptor.reset(
          new v8::PropertyDescriptor(v8::Utils::ToLocal(desc->value())));
    }
  }
  if (desc->has_enumerable()) {
    descriptor->set_enumerable(desc->enumerable());
  }
  if (desc->has_configurable()) {
    descriptor->set_configurable(desc->configurable());
  }

  if (it->IsElement()) {
    result = !args.CallIndexedDefiner(interceptor, it->index(), *descriptor)
                  .is_null();
  } else {
    result =
        !args.CallNamedDefiner(interceptor, it->name(), *descriptor).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

}  // namespace

// ES6 9.1.6.1
// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(
    LookupIterator* it, PropertyDescriptor* desc,
    Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = it->isolate();
  // 1. Let current be O.[[GetOwnProperty]](P).
  // 2. ReturnIfAbrupt(current).
  PropertyDescriptor current;
  MAYBE_RETURN(GetOwnPropertyDescriptor(it, &current), Nothing<bool>());

  it->Restart();
  // Handle interceptor
  for (; it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->HolderIsReceiverOrHiddenPrototype()) {
        Maybe<bool> result = DefinePropertyWithInterceptorInternal(
            it, it->GetInterceptor(), should_throw, desc);
        if (result.IsNothing() || result.FromJust()) {
          return result;
        }
      }
    }
  }

  // TODO(jkummerow/verwaest): It would be nice if we didn't have to reset
  // the iterator every time. Currently, the reasons why we need it are:
  // - handle interceptors correctly
  // - handle accessors correctly (which might change the holder's map)
  it->Restart();
  // 3. Let extensible be the value of the [[Extensible]] internal slot of O.
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  bool extensible = JSObject::IsExtensible(object);

  return ValidateAndApplyPropertyDescriptor(
      isolate, it, extensible, desc, &current, should_throw, Handle<Name>());
}

// ES6 9.1.6.2
// static
Maybe<bool> JSReceiver::IsCompatiblePropertyDescriptor(
    Isolate* isolate, bool extensible, PropertyDescriptor* desc,
    PropertyDescriptor* current, Handle<Name> property_name,
    Maybe<ShouldThrow> should_throw) {
  // 1. Return ValidateAndApplyPropertyDescriptor(undefined, undefined,
  //    Extensible, Desc, Current).
  return ValidateAndApplyPropertyDescriptor(
      isolate, nullptr, extensible, desc, current, should_throw, property_name);
}

// ES6 9.1.6.3
// static
Maybe<bool> JSReceiver::ValidateAndApplyPropertyDescriptor(
    Isolate* isolate, LookupIterator* it, bool extensible,
    PropertyDescriptor* desc, PropertyDescriptor* current,
    Maybe<ShouldThrow> should_throw, Handle<Name> property_name) {
  // We either need a LookupIterator, or a property name.
  DCHECK((it == nullptr) != property_name.is_null());
  Handle<JSObject> object;
  if (it != nullptr) object = Handle<JSObject>::cast(it->GetReceiver());
  bool desc_is_data_descriptor = PropertyDescriptor::IsDataDescriptor(desc);
  bool desc_is_accessor_descriptor =
      PropertyDescriptor::IsAccessorDescriptor(desc);
  bool desc_is_generic_descriptor =
      PropertyDescriptor::IsGenericDescriptor(desc);
  // 1. (Assert)
  // 2. If current is undefined, then
  if (current->is_empty()) {
    // 2a. If extensible is false, return false.
    if (!extensible) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kDefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 2c. If IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then:
    // (This is equivalent to !IsAccessorDescriptor(desc).)
    DCHECK((desc_is_generic_descriptor || desc_is_data_descriptor) ==
           !desc_is_accessor_descriptor);
    if (!desc_is_accessor_descriptor) {
      // 2c i. If O is not undefined, create an own data property named P of
      // object O whose [[Value]], [[Writable]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != nullptr) {
        if (!desc->has_writable()) desc->set_writable(false);
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> value(
            desc->has_value()
                ? desc->value()
                : Handle<Object>::cast(isolate->factory()->undefined_value()));
        MaybeHandle<Object> result =
            JSObject::DefineOwnPropertyIgnoreAttributes(it, value,
                                                        desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    } else {
      // 2d. Else Desc must be an accessor Property Descriptor,
      DCHECK(desc_is_accessor_descriptor);
      // 2d i. If O is not undefined, create an own accessor property named P
      // of object O whose [[Get]], [[Set]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != nullptr) {
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> getter(
            desc->has_get()
                ? desc->get()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        Handle<Object> setter(
            desc->has_set()
                ? desc->set()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        MaybeHandle<Object> result =
            JSObject::DefineAccessor(it, getter, setter, desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    }
    // 2e. Return true.
    return Just(true);
  }
  // 3. Return true, if every field in Desc is absent.
  // 4. Return true, if every field in Desc also occurs in current and the
  // value of every field in Desc is the same value as the corresponding field
  // in current when compared using the SameValue algorithm.
  if ((!desc->has_enumerable() ||
       desc->enumerable() == current->enumerable()) &&
      (!desc->has_configurable() ||
       desc->configurable() == current->configurable()) &&
      (!desc->has_value() ||
       (current->has_value() && current->value()->SameValue(*desc->value()))) &&
      (!desc->has_writable() ||
       (current->has_writable() && current->writable() == desc->writable())) &&
      (!desc->has_get() ||
       (current->has_get() && current->get()->SameValue(*desc->get()))) &&
      (!desc->has_set() ||
       (current->has_set() && current->set()->SameValue(*desc->set())))) {
    return Just(true);
  }
  // 5. If the [[Configurable]] field of current is false, then
  if (!current->configurable()) {
    // 5a. Return false, if the [[Configurable]] field of Desc is true.
    if (desc->has_configurable() && desc->configurable()) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 5b. Return false, if the [[Enumerable]] field of Desc is present and the
    // [[Enumerable]] fields of current and Desc are the Boolean negation of
    // each other.
    if (desc->has_enumerable() && desc->enumerable() != current->enumerable()) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
  }

  bool current_is_data_descriptor =
      PropertyDescriptor::IsDataDescriptor(current);
  // 6. If IsGenericDescriptor(Desc) is true, no further validation is required.
  if (desc_is_generic_descriptor) {
    // Nothing to see here.

    // 7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) have
    // different results, then:
  } else if (current_is_data_descriptor != desc_is_data_descriptor) {
    // 7a. Return false, if the [[Configurable]] field of current is false.
    if (!current->configurable()) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed,
                       it != nullptr ? it->GetName() : property_name));
    }
    // 7b. If IsDataDescriptor(current) is true, then:
    if (current_is_data_descriptor) {
      // 7b i. If O is not undefined, convert the property named P of object O
      // from a data property to an accessor property. Preserve the existing
      // values of the converted property's [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property's attributes to their
      // default values.
      // --> Folded into step 10.
    } else {
      // 7c i. If O is not undefined, convert the property named P of object O
      // from an accessor property to a data property. Preserve the existing
      // values of the converted property’s [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property’s attributes to their
      // default values.
      // --> Folded into step 10.
    }

    // 8. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both
    // true, then:
  } else if (current_is_data_descriptor && desc_is_data_descriptor) {
    // 8a. If the [[Configurable]] field of current is false, then:
    if (!current->configurable()) {
      // 8a i. Return false, if the [[Writable]] field of current is false and
      // the [[Writable]] field of Desc is true.
      if (!current->writable() && desc->has_writable() && desc->writable()) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
      // 8a ii. If the [[Writable]] field of current is false, then:
      if (!current->writable()) {
        // 8a ii 1. Return false, if the [[Value]] field of Desc is present and
        // SameValue(Desc.[[Value]], current.[[Value]]) is false.
        if (desc->has_value() && !desc->value()->SameValue(*current->value())) {
          RETURN_FAILURE(
              isolate, GetShouldThrow(isolate, should_throw),
              NewTypeError(MessageTemplate::kRedefineDisallowed,
                           it != nullptr ? it->GetName() : property_name));
        }
      }
    }
  } else {
    // 9. Else IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc)
    // are both true,
    DCHECK(PropertyDescriptor::IsAccessorDescriptor(current) &&
           desc_is_accessor_descriptor);
    // 9a. If the [[Configurable]] field of current is false, then:
    if (!current->configurable()) {
      // 9a i. Return false, if the [[Set]] field of Desc is present and
      // SameValue(Desc.[[Set]], current.[[Set]]) is false.
      if (desc->has_set() && !desc->set()->SameValue(*current->set())) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
      // 9a ii. Return false, if the [[Get]] field of Desc is present and
      // SameValue(Desc.[[Get]], current.[[Get]]) is false.
      if (desc->has_get() && !desc->get()->SameValue(*current->get())) {
        RETURN_FAILURE(
            isolate, GetShouldThrow(isolate, should_throw),
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != nullptr ? it->GetName() : property_name));
      }
    }
  }

  // 10. If O is not undefined, then:
  if (it != nullptr) {
    // 10a. For each field of Desc that is present, set the corresponding
    // attribute of the property named P of object O to the value of the field.
    PropertyAttributes attrs = NONE;

    if (desc->has_enumerable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->enumerable() ? NONE : DONT_ENUM));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->enumerable() ? NONE : DONT_ENUM));
    }
    if (desc->has_configurable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->configurable() ? NONE : DONT_DELETE));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->configurable() ? NONE : DONT_DELETE));
    }
    if (desc_is_data_descriptor ||
        (desc_is_generic_descriptor && current_is_data_descriptor)) {
      if (desc->has_writable()) {
        attrs = static_cast<PropertyAttributes>(
            attrs | (desc->writable() ? NONE : READ_ONLY));
      } else {
        attrs = static_cast<PropertyAttributes>(
            attrs | (current->writable() ? NONE : READ_ONLY));
      }
      Handle<Object> value(
          desc->has_value() ? desc->value()
                            : current->has_value()
                                  ? current->value()
                                  : Handle<Object>::cast(
                                        isolate->factory()->undefined_value()));
      return JSObject::DefineOwnPropertyIgnoreAttributes(it, value, attrs,
                                                         should_throw);
    } else {
      DCHECK(desc_is_accessor_descriptor ||
             (desc_is_generic_descriptor &&
              PropertyDescriptor::IsAccessorDescriptor(current)));
      Handle<Object> getter(
          desc->has_get()
              ? desc->get()
              : current->has_get()
                    ? current->get()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      Handle<Object> setter(
          desc->has_set()
              ? desc->set()
              : current->has_set()
                    ? current->set()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      MaybeHandle<Object> result =
          JSObject::DefineAccessor(it, getter, setter, attrs);
      if (result.is_null()) return Nothing<bool>();
    }
  }

  // 11. Return true.
  return Just(true);
}

// static
Maybe<bool> JSReceiver::CreateDataProperty(Isolate* isolate,
                                           Handle<JSReceiver> object,
                                           Handle<Name> key,
                                           Handle<Object> value,
                                           Maybe<ShouldThrow> should_throw) {
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, object, key,
                                                        LookupIterator::OWN);
  return CreateDataProperty(&it, value, should_throw);
}

// static
Maybe<bool> JSReceiver::CreateDataProperty(LookupIterator* it,
                                           Handle<Object> value,
                                           Maybe<ShouldThrow> should_throw) {
  DCHECK(!it->check_prototype_chain());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (receiver->IsJSObject()) {
    return JSObject::CreateDataProperty(it, value, should_throw);  // Shortcut.
  }

  PropertyDescriptor new_desc;
  new_desc.set_value(value);
  new_desc.set_writable(true);
  new_desc.set_enumerable(true);
  new_desc.set_configurable(true);

  return JSReceiver::DefineOwnProperty(isolate, receiver, it->GetName(),
                                       &new_desc, should_throw);
}

// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(Isolate* isolate,
                                                 Handle<JSReceiver> object,
                                                 Handle<Object> key,
                                                 PropertyDescriptor* desc) {
  bool success = false;
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey...
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success, LookupIterator::OWN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.
  return GetOwnPropertyDescriptor(&it, desc);
}

namespace {

Maybe<bool> GetPropertyDescriptorWithInterceptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  Handle<InterceptorInfo> interceptor;

  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (it->HasAccess()) {
      it->Next();
    } else {
      interceptor = it->GetInterceptorForFailedAccessCheck();
      if (interceptor.is_null() &&
          (!JSObject::AllCanRead(it) ||
           it->state() != LookupIterator::INTERCEPTOR)) {
        it->Restart();
        return Just(false);
      }
    }
  }

  if (it->state() == LookupIterator::INTERCEPTOR) {
    interceptor = it->GetInterceptor();
  }
  if (interceptor.is_null()) return Just(false);
  Isolate* isolate = it->isolate();
  if (interceptor->descriptor().IsUndefined(isolate)) return Just(false);

  Handle<Object> result;
  Handle<JSObject> holder = it->GetHolder<JSObject>();

  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(kDontThrow));
  if (it->IsElement()) {
    result = args.CallIndexedDescriptor(interceptor, it->index());
  } else {
    result = args.CallNamedDescriptor(interceptor, it->name());
  }
  if (!result.is_null()) {
    // Request successfully intercepted, try to set the property
    // descriptor.
    Utils::ApiCheck(
        PropertyDescriptor::ToPropertyDescriptor(isolate, result, desc),
        it->IsElement() ? "v8::IndexedPropertyDescriptorCallback"
                        : "v8::NamedPropertyDescriptorCallback",
        "Invalid property descriptor.");

    return Just(true);
  }

  it->Next();
  return Just(false);
}
}  // namespace

// ES6 9.1.5.1
// Returns true on success, false if the property didn't exist, nothing if
// an exception was thrown.
// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  Isolate* isolate = it->isolate();
  // "Virtual" dispatch.
  if (it->IsFound() && it->GetHolder<JSReceiver>()->IsJSProxy()) {
    return JSProxy::GetOwnPropertyDescriptor(isolate, it->GetHolder<JSProxy>(),
                                             it->GetName(), desc);
  }

  Maybe<bool> intercepted = GetPropertyDescriptorWithInterceptor(it, desc);
  MAYBE_RETURN(intercepted, Nothing<bool>());
  if (intercepted.FromJust()) {
    return Just(true);
  }

  // Request was not intercepted, continue as normal.
  // 1. (Assert)
  // 2. If O does not have an own property with key P, return undefined.
  Maybe<PropertyAttributes> maybe = JSObject::GetPropertyAttributes(it);
  MAYBE_RETURN(maybe, Nothing<bool>());
  PropertyAttributes attrs = maybe.FromJust();
  if (attrs == ABSENT) return Just(false);
  DCHECK(!isolate->has_pending_exception());

  // 3. Let D be a newly created Property Descriptor with no fields.
  DCHECK(desc->is_empty());
  // 4. Let X be O's own property whose key is P.
  // 5. If X is a data property, then
  bool is_accessor_pair = it->state() == LookupIterator::ACCESSOR &&
                          it->GetAccessors()->IsAccessorPair();
  if (!is_accessor_pair) {
    // 5a. Set D.[[Value]] to the value of X's [[Value]] attribute.
    Handle<Object> value;
    if (!Object::GetProperty(it).ToHandle(&value)) {
      DCHECK(isolate->has_pending_exception());
      return Nothing<bool>();
    }
    desc->set_value(value);
    // 5b. Set D.[[Writable]] to the value of X's [[Writable]] attribute
    desc->set_writable((attrs & READ_ONLY) == 0);
  } else {
    // 6. Else X is an accessor property, so
    Handle<AccessorPair> accessors =
        Handle<AccessorPair>::cast(it->GetAccessors());
    Handle<NativeContext> native_context =
        it->GetHolder<JSReceiver>()->GetCreationContext();
    // 6a. Set D.[[Get]] to the value of X's [[Get]] attribute.
    desc->set_get(AccessorPair::GetComponent(isolate, native_context, accessors,
                                             ACCESSOR_GETTER));
    // 6b. Set D.[[Set]] to the value of X's [[Set]] attribute.
    desc->set_set(AccessorPair::GetComponent(isolate, native_context, accessors,
                                             ACCESSOR_SETTER));
  }

  // 7. Set D.[[Enumerable]] to the value of X's [[Enumerable]] attribute.
  desc->set_enumerable((attrs & DONT_ENUM) == 0);
  // 8. Set D.[[Configurable]] to the value of X's [[Configurable]] attribute.
  desc->set_configurable((attrs & DONT_DELETE) == 0);
  // 9. Return D.
  DCHECK(PropertyDescriptor::IsAccessorDescriptor(desc) !=
         PropertyDescriptor::IsDataDescriptor(desc));
  return Just(true);
}
Maybe<bool> JSReceiver::SetIntegrityLevel(Handle<JSReceiver> receiver,
                                          IntegrityLevel level,
                                          ShouldThrow should_throw) {
  DCHECK(level == SEALED || level == FROZEN);

  if (receiver->IsJSObject()) {
    Handle<JSObject> object = Handle<JSObject>::cast(receiver);

    if (!object->HasSloppyArgumentsElements() &&
        !object->IsJSModuleNamespace()) {  // Fast path.
      // Prevent memory leaks by not adding unnecessary transitions.
      Maybe<bool> test = JSObject::TestIntegrityLevel(object, level);
      MAYBE_RETURN(test, Nothing<bool>());
      if (test.FromJust()) return test;

      if (level == SEALED) {
        return JSObject::PreventExtensionsWithTransition<SEALED>(object,
                                                                 should_throw);
      } else {
        return JSObject::PreventExtensionsWithTransition<FROZEN>(object,
                                                                 should_throw);
      }
    }
  }

  Isolate* isolate = receiver->GetIsolate();

  MAYBE_RETURN(JSReceiver::PreventExtensions(receiver, should_throw),
               Nothing<bool>());

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  PropertyDescriptor no_conf;
  no_conf.set_configurable(false);

  PropertyDescriptor no_conf_no_write;
  no_conf_no_write.set_configurable(false);
  no_conf_no_write.set_writable(false);

  if (level == SEALED) {
    for (int i = 0; i < keys->length(); ++i) {
      Handle<Object> key(keys->get(i), isolate);
      MAYBE_RETURN(DefineOwnProperty(isolate, receiver, key, &no_conf,
                                     Just(kThrowOnError)),
                   Nothing<bool>());
    }
    return Just(true);
  }

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      PropertyDescriptor desc =
          PropertyDescriptor::IsAccessorDescriptor(&current_desc)
              ? no_conf
              : no_conf_no_write;
      MAYBE_RETURN(
          DefineOwnProperty(isolate, receiver, key, &desc, Just(kThrowOnError)),
          Nothing<bool>());
    }
  }
  return Just(true);
}

namespace {
Maybe<bool> GenericTestIntegrityLevel(Handle<JSReceiver> receiver,
                                      PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);

  Maybe<bool> extensible = JSReceiver::IsExtensible(receiver);
  MAYBE_RETURN(extensible, Nothing<bool>());
  if (extensible.FromJust()) return Just(false);

  Isolate* isolate = receiver->GetIsolate();

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      if (current_desc.configurable()) return Just(false);
      if (level == FROZEN &&
          PropertyDescriptor::IsDataDescriptor(&current_desc) &&
          current_desc.writable()) {
        return Just(false);
      }
    }
  }
  return Just(true);
}

}  // namespace

Maybe<bool> JSReceiver::TestIntegrityLevel(Handle<JSReceiver> receiver,
                                           IntegrityLevel level) {
  if (!receiver->map().IsCustomElementsReceiverMap()) {
    return JSObject::TestIntegrityLevel(Handle<JSObject>::cast(receiver),
                                        level);
  }
  return GenericTestIntegrityLevel(receiver, level);
}

Maybe<bool> JSReceiver::PreventExtensions(Handle<JSReceiver> object,
                                          ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::PreventExtensions(Handle<JSProxy>::cast(object),
                                      should_throw);
  }
  DCHECK(object->IsJSObject());
  return JSObject::PreventExtensions(Handle<JSObject>::cast(object),
                                     should_throw);
}

Maybe<bool> JSReceiver::IsExtensible(Handle<JSReceiver> object) {
  if (object->IsJSProxy()) {
    return JSProxy::IsExtensible(Handle<JSProxy>::cast(object));
  }
  return Just(JSObject::IsExtensible(Handle<JSObject>::cast(object)));
}

// static
MaybeHandle<Object> JSReceiver::ToPrimitive(Handle<JSReceiver> receiver,
                                            ToPrimitiveHint hint) {
  Isolate* const isolate = receiver->GetIsolate();
  Handle<Object> exotic_to_prim;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, exotic_to_prim,
      Object::GetMethod(receiver, isolate->factory()->to_primitive_symbol()),
      Object);
  if (!exotic_to_prim->IsUndefined(isolate)) {
    Handle<Object> hint_string =
        isolate->factory()->ToPrimitiveHintString(hint);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, exotic_to_prim, receiver, 1, &hint_string),
        Object);
    if (result->IsPrimitive()) return result;
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                    Object);
  }
  return OrdinaryToPrimitive(receiver, (hint == ToPrimitiveHint::kString)
                                           ? OrdinaryToPrimitiveHint::kString
                                           : OrdinaryToPrimitiveHint::kNumber);
}

// static
MaybeHandle<Object> JSReceiver::OrdinaryToPrimitive(
    Handle<JSReceiver> receiver, OrdinaryToPrimitiveHint hint) {
  Isolate* const isolate = receiver->GetIsolate();
  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = isolate->factory()->valueOf_string();
      method_names[1] = isolate->factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = isolate->factory()->toString_string();
      method_names[1] = isolate->factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    Handle<Object> method;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, method,
                               JSReceiver::GetProperty(isolate, receiver, name),
                               Object);
    if (method->IsCallable()) {
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          Execution::Call(isolate, method, receiver, 0, nullptr), Object);
      if (result->IsPrimitive()) return result;
    }
  }
  THROW_NEW_ERROR(isolate,
                  NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                  Object);
}

V8_WARN_UNUSED_RESULT Maybe<bool> FastGetOwnValuesOrEntries(
    Isolate* isolate, Handle<JSReceiver> receiver, bool get_entries,
    Handle<FixedArray>* result) {
  Handle<Map> map(JSReceiver::cast(*receiver).map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> object(JSObject::cast(*receiver), isolate);
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  int number_of_own_elements =
      object->GetElementsAccessor()->GetCapacity(*object, object->elements());

  if (number_of_own_elements >
      FixedArray::kMaxLength - number_of_own_descriptors) {
    isolate->Throw(*isolate->factory()->NewRangeError(
        MessageTemplate::kInvalidArrayLength));
    return Nothing<bool>();
  }
  Handle<FixedArray> values_or_entries = isolate->factory()->NewFixedArray(
      number_of_own_descriptors + number_of_own_elements);
  int count = 0;

  if (object->elements() != ReadOnlyRoots(isolate).empty_fixed_array()) {
    MAYBE_RETURN(object->GetElementsAccessor()->CollectValuesOrEntries(
                     isolate, object, values_or_entries, get_entries, &count,
                     ENUMERABLE_STRINGS),
                 Nothing<bool>());
  }

  // We may have already lost stability, if CollectValuesOrEntries had
  // side-effects.
  bool stable = *map == object->map();
  if (stable) {
    *descriptors.location() = map->instance_descriptors().ptr();
  }

  for (InternalIndex index : InternalIndex::Range(number_of_own_descriptors)) {
    HandleScope inner_scope(isolate);

    Handle<Name> next_key(descriptors->GetKey(index), isolate);
    if (!next_key->IsString()) continue;
    Handle<Object> prop_value;

    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      DCHECK_EQ(object->map(), *map);
      DCHECK_EQ(*descriptors, map->instance_descriptors());

      PropertyDetails details = descriptors->GetDetails(index);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetStrongValue(index), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex field_index = FieldIndex::ForPropertyIndex(
              *map, details.field_index(), representation);
          prop_value =
              JSObject::FastPropertyAt(object, representation, field_index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value,
            JSReceiver::GetProperty(isolate, object, next_key),
            Nothing<bool>());
        stable = object->map() == *map;
        *descriptors.location() = map->instance_descriptors().ptr();
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(isolate, object, next_key,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (get_entries) {
      prop_value = MakeEntryPair(isolate, next_key, prop_value);
    }

    values_or_entries->set(count, *prop_value);
    count++;
  }

  DCHECK_LE(count, values_or_entries->length());
  *result = FixedArray::ShrinkOrEmpty(isolate, values_or_entries, count);
  return Just(true);
}

MaybeHandle<FixedArray> GetOwnValuesOrEntries(Isolate* isolate,
                                              Handle<JSReceiver> object,
                                              PropertyFilter filter,
                                              bool try_fast_path,
                                              bool get_entries) {
  Handle<FixedArray> values_or_entries;
  if (try_fast_path && filter == ENUMERABLE_STRINGS) {
    Maybe<bool> fast_values_or_entries = FastGetOwnValuesOrEntries(
        isolate, object, get_entries, &values_or_entries);
    if (fast_values_or_entries.IsNothing()) return MaybeHandle<FixedArray>();
    if (fast_values_or_entries.FromJust()) return values_or_entries;
  }

  PropertyFilter key_filter =
      static_cast<PropertyFilter>(filter & ~ONLY_ENUMERABLE);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly, key_filter,
                              GetKeysConversion::kConvertToString),
      MaybeHandle<FixedArray>());

  values_or_entries = isolate->factory()->NewFixedArray(keys->length());
  int length = 0;

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key =
        Handle<Name>::cast(handle(keys->get(isolate, i), isolate));

    if (filter & ONLY_ENUMERABLE) {
      PropertyDescriptor descriptor;
      Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
          isolate, object, key, &descriptor);
      MAYBE_RETURN(did_get_descriptor, MaybeHandle<FixedArray>());
      if (!did_get_descriptor.FromJust() || !descriptor.enumerable()) continue;
    }

    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, Object::GetPropertyOrElement(isolate, object, key),
        MaybeHandle<FixedArray>());

    if (get_entries) {
      Handle<FixedArray> entry_storage =
          isolate->factory()->NewUninitializedFixedArray(2);
      entry_storage->set(0, *key);
      entry_storage->set(1, *value);
      value = isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                         PACKED_ELEMENTS, 2);
    }

    values_or_entries->set(length, *value);
    length++;
  }
  DCHECK_LE(length, values_or_entries->length());
  return FixedArray::ShrinkOrEmpty(isolate, values_or_entries, length);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnValues(Handle<JSReceiver> object,
                                                 PropertyFilter filter,
                                                 bool try_fast_path) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter,
                               try_fast_path, false);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnEntries(Handle<JSReceiver> object,
                                                  PropertyFilter filter,
                                                  bool try_fast_path) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter,
                               try_fast_path, true);
}

Maybe<bool> JSReceiver::SetPrototype(Handle<JSReceiver> object,
                                     Handle<Object> value, bool from_javascript,
                                     ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::SetPrototype(Handle<JSProxy>::cast(object), value,
                                 from_javascript, should_throw);
  }
  return JSObject::SetPrototype(Handle<JSObject>::cast(object), value,
                                from_javascript, should_throw);
}

bool JSReceiver::HasProxyInPrototype(Isolate* isolate) {
  for (PrototypeIterator iter(isolate, *this, kStartAtReceiver,
                              PrototypeIterator::END_AT_NULL);
       !iter.IsAtEnd(); iter.AdvanceIgnoringProxies()) {
    if (iter.GetCurrent().IsJSProxy()) return true;
  }
  return false;
}

// static
MaybeHandle<JSObject> JSObject::New(Handle<JSFunction> constructor,
                                    Handle<JSReceiver> new_target,
                                    Handle<AllocationSite> site) {
  // If called through new, new.target can be:
  // - a subclass of constructor,
  // - a proxy wrapper around constructor, or
  // - the constructor itself.
  // If called through Reflect.construct, it's guaranteed to be a constructor.
  Isolate* const isolate = constructor->GetIsolate();
  DCHECK(constructor->IsConstructor());
  DCHECK(new_target->IsConstructor());
  DCHECK(!constructor->has_initial_map() ||
         constructor->initial_map().instance_type() != JS_FUNCTION_TYPE);

  Handle<Map> initial_map;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, initial_map,
      JSFunction::GetDerivedMap(isolate, constructor, new_target), JSObject);
  Handle<JSObject> result = isolate->factory()->NewFastOrSlowJSObjectFromMap(
      initial_map, NameDictionary::kInitialCapacity, AllocationType::kYoung,
      site);
  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();
  return result;
}

// 9.1.12 ObjectCreate ( proto [ , internalSlotsList ] )
// Notice: This is NOT 19.1.2.2 Object.create ( O, Properties )
MaybeHandle<JSObject> JSObject::ObjectCreate(Isolate* isolate,
                                             Handle<Object> prototype) {
  // Generate the map with the specified {prototype} based on the Object
  // function's initial map from the current native context.
  // TODO(bmeurer): Use a dedicated cache for Object.create; think about
  // slack tracking for Object.create.
  Handle<Map> map =
      Map::GetObjectCreateMap(isolate, Handle<HeapObject>::cast(prototype));

  // Actually allocate the object.
  return isolate->factory()->NewFastOrSlowJSObjectFromMap(map);
}

void JSObject::EnsureWritableFastElements(Handle<JSObject> object) {
  DCHECK(object->HasSmiOrObjectElements() ||
         object->HasFastStringWrapperElements() ||
         object->HasAnyNonextensibleElements());
  FixedArray raw_elems = FixedArray::cast(object->elements());
  Isolate* isolate = object->GetIsolate();
  if (raw_elems.map() != ReadOnlyRoots(isolate).fixed_cow_array_map()) return;
  Handle<FixedArray> elems(raw_elems, isolate);
  Handle<FixedArray> writable_elems = isolate->factory()->CopyFixedArrayWithMap(
      elems, isolate->factory()->fixed_array_map());
  object->set_elements(*writable_elems);
  isolate->counters()->cow_arrays_converted()->Increment();
}

int JSObject::GetHeaderSize(InstanceType type,
                            bool function_has_prototype_slot) {
  switch (type) {
    case JS_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_GENERATOR_OBJECT_TYPE:
      return JSGeneratorObject::kSize;
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
      return JSAsyncFunctionObject::kSize;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      return JSAsyncGeneratorObject::kSize;
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
      return JSAsyncFromSyncIterator::kSize;
    case JS_GLOBAL_PROXY_TYPE:
      return JSGlobalProxy::kSize;
    case JS_GLOBAL_OBJECT_TYPE:
      return JSGlobalObject::kSize;
    case JS_BOUND_FUNCTION_TYPE:
      return JSBoundFunction::kSize;
    case JS_FUNCTION_TYPE:
      return JSFunction::GetHeaderSize(function_has_prototype_slot);
    case JS_PRIMITIVE_WRAPPER_TYPE:
      return JSPrimitiveWrapper::kSize;
    case JS_DATE_TYPE:
      return JSDate::kSize;
    case JS_ARRAY_TYPE:
      return JSArray::kSize;
    case JS_ARRAY_BUFFER_TYPE:
      return JSArrayBuffer::kHeaderSize;
    case JS_ARRAY_ITERATOR_TYPE:
      return JSArrayIterator::kSize;
    case JS_TYPED_ARRAY_TYPE:
      return JSTypedArray::kHeaderSize;
    case JS_DATA_VIEW_TYPE:
      return JSDataView::kHeaderSize;
    case JS_SET_TYPE:
      return JSSet::kSize;
    case JS_MAP_TYPE:
      return JSMap::kSize;
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      return JSSetIterator::kSize;
    case JS_MAP_KEY_ITERATOR_TYPE:
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_MAP_VALUE_ITERATOR_TYPE:
      return JSMapIterator::kSize;
    case WEAK_CELL_TYPE:
      return WeakCell::kSize;
    case JS_WEAK_REF_TYPE:
      return JSWeakRef::kSize;
    case JS_FINALIZATION_GROUP_TYPE:
      return JSFinalizationGroup::kSize;
    case JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE:
      return JSFinalizationGroupCleanupIterator::kSize;
    case JS_WEAK_MAP_TYPE:
      return JSWeakMap::kSize;
    case JS_WEAK_SET_TYPE:
      return JSWeakSet::kSize;
    case JS_PROMISE_TYPE:
      return JSPromise::kSize;
    case JS_REG_EXP_TYPE:
      return JSRegExp::kSize;
    case JS_REG_EXP_STRING_ITERATOR_TYPE:
      return JSRegExpStringIterator::kSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kSize;
    case JS_ARGUMENTS_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_ERROR_TYPE:
      return JSObject::kHeaderSize;
    case JS_STRING_ITERATOR_TYPE:
      return JSStringIterator::kSize;
    case JS_MODULE_NAMESPACE_TYPE:
      return JSModuleNamespace::kHeaderSize;
#ifdef V8_INTL_SUPPORT
    case JS_V8_BREAK_ITERATOR_TYPE:
      return JSV8BreakIterator::kSize;
    case JS_COLLATOR_TYPE:
      return JSCollator::kSize;
    case JS_DATE_TIME_FORMAT_TYPE:
      return JSDateTimeFormat::kSize;
    case JS_LIST_FORMAT_TYPE:
      return JSListFormat::kSize;
    case JS_LOCALE_TYPE:
      return JSLocale::kSize;
    case JS_NUMBER_FORMAT_TYPE:
      return JSNumberFormat::kSize;
    case JS_PLURAL_RULES_TYPE:
      return JSPluralRules::kSize;
    case JS_RELATIVE_TIME_FORMAT_TYPE:
      return JSRelativeTimeFormat::kSize;
    case JS_SEGMENT_ITERATOR_TYPE:
      return JSSegmentIterator::kSize;
    case JS_SEGMENTER_TYPE:
      return JSSegmenter::kSize;
#endif  // V8_INTL_SUPPORT
    case WASM_GLOBAL_OBJECT_TYPE:
      return WasmGlobalObject::kSize;
    case WASM_INSTANCE_OBJECT_TYPE:
      return WasmInstanceObject::kSize;
    case WASM_MEMORY_OBJECT_TYPE:
      return WasmMemoryObject::kSize;
    case WASM_MODULE_OBJECT_TYPE:
      return WasmModuleObject::kSize;
    case WASM_TABLE_OBJECT_TYPE:
      return WasmTableObject::kSize;
    case WASM_EXCEPTION_OBJECT_TYPE:
      return WasmExceptionObject::kSize;
    default:
      UNREACHABLE();
  }
}

// static
bool JSObject::AllCanRead(LookupIterator* it) {
  // Skip current iteration, it's in state ACCESS_CHECK or INTERCEPTOR, both of
  // which have already been checked.
  DCHECK(it->state() == LookupIterator::ACCESS_CHECK ||
         it->state() == LookupIterator::INTERCEPTOR);
  for (it->Next(); it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      auto accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors).all_can_read()) return true;
      }
    } else if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->GetInterceptor()->all_can_read()) return true;
    } else if (it->state() == LookupIterator::JSPROXY) {
      // Stop lookupiterating. And no, AllCanNotRead.
      return false;
    }
  }
  return false;
}

MaybeHandle<Object> JSObject::GetPropertyWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return Object::GetPropertyWithAccessor(it);
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      bool done;
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                                 GetPropertyWithInterceptor(it, &done), Object);
      if (done) return result;
    }

  } else {
    Handle<Object> result;
    bool done;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        GetPropertyWithInterceptorInternal(it, interceptor, &done), Object);
    if (done) return result;
  }

  // Cross-Origin [[Get]] of Well-Known Symbols does not throw, and returns
  // undefined.
  Handle<Name> name = it->GetName();
  if (name->IsSymbol() && Symbol::cast(*name).is_well_known_symbol()) {
    return it->factory()->undefined_value();
  }

  isolate->ReportFailedAccessCheck(checked);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  return it->factory()->undefined_value();
}

Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return Just(it->property_attributes());
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      auto result = GetPropertyAttributesWithInterceptor(it);
      if (isolate->has_scheduled_exception()) break;
      if (result.IsJust() && result.FromJust() != ABSENT) return result;
    }
  } else {
    Maybe<PropertyAttributes> result =
        GetPropertyAttributesWithInterceptorInternal(it, interceptor);
    if (isolate->has_pending_exception()) return Nothing<PropertyAttributes>();
    if (result.FromMaybe(ABSENT) != ABSENT) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}

// static
bool JSObject::AllCanWrite(LookupIterator* it) {
  for (; it->IsFound() && it->state() != LookupIterator::JSPROXY; it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors).all_can_write()) return true;
      }
    }
  }
  return false;
}

Maybe<bool> JSObject::SetPropertyWithFailedAccessCheck(
    LookupIterator* it, Handle<Object> value, Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    if (AllCanWrite(it)) {
      return Object::SetPropertyWithAccessor(it, value, should_throw);
    }
  } else {
    Maybe<bool> result = SetPropertyWithInterceptorInternal(
        it, interceptor, should_throw, value);
    if (isolate->has_pending_exception()) return Nothing<bool>();
    if (result.IsJust()) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  return Just(true);
}

void JSObject::SetNormalizedProperty(Handle<JSObject> object, Handle<Name> name,
                                     Handle<Object> value,
                                     PropertyDetails details) {
  DCHECK(!object->HasFastProperties());
  DCHECK(name->IsUniqueName());
  Isolate* isolate = object->GetIsolate();

  uint32_t hash = name->Hash();

  if (object->IsJSGlobalObject()) {
    Handle<JSGlobalObject> global_obj = Handle<JSGlobalObject>::cast(object);
    Handle<GlobalDictionary> dictionary(global_obj->global_dictionary(),
                                        isolate);
    int entry = dictionary->FindEntry(ReadOnlyRoots(isolate), name, hash);

    if (entry == GlobalDictionary::kNotFound) {
      DCHECK_IMPLIES(global_obj->map().is_prototype_map(),
                     Map::IsPrototypeChainInvalidated(global_obj->map()));
      auto cell = isolate->factory()->NewPropertyCell(name);
      cell->set_value(*value);
      auto cell_type = value->IsUndefined(isolate)
                           ? PropertyCellType::kUndefined
                           : PropertyCellType::kConstant;
      details = details.set_cell_type(cell_type);
      value = cell;
      dictionary =
          GlobalDictionary::Add(isolate, dictionary, name, value, details);
      global_obj->set_global_dictionary(*dictionary);
    } else {
      Handle<PropertyCell> cell = PropertyCell::PrepareForValue(
          isolate, dictionary, entry, value, details);
      cell->set_value(*value);
    }
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);

    int entry = dictionary->FindEntry(isolate, name);
    if (entry == NameDictionary::kNotFound) {
      DCHECK_IMPLIES(object->map().is_prototype_map(),
                     Map::IsPrototypeChainInvalidated(object->map()));
      dictionary =
          NameDictionary::Add(isolate, dictionary, name, value, details);
      object->SetProperties(*dictionary);
    } else {
      PropertyDetails original_details = dictionary->DetailsAt(entry);
      int enumeration_index = original_details.dictionary_index();
      DCHECK_GT(enumeration_index, 0);
      details = details.set_index(enumeration_index);
      dictionary->SetEntry(isolate, entry, *name, *value, details);
    }
  }
}

void JSObject::JSObjectShortPrint(StringStream* accumulator) {
  switch (map().instance_type()) {
    case JS_ARRAY_TYPE: {
      double length = JSArray::cast(*this).length().IsUndefined()
                          ? 0
                          : JSArray::cast(*this).length().Number();
      accumulator->Add("<JSArray[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      JSBoundFunction bound_function = JSBoundFunction::cast(*this);
      accumulator->Add("<JSBoundFunction");
      accumulator->Add(" (BoundTargetFunction %p)>",
                       reinterpret_cast<void*>(
                           bound_function.bound_target_function().ptr()));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JSWeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JSWeakSet>");
      break;
    }
    case JS_REG_EXP_TYPE: {
      accumulator->Add("<JSRegExp");
      JSRegExp regexp = JSRegExp::cast(*this);
      if (regexp.source().IsString()) {
        accumulator->Add(" ");
        String::cast(regexp.source()).StringShortPrint(accumulator);
      }
      accumulator->Add(">");

      break;
    }
    case JS_FUNCTION_TYPE: {
      JSFunction function = JSFunction::cast(*this);
      Object fun_name = function.shared().DebugName();
      bool printed = false;
      if (fun_name.IsString()) {
        String str = String::cast(fun_name);
        if (str.length() > 0) {
          accumulator->Add("<JSFunction ");
          accumulator->Put(str);
          printed = true;
        }
      }
      if (!printed) {
        accumulator->Add("<JSFunction");
      }
      if (FLAG_trace_file_names) {
        Object source_name = Script::cast(function.shared().script()).name();
        if (source_name.IsString()) {
          String str = String::cast(source_name);
          if (str.length() > 0) {
            accumulator->Add(" <");
            accumulator->Put(str);
            accumulator->Add(">");
          }
        }
      }
      accumulator->Add(" (sfi = %p)",
                       reinterpret_cast<void*>(function.shared().ptr()));
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JSGenerator>");
      break;
    }
    case JS_ASYNC_FUNCTION_OBJECT_TYPE: {
      accumulator->Add("<JSAsyncFunctionObject>");
      break;
    }
    case JS_ASYNC_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS AsyncGenerator>");
      break;
    }

    // All other JSObjects are rather similar to each other (JSObject,
    // JSGlobalProxy, JSGlobalObject, JSUndetectable, JSPrimitiveWrapper).
    default: {
      Map map_of_this = map();
      Heap* heap = GetHeap();
      Object constructor = map_of_this.GetConstructor();
      bool printed = false;
      if (constructor.IsHeapObject() &&
          !heap->Contains(HeapObject::cast(constructor))) {
        accumulator->Add("!!!INVALID CONSTRUCTOR!!!");
      } else {
        bool global_object = IsJSGlobalProxy();
        if (constructor.IsJSFunction()) {
          if (!heap->Contains(JSFunction::cast(constructor).shared())) {
            accumulator->Add("!!!INVALID SHARED ON CONSTRUCTOR!!!");
          } else {
            String constructor_name =
                JSFunction::cast(constructor).shared().Name();
            if (constructor_name.length() > 0) {
              accumulator->Add(global_object ? "<GlobalObject " : "<");
              accumulator->Put(constructor_name);
              accumulator->Add(" %smap = %p",
                               map_of_this.is_deprecated() ? "deprecated-" : "",
                               map_of_this);
              printed = true;
            }
          }
        } else if (constructor.IsFunctionTemplateInfo()) {
          accumulator->Add(global_object ? "<RemoteObject>" : "<RemoteObject>");
          printed = true;
        }
        if (!printed) {
          accumulator->Add("<JS%sObject", global_object ? "Global " : "");
        }
      }
      if (IsJSPrimitiveWrapper()) {
        accumulator->Add(" value = ");
        JSPrimitiveWrapper::cast(*this).value().ShortPrint(accumulator);
      }
      accumulator->Put('>');
      break;
    }
  }
}

void JSObject::PrintElementsTransition(FILE* file, Handle<JSObject> object,
                                       ElementsKind from_kind,
                                       Handle<FixedArrayBase> from_elements,
                                       ElementsKind to_kind,
                                       Handle<FixedArrayBase> to_elements) {
  if (from_kind != to_kind) {
    OFStream os(file);
    os << "elements transition [" << ElementsKindToString(from_kind) << " -> "
       << ElementsKindToString(to_kind) << "] in ";
    JavaScriptFrame::PrintTop(object->GetIsolate(), file, false, true);
    PrintF(file, " for ");
    object->ShortPrint(file);
    PrintF(file, " from ");
    from_elements->ShortPrint(file);
    PrintF(file, " to ");
    to_elements->ShortPrint(file);
    PrintF(file, "\n");
  }
}

void JSObject::PrintInstanceMigration(FILE* file, Map original_map,
                                      Map new_map) {
  if (new_map.is_dictionary_map()) {
    PrintF(file, "[migrating to slow]\n");
    return;
  }
  PrintF(file, "[migrating]");
  DescriptorArray o = original_map.instance_descriptors();
  DescriptorArray n = new_map.instance_descriptors();
  for (InternalIndex i : original_map.IterateOwnDescriptors()) {
    Representation o_r = o.GetDetails(i).representation();
    Representation n_r = n.GetDetails(i).representation();
    if (!o_r.Equals(n_r)) {
      String::cast(o.GetKey(i)).PrintOn(file);
      PrintF(file, ":%s->%s ", o_r.Mnemonic(), n_r.Mnemonic());
    } else if (o.GetDetails(i).location() == kDescriptor &&
               n.GetDetails(i).location() == kField) {
      Name name = o.GetKey(i);
      if (name.IsString()) {
        String::cast(name).PrintOn(file);
      } else {
        PrintF(file, "{symbol %p}", reinterpret_cast<void*>(name.ptr()));
      }
      PrintF(file, " ");
    }
  }
  if (original_map.elements_kind() != new_map.elements_kind()) {
    PrintF(file, "elements_kind[%i->%i]", original_map.elements_kind(),
           new_map.elements_kind());
  }
  PrintF(file, "\n");
}

bool JSObject::IsUnmodifiedApiObject(FullObjectSlot o) {
  Object object = *o;
  if (object.IsSmi()) return false;
  HeapObject heap_object = HeapObject::cast(object);
  if (!object.IsJSObject()) return false;
  JSObject js_object = JSObject::cast(object);
  if (!js_object.IsDroppableApiWrapper()) return false;
  Object maybe_constructor = js_object.map().GetConstructor();
  if (!maybe_constructor.IsJSFunction()) return false;
  JSFunction constructor = JSFunction::cast(maybe_constructor);
  if (js_object.elements().length() != 0) return false;
  // Check that the object is not a key in a WeakMap (over-approximation).
  if (!js_object.GetIdentityHash().IsUndefined()) return false;

  return constructor.initial_map() == heap_object.map();
}

// static
void JSObject::UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                               Handle<Map> new_map,
                                               Isolate* isolate) {
  DCHECK(old_map->is_prototype_map());
  DCHECK(new_map->is_prototype_map());
  bool was_registered = JSObject::UnregisterPrototypeUser(old_map, isolate);
  new_map->set_prototype_info(old_map->prototype_info());
  old_map->set_prototype_info(Smi::kZero);
  if (FLAG_trace_prototype_users) {
    PrintF("Moving prototype_info %p from map %p to map %p.\n",
           reinterpret_cast<void*>(new_map->prototype_info().ptr()),
           reinterpret_cast<void*>(old_map->ptr()),
           reinterpret_cast<void*>(new_map->ptr()));
  }
  if (was_registered) {
    if (new_map->prototype_info().IsPrototypeInfo()) {
      // The new map isn't registered with its prototype yet; reflect this fact
      // in the PrototypeInfo it just inherited from the old map.
      PrototypeInfo::cast(new_map->prototype_info())
          .set_registry_slot(PrototypeInfo::UNREGISTERED);
    }
    JSObject::LazyRegisterPrototypeUser(new_map, isolate);
  }
}

// static
void JSObject::NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                               Isolate* isolate) {
  if (!old_map->is_prototype_map()) return;

  InvalidatePrototypeChains(*old_map);

  // If the map was registered with its prototype before, ensure that it
  // registers with its new prototype now. This preserves the invariant that
  // when a map on a prototype chain is registered with its prototype, then
  // all prototypes further up the chain are also registered with their
  // respective prototypes.
  UpdatePrototypeUserRegistration(old_map, new_map, isolate);
}

namespace {

// To migrate a fast instance to a fast map:
// - First check whether the instance needs to be rewritten. If not, simply
//   change the map.
// - Otherwise, allocate a fixed array large enough to hold all fields, in
//   addition to unused space.
// - Copy all existing properties in, in the following order: backing store
//   properties, unused fields, inobject properties.
// - If all allocation succeeded, commit the state atomically:
//   * Copy inobject properties from the backing store back into the object.
//   * Trim the difference in instance size of the object. This also cleanly
//     frees inobject properties that moved to the backing store.
//   * If there are properties left in the backing store, trim of the space used
//     to temporarily store the inobject properties.
//   * If there are properties left in the backing store, install the backing
//     store.
void MigrateFastToFast(Isolate* isolate, Handle<JSObject> object,
                       Handle<Map> new_map) {
  Handle<Map> old_map(object->map(), isolate);
  // In case of a regular transition.
  if (new_map->GetBackPointer(isolate) == *old_map) {
    // If the map does not add named properties, simply set the map.
    if (old_map->NumberOfOwnDescriptors() ==
        new_map->NumberOfOwnDescriptors()) {
      object->synchronized_set_map(*new_map);
      return;
    }

    // If the map adds a new kDescriptor property, simply set the map.
    PropertyDetails details = new_map->GetLastDescriptorDetails(isolate);
    if (details.location() == kDescriptor) {
      object->synchronized_set_map(*new_map);
      return;
    }

    // Check if we still have space in the {object}, in which case we
    // can also simply set the map (modulo a special case for mutable
    // double boxes).
    FieldIndex index =
        FieldIndex::ForDescriptor(isolate, *new_map, new_map->LastAdded());
    if (index.is_inobject() || index.outobject_array_index() <
                                   object->property_array(isolate).length()) {
      // We still need to allocate HeapNumbers for double fields
      // if either double field unboxing is disabled or the double field
      // is in the PropertyArray backing store (where we don't support
      // double field unboxing).
      if (index.is_double() && !new_map->IsUnboxedDoubleField(isolate, index)) {
        auto value = isolate->factory()->NewHeapNumberWithHoleNaN();
        object->RawFastPropertyAtPut(index, *value);
      }
      object->synchronized_set_map(*new_map);
      return;
    }

    // This migration is a transition from a map that has run out of property
    // space. Extend the backing store.
    int grow_by = new_map->UnusedPropertyFields() + 1;
    Handle<PropertyArray> old_storage(object->property_array(isolate), isolate);
    Handle<PropertyArray> new_storage =
        isolate->factory()->CopyPropertyArrayAndGrow(old_storage, grow_by);

    // Properly initialize newly added property.
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumberWithHoleNaN();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    DCHECK_EQ(kField, details.location());
    DCHECK_EQ(kData, details.kind());
    DCHECK(!index.is_inobject());  // Must be a backing store index.
    new_storage->set(index.outobject_array_index(), *value);

    // From here on we cannot fail and we shouldn't GC anymore.
    DisallowHeapAllocation no_allocation;

    // Set the new property value and do the map transition.
    object->SetProperties(*new_storage);
    object->synchronized_set_map(*new_map);
    return;
  }

  int old_number_of_fields;
  int number_of_fields = new_map->NumberOfFields();
  int inobject = new_map->GetInObjectProperties();
  int unused = new_map->UnusedPropertyFields();

  // Nothing to do if no functions were converted to fields and no smis were
  // converted to doubles.
  if (!old_map->InstancesNeedRewriting(*new_map, number_of_fields, inobject,
                                       unused, &old_number_of_fields)) {
    object->synchronized_set_map(*new_map);
    return;
  }

  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  Handle<PropertyArray> array = isolate->factory()->NewPropertyArray(external);

  // We use this array to temporarily store the inobject properties.
  Handle<FixedArray> inobject_props =
      isolate->factory()->NewFixedArray(inobject);

  Handle<DescriptorArray> old_descriptors(
      old_map->instance_descriptors(isolate), isolate);
  Handle<DescriptorArray> new_descriptors(
      new_map->instance_descriptors(isolate), isolate);
  int old_nof = old_map->NumberOfOwnDescriptors();
  int new_nof = new_map->NumberOfOwnDescriptors();

  // This method only supports generalizing instances to at least the same
  // number of properties.
  DCHECK(old_nof <= new_nof);

  for (InternalIndex i : InternalIndex::Range(old_nof)) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Representation old_representation = old_details.representation();
    Representation representation = details.representation();
    Handle<Object> value;
    if (old_details.location() == kDescriptor) {
      if (old_details.kind() == kAccessor) {
        // In case of kAccessor -> kData property reconfiguration, the property
        // must already be prepared for data of certain type.
        DCHECK(!details.representation().IsNone());
        if (details.representation().IsDouble()) {
          value = isolate->factory()->NewHeapNumberWithHoleNaN();
        } else {
          value = isolate->factory()->uninitialized_value();
        }
      } else {
        DCHECK_EQ(kData, old_details.kind());
        value = handle(old_descriptors->GetStrongValue(isolate, i), isolate);
        DCHECK(!old_representation.IsDouble() && !representation.IsDouble());
      }
    } else {
      DCHECK_EQ(kField, old_details.location());
      FieldIndex index = FieldIndex::ForDescriptor(isolate, *old_map, i);
      if (object->IsUnboxedDoubleField(isolate, index)) {
        uint64_t old_bits = object->RawFastDoublePropertyAsBitsAt(index);
        value = isolate->factory()->NewHeapNumberFromBits(old_bits);
      } else {
        value = handle(object->RawFastPropertyAt(isolate, index), isolate);
        if (!old_representation.IsDouble() && representation.IsDouble()) {
          DCHECK_IMPLIES(old_representation.IsNone(),
                         value->IsUninitialized(isolate));
          value = Object::NewStorageFor(isolate, value, representation);
        } else if (old_representation.IsDouble() &&
                   !representation.IsDouble()) {
          value = Object::WrapForRead(isolate, value, old_representation);
        }
      }
    }
    DCHECK(!(representation.IsDouble() && value->IsSmi()));
    int target_index = new_descriptors->GetFieldIndex(i);
    if (target_index < inobject) {
      inobject_props->set(target_index, *value);
    } else {
      array->set(target_index - inobject, *value);
    }
  }

  for (InternalIndex i : InternalIndex::Range(old_nof, new_nof)) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumberWithHoleNaN();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    int target_index = new_descriptors->GetFieldIndex(i);
    if (target_index < inobject) {
      inobject_props->set(target_index, *value);
    } else {
      array->set(target_index - inobject, *value);
    }
  }

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  Heap* heap = isolate->heap();

  // Invalidate slots manually later in case of tagged to untagged translation.
  // In all other cases the recorded slot remains dereferenceable.
  heap->NotifyObjectLayoutChange(*object, no_allocation,
                                 InvalidateRecordedSlots::kNo);

  // Copy (real) inobject properties. If necessary, stop at number_of_fields to
  // avoid overwriting |one_pointer_filler_map|.
  int limit = Min(inobject, number_of_fields);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
    Object value = inobject_props->get(isolate, i);
    // Can't use JSObject::FastPropertyAtPut() because proper map was not set
    // yet.
    if (new_map->IsUnboxedDoubleField(isolate, index)) {
      DCHECK(value.IsHeapNumber(isolate));
      // Ensure that all bits of the double value are preserved.
      object->RawFastDoublePropertyAsBitsAtPut(
          index, HeapNumber::cast(value).value_as_bits());
      if (i < old_number_of_fields && !old_map->IsUnboxedDoubleField(index)) {
        // Transition from tagged to untagged slot.
        MemoryChunk* chunk = MemoryChunk::FromHeapObject(*object);
        chunk->InvalidateRecordedSlots(*object);
      } else {
#ifdef DEBUG
        heap->VerifyClearedSlot(*object, object->RawField(index.offset()));
#endif
      }
    } else {
      object->RawFastPropertyAtPut(index, value);
    }
  }

  object->SetProperties(*array);

  // Create filler object past the new instance size.
  int old_instance_size = old_map->instance_size();
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_instance_size - new_instance_size;
  DCHECK_GE(instance_size_delta, 0);

  if (instance_size_delta > 0) {
    Address address = object->address();
    heap->CreateFillerObjectAt(address + new_instance_size, instance_size_delta,
                               ClearRecordedSlots::kYes);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);
}

void MigrateFastToSlow(Isolate* isolate, Handle<JSObject> object,
                       Handle<Map> new_map,
                       int expected_additional_properties) {
  // The global object is always normalized.
  DCHECK(!object->IsJSGlobalObject(isolate));
  // JSGlobalProxy must never be normalized
  DCHECK(!object->IsJSGlobalProxy(isolate));

  DCHECK_IMPLIES(new_map->is_prototype_map(),
                 Map::IsPrototypeChainInvalidated(*new_map));

  HandleScope scope(isolate);
  Handle<Map> map(object->map(isolate), isolate);

  // Allocate new content.
  int real_size = map->NumberOfOwnDescriptors();
  int property_count = real_size;
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    // Make space for two more properties.
    property_count += NameDictionary::kInitialCapacity;
  }
  Handle<NameDictionary> dictionary =
      NameDictionary::New(isolate, property_count);

  Handle<DescriptorArray> descs(map->instance_descriptors(isolate), isolate);
  for (InternalIndex i : InternalIndex::Range(real_size)) {
    PropertyDetails details = descs->GetDetails(i);
    Handle<Name> key(descs->GetKey(isolate, i), isolate);
    Handle<Object> value;
    if (details.location() == kField) {
      FieldIndex index = FieldIndex::ForDescriptor(isolate, *map, i);
      if (details.kind() == kData) {
        if (object->IsUnboxedDoubleField(isolate, index)) {
          double old_value = object->RawFastDoublePropertyAt(index);
          value = isolate->factory()->NewHeapNumber(old_value);
        } else {
          value = handle(object->RawFastPropertyAt(isolate, index), isolate);
          if (details.representation().IsDouble()) {
            DCHECK(value->IsHeapNumber(isolate));
            double old_value = Handle<HeapNumber>::cast(value)->value();
            value = isolate->factory()->NewHeapNumber(old_value);
          }
        }
      } else {
        DCHECK_EQ(kAccessor, details.kind());
        value = handle(object->RawFastPropertyAt(isolate, index), isolate);
      }

    } else {
      DCHECK_EQ(kDescriptor, details.location());
      value = handle(descs->GetStrongValue(isolate, i), isolate);
    }
    DCHECK(!value.is_null());
    PropertyDetails d(details.kind(), details.attributes(),
                      PropertyCellType::kNoCell);
    dictionary = NameDictionary::Add(isolate, dictionary, key, value, d);
  }

  // Copy the next enumeration index from instance descriptor.
  dictionary->SetNextEnumerationIndex(real_size + 1);

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  Heap* heap = isolate->heap();

  // Invalidate slots manually later in case the new map has in-object
  // properties. If not, it is not possible to store an untagged value
  // in a recorded slot.
  heap->NotifyObjectLayoutChange(*object, no_allocation,
                                 InvalidateRecordedSlots::kNo);

  // Resize the object in the heap if necessary.
  int old_instance_size = map->instance_size();
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_instance_size - new_instance_size;
  DCHECK_GE(instance_size_delta, 0);

  if (instance_size_delta > 0) {
    heap->CreateFillerObjectAt(object->address() + new_instance_size,
                               instance_size_delta, ClearRecordedSlots::kYes);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);

  object->SetProperties(*dictionary);

  // Ensure that in-object space of slow-mode object does not contain random
  // garbage.
  int inobject_properties = new_map->GetInObjectProperties();
  if (inobject_properties) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(*object);
    chunk->InvalidateRecordedSlots(*object);

    for (int i = 0; i < inobject_properties; i++) {
      FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
      object->RawFastPropertyAtPut(index, Smi::kZero);
    }
  }

  isolate->counters()->props_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    StdoutStream os;
    os << "Object properties have been normalized:\n";
    object->Print(os);
  }
#endif
}

}  // namespace

void JSObject::MigrateToMap(Isolate* isolate, Handle<JSObject> object,
                            Handle<Map> new_map,
                            int expected_additional_properties) {
  if (object->map(isolate) == *new_map) return;
  Handle<Map> old_map(object->map(isolate), isolate);
  NotifyMapChange(old_map, new_map, isolate);

  if (old_map->is_dictionary_map()) {
    // For slow-to-fast migrations JSObject::MigrateSlowToFast()
    // must be used instead.
    CHECK(new_map->is_dictionary_map());

    // Slow-to-slow migration is trivial.
    object->synchronized_set_map(*new_map);
  } else if (!new_map->is_dictionary_map()) {
    MigrateFastToFast(isolate, object, new_map);
    if (old_map->is_prototype_map()) {
      DCHECK(!old_map->is_stable());
      DCHECK(new_map->is_stable());
      DCHECK(new_map->owns_descriptors());
      DCHECK(old_map->owns_descriptors());
      // Transfer ownership to the new map. Keep the descriptor pointer of the
      // old map intact because the concurrent marker might be iterating the
      // object with the old map.
      old_map->set_owns_descriptors(false);
      DCHECK(old_map->is_abandoned_prototype_map());
      // Ensure that no transition was inserted for prototype migrations.
      DCHECK_EQ(0, TransitionsAccessor(isolate, old_map).NumberOfTransitions());
      DCHECK(new_map->GetBackPointer(isolate).IsUndefined(isolate));
      DCHECK(object->map(isolate) != *old_map);
    }
  } else {
    MigrateFastToSlow(isolate, object, new_map, expected_additional_properties);
  }

  // Careful: Don't allocate here!
  // For some callers of this method, |object| might be in an inconsistent
  // state now: the new map might have a new elements_kind, but the object's
  // elements pointer hasn't been updated yet. Callers will fix this, but in
  // the meantime, (indirectly) calling JSObjectVerify() must be avoided.
  // When adding code here, add a DisallowHeapAllocation too.
}

void JSObject::ForceSetPrototype(Handle<JSObject> object,
                                 Handle<HeapObject> proto) {
  // object.__proto__ = proto;
  Isolate* isolate = object->GetIsolate();
  Handle<Map> old_map = Handle<Map>(object->map(), isolate);
  Handle<Map> new_map = Map::Copy(isolate, old_map, "ForceSetPrototype");
  Map::SetPrototype(isolate, new_map, proto);
  JSObject::MigrateToMap(isolate, object, new_map);
}

Maybe<bool> JSObject::SetPropertyWithInterceptor(
    LookupIterator* it, Maybe<ShouldThrow> should_throw, Handle<Object> value) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return SetPropertyWithInterceptorInternal(it, it->GetInterceptor(),
                                            should_throw, value);
}

Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Handle<Map> map(object->map(), object->GetIsolate());
  return Map::TransitionElementsTo(object->GetIsolate(), map, to_kind);
}

// static
MaybeHandle<NativeContext> JSObject::GetFunctionRealm(Handle<JSObject> object) {
  DCHECK(object->map().is_constructor());
  DCHECK(!object->IsJSFunction());
  return object->GetCreationContext();
}

void JSObject::AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map) {
  DCHECK(object->map().GetInObjectProperties() == map->GetInObjectProperties());
  ElementsKind obj_kind = object->map().elements_kind();
  ElementsKind map_kind = map->elements_kind();
  if (map_kind != obj_kind) {
    ElementsKind to_kind = GetMoreGeneralElementsKind(map_kind, obj_kind);
    if (IsDictionaryElementsKind(obj_kind)) {
      to_kind = obj_kind;
    }
    if (IsDictionaryElementsKind(to_kind)) {
      NormalizeElements(object);
    } else {
      TransitionElementsKind(object, to_kind);
    }
    map = Map::ReconfigureElementsKind(object->GetIsolate(), map, to_kind);
  }
  int number_of_fields = map->NumberOfFields();
  int inobject = map->GetInObjectProperties();
  int unused = map->UnusedPropertyFields();
  int total_size = number_of_fields + unused;
  int external = total_size - inobject;
  // Allocate mutable double boxes if necessary. It is always necessary if we
  // have external properties, but is also necessary if we only have inobject
  // properties but don't unbox double fields.
  if (!FLAG_unbox_double_fields || external > 0) {
    Isolate* isolate = object->GetIsolate();

    Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
    Handle<FixedArray> storage;
    if (!FLAG_unbox_double_fields) {
      storage = isolate->factory()->NewFixedArray(inobject);
    }

    Handle<PropertyArray> array =
        isolate->factory()->NewPropertyArray(external);

    for (InternalIndex i : map->IterateOwnDescriptors()) {
      PropertyDetails details = descriptors->GetDetails(i);
      Representation representation = details.representation();
      if (!representation.IsDouble()) continue;
      FieldIndex index = FieldIndex::ForDescriptor(*map, i);
      if (map->IsUnboxedDoubleField(index)) continue;
      auto box = isolate->factory()->NewHeapNumberWithHoleNaN();
      if (index.is_inobject()) {
        storage->set(index.property_index(), *box);
      } else {
        array->set(index.outobject_array_index(), *box);
      }
    }

    object->SetProperties(*array);

    if (!FLAG_unbox_double_fields) {
      for (int i = 0; i < inobject; i++) {
        FieldIndex index = FieldIndex::ForPropertyIndex(*map, i);
        Object value = storage->get(i);
        object->RawFastPropertyAtPut(index, value);
      }
    }
  }
  object->synchronized_set_map(*map);
}

void JSObject::MigrateInstance(Isolate* isolate, Handle<JSObject> object) {
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> map = Map::Update(isolate, original_map);
  map->set_is_migration_target(true);
  JSObject::MigrateToMap(isolate, object, map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, *map);
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify(isolate);
  }
#endif
}

// static
bool JSObject::TryMigrateInstance(Isolate* isolate, Handle<JSObject> object) {
  DisallowDeoptimization no_deoptimization(isolate);
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> new_map;
  if (!Map::TryUpdate(isolate, original_map).ToHandle(&new_map)) {
    return false;
  }
  JSObject::MigrateToMap(isolate, object, new_map);
  if (FLAG_trace_migration && *original_map != object->map()) {
    object->PrintInstanceMigration(stdout, *original_map, object->map());
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify(isolate);
  }
#endif
  return true;
}

void JSObject::AddProperty(Isolate* isolate, Handle<JSObject> object,
                           Handle<Name> name, Handle<Object> value,
                           PropertyAttributes attributes) {
  LookupIterator it(isolate, object, name, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
#ifdef DEBUG
  uint32_t index;
  DCHECK(!object->IsJSProxy());
  DCHECK(!name->AsArrayIndex(&index));
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  DCHECK(maybe.IsJust());
  DCHECK(!it.IsFound());
  DCHECK(object->map().is_extensible() || name->IsPrivate());
#endif
  CHECK(Object::AddDataProperty(&it, value, attributes,
                                Just(ShouldThrow::kThrowOnError),
                                StoreOrigin::kNamed)
            .IsJust());
}

void JSObject::AddProperty(Isolate* isolate, Handle<JSObject> object,
                           const char* name, Handle<Object> value,
                           PropertyAttributes attributes) {
  JSObject::AddProperty(isolate, object,
                        isolate->factory()->InternalizeUtf8String(name), value,
                        attributes);
}

// Reconfigures a property to a data property with attributes, even if it is not
// reconfigurable.
// Requires a LookupIterator that does not look at the prototype chain beyond
// hidden prototypes.
MaybeHandle<Object> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    AccessorInfoHandling handling) {
  MAYBE_RETURN_NULL(DefineOwnPropertyIgnoreAttributes(
      it, value, attributes, Just(ShouldThrow::kThrowOnError), handling));
  return value;
}

Maybe<bool> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    Maybe<ShouldThrow> should_throw, AccessorInfoHandling handling) {
  it->UpdateProtector();
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (!it->HasAccess()) {
          it->isolate()->ReportFailedAccessCheck(it->GetHolder<JSObject>());
          RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
          return Just(true);
        }
        break;

      // If there's an interceptor, try to store the property with the
      // interceptor.
      // In case of success, the attributes will have been reset to the default
      // attributes of the interceptor, rather than the incoming attributes.
      //
      // TODO(verwaest): JSProxy afterwards verify the attributes that the
      // JSProxy claims it has, and verifies that they are compatible. If not,
      // they throw. Here we should do the same.
      case LookupIterator::INTERCEPTOR:
        if (handling == DONT_FORCE_FIELD) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        }
        break;

      case LookupIterator::ACCESSOR: {
        Handle<Object> accessors = it->GetAccessors();

        // Special handling for AccessorInfo, which behaves like a data
        // property.
        if (accessors->IsAccessorInfo() && handling == DONT_FORCE_FIELD) {
          PropertyAttributes current_attributes = it->property_attributes();
          // Ensure the context isn't changed after calling into accessors.
          AssertNoContextChange ncc(it->isolate());

          // Update the attributes before calling the setter. The setter may
          // later change the shape of the property.
          if (current_attributes != attributes) {
            it->TransitionToAccessorPair(accessors, attributes);
          }

          return Object::SetPropertyWithAccessor(it, value, should_throw);
        }

        it->ReconfigureDataProperty(value, attributes);
        return Just(true);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Object::RedefineIncompatibleProperty(
            it->isolate(), it->GetName(), value, should_throw);

      case LookupIterator::DATA: {
        // Regular property update if the attributes match.
        if (it->property_attributes() == attributes) {
          return Object::SetDataProperty(it, value);
        }

        // Special case: properties of typed arrays cannot be reconfigured to
        // non-writable nor to non-enumerable.
        if (it->IsElement() && object->HasTypedArrayElements()) {
          return Object::RedefineIncompatibleProperty(
              it->isolate(), it->GetName(), value, should_throw);
        }

        // Reconfigure the data property if the attributes mismatch.
        it->ReconfigureDataProperty(value, attributes);

        return Just(true);
      }
    }
  }

  return Object::AddDataProperty(it, value, attributes, should_throw,
                                 StoreOrigin::kNamed);
}

MaybeHandle<Object> JSObject::SetOwnPropertyIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  DCHECK(!value->IsTheHole());
  LookupIterator it(object, name, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::SetOwnElementIgnoreAttributes(
    Handle<JSObject> object, uint32_t index, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::DefinePropertyOrElementIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithInterceptor(
    LookupIterator* it) {
  return GetPropertyAttributesWithInterceptorInternal(it, it->GetInterceptor());
}

void JSObject::NormalizeProperties(Isolate* isolate, Handle<JSObject> object,
                                   PropertyNormalizationMode mode,
                                   int expected_additional_properties,
                                   const char* reason) {
  if (!object->HasFastProperties()) return;

  Handle<Map> map(object->map(), isolate);
  Handle<Map> new_map =
      Map::Normalize(isolate, map, map->elements_kind(), mode, reason);

  JSObject::MigrateToMap(isolate, object, new_map,
                         expected_additional_properties);
}

void JSObject::MigrateSlowToFast(Handle<JSObject> object,
                                 int unused_property_fields,
                                 const char* reason) {
  if (object->HasFastProperties()) return;
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<NameDictionary> dictionary(object->property_dictionary(), isolate);

  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  int number_of_elements = dictionary->NumberOfElements();
  if (number_of_elements > kMaxNumberOfDescriptors) return;

  Handle<FixedArray> iteration_order =
      NameDictionary::IterationIndices(isolate, dictionary);

  int instance_descriptor_length = iteration_order->length();
  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::ToInt(iteration_order->get(i));
    DCHECK(dictionary->IsKey(roots, dictionary->KeyAt(index)));

    PropertyKind kind = dictionary->DetailsAt(index).kind();
    if (kind == kData) {
      number_of_fields += 1;
    }
  }

  Handle<Map> old_map(object->map(), isolate);

  int inobject_props = old_map->GetInObjectProperties();

  // Allocate new map.
  Handle<Map> new_map = Map::CopyDropDescriptors(isolate, old_map);
  // We should not only set this bit if we need to. We should not retain the
  // old bit because turning a map into dictionary always sets this bit.
  new_map->set_may_have_interesting_symbols(new_map->has_named_interceptor() ||
                                            new_map->is_access_check_needed());
  new_map->set_is_dictionary_map(false);

  NotifyMapChange(old_map, new_map, isolate);

  if (instance_descriptor_length == 0) {
    DisallowHeapAllocation no_gc;
    DCHECK_LE(unused_property_fields, inobject_props);
    // Transform the object.
    new_map->SetInObjectUnusedPropertyFields(inobject_props);
    object->synchronized_set_map(*new_map);
    object->SetProperties(ReadOnlyRoots(isolate).empty_fixed_array());
    // Check that it really works.
    DCHECK(object->HasFastProperties());
    if (FLAG_trace_maps) {
      LOG(isolate, MapEvent("SlowToFast", *old_map, *new_map, reason));
    }
    return;
  }

  // Allocate the instance descriptor.
  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, instance_descriptor_length, 0);

  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the property array for the fields.
  Handle<PropertyArray> fields =
      factory->NewPropertyArray(number_of_allocated_fields);

  bool is_transitionable_elements_kind =
      IsTransitionableFastElementsKind(old_map->elements_kind());

  // Fill in the instance descriptor and the fields.
  int current_offset = 0;
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::ToInt(iteration_order->get(i));
    Name k = dictionary->NameAt(index);
    // Dictionary keys are internalized upon insertion.
    // TODO(jkummerow): Turn this into a DCHECK if it's not hit in the wild.
    CHECK(k.IsUniqueName());
    Handle<Name> key(k, isolate);

    // Properly mark the {new_map} if the {key} is an "interesting symbol".
    if (key->IsInterestingSymbol()) {
      new_map->set_may_have_interesting_symbols(true);
    }

    Object value = dictionary->ValueAt(index);

    PropertyDetails details = dictionary->DetailsAt(index);
    DCHECK_EQ(kField, details.location());
    DCHECK_EQ(PropertyConstness::kMutable, details.constness());

    Descriptor d;
    if (details.kind() == kData) {
      // Ensure that we make constant field only when elements kind is not
      // transitionable.
      PropertyConstness constness = is_transitionable_elements_kind
                                        ? PropertyConstness::kMutable
                                        : PropertyConstness::kConst;
      d = Descriptor::DataField(
          key, current_offset, details.attributes(), constness,
          // TODO(verwaest): value->OptimalRepresentation();
          Representation::Tagged(), MaybeObjectHandle(FieldType::Any(isolate)));
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      d = Descriptor::AccessorConstant(key, handle(value, isolate),
                                       details.attributes());
    }
    details = d.GetDetails();
    if (details.location() == kField) {
      if (current_offset < inobject_props) {
        object->InObjectPropertyAtPut(current_offset, value,
                                      UPDATE_WRITE_BARRIER);
      } else {
        int offset = current_offset - inobject_props;
        fields->set(offset, value);
      }
      current_offset += details.field_width_in_words();
    }
    descriptors->Set(InternalIndex(i), &d);
  }
  DCHECK(current_offset == number_of_fields);

  descriptors->Sort();

  Handle<LayoutDescriptor> layout_descriptor = LayoutDescriptor::New(
      isolate, new_map, descriptors, descriptors->number_of_descriptors());

  DisallowHeapAllocation no_gc;
  new_map->InitializeDescriptors(isolate, *descriptors, *layout_descriptor);
  if (number_of_allocated_fields == 0) {
    new_map->SetInObjectUnusedPropertyFields(unused_property_fields);
  } else {
    new_map->SetOutOfObjectUnusedPropertyFields(unused_property_fields);
  }

  if (FLAG_trace_maps) {
    LOG(isolate, MapEvent("SlowToFast", *old_map, *new_map, reason));
  }
  // Transform the object.
  object->synchronized_set_map(*new_map);

  object->SetProperties(*fields);
  DCHECK(object->IsJSObject());

  // Check that it really works.
  DCHECK(object->HasFastProperties());
}

void JSObject::RequireSlowElements(NumberDictionary dictionary) {
  DCHECK_NE(dictionary,
            ReadOnlyRoots(GetIsolate()).empty_slow_element_dictionary());
  if (dictionary.requires_slow_elements()) return;
  dictionary.set_requires_slow_elements();
  if (map().is_prototype_map()) {
    // If this object is a prototype (the callee will check), invalidate any
    // prototype chains involving it.
    InvalidatePrototypeChains(map());
  }
}

Handle<NumberDictionary> JSObject::NormalizeElements(Handle<JSObject> object) {
  DCHECK(!object->HasTypedArrayElements());
  Isolate* isolate = object->GetIsolate();
  bool is_sloppy_arguments = object->HasSloppyArgumentsElements();
  {
    DisallowHeapAllocation no_gc;
    FixedArrayBase elements = object->elements();

    if (is_sloppy_arguments) {
      elements = SloppyArgumentsElements::cast(elements).arguments();
    }

    if (elements.IsNumberDictionary()) {
      return handle(NumberDictionary::cast(elements), isolate);
    }
  }

  DCHECK(object->HasSmiOrObjectElements() || object->HasDoubleElements() ||
         object->HasFastArgumentsElements() ||
         object->HasFastStringWrapperElements() ||
         object->HasSealedElements() || object->HasNonextensibleElements());

  Handle<NumberDictionary> dictionary =
      object->GetElementsAccessor()->Normalize(object);

  // Switch to using the dictionary as the backing storage for elements.
  ElementsKind target_kind = is_sloppy_arguments
                                 ? SLOW_SLOPPY_ARGUMENTS_ELEMENTS
                                 : object->HasFastStringWrapperElements()
                                       ? SLOW_STRING_WRAPPER_ELEMENTS
                                       : DICTIONARY_ELEMENTS;
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(object, target_kind);
  // Set the new map first to satify the elements type assert in set_elements().
  JSObject::MigrateToMap(isolate, object, new_map);

  if (is_sloppy_arguments) {
    SloppyArgumentsElements::cast(object->elements())
        .set_arguments(*dictionary);
  } else {
    object->set_elements(*dictionary);
  }

  isolate->counters()->elements_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    StdoutStream os;
    os << "Object elements have been normalized:\n";
    object->Print(os);
  }
#endif

  DCHECK(object->HasDictionaryElements() ||
         object->HasSlowArgumentsElements() ||
         object->HasSlowStringWrapperElements());
  return dictionary;
}

Maybe<bool> JSObject::DeletePropertyWithInterceptor(LookupIterator* it,
                                                    ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (interceptor->deleter().IsUndefined(isolate)) return Nothing<bool>();

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Just(should_throw));
  Handle<Object> result;
  if (it->IsElement()) {
    result = args.CallIndexedDeleter(interceptor, it->index());
  } else {
    result = args.CallNamedDeleter(interceptor, it->name());
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.is_null()) return Nothing<bool>();

  DCHECK(result->IsBoolean());
  // Rebox CustomArguments::kReturnValueOffset before returning.
  return Just(result->IsTrue(isolate));
}

Maybe<bool> JSObject::CreateDataProperty(LookupIterator* it,
                                         Handle<Object> value,
                                         Maybe<ShouldThrow> should_throw) {
  DCHECK(it->GetReceiver()->IsJSObject());
  MAYBE_RETURN(JSReceiver::GetPropertyAttributes(it), Nothing<bool>());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (it->IsFound()) {
    Maybe<PropertyAttributes> attributes = GetPropertyAttributes(it);
    MAYBE_RETURN(attributes, Nothing<bool>());
    if ((attributes.FromJust() & DONT_DELETE) != 0) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kRedefineDisallowed, it->GetName()));
    }
  } else {
    if (!JSObject::IsExtensible(Handle<JSObject>::cast(it->GetReceiver()))) {
      RETURN_FAILURE(
          isolate, GetShouldThrow(isolate, should_throw),
          NewTypeError(MessageTemplate::kDefineDisallowed, it->GetName()));
    }
  }

  RETURN_ON_EXCEPTION_VALUE(it->isolate(),
                            DefineOwnPropertyIgnoreAttributes(it, value, NONE),
                            Nothing<bool>());

  return Just(true);
}

namespace {

template <typename Dictionary>
bool TestDictionaryPropertiesIntegrityLevel(Dictionary dict,
                                            ReadOnlyRoots roots,
                                            PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);

  uint32_t capacity = dict.Capacity();
  for (uint32_t i = 0; i < capacity; i++) {
    Object key;
    if (!dict.ToKey(roots, i, &key)) continue;
    if (key.FilterKey(ALL_PROPERTIES)) continue;
    PropertyDetails details = dict.DetailsAt(i);
    if (details.IsConfigurable()) return false;
    if (level == FROZEN && details.kind() == kData && !details.IsReadOnly()) {
      return false;
    }
  }
  return true;
}

bool TestFastPropertiesIntegrityLevel(Map map, PropertyAttributes level) {
  DCHECK(level == SEALED || level == FROZEN);
  DCHECK(!map.IsCustomElementsReceiverMap());
  DCHECK(!map.is_dictionary_map());

  DescriptorArray descriptors = map.instance_descriptors();
  for (InternalIndex i : map.IterateOwnDescriptors()) {
    if (descriptors.GetKey(i).IsPrivate()) continue;
    PropertyDetails details = descriptors.GetDetails(i);
    if (details.IsConfigurable()) return false;
    if (level == FROZEN && details.kind() == kData && !details.IsReadOnly()) {
      return false;
    }
  }
  return true;
}

bool TestPropertiesIntegrityLevel(JSObject object, PropertyAttributes level) {
  DCHECK(!object.map().IsCustomElementsReceiverMap());

  if (object.HasFastProperties()) {
    return TestFastPropertiesIntegrityLevel(object.map(), level);
  }

  return TestDictionaryPropertiesIntegrityLevel(
      object.property_dictionary(), object.GetReadOnlyRoots(), level);
}

bool TestElementsIntegrityLevel(JSObject object, PropertyAttributes level) {
  DCHECK(!object.HasSloppyArgumentsElements());

  ElementsKind kind = object.GetElementsKind();

  if (IsDictionaryElementsKind(kind)) {
    return TestDictionaryPropertiesIntegrityLevel(
        NumberDictionary::cast(object.elements()), object.GetReadOnlyRoots(),
        level);
  }
  if (IsTypedArrayElementsKind(kind)) {
    if (level == FROZEN && JSArrayBufferView::cast(object).byte_length() > 0)
      return false;  // TypedArrays with elements can't be frozen.
    return TestPropertiesIntegrityLevel(object, level);
  }
  if (IsFrozenElementsKind(kind)) return true;
  if (IsSealedElementsKind(kind) && level != FROZEN) return true;
  if (IsNonextensibleElementsKind(kind) && level == NONE) return true;

  ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
  // Only DICTIONARY_ELEMENTS and SLOW_SLOPPY_ARGUMENTS_ELEMENTS have
  // PropertyAttributes so just test if empty
  return accessor->NumberOfElements(object) == 0;
}

bool FastTestIntegrityLevel(JSObject object, PropertyAttributes level) {
  DCHECK(!object.map().IsCustomElementsReceiverMap());

  return !object.map().is_extensible() &&
         TestElementsIntegrityLevel(object, level) &&
         TestPropertiesIntegrityLevel(object, level);
}

}  // namespace

Maybe<bool> JSObject::TestIntegrityLevel(Handle<JSObject> object,
                                         IntegrityLevel level) {
  if (!object->map().IsCustomElementsReceiverMap() &&
      !object->HasSloppyArgumentsElements()) {
    return Just(FastTestIntegrityLevel(*object, level));
  }
  return GenericTestIntegrityLevel(Handle<JSReceiver>::cast(object), level);
}

Maybe<bool> JSObject::PreventExtensions(Handle<JSObject> object,
                                        ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

  if (!object->HasSloppyArgumentsElements()) {
    return PreventExtensionsWithTransition<NONE>(object, should_throw);
  }

  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (!object->map().is_extensible()) return Just(true);

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensions(PrototypeIterator::GetCurrent<JSObject>(iter),
                             should_throw);
  }

  if (object->map().has_named_interceptor() ||
      object->map().has_indexed_interceptor()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kCannotPreventExt));
  }

  if (!object->HasTypedArrayElements()) {
    // If there are fast elements we normalize.
    Handle<NumberDictionary> dictionary = NormalizeElements(object);
    DCHECK(object->HasDictionaryElements() ||
           object->HasSlowArgumentsElements());

    // Make sure that we never go back to fast case.
    if (*dictionary != ReadOnlyRoots(isolate).empty_slow_element_dictionary()) {
      object->RequireSlowElements(*dictionary);
    }
  }

  // Do a map transition, other objects with this map may still
  // be extensible.
  // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
  Handle<Map> new_map =
      Map::Copy(isolate, handle(object->map(), isolate), "PreventExtensions");

  new_map->set_is_extensible(false);
  JSObject::MigrateToMap(isolate, object, new_map);
  DCHECK(!object->map().is_extensible());

  return Just(true);
}

bool JSObject::IsExtensible(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    return true;
  }
  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, *object);
    if (iter.IsAtEnd()) return false;
    DCHECK(iter.GetCurrent().IsJSGlobalObject());
    return iter.GetCurrent<JSObject>().map().is_extensible();
  }
  return object->map().is_extensible();
}

template <typename Dictionary>
void JSObject::ApplyAttributesToDictionary(
    Isolate* isolate, ReadOnlyRoots roots, Handle<Dictionary> dictionary,
    const PropertyAttributes attributes) {
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object k;
    if (!dictionary->ToKey(roots, i, &k)) continue;
    if (k.FilterKey(ALL_PROPERTIES)) continue;
    PropertyDetails details = dictionary->DetailsAt(i);
    int attrs = attributes;
    // READ_ONLY is an invalid attribute for JS setters/getters.
    if ((attributes & READ_ONLY) && details.kind() == kAccessor) {
      Object v = dictionary->ValueAt(i);
      if (v.IsAccessorPair()) attrs &= ~READ_ONLY;
    }
    details = details.CopyAddAttributes(static_cast<PropertyAttributes>(attrs));
    dictionary->DetailsAtPut(isolate, i, details);
  }
}

template void JSObject::ApplyAttributesToDictionary(
    Isolate* isolate, ReadOnlyRoots roots, Handle<NumberDictionary> dictionary,
    const PropertyAttributes attributes);

Handle<NumberDictionary> CreateElementDictionary(Isolate* isolate,
                                                 Handle<JSObject> object) {
  Handle<NumberDictionary> new_element_dictionary;
  if (!object->HasTypedArrayElements() && !object->HasDictionaryElements() &&
      !object->HasSlowStringWrapperElements()) {
    int length = object->IsJSArray()
                     ? Smi::ToInt(Handle<JSArray>::cast(object)->length())
                     : object->elements().length();
    new_element_dictionary =
        length == 0 ? isolate->factory()->empty_slow_element_dictionary()
                    : object->GetElementsAccessor()->Normalize(object);
  }
  return new_element_dictionary;
}

template <PropertyAttributes attrs>
Maybe<bool> JSObject::PreventExtensionsWithTransition(
    Handle<JSObject> object, ShouldThrow should_throw) {
  STATIC_ASSERT(attrs == NONE || attrs == SEALED || attrs == FROZEN);

  // Sealing/freezing sloppy arguments or namespace objects should be handled
  // elsewhere.
  DCHECK(!object->HasSloppyArgumentsElements());
  DCHECK_IMPLIES(object->IsJSModuleNamespace(), attrs == NONE);

  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (attrs == NONE && !object->map().is_extensible()) return Just(true);
  {
    ElementsKind old_elements_kind = object->map().elements_kind();
    if (IsFrozenElementsKind(old_elements_kind)) return Just(true);
    if (attrs != FROZEN && IsSealedElementsKind(old_elements_kind))
      return Just(true);
  }

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensionsWithTransition<attrs>(
        PrototypeIterator::GetCurrent<JSObject>(iter), should_throw);
  }

  if (object->map().has_named_interceptor() ||
      object->map().has_indexed_interceptor()) {
    MessageTemplate message = MessageTemplate::kNone;
    switch (attrs) {
      case NONE:
        message = MessageTemplate::kCannotPreventExt;
        break;

      case SEALED:
        message = MessageTemplate::kCannotSeal;
        break;

      case FROZEN:
        message = MessageTemplate::kCannotFreeze;
        break;
    }
    RETURN_FAILURE(isolate, should_throw, NewTypeError(message));
  }

  Handle<Symbol> transition_marker;
  if (attrs == NONE) {
    transition_marker = isolate->factory()->nonextensible_symbol();
  } else if (attrs == SEALED) {
    transition_marker = isolate->factory()->sealed_symbol();
  } else {
    DCHECK(attrs == FROZEN);
    transition_marker = isolate->factory()->frozen_symbol();
  }

  // Currently, there are only have sealed/frozen Object element kinds and
  // Map::MigrateToMap doesn't handle properties' attributes reconfiguring and
  // elements kind change in one go. If seal or freeze with Smi or Double
  // elements kind, we will transition to Object elements kind first to make
  // sure of valid element access.
  if (FLAG_enable_sealed_frozen_elements_kind) {
    switch (object->map().elements_kind()) {
      case PACKED_SMI_ELEMENTS:
      case PACKED_DOUBLE_ELEMENTS:
        JSObject::TransitionElementsKind(object, PACKED_ELEMENTS);
        break;
      case HOLEY_SMI_ELEMENTS:
      case HOLEY_DOUBLE_ELEMENTS:
        JSObject::TransitionElementsKind(object, HOLEY_ELEMENTS);
        break;
      default:
        break;
    }
  }

  // Make sure we only use this element dictionary in case we can't transition
  // to sealed, frozen elements kind.
  Handle<NumberDictionary> new_element_dictionary;

  Handle<Map> old_map(object->map(), isolate);
  old_map = Map::Update(isolate, old_map);
  TransitionsAccessor transitions(isolate, old_map);
  Map transition = transitions.SearchSpecial(*transition_marker);
  if (!transition.is_null()) {
    Handle<Map> transition_map(transition, isolate);
    DCHECK(transition_map->has_dictionary_elements() ||
           transition_map->has_typed_array_elements() ||
           transition_map->elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS ||
           transition_map->has_any_nonextensible_elements());
    DCHECK(!transition_map->is_extensible());
    if (!transition_map->has_any_nonextensible_elements()) {
      new_element_dictionary = CreateElementDictionary(isolate, object);
    }
    JSObject::MigrateToMap(isolate, object, transition_map);
  } else if (transitions.CanHaveMoreTransitions()) {
    // Create a new descriptor array with the appropriate property attributes
    Handle<Map> new_map = Map::CopyForPreventExtensions(
        isolate, old_map, attrs, transition_marker, "CopyForPreventExtensions");
    if (!new_map->has_any_nonextensible_elements()) {
      new_element_dictionary = CreateElementDictionary(isolate, object);
    }
    JSObject::MigrateToMap(isolate, object, new_map);
  } else {
    DCHECK(old_map->is_dictionary_map() || !old_map->is_prototype_map());
    // Slow path: need to normalize properties for safety
    NormalizeProperties(isolate, object, CLEAR_INOBJECT_PROPERTIES, 0,
                        "SlowPreventExtensions");

    // Create a new map, since other objects with this map may be extensible.
    // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
    Handle<Map> new_map = Map::Copy(isolate, handle(object->map(), isolate),
                                    "SlowCopyForPreventExtensions");
    new_map->set_is_extensible(false);
    new_element_dictionary = CreateElementDictionary(isolate, object);
    if (!new_element_dictionary.is_null()) {
      ElementsKind new_kind =
          IsStringWrapperElementsKind(old_map->elements_kind())
              ? SLOW_STRING_WRAPPER_ELEMENTS
              : DICTIONARY_ELEMENTS;
      new_map->set_elements_kind(new_kind);
    }
    JSObject::MigrateToMap(isolate, object, new_map);

    if (attrs != NONE) {
      ReadOnlyRoots roots(isolate);
      if (object->IsJSGlobalObject()) {
        Handle<GlobalDictionary> dictionary(
            JSGlobalObject::cast(*object).global_dictionary(), isolate);
        JSObject::ApplyAttributesToDictionary(isolate, roots, dictionary,
                                              attrs);
      } else {
        Handle<NameDictionary> dictionary(object->property_dictionary(),
                                          isolate);
        JSObject::ApplyAttributesToDictionary(isolate, roots, dictionary,
                                              attrs);
      }
    }
  }

  if (object->map().has_any_nonextensible_elements()) {
    DCHECK(new_element_dictionary.is_null());
    return Just(true);
  }

  // Both seal and preventExtensions always go through without modifications to
  // typed array elements. Freeze works only if there are no actual elements.
  if (object->HasTypedArrayElements()) {
    if (attrs == FROZEN && JSArrayBufferView::cast(*object).byte_length() > 0) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kCannotFreezeArrayBufferView));
      return Nothing<bool>();
    }
    return Just(true);
  }

  DCHECK(object->map().has_dictionary_elements() ||
         object->map().elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS);
  if (!new_element_dictionary.is_null()) {
    object->set_elements(*new_element_dictionary);
  }

  if (object->elements() !=
      ReadOnlyRoots(isolate).empty_slow_element_dictionary()) {
    Handle<NumberDictionary> dictionary(object->element_dictionary(), isolate);
    // Make sure we never go back to the fast case
    object->RequireSlowElements(*dictionary);
    if (attrs != NONE) {
      JSObject::ApplyAttributesToDictionary(isolate, ReadOnlyRoots(isolate),
                                            dictionary, attrs);
    }
  }

  return Just(true);
}

Handle<Object> JSObject::FastPropertyAt(Handle<JSObject> object,
                                        Representation representation,
                                        FieldIndex index) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsUnboxedDoubleField(index)) {
    DCHECK(representation.IsDouble());
    double value = object->RawFastDoublePropertyAt(index);
    return isolate->factory()->NewHeapNumber(value);
  }
  Handle<Object> raw_value(object->RawFastPropertyAt(index), isolate);
  return Object::WrapForRead(isolate, raw_value, representation);
}

// TODO(cbruni/jkummerow): Consider moving this into elements.cc.
bool JSObject::HasEnumerableElements() {
  // TODO(cbruni): cleanup
  JSObject object = *this;
  switch (object.GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_NONEXTENSIBLE_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS: {
      int length = object.IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object).length())
                       : object.elements().length();
      return length > 0;
    }
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_NONEXTENSIBLE_ELEMENTS:
    case HOLEY_ELEMENTS: {
      FixedArray elements = FixedArray::cast(object.elements());
      int length = object.IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object).length())
                       : elements.length();
      Isolate* isolate = GetIsolate();
      for (int i = 0; i < length; i++) {
        if (!elements.is_the_hole(isolate, i)) return true;
      }
      return false;
    }
    case HOLEY_DOUBLE_ELEMENTS: {
      int length = object.IsJSArray()
                       ? Smi::ToInt(JSArray::cast(object).length())
                       : object.elements().length();
      // Zero-length arrays would use the empty FixedArray...
      if (length == 0) return false;
      // ...so only cast to FixedDoubleArray otherwise.
      FixedDoubleArray elements = FixedDoubleArray::cast(object.elements());
      for (int i = 0; i < length; i++) {
        if (!elements.is_the_hole(i)) return true;
      }
      return false;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        size_t length = JSTypedArray::cast(object).length();
        return length > 0;
      }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary elements = NumberDictionary::cast(object.elements());
      return elements.NumberOfEnumerableProperties() > 0;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      // We're approximating non-empty arguments objects here.
      return true;
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      if (String::cast(JSPrimitiveWrapper::cast(object).value()).length() > 0) {
        return true;
      }
      return object.elements().length() > 0;
    case NO_ELEMENTS:
      return false;
  }
  UNREACHABLE();
}

MaybeHandle<Object> JSObject::DefineAccessor(Handle<JSObject> object,
                                             Handle<Name> name,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return DefineAccessor(&it, getter, setter, attributes);
}

MaybeHandle<Object> JSObject::DefineAccessor(LookupIterator* it,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = it->isolate();

  it->UpdateProtector();

  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (!it->HasAccess()) {
      isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return isolate->factory()->undefined_value();
    }
    it->Next();
  }

  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  // Ignore accessors on typed arrays.
  if (it->IsElement() && object->HasTypedArrayElements()) {
    return it->factory()->undefined_value();
  }

  DCHECK(getter->IsCallable() || getter->IsUndefined(isolate) ||
         getter->IsNull(isolate) || getter->IsFunctionTemplateInfo());
  DCHECK(setter->IsCallable() || setter->IsUndefined(isolate) ||
         setter->IsNull(isolate) || setter->IsFunctionTemplateInfo());
  it->TransitionToAccessorProperty(getter, setter, attributes);

  return isolate->factory()->undefined_value();
}

MaybeHandle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                          Handle<Name> name,
                                          Handle<AccessorInfo> info,
                                          PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);

  // Duplicate ACCESS_CHECK outside of GetPropertyAttributes for the case that
  // the FailedAccessCheckCallbackFunction doesn't throw an exception.
  //
  // TODO(verwaest): Force throw an exception if the callback doesn't, so we can
  // remove reliance on default return values.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return it.factory()->undefined_value();
    }
    it.Next();
  }

  // Ignore accessors on typed arrays.
  if (it.IsElement() && object->HasTypedArrayElements()) {
    return it.factory()->undefined_value();
  }

  CHECK(GetPropertyAttributes(&it).IsJust());

  // ES5 forbids turning a property into an accessor if it's not
  // configurable. See 8.6.1 (Table 5).
  if (it.IsFound() && !it.IsConfigurable()) {
    return it.factory()->undefined_value();
  }

  it.TransitionToAccessorPair(info, attributes);

  return object;
}

Object JSObject::SlowReverseLookup(Object value) {
  if (HasFastProperties()) {
    DescriptorArray descs = map().instance_descriptors();
    bool value_is_number = value.IsNumber();
    for (InternalIndex i : map().IterateOwnDescriptors()) {
      PropertyDetails details = descs.GetDetails(i);
      if (details.location() == kField) {
        DCHECK_EQ(kData, details.kind());
        FieldIndex field_index = FieldIndex::ForDescriptor(map(), i);
        if (IsUnboxedDoubleField(field_index)) {
          if (value_is_number) {
            double property = RawFastDoublePropertyAt(field_index);
            if (property == value.Number()) {
              return descs.GetKey(i);
            }
          }
        } else {
          Object property = RawFastPropertyAt(field_index);
          if (field_index.is_double()) {
            DCHECK(property.IsHeapNumber());
            if (value_is_number && property.Number() == value.Number()) {
              return descs.GetKey(i);
            }
          } else if (property == value) {
            return descs.GetKey(i);
          }
        }
      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          if (descs.GetStrongValue(i) == value) {
            return descs.GetKey(i);
          }
        }
      }
    }
    return GetReadOnlyRoots().undefined_value();
  } else if (IsJSGlobalObject()) {
    return JSGlobalObject::cast(*this).global_dictionary().SlowReverseLookup(
        value);
  } else {
    return property_dictionary().SlowReverseLookup(value);
  }
}

void JSObject::PrototypeRegistryCompactionCallback(HeapObject value,
                                                   int old_index,
                                                   int new_index) {
  DCHECK(value.IsMap() && Map::cast(value).is_prototype_map());
  Map map = Map::cast(value);
  DCHECK(map.prototype_info().IsPrototypeInfo());
  PrototypeInfo proto_info = PrototypeInfo::cast(map.prototype_info());
  DCHECK_EQ(old_index, proto_info.registry_slot());
  proto_info.set_registry_slot(new_index);
}

// static
void JSObject::MakePrototypesFast(Handle<Object> receiver,
                                  WhereToStart where_to_start,
                                  Isolate* isolate) {
  if (!receiver->IsJSReceiver()) return;
  for (PrototypeIterator iter(isolate, Handle<JSReceiver>::cast(receiver),
                              where_to_start);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) return;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    Map current_map = current_obj->map();
    if (current_map.is_prototype_map()) {
      // If the map is already marked as should be fast, we're done. Its
      // prototypes will have been marked already as well.
      if (current_map.should_be_fast_prototype_map()) return;
      Handle<Map> map(current_map, isolate);
      Map::SetShouldBeFastPrototypeMap(map, true, isolate);
      JSObject::OptimizeAsPrototype(current_obj);
    }
  }
}

static bool PrototypeBenefitsFromNormalization(Handle<JSObject> object) {
  DisallowHeapAllocation no_gc;
  if (!object->HasFastProperties()) return false;
  if (object->IsJSGlobalProxy()) return false;
  if (object->GetIsolate()->bootstrapper()->IsActive()) return false;
  return !object->map().is_prototype_map() ||
         !object->map().should_be_fast_prototype_map();
}

// static
void JSObject::OptimizeAsPrototype(Handle<JSObject> object,
                                   bool enable_setup_mode) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsJSGlobalObject()) return;
  if (enable_setup_mode && PrototypeBenefitsFromNormalization(object)) {
    // First normalize to ensure all JSFunctions are DATA_CONSTANT.
    JSObject::NormalizeProperties(isolate, object, KEEP_INOBJECT_PROPERTIES, 0,
                                  "NormalizeAsPrototype");
  }
  if (object->map().is_prototype_map()) {
    if (object->map().should_be_fast_prototype_map() &&
        !object->HasFastProperties()) {
      JSObject::MigrateSlowToFast(object, 0, "OptimizeAsPrototype");
    }
  } else {
    Handle<Map> new_map =
        Map::Copy(isolate, handle(object->map(), isolate), "CopyAsPrototype");
    JSObject::MigrateToMap(isolate, object, new_map);
    object->map().set_is_prototype_map(true);

    // Replace the pointer to the exact constructor with the Object function
    // from the same context if undetectable from JS. This is to avoid keeping
    // memory alive unnecessarily.
    Object maybe_constructor = object->map().GetConstructor();
    if (maybe_constructor.IsJSFunction()) {
      JSFunction constructor = JSFunction::cast(maybe_constructor);
      if (!constructor.shared().IsApiFunction()) {
        Context context = constructor.context().native_context();
        JSFunction object_function = context.object_function();
        object->map().SetConstructor(object_function);
      }
    }
  }
}

// static
void JSObject::ReoptimizeIfPrototype(Handle<JSObject> object) {
  if (!object->map().is_prototype_map()) return;
  if (!object->map().should_be_fast_prototype_map()) return;
  OptimizeAsPrototype(object);
}

// static
void JSObject::LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  // Contract: In line with InvalidatePrototypeChains()'s requirements,
  // leaf maps don't need to register as users, only prototypes do.
  DCHECK(user->is_prototype_map());

  Handle<Map> current_user = user;
  Handle<PrototypeInfo> current_user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  for (PrototypeIterator iter(isolate, user); !iter.IsAtEnd(); iter.Advance()) {
    // Walk up the prototype chain as far as links haven't been registered yet.
    if (current_user_info->registry_slot() != PrototypeInfo::UNREGISTERED) {
      break;
    }
    Handle<Object> maybe_proto = PrototypeIterator::GetCurrent(iter);
    // Proxies on the prototype chain are not supported. They make it
    // impossible to make any assumptions about the prototype chain anyway.
    if (maybe_proto->IsJSProxy()) return;
    Handle<JSObject> proto = Handle<JSObject>::cast(maybe_proto);
    Handle<PrototypeInfo> proto_info =
        Map::GetOrCreatePrototypeInfo(proto, isolate);
    Handle<Object> maybe_registry(proto_info->prototype_users(), isolate);
    Handle<WeakArrayList> registry =
        maybe_registry->IsSmi()
            ? handle(ReadOnlyRoots(isolate->heap()).empty_weak_array_list(),
                     isolate)
            : Handle<WeakArrayList>::cast(maybe_registry);
    int slot = 0;
    Handle<WeakArrayList> new_array =
        PrototypeUsers::Add(isolate, registry, current_user, &slot);
    current_user_info->set_registry_slot(slot);
    if (!maybe_registry.is_identical_to(new_array)) {
      proto_info->set_prototype_users(*new_array);
    }
    if (FLAG_trace_prototype_users) {
      PrintF("Registering %p as a user of prototype %p (map=%p).\n",
             reinterpret_cast<void*>(current_user->ptr()),
             reinterpret_cast<void*>(proto->ptr()),
             reinterpret_cast<void*>(proto->map().ptr()));
    }

    current_user = handle(proto->map(), isolate);
    current_user_info = proto_info;
  }
}

// Can be called regardless of whether |user| was actually registered with
// |prototype|. Returns true when there was a registration.
// static
bool JSObject::UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  DCHECK(user->is_prototype_map());
  // If it doesn't have a PrototypeInfo, it was never registered.
  if (!user->prototype_info().IsPrototypeInfo()) return false;
  // If it had no prototype before, see if it had users that might expect
  // registration.
  if (!user->prototype().IsJSObject()) {
    Object users =
        PrototypeInfo::cast(user->prototype_info()).prototype_users();
    return users.IsWeakArrayList();
  }
  Handle<JSObject> prototype(JSObject::cast(user->prototype()), isolate);
  Handle<PrototypeInfo> user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  int slot = user_info->registry_slot();
  if (slot == PrototypeInfo::UNREGISTERED) return false;
  DCHECK(prototype->map().is_prototype_map());
  Object maybe_proto_info = prototype->map().prototype_info();
  // User knows its registry slot, prototype info and user registry must exist.
  DCHECK(maybe_proto_info.IsPrototypeInfo());
  Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(maybe_proto_info),
                                   isolate);
  Handle<WeakArrayList> prototype_users(
      WeakArrayList::cast(proto_info->prototype_users()), isolate);
  DCHECK_EQ(prototype_users->Get(slot), HeapObjectReference::Weak(*user));
  PrototypeUsers::MarkSlotEmpty(*prototype_users, slot);
  if (FLAG_trace_prototype_users) {
    PrintF("Unregistering %p as a user of prototype %p.\n",
           reinterpret_cast<void*>(user->ptr()),
           reinterpret_cast<void*>(prototype->ptr()));
  }
  return true;
}

namespace {

// This function must be kept in sync with
// AccessorAssembler::InvalidateValidityCellIfPrototype() which does pre-checks
// before jumping here.
void InvalidateOnePrototypeValidityCellInternal(Map map) {
  DCHECK(map.is_prototype_map());
  if (FLAG_trace_prototype_users) {
    PrintF("Invalidating prototype map %p 's cell\n",
           reinterpret_cast<void*>(map.ptr()));
  }
  Object maybe_cell = map.prototype_validity_cell();
  if (maybe_cell.IsCell()) {
    // Just set the value; the cell will be replaced lazily.
    Cell cell = Cell::cast(maybe_cell);
    cell.set_value(Smi::FromInt(Map::kPrototypeChainInvalid));
  }
}

void InvalidatePrototypeChainsInternal(Map map) {
  InvalidateOnePrototypeValidityCellInternal(map);

  Object maybe_proto_info = map.prototype_info();
  if (!maybe_proto_info.IsPrototypeInfo()) return;
  PrototypeInfo proto_info = PrototypeInfo::cast(maybe_proto_info);
  if (!proto_info.prototype_users().IsWeakArrayList()) {
    return;
  }
  WeakArrayList prototype_users =
      WeakArrayList::cast(proto_info.prototype_users());
  // For now, only maps register themselves as users.
  for (int i = PrototypeUsers::kFirstIndex; i < prototype_users.length(); ++i) {
    HeapObject heap_object;
    if (prototype_users.Get(i)->GetHeapObjectIfWeak(&heap_object) &&
        heap_object.IsMap()) {
      // Walk the prototype chain (backwards, towards leaf objects) if
      // necessary.
      InvalidatePrototypeChainsInternal(Map::cast(heap_object));
    }
  }
}

}  // namespace

// static
Map JSObject::InvalidatePrototypeChains(Map map) {
  DisallowHeapAllocation no_gc;
  InvalidatePrototypeChainsInternal(map);
  return map;
}

// We also invalidate global objects validity cell when a new lexical
// environment variable is added. This is necessary to ensure that
// Load/StoreGlobalIC handlers that load/store from global object's prototype
// get properly invalidated.
// Note, that the normal Load/StoreICs that load/store through the global object
// in the prototype chain are not affected by appearance of a new lexical
// variable and therefore we don't propagate invalidation down.
// static
void JSObject::InvalidatePrototypeValidityCell(JSGlobalObject global) {
  DisallowHeapAllocation no_gc;
  InvalidateOnePrototypeValidityCellInternal(global.map());
}

Maybe<bool> JSObject::SetPrototype(Handle<JSObject> object,
                                   Handle<Object> value, bool from_javascript,
                                   ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

#ifdef DEBUG
  int size = object->Size();
#endif

  if (from_javascript) {
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayAccess(handle(isolate->context(), isolate), object)) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kNoAccess));
    }
  } else {
    DCHECK(!object->IsAccessCheckNeeded());
  }

  // Silently ignore the change if value is not a JSObject or null.
  // SpiderMonkey behaves this way.
  if (!value->IsJSReceiver() && !value->IsNull(isolate)) return Just(true);

  bool all_extensible = object->map().is_extensible();
  Handle<JSObject> real_receiver = object;
  if (from_javascript) {
    // Find the first object in the chain whose prototype object is not
    // hidden.
    PrototypeIterator iter(isolate, real_receiver, kStartAtPrototype,
                           PrototypeIterator::END_AT_NON_HIDDEN);
    while (!iter.IsAtEnd()) {
      // Casting to JSObject is fine because hidden prototypes are never
      // JSProxies.
      real_receiver = PrototypeIterator::GetCurrent<JSObject>(iter);
      iter.Advance();
      all_extensible = all_extensible && real_receiver->map().is_extensible();
    }
  }
  Handle<Map> map(real_receiver->map(), isolate);

  // Nothing to do if prototype is already set.
  if (map->prototype() == *value) return Just(true);

  bool immutable_proto = map->is_immutable_proto();
  if (immutable_proto) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kImmutablePrototypeSet, object));
  }

  // From 8.6.2 Object Internal Methods
  // ...
  // In addition, if [[Extensible]] is false the value of the [[Class]] and
  // [[Prototype]] internal properties of the object may not be modified.
  // ...
  // Implementation specific extensions that modify [[Class]], [[Prototype]]
  // or [[Extensible]] must not violate the invariants defined in the preceding
  // paragraph.
  if (!all_extensible) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNonExtensibleProto, object));
  }

  // Before we can set the prototype we need to be sure prototype cycles are
  // prevented.  It is sufficient to validate that the receiver is not in the
  // new prototype chain.
  if (value->IsJSReceiver()) {
    for (PrototypeIterator iter(isolate, JSReceiver::cast(*value),
                                kStartAtReceiver);
         !iter.IsAtEnd(); iter.Advance()) {
      if (iter.GetCurrent<JSReceiver>() == *object) {
        // Cycle detected.
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kCyclicProto));
      }
    }
  }

  // Set the new prototype of the object.

  isolate->UpdateNoElementsProtectorOnSetPrototype(real_receiver);

  Handle<Map> new_map =
      Map::TransitionToPrototype(isolate, map, Handle<HeapObject>::cast(value));
  DCHECK(new_map->prototype() == *value);
  JSObject::MigrateToMap(isolate, real_receiver, new_map);

  DCHECK(size == object->Size());
  return Just(true);
}

// static
void JSObject::SetImmutableProto(Handle<JSObject> object) {
  DCHECK(!object->IsAccessCheckNeeded());  // Never called from JS
  Handle<Map> map(object->map(), object->GetIsolate());

  // Nothing to do if prototype is already set.
  if (map->is_immutable_proto()) return;

  Handle<Map> new_map =
      Map::TransitionToImmutableProto(object->GetIsolate(), map);
  object->synchronized_set_map(*new_map);
}

void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Arguments* args, uint32_t first_arg,
                                        uint32_t arg_count,
                                        EnsureElementsMode mode) {
  // Elements in |Arguments| are ordered backwards (because they're on the
  // stack), but the method that's called here iterates over them in forward
  // direction.
  return EnsureCanContainElements(
      object, args->slot_at(first_arg + arg_count - 1), arg_count, mode);
}

void JSObject::ValidateElements(JSObject object) {
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    object.GetElementsAccessor()->Validate(object);
  }
#endif
}

bool JSObject::WouldConvertToSlowElements(uint32_t index) {
  if (!HasFastElements()) return false;
  uint32_t capacity = static_cast<uint32_t>(elements().length());
  uint32_t new_capacity;
  return ShouldConvertToSlowElements(*this, capacity, index, &new_capacity);
}

static bool ShouldConvertToFastElements(JSObject object,
                                        NumberDictionary dictionary,
                                        uint32_t index,
                                        uint32_t* new_capacity) {
  // If properties with non-standard attributes or accessors were added, we
  // cannot go back to fast elements.
  if (dictionary.requires_slow_elements()) return false;

  // Adding a property with this index will require slow elements.
  if (index >= static_cast<uint32_t>(Smi::kMaxValue)) return false;

  if (object.IsJSArray()) {
    Object length = JSArray::cast(object).length();
    if (!length.IsSmi()) return false;
    *new_capacity = static_cast<uint32_t>(Smi::ToInt(length));
  } else if (object.IsJSSloppyArgumentsObject()) {
    return false;
  } else {
    *new_capacity = dictionary.max_number_key() + 1;
  }
  *new_capacity = Max(index + 1, *new_capacity);

  uint32_t dictionary_size = static_cast<uint32_t>(dictionary.Capacity()) *
                             NumberDictionary::kEntrySize;

  // Turn fast if the dictionary only saves 50% space.
  return 2 * dictionary_size >= *new_capacity;
}

static ElementsKind BestFittingFastElementsKind(JSObject object) {
  if (!object.map().CanHaveFastTransitionableElementsKind()) {
    return HOLEY_ELEMENTS;
  }
  if (object.HasSloppyArgumentsElements()) {
    return FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  }
  if (object.HasStringWrapperElements()) {
    return FAST_STRING_WRAPPER_ELEMENTS;
  }
  DCHECK(object.HasDictionaryElements());
  NumberDictionary dictionary = object.element_dictionary();
  ElementsKind kind = HOLEY_SMI_ELEMENTS;
  for (int i = 0; i < dictionary.Capacity(); i++) {
    Object key = dictionary.KeyAt(i);
    if (key.IsNumber()) {
      Object value = dictionary.ValueAt(i);
      if (!value.IsNumber()) return HOLEY_ELEMENTS;
      if (!value.IsSmi()) {
        if (!FLAG_unbox_double_arrays) return HOLEY_ELEMENTS;
        kind = HOLEY_DOUBLE_ELEMENTS;
      }
    }
  }
  return kind;
}

// static
void JSObject::AddDataElement(Handle<JSObject> object, uint32_t index,
                              Handle<Object> value,
                              PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  DCHECK(object->map(isolate).is_extensible());

  uint32_t old_length = 0;
  uint32_t new_capacity = 0;

  if (object->IsJSArray(isolate)) {
    CHECK(JSArray::cast(*object).length().ToArrayLength(&old_length));
  }

  ElementsKind kind = object->GetElementsKind(isolate);
  FixedArrayBase elements = object->elements(isolate);
  ElementsKind dictionary_kind = DICTIONARY_ELEMENTS;
  if (IsSloppyArgumentsElementsKind(kind)) {
    elements = SloppyArgumentsElements::cast(elements).arguments(isolate);
    dictionary_kind = SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
  } else if (IsStringWrapperElementsKind(kind)) {
    dictionary_kind = SLOW_STRING_WRAPPER_ELEMENTS;
  }

  if (attributes != NONE) {
    kind = dictionary_kind;
  } else if (elements.IsNumberDictionary(isolate)) {
    kind = ShouldConvertToFastElements(
               *object, NumberDictionary::cast(elements), index, &new_capacity)
               ? BestFittingFastElementsKind(*object)
               : dictionary_kind;
  } else if (ShouldConvertToSlowElements(
                 *object, static_cast<uint32_t>(elements.length()), index,
                 &new_capacity)) {
    kind = dictionary_kind;
  }

  ElementsKind to = value->OptimalElementsKind(isolate);
  if (IsHoleyElementsKind(kind) || !object->IsJSArray(isolate) ||
      index > old_length) {
    to = GetHoleyElementsKind(to);
    kind = GetHoleyElementsKind(kind);
  }
  to = GetMoreGeneralElementsKind(kind, to);
  ElementsAccessor* accessor = ElementsAccessor::ForKind(to);
  accessor->Add(object, index, value, attributes, new_capacity);

  if (object->IsJSArray(isolate) && index >= old_length) {
    Handle<Object> new_length =
        isolate->factory()->NewNumberFromUint(index + 1);
    JSArray::cast(*object).set_length(*new_length);
  }
}

template <AllocationSiteUpdateMode update_or_check>
bool JSObject::UpdateAllocationSite(Handle<JSObject> object,
                                    ElementsKind to_kind) {
  if (!object->IsJSArray()) return false;

  if (!Heap::InYoungGeneration(*object)) return false;

  if (Heap::IsLargeObject(*object)) return false;

  Handle<AllocationSite> site;
  {
    DisallowHeapAllocation no_allocation;

    Heap* heap = object->GetHeap();
    AllocationMemento memento =
        heap->FindAllocationMemento<Heap::kForRuntime>(object->map(), *object);
    if (memento.is_null()) return false;

    // Walk through to the Allocation Site
    site = handle(memento.GetAllocationSite(), heap->isolate());
  }
  return AllocationSite::DigestTransitionFeedback<update_or_check>(site,
                                                                   to_kind);
}

template bool
JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kCheckOnly>(
    Handle<JSObject> object, ElementsKind to_kind);

template bool JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kUpdate>(
    Handle<JSObject> object, ElementsKind to_kind);

void JSObject::TransitionElementsKind(Handle<JSObject> object,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = object->GetElementsKind();

  if (IsHoleyElementsKind(from_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
  }

  if (from_kind == to_kind) return;

  // This method should never be called for any other case.
  DCHECK(IsFastElementsKind(from_kind) ||
         IsNonextensibleElementsKind(from_kind));
  DCHECK(IsFastElementsKind(to_kind) || IsNonextensibleElementsKind(to_kind));
  DCHECK_NE(TERMINAL_FAST_ELEMENTS_KIND, from_kind);

  UpdateAllocationSite(object, to_kind);
  Isolate* isolate = object->GetIsolate();
  if (object->elements() == ReadOnlyRoots(isolate).empty_fixed_array() ||
      IsDoubleElementsKind(from_kind) == IsDoubleElementsKind(to_kind)) {
    // No change is needed to the elements() buffer, the transition
    // only requires a map change.
    Handle<Map> new_map = GetElementsTransitionMap(object, to_kind);
    JSObject::MigrateToMap(isolate, object, new_map);
    if (FLAG_trace_elements_transitions) {
      Handle<FixedArrayBase> elms(object->elements(), isolate);
      PrintElementsTransition(stdout, object, from_kind, elms, to_kind, elms);
    }
  } else {
    DCHECK((IsSmiElementsKind(from_kind) && IsDoubleElementsKind(to_kind)) ||
           (IsDoubleElementsKind(from_kind) && IsObjectElementsKind(to_kind)));
    uint32_t c = static_cast<uint32_t>(object->elements().length());
    ElementsAccessor::ForKind(to_kind)->GrowCapacityAndConvert(object, c);
  }
}

template <typename BackingStore>
static int HoleyElementsUsage(JSObject object, BackingStore store) {
  Isolate* isolate = object.GetIsolate();
  int limit = object.IsJSArray() ? Smi::ToInt(JSArray::cast(object).length())
                                 : store.length();
  int used = 0;
  for (int i = 0; i < limit; ++i) {
    if (!store.is_the_hole(isolate, i)) ++used;
  }
  return used;
}

int JSObject::GetFastElementsUsage() {
  FixedArrayBase store = elements();
  switch (GetElementsKind()) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_NONEXTENSIBLE_ELEMENTS:
      return IsJSArray() ? Smi::ToInt(JSArray::cast(*this).length())
                         : store.length();
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      store = SloppyArgumentsElements::cast(store).arguments();
      V8_FALLTHROUGH;
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_NONEXTENSIBLE_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
      return HoleyElementsUsage(*this, FixedArray::cast(store));
    case HOLEY_DOUBLE_ELEMENTS:
      if (elements().length() == 0) return 0;
      return HoleyElementsUsage(*this, FixedDoubleArray::cast(store));

    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NO_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      UNREACHABLE();
  }
  return 0;
}

MaybeHandle<Object> JSObject::GetPropertyWithInterceptor(LookupIterator* it,
                                                         bool* done) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return GetPropertyWithInterceptorInternal(it, it->GetInterceptor(), done);
}

Maybe<bool> JSObject::HasRealNamedProperty(Handle<JSObject> object,
                                           Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}

Maybe<bool> JSObject::HasRealElementProperty(Handle<JSObject> object,
                                             uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}

Maybe<bool> JSObject::HasRealNamedCallbackProperty(Handle<JSObject> object,
                                                   Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      object->GetIsolate(), object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe_result = GetPropertyAttributes(&it);
  return maybe_result.IsJust() ? Just(it.state() == LookupIterator::ACCESSOR)
                               : Nothing<bool>();
}

bool JSObject::IsApiWrapper() {
  // These object types can carry information relevant for embedders. The
  // *_API_* types are generated through templates which can have embedder
  // fields. The other types have their embedder fields added at compile time.
  auto instance_type = map().instance_type();
  return instance_type == JS_API_OBJECT_TYPE ||
         instance_type == JS_ARRAY_BUFFER_TYPE ||
         instance_type == JS_DATA_VIEW_TYPE ||
         instance_type == JS_GLOBAL_OBJECT_TYPE ||
         instance_type == JS_GLOBAL_PROXY_TYPE ||
         instance_type == JS_SPECIAL_API_OBJECT_TYPE ||
         instance_type == JS_TYPED_ARRAY_TYPE;
}

bool JSObject::IsDroppableApiWrapper() {
  auto instance_type = map().instance_type();
  return instance_type == JS_API_OBJECT_TYPE ||
         instance_type == JS_SPECIAL_API_OBJECT_TYPE;
}

// static
MaybeHandle<NativeContext> JSBoundFunction::GetFunctionRealm(
    Handle<JSBoundFunction> function) {
  DCHECK(function->map().is_constructor());
  return JSReceiver::GetFunctionRealm(
      handle(function->bound_target_function(), function->GetIsolate()));
}

// static
MaybeHandle<String> JSBoundFunction::GetName(Isolate* isolate,
                                             Handle<JSBoundFunction> function) {
  Handle<String> prefix = isolate->factory()->bound__string();
  Handle<String> target_name = prefix;
  Factory* factory = isolate->factory();
  // Concatenate the "bound " up to the last non-bound target.
  while (function->bound_target_function().IsJSBoundFunction()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, target_name,
                               factory->NewConsString(prefix, target_name),
                               String);
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
  }
  if (function->bound_target_function().IsJSFunction()) {
    Handle<JSFunction> target(
        JSFunction::cast(function->bound_target_function()), isolate);
    Handle<Object> name = JSFunction::GetName(isolate, target);
    if (!name->IsString()) return target_name;
    return factory->NewConsString(target_name, Handle<String>::cast(name));
  }
  // This will omit the proper target name for bound JSProxies.
  return target_name;
}

// static
Maybe<int> JSBoundFunction::GetLength(Isolate* isolate,
                                      Handle<JSBoundFunction> function) {
  int nof_bound_arguments = function->bound_arguments().length();
  while (function->bound_target_function().IsJSBoundFunction()) {
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
    // Make sure we never overflow {nof_bound_arguments}, the number of
    // arguments of a function is strictly limited by the max length of an
    // JSAarray, Smi::kMaxValue is thus a reasonably good overestimate.
    int length = function->bound_arguments().length();
    if (V8_LIKELY(Smi::kMaxValue - nof_bound_arguments > length)) {
      nof_bound_arguments += length;
    } else {
      nof_bound_arguments = Smi::kMaxValue;
    }
  }
  // All non JSFunction targets get a direct property and don't use this
  // accessor.
  Handle<JSFunction> target(JSFunction::cast(function->bound_target_function()),
                            isolate);
  int target_length = target->length();

  int length = Max(0, target_length - nof_bound_arguments);
  return Just(length);
}

// static
Handle<String> JSBoundFunction::ToString(Handle<JSBoundFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  return isolate->factory()->function_native_code_string();
}

// static
Handle<Object> JSFunction::GetName(Isolate* isolate,
                                   Handle<JSFunction> function) {
  if (function->shared().name_should_print_as_anonymous()) {
    return isolate->factory()->anonymous_string();
  }
  return handle(function->shared().Name(), isolate);
}

// static
Handle<NativeContext> JSFunction::GetFunctionRealm(
    Handle<JSFunction> function) {
  DCHECK(function->map().is_constructor());
  return handle(function->context().native_context(), function->GetIsolate());
}

void JSFunction::MarkForOptimization(ConcurrencyMode mode) {
  Isolate* isolate = GetIsolate();
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    mode = ConcurrencyMode::kNotConcurrent;
  }

  DCHECK(!is_compiled() || IsInterpreted());
  DCHECK(shared().IsInterpreted());
  DCHECK(!IsOptimized());
  DCHECK(!HasOptimizedCode());
  DCHECK(shared().allows_lazy_compilation() ||
         !shared().optimization_disabled());

  if (mode == ConcurrencyMode::kConcurrent) {
    if (IsInOptimizationQueue()) {
      if (FLAG_trace_concurrent_recompilation) {
        PrintF("  ** Not marking ");
        ShortPrint();
        PrintF(" -- already in optimization queue.\n");
      }
      return;
    }
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Marking ");
      ShortPrint();
      PrintF(" for concurrent recompilation.\n");
    }
  }

  SetOptimizationMarker(mode == ConcurrencyMode::kConcurrent
                            ? OptimizationMarker::kCompileOptimizedConcurrent
                            : OptimizationMarker::kCompileOptimized);
}

// static
void JSFunction::EnsureClosureFeedbackCellArray(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  DCHECK(function->shared().is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
  if (function->has_closure_feedback_cell_array() ||
      function->has_feedback_vector()) {
    return;
  }
  if (function->shared().HasAsmWasmData()) return;

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  DCHECK(function->shared().HasBytecodeArray());
  Handle<HeapObject> feedback_cell_array =
      ClosureFeedbackCellArray::New(isolate, shared);
  // Many closure cell is used as a way to specify that there is no
  // feedback cell for this function and a new feedback cell has to be
  // allocated for this funciton. For ex: for eval functions, we have to create
  // a feedback cell and cache it along with the code. It is safe to use
  // many_closure_cell to indicate this because in regular cases, it should
  // already have a feedback_vector / feedback cell array allocated.
  if (function->raw_feedback_cell() == isolate->heap()->many_closures_cell()) {
    Handle<FeedbackCell> feedback_cell =
        isolate->factory()->NewOneClosureCell(feedback_cell_array);
    function->set_raw_feedback_cell(*feedback_cell);
  } else {
    function->raw_feedback_cell().set_value(*feedback_cell_array);
  }
}

// static
void JSFunction::EnsureFeedbackVector(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  DCHECK(function->shared().is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
  if (function->has_feedback_vector()) return;
  if (function->shared().HasAsmWasmData()) return;

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  DCHECK(function->shared().HasBytecodeArray());

  EnsureClosureFeedbackCellArray(function);
  Handle<ClosureFeedbackCellArray> closure_feedback_cell_array =
      handle(function->closure_feedback_cell_array(), isolate);
  Handle<HeapObject> feedback_vector =
      FeedbackVector::New(isolate, shared, closure_feedback_cell_array);
  // EnsureClosureFeedbackCellArray should handle the special case where we need
  // to allocate a new feedback cell. Please look at comment in that function
  // for more details.
  DCHECK(function->raw_feedback_cell() !=
         isolate->heap()->many_closures_cell());
  function->raw_feedback_cell().set_value(*feedback_vector);
}

// static
void JSFunction::InitializeFeedbackCell(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();

  if (function->has_feedback_vector()) {
    CHECK_EQ(function->feedback_vector().length(),
             function->feedback_vector().metadata().slot_count());
    return;
  }

  bool needs_feedback_vector = !FLAG_lazy_feedback_allocation;
  // We need feedback vector for certain log events, collecting type profile
  // and more precise code coverage.
  if (FLAG_log_function_events) needs_feedback_vector = true;
  if (!isolate->is_best_effort_code_coverage()) needs_feedback_vector = true;
  if (isolate->is_collecting_type_profile()) needs_feedback_vector = true;
  if (FLAG_always_opt) needs_feedback_vector = true;

  if (needs_feedback_vector) {
    EnsureFeedbackVector(function);
  } else {
    EnsureClosureFeedbackCellArray(function);
  }
}

namespace {

void SetInstancePrototype(Isolate* isolate, Handle<JSFunction> function,
                          Handle<JSReceiver> value) {
  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    function->CompleteInobjectSlackTrackingIfActive();

    Handle<Map> initial_map(function->initial_map(), isolate);

    if (!isolate->bootstrapper()->IsActive() &&
        initial_map->instance_type() == JS_OBJECT_TYPE) {
      // Put the value in the initial map field until an initial map is needed.
      // At that point, a new initial map is created and the prototype is put
      // into the initial map where it belongs.
      function->set_prototype_or_initial_map(*value);
    } else {
      Handle<Map> new_map =
          Map::Copy(isolate, initial_map, "SetInstancePrototype");
      JSFunction::SetInitialMap(function, new_map, value);

      // If the function is used as the global Array function, cache the
      // updated initial maps (and transitioned versions) in the native context.
      Handle<Context> native_context(function->context().native_context(),
                                     isolate);
      Handle<Object> array_function(
          native_context->get(Context::ARRAY_FUNCTION_INDEX), isolate);
      if (array_function->IsJSFunction() &&
          *function == JSFunction::cast(*array_function)) {
        CacheInitialJSArrayMaps(isolate, native_context, new_map);
      }
    }

    // Deoptimize all code that embeds the previous initial map.
    initial_map->dependent_code().DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kInitialMapChangedGroup);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value);
    if (value->IsJSObject()) {
      // Optimize as prototype to detach it from its transition tree.
      JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value));
    }
  }
}

}  // anonymous namespace

void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->IsConstructor() ||
         IsGeneratorFunction(function->shared().kind()));
  Isolate* isolate = function->GetIsolate();
  Handle<JSReceiver> construct_prototype;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Handle<Map> new_map =
        Map::Copy(isolate, handle(function->map(), isolate), "SetPrototype");

    JSObject::MigrateToMap(isolate, function, new_map);
    new_map->SetConstructor(*value);
    new_map->set_has_non_instance_prototype(true);

    FunctionKind kind = function->shared().kind();
    Handle<Context> native_context(function->context().native_context(),
                                   isolate);

    construct_prototype = Handle<JSReceiver>(
        IsGeneratorFunction(kind)
            ? IsAsyncFunction(kind)
                  ? native_context->initial_async_generator_prototype()
                  : native_context->initial_generator_prototype()
            : native_context->initial_object_prototype(),
        isolate);
  } else {
    construct_prototype = Handle<JSReceiver>::cast(value);
    function->map().set_has_non_instance_prototype(false);
  }

  SetInstancePrototype(isolate, function, construct_prototype);
}

void JSFunction::SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                               Handle<HeapObject> prototype) {
  if (map->prototype() != *prototype)
    Map::SetPrototype(function->GetIsolate(), map, prototype);
  function->set_prototype_or_initial_map(*map);
  map->SetConstructor(*function);
  if (FLAG_trace_maps) {
    LOG(function->GetIsolate(), MapEvent("InitialMap", Map(), *map, "",
                                         function->shared().DebugName()));
  }
}

void JSFunction::EnsureHasInitialMap(Handle<JSFunction> function) {
  DCHECK(function->has_prototype_slot());
  DCHECK(function->IsConstructor() ||
         IsResumableFunction(function->shared().kind()));
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  InstanceType instance_type;
  if (IsResumableFunction(function->shared().kind())) {
    instance_type = IsAsyncGeneratorFunction(function->shared().kind())
                        ? JS_ASYNC_GENERATOR_OBJECT_TYPE
                        : JS_GENERATOR_OBJECT_TYPE;
  } else {
    instance_type = JS_OBJECT_TYPE;
  }

  int instance_size;
  int inobject_properties;
  int expected_nof_properties =
      CalculateExpectedNofProperties(isolate, function);
  CalculateInstanceSizeHelper(instance_type, false, 0, expected_nof_properties,
                              &instance_size, &inobject_properties);

  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size,
                                               TERMINAL_FAST_ELEMENTS_KIND,
                                               inobject_properties);

  // Fetch or allocate prototype.
  Handle<HeapObject> prototype;
  if (function->has_instance_prototype()) {
    prototype = handle(function->instance_prototype(), isolate);
  } else {
    prototype = isolate->factory()->NewFunctionPrototype(function);
  }
  DCHECK(map->has_fast_object_elements());

  // Finally link initial map and constructor function.
  DCHECK(prototype->IsJSReceiver());
  JSFunction::SetInitialMap(function, map, prototype);
  map->StartInobjectSlackTracking();
}

#ifdef DEBUG
namespace {

bool CanSubclassHaveInobjectProperties(InstanceType instance_type) {
  switch (instance_type) {
    case JS_API_OBJECT_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_DATE_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_COLLATOR_TYPE:
    case JS_DATE_TIME_FORMAT_TYPE:
    case JS_LIST_FORMAT_TYPE:
    case JS_LOCALE_TYPE:
    case JS_NUMBER_FORMAT_TYPE:
    case JS_PLURAL_RULES_TYPE:
    case JS_RELATIVE_TIME_FORMAT_TYPE:
    case JS_SEGMENT_ITERATOR_TYPE:
    case JS_SEGMENTER_TYPE:
    case JS_V8_BREAK_ITERATOR_TYPE:
#endif
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MAP_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_FINALIZATION_GROUP_TYPE:
    case JS_ARGUMENTS_OBJECT_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REG_EXP_TYPE:
    case JS_SET_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_PRIMITIVE_WRAPPER_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_REF_TYPE:
    case JS_WEAK_SET_TYPE:
    case WASM_GLOBAL_OBJECT_TYPE:
    case WASM_INSTANCE_OBJECT_TYPE:
    case WASM_MEMORY_OBJECT_TYPE:
    case WASM_MODULE_OBJECT_TYPE:
    case WASM_TABLE_OBJECT_TYPE:
      return true;

    case BIGINT_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case CELL_TYPE:
    case CODE_TYPE:
    case FILLER_TYPE:
    case FIXED_ARRAY_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case FEEDBACK_METADATA_TYPE:
    case FOREIGN_TYPE:
    case FREE_SPACE_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case ORDERED_NAME_DICTIONARY_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case HEAP_NUMBER_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_PROXY_TYPE:
    case MAP_TYPE:
    case ODDBALL_TYPE:
    case PROPERTY_CELL_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case SYMBOL_TYPE:
    case ALLOCATION_SITE_TYPE:

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:
#undef TYPED_ARRAY_CASE

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      // We must not end up here for these instance types at all.
      UNREACHABLE();
    // Fall through.
    default:
      return false;
  }
}

}  // namespace
#endif

namespace {

bool FastInitializeDerivedMap(Isolate* isolate, Handle<JSFunction> new_target,
                              Handle<JSFunction> constructor,
                              Handle<Map> constructor_initial_map) {
  // Use the default intrinsic prototype instead.
  if (!new_target->has_prototype_slot()) return false;
  // Check that |function|'s initial map still in sync with the |constructor|,
  // otherwise we must create a new initial map for |function|.
  if (new_target->has_initial_map() &&
      new_target->initial_map().GetConstructor() == *constructor) {
    DCHECK(new_target->instance_prototype().IsJSReceiver());
    return true;
  }
  InstanceType instance_type = constructor_initial_map->instance_type();
  DCHECK(CanSubclassHaveInobjectProperties(instance_type));
  // Create a new map with the size and number of in-object properties
  // suggested by |function|.

  // Link initial map and constructor function if the new.target is actually a
  // subclass constructor.
  if (!IsDerivedConstructor(new_target->shared().kind())) return false;

  int instance_size;
  int in_object_properties;
  int embedder_fields =
      JSObject::GetEmbedderFieldCount(*constructor_initial_map);
  int expected_nof_properties =
      JSFunction::CalculateExpectedNofProperties(isolate, new_target);
  JSFunction::CalculateInstanceSizeHelper(
      instance_type, true, embedder_fields, expected_nof_properties,
      &instance_size, &in_object_properties);

  int pre_allocated = constructor_initial_map->GetInObjectProperties() -
                      constructor_initial_map->UnusedPropertyFields();
  CHECK_LE(constructor_initial_map->UsedInstanceSize(), instance_size);
  int unused_property_fields = in_object_properties - pre_allocated;
  Handle<Map> map =
      Map::CopyInitialMap(isolate, constructor_initial_map, instance_size,
                          in_object_properties, unused_property_fields);
  map->set_new_target_is_base(false);
  Handle<HeapObject> prototype(new_target->instance_prototype(), isolate);
  JSFunction::SetInitialMap(new_target, map, prototype);
  DCHECK(new_target->instance_prototype().IsJSReceiver());
  map->SetConstructor(*constructor);
  map->set_construction_counter(Map::kNoSlackTracking);
  map->StartInobjectSlackTracking();
  return true;
}

}  // namespace

// static
MaybeHandle<Map> JSFunction::GetDerivedMap(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<JSReceiver> new_target) {
  EnsureHasInitialMap(constructor);

  Handle<Map> constructor_initial_map(constructor->initial_map(), isolate);
  if (*new_target == *constructor) return constructor_initial_map;

  Handle<Map> result_map;
  // Fast case, new.target is a subclass of constructor. The map is cacheable
  // (and may already have been cached). new.target.prototype is guaranteed to
  // be a JSReceiver.
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (FastInitializeDerivedMap(isolate, function, constructor,
                                 constructor_initial_map)) {
      return handle(function->initial_map(), isolate);
    }
  }

  // Slow path, new.target is either a proxy or can't cache the map.
  // new.target.prototype is not guaranteed to be a JSReceiver, and may need to
  // fall back to the intrinsicDefaultProto.
  Handle<Object> prototype;
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (function->has_prototype_slot()) {
      // Make sure the new.target.prototype is cached.
      EnsureHasInitialMap(function);
      prototype = handle(function->prototype(), isolate);
    } else {
      // No prototype property, use the intrinsict default proto further down.
      prototype = isolate->factory()->undefined_value();
    }
  } else {
    Handle<String> prototype_string = isolate->factory()->prototype_string();
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, prototype,
        JSReceiver::GetProperty(isolate, new_target, prototype_string), Map);
    // The above prototype lookup might change the constructor and its
    // prototype, hence we have to reload the initial map.
    EnsureHasInitialMap(constructor);
    constructor_initial_map = handle(constructor->initial_map(), isolate);
  }

  // If prototype is not a JSReceiver, fetch the intrinsicDefaultProto from the
  // correct realm. Rather than directly fetching the .prototype, we fetch the
  // constructor that points to the .prototype. This relies on
  // constructor.prototype being FROZEN for those constructors.
  if (!prototype->IsJSReceiver()) {
    Handle<Context> context;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, context,
                               JSReceiver::GetFunctionRealm(new_target), Map);
    DCHECK(context->IsNativeContext());
    Handle<Object> maybe_index = JSReceiver::GetDataProperty(
        constructor, isolate->factory()->native_context_index_symbol());
    int index = maybe_index->IsSmi() ? Smi::ToInt(*maybe_index)
                                     : Context::OBJECT_FUNCTION_INDEX;
    Handle<JSFunction> realm_constructor(JSFunction::cast(context->get(index)),
                                         isolate);
    prototype = handle(realm_constructor->prototype(), isolate);
  }

  Handle<Map> map = Map::CopyInitialMap(isolate, constructor_initial_map);
  map->set_new_target_is_base(false);
  CHECK(prototype->IsJSReceiver());
  if (map->prototype() != *prototype)
    Map::SetPrototype(isolate, map, Handle<HeapObject>::cast(prototype));
  map->SetConstructor(*constructor);
  return map;
}

int JSFunction::ComputeInstanceSizeWithMinSlack(Isolate* isolate) {
  CHECK(has_initial_map());
  if (initial_map().IsInobjectSlackTrackingInProgress()) {
    int slack = initial_map().ComputeMinObjectSlack(isolate);
    return initial_map().InstanceSizeFromSlack(slack);
  }
  return initial_map().instance_size();
}

void JSFunction::PrintName(FILE* out) {
  std::unique_ptr<char[]> name = shared().DebugName().ToCString();
  PrintF(out, "%s", name.get());
}

Handle<String> JSFunction::GetName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name =
      JSReceiver::GetDataProperty(function, isolate->factory()->name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return handle(function->shared().DebugName(), isolate);
}

Handle<String> JSFunction::GetDebugName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name = JSReceiver::GetDataProperty(
      function, isolate->factory()->display_name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return JSFunction::GetName(function);
}

bool JSFunction::SetName(Handle<JSFunction> function, Handle<Name> name,
                         Handle<String> prefix) {
  Isolate* isolate = function->GetIsolate();
  Handle<String> function_name;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name,
                                   Name::ToFunctionName(isolate, name), false);
  if (prefix->length() > 0) {
    IncrementalStringBuilder builder(isolate);
    builder.AppendString(prefix);
    builder.AppendCharacter(' ');
    builder.AppendString(function_name);
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name, builder.Finish(),
                                     false);
  }
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      JSObject::DefinePropertyOrElementIgnoreAttributes(
          function, isolate->factory()->name_string(), function_name,
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY)),
      false);
  return true;
}

namespace {

Handle<String> NativeCodeFunctionSourceString(
    Handle<SharedFunctionInfo> shared_info) {
  Isolate* const isolate = shared_info->GetIsolate();
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("function ");
  builder.AppendString(handle(shared_info->Name(), isolate));
  builder.AppendCString("() { [native code] }");
  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
Handle<String> JSFunction::ToString(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  // Check if {function} should hide its source code.
  if (!shared_info->IsUserJavaScript()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // Check if we should print {function} as a class.
  Handle<Object> maybe_class_positions = JSReceiver::GetDataProperty(
      function, isolate->factory()->class_positions_symbol());
  if (maybe_class_positions->IsClassPositions()) {
    ClassPositions class_positions =
        ClassPositions::cast(*maybe_class_positions);
    int start_position = class_positions.start();
    int end_position = class_positions.end();
    Handle<String> script_source(
        String::cast(Script::cast(shared_info->script()).source()), isolate);
    return isolate->factory()->NewSubString(script_source, start_position,
                                            end_position);
  }

  // Check if we have source code for the {function}.
  if (!shared_info->HasSourceCode()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  if (shared_info->function_token_position() == kNoSourcePosition) {
    // If the function token position isn't valid, return [native code] to
    // ensure calling eval on the returned source code throws rather than
    // giving inconsistent call behaviour.
    isolate->CountUsage(
        v8::Isolate::UseCounterFeature::kFunctionTokenOffsetTooLongForToString);
    return NativeCodeFunctionSourceString(shared_info);
  }
  return Handle<String>::cast(
      SharedFunctionInfo::GetSourceCodeHarmony(shared_info));
}

// static
int JSFunction::CalculateExpectedNofProperties(Isolate* isolate,
                                               Handle<JSFunction> function) {
  int expected_nof_properties = 0;
  for (PrototypeIterator iter(isolate, function, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    if (!current->IsJSFunction()) break;
    Handle<JSFunction> func = Handle<JSFunction>::cast(current);
    // The super constructor should be compiled for the number of expected
    // properties to be available.
    Handle<SharedFunctionInfo> shared(func->shared(), isolate);
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope());
    if (is_compiled_scope.is_compiled() ||
        Compiler::Compile(func, Compiler::CLEAR_EXCEPTION,
                          &is_compiled_scope)) {
      DCHECK(shared->is_compiled());
      int count = shared->expected_nof_properties();
      // Check that the estimate is sane.
      if (expected_nof_properties <= JSObject::kMaxInObjectProperties - count) {
        expected_nof_properties += count;
      } else {
        return JSObject::kMaxInObjectProperties;
      }
    } else {
      // In case there was a compilation error for the constructor we will
      // throw an error during instantiation.
      break;
    }
  }
  // Inobject slack tracking will reclaim redundant inobject space
  // later, so we can afford to adjust the estimate generously,
  // meaning we over-allocate by at least 8 slots in the beginning.
  if (expected_nof_properties > 0) {
    expected_nof_properties += 8;
    if (expected_nof_properties > JSObject::kMaxInObjectProperties) {
      expected_nof_properties = JSObject::kMaxInObjectProperties;
    }
  }
  return expected_nof_properties;
}

// static
void JSFunction::CalculateInstanceSizeHelper(InstanceType instance_type,
                                             bool has_prototype_slot,
                                             int requested_embedder_fields,
                                             int requested_in_object_properties,
                                             int* instance_size,
                                             int* in_object_properties) {
  DCHECK_LE(static_cast<unsigned>(requested_embedder_fields),
            JSObject::kMaxEmbedderFields);
  int header_size = JSObject::GetHeaderSize(instance_type, has_prototype_slot);
  if (requested_embedder_fields) {
    // If there are embedder fields, then the embedder fields start offset must
    // be properly aligned (embedder fields are located between object header
    // and inobject fields).
    header_size = RoundUp<kSystemPointerSize>(header_size);
    requested_embedder_fields *= kEmbedderDataSlotSizeInTaggedSlots;
  }
  int max_nof_fields =
      (JSObject::kMaxInstanceSize - header_size) >> kTaggedSizeLog2;
  CHECK_LE(max_nof_fields, JSObject::kMaxInObjectProperties);
  CHECK_LE(static_cast<unsigned>(requested_embedder_fields),
           static_cast<unsigned>(max_nof_fields));
  *in_object_properties = Min(requested_in_object_properties,
                              max_nof_fields - requested_embedder_fields);
  *instance_size =
      header_size +
      ((requested_embedder_fields + *in_object_properties) << kTaggedSizeLog2);
  CHECK_EQ(*in_object_properties,
           ((*instance_size - header_size) >> kTaggedSizeLog2) -
               requested_embedder_fields);
  CHECK_LE(static_cast<unsigned>(*instance_size),
           static_cast<unsigned>(JSObject::kMaxInstanceSize));
}

void JSFunction::ClearTypeFeedbackInfo() {
  ResetIfBytecodeFlushed();
  if (has_feedback_vector()) {
    FeedbackVector vector = feedback_vector();
    Isolate* isolate = GetIsolate();
    if (vector.ClearSlots(isolate)) {
      IC::OnFeedbackChanged(isolate, vector, FeedbackSlot::Invalid(),
                            "ClearTypeFeedbackInfo");
    }
  }
}

void JSGlobalObject::InvalidatePropertyCell(Handle<JSGlobalObject> global,
                                            Handle<Name> name) {
  // Regardless of whether the property is there or not invalidate
  // Load/StoreGlobalICs that load/store through global object's prototype.
  JSObject::InvalidatePrototypeValidityCell(*global);

  DCHECK(!global->HasFastProperties());
  auto dictionary = handle(global->global_dictionary(), global->GetIsolate());
  int entry = dictionary->FindEntry(global->GetIsolate(), name);
  if (entry == GlobalDictionary::kNotFound) return;
  PropertyCell::InvalidateEntry(global->GetIsolate(), dictionary, entry);
}

Handle<PropertyCell> JSGlobalObject::EnsureEmptyPropertyCell(
    Handle<JSGlobalObject> global, Handle<Name> name,
    PropertyCellType cell_type, int* entry_out) {
  Isolate* isolate = global->GetIsolate();
  DCHECK(!global->HasFastProperties());
  Handle<GlobalDictionary> dictionary(global->global_dictionary(), isolate);
  int entry = dictionary->FindEntry(isolate, name);
  Handle<PropertyCell> cell;
  if (entry != GlobalDictionary::kNotFound) {
    if (entry_out) *entry_out = entry;
    cell = handle(dictionary->CellAt(entry), isolate);
    PropertyCellType original_cell_type = cell->property_details().cell_type();
    DCHECK(original_cell_type == PropertyCellType::kInvalidated ||
           original_cell_type == PropertyCellType::kUninitialized);
    DCHECK(cell->value().IsTheHole(isolate));
    if (original_cell_type == PropertyCellType::kInvalidated) {
      cell = PropertyCell::InvalidateEntry(isolate, dictionary, entry);
    }
    PropertyDetails details(kData, NONE, cell_type);
    cell->set_property_details(details);
    return cell;
  }
  cell = isolate->factory()->NewPropertyCell(name);
  PropertyDetails details(kData, NONE, cell_type);
  dictionary = GlobalDictionary::Add(isolate, dictionary, name, cell, details,
                                     entry_out);
  // {*entry_out} is initialized inside GlobalDictionary::Add().
  global->SetProperties(*dictionary);
  return cell;
}

// static
MaybeHandle<JSDate> JSDate::New(Handle<JSFunction> constructor,
                                Handle<JSReceiver> new_target, double tv) {
  Isolate* const isolate = constructor->GetIsolate();
  Handle<JSObject> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      JSObject::New(constructor, new_target, Handle<AllocationSite>::null()),
      JSDate);
  if (-DateCache::kMaxTimeInMs <= tv && tv <= DateCache::kMaxTimeInMs) {
    tv = DoubleToInteger(tv) + 0.0;
  } else {
    tv = std::numeric_limits<double>::quiet_NaN();
  }
  Handle<Object> value = isolate->factory()->NewNumber(tv);
  Handle<JSDate>::cast(result)->SetValue(*value, std::isnan(tv));
  return Handle<JSDate>::cast(result);
}

// static
double JSDate::CurrentTimeValue(Isolate* isolate) {
  if (FLAG_log_internal_timer_events) LOG(isolate, CurrentTimeEvent());

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  return std::floor(V8::GetCurrentPlatform()->CurrentClockTimeMillis());
}

// static
Address JSDate::GetField(Address raw_object, Address smi_index) {
  Object object(raw_object);
  Smi index(smi_index);
  return JSDate::cast(object)
      .DoGetField(static_cast<FieldIndex>(index.value()))
      .ptr();
}

Object JSDate::DoGetField(FieldIndex index) {
  DCHECK_NE(index, kDateValue);

  DateCache* date_cache = GetIsolate()->date_cache();

  if (index < kFirstUncachedField) {
    Object stamp = cache_stamp();
    if (stamp != date_cache->stamp() && stamp.IsSmi()) {
      // Since the stamp is not NaN, the value is also not NaN.
      int64_t local_time_ms =
          date_cache->ToLocal(static_cast<int64_t>(value().Number()));
      SetCachedFields(local_time_ms, date_cache);
    }
    switch (index) {
      case kYear:
        return year();
      case kMonth:
        return month();
      case kDay:
        return day();
      case kWeekday:
        return weekday();
      case kHour:
        return hour();
      case kMinute:
        return min();
      case kSecond:
        return sec();
      default:
        UNREACHABLE();
    }
  }

  if (index >= kFirstUTCField) {
    return GetUTCField(index, value().Number(), date_cache);
  }

  double time = value().Number();
  if (std::isnan(time)) return GetReadOnlyRoots().nan_value();

  int64_t local_time_ms = date_cache->ToLocal(static_cast<int64_t>(time));
  int days = DateCache::DaysFromTime(local_time_ms);

  if (index == kDays) return Smi::FromInt(days);

  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  if (index == kMillisecond) return Smi::FromInt(time_in_day_ms % 1000);
  DCHECK_EQ(index, kTimeInDay);
  return Smi::FromInt(time_in_day_ms);
}

Object JSDate::GetUTCField(FieldIndex index, double value,
                           DateCache* date_cache) {
  DCHECK_GE(index, kFirstUTCField);

  if (std::isnan(value)) return GetReadOnlyRoots().nan_value();

  int64_t time_ms = static_cast<int64_t>(value);

  if (index == kTimezoneOffset) {
    GetIsolate()->CountUsage(v8::Isolate::kDateGetTimezoneOffset);
    return Smi::FromInt(date_cache->TimezoneOffset(time_ms));
  }

  int days = DateCache::DaysFromTime(time_ms);

  if (index == kWeekdayUTC) return Smi::FromInt(date_cache->Weekday(days));

  if (index <= kDayUTC) {
    int year, month, day;
    date_cache->YearMonthDayFromDays(days, &year, &month, &day);
    if (index == kYearUTC) return Smi::FromInt(year);
    if (index == kMonthUTC) return Smi::FromInt(month);
    DCHECK_EQ(index, kDayUTC);
    return Smi::FromInt(day);
  }

  int time_in_day_ms = DateCache::TimeInDay(time_ms, days);
  switch (index) {
    case kHourUTC:
      return Smi::FromInt(time_in_day_ms / (60 * 60 * 1000));
    case kMinuteUTC:
      return Smi::FromInt((time_in_day_ms / (60 * 1000)) % 60);
    case kSecondUTC:
      return Smi::FromInt((time_in_day_ms / 1000) % 60);
    case kMillisecondUTC:
      return Smi::FromInt(time_in_day_ms % 1000);
    case kDaysUTC:
      return Smi::FromInt(days);
    case kTimeInDayUTC:
      return Smi::FromInt(time_in_day_ms);
    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

// static
Handle<Object> JSDate::SetValue(Handle<JSDate> date, double v) {
  Isolate* const isolate = date->GetIsolate();
  Handle<Object> value = isolate->factory()->NewNumber(v);
  bool value_is_nan = std::isnan(v);
  date->SetValue(*value, value_is_nan);
  return value;
}

void JSDate::SetValue(Object value, bool is_value_nan) {
  set_value(value);
  if (is_value_nan) {
    HeapNumber nan = GetReadOnlyRoots().nan_value();
    set_cache_stamp(nan, SKIP_WRITE_BARRIER);
    set_year(nan, SKIP_WRITE_BARRIER);
    set_month(nan, SKIP_WRITE_BARRIER);
    set_day(nan, SKIP_WRITE_BARRIER);
    set_hour(nan, SKIP_WRITE_BARRIER);
    set_min(nan, SKIP_WRITE_BARRIER);
    set_sec(nan, SKIP_WRITE_BARRIER);
    set_weekday(nan, SKIP_WRITE_BARRIER);
  } else {
    set_cache_stamp(Smi::FromInt(DateCache::kInvalidStamp), SKIP_WRITE_BARRIER);
  }
}

void JSDate::SetCachedFields(int64_t local_time_ms, DateCache* date_cache) {
  int days = DateCache::DaysFromTime(local_time_ms);
  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  int year, month, day;
  date_cache->YearMonthDayFromDays(days, &year, &month, &day);
  int weekday = date_cache->Weekday(days);
  int hour = time_in_day_ms / (60 * 60 * 1000);
  int min = (time_in_day_ms / (60 * 1000)) % 60;
  int sec = (time_in_day_ms / 1000) % 60;
  set_cache_stamp(date_cache->stamp());
  set_year(Smi::FromInt(year), SKIP_WRITE_BARRIER);
  set_month(Smi::FromInt(month), SKIP_WRITE_BARRIER);
  set_day(Smi::FromInt(day), SKIP_WRITE_BARRIER);
  set_weekday(Smi::FromInt(weekday), SKIP_WRITE_BARRIER);
  set_hour(Smi::FromInt(hour), SKIP_WRITE_BARRIER);
  set_min(Smi::FromInt(min), SKIP_WRITE_BARRIER);
  set_sec(Smi::FromInt(sec), SKIP_WRITE_BARRIER);
}

// static
void JSMessageObject::EnsureSourcePositionsAvailable(
    Isolate* isolate, Handle<JSMessageObject> message) {
  if (!message->DidEnsureSourcePositionsAvailable()) {
    DCHECK_EQ(message->start_position(), -1);
    DCHECK_GE(message->bytecode_offset().value(), 0);
    Handle<SharedFunctionInfo> shared_info(
        SharedFunctionInfo::cast(message->shared_info()), isolate);
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared_info);
    DCHECK(shared_info->HasBytecodeArray());
    int position = shared_info->abstract_code().SourcePosition(
        message->bytecode_offset().value());
    DCHECK_GE(position, 0);
    message->set_start_position(position);
    message->set_end_position(position + 1);
    message->set_shared_info(ReadOnlyRoots(isolate).undefined_value());
  }
}

int JSMessageObject::GetLineNumber() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  if (start_position() == -1) return Message::kNoLineNumberInfo;

  Handle<Script> the_script(script(), GetIsolate());

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return Message::kNoLineNumberInfo;
  }

  return info.line + 1;
}

int JSMessageObject::GetColumnNumber() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  if (start_position() == -1) return -1;

  Handle<Script> the_script(script(), GetIsolate());

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return -1;
  }

  return info.column;  // Note: No '+1' in contrast to GetLineNumber.
}

Handle<String> JSMessageObject::GetSourceLine() const {
  Isolate* isolate = GetIsolate();
  Handle<Script> the_script(script(), isolate);

  if (the_script->type() == Script::TYPE_WASM) {
    return isolate->factory()->empty_string();
  }

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  DCHECK(DidEnsureSourcePositionsAvailable());
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return isolate->factory()->empty_string();
  }

  Handle<String> src = handle(String::cast(the_script->source()), isolate);
  return isolate->factory()->NewSubString(src, info.line_start, info.line_end);
}

}  // namespace internal
}  // namespace v8
