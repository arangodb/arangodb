// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/heap/heap-write-barrier-inl.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/elements.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/prototype.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, to_map, 1);
  ElementsKind to_kind = to_map->elements_kind();
  ElementsAccessor::ForKind(to_kind)->TransitionElementsKind(object, to_map);
  return *object;
}

RUNTIME_FUNCTION(Runtime_TransitionElementsKindWithKind) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, elements_kind_smi, 1);
  ElementsKind to_kind = static_cast<ElementsKind>(elements_kind_smi->value());
  JSObject::TransitionElementsKind(object, to_kind);
  return *object;
}

RUNTIME_FUNCTION(Runtime_NewArray) {
  HandleScope scope(isolate);
  DCHECK_LE(3, args.length());
  int const argc = args.length() - 3;
  // TODO(bmeurer): Remove this Arguments nonsense.
  Arguments argv(argc, args.address_of_arg_at(1));
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, new_target, argc + 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, type_info, argc + 2);
  // TODO(bmeurer): Use MaybeHandle to pass around the AllocationSite.
  Handle<AllocationSite> site = type_info->IsAllocationSite()
                                    ? Handle<AllocationSite>::cast(type_info)
                                    : Handle<AllocationSite>::null();

  Factory* factory = isolate->factory();

  // If called through new, new.target can be:
  // - a subclass of constructor,
  // - a proxy wrapper around constructor, or
  // - the constructor itself.
  // If called through Reflect.construct, it's guaranteed to be a constructor by
  // REFLECT_CONSTRUCT_PREPARE.
  DCHECK(new_target->IsConstructor());

  bool holey = false;
  bool can_use_type_feedback = !site.is_null();
  bool can_inline_array_constructor = true;
  if (argv.length() == 1) {
    Handle<Object> argument_one = argv.at<Object>(0);
    if (argument_one->IsSmi()) {
      int value = Handle<Smi>::cast(argument_one)->value();
      if (value < 0 ||
          JSArray::SetLengthWouldNormalize(isolate->heap(), value)) {
        // the array is a dictionary in this case.
        can_use_type_feedback = false;
      } else if (value != 0) {
        holey = true;
        if (value >= JSArray::kInitialMaxFastElementArray) {
          can_inline_array_constructor = false;
        }
      }
    } else {
      // Non-smi length argument produces a dictionary
      can_use_type_feedback = false;
    }
  }

  Handle<Map> initial_map;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, initial_map,
      JSFunction::GetDerivedMap(isolate, constructor, new_target));

  ElementsKind to_kind = can_use_type_feedback ? site->GetElementsKind()
                                               : initial_map->elements_kind();
  if (holey && !IsHoleyElementsKind(to_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
    // Update the allocation site info to reflect the advice alteration.
    if (!site.is_null()) site->SetElementsKind(to_kind);
  }

  // We should allocate with an initial map that reflects the allocation site
  // advice. Therefore we use AllocateJSObjectFromMap instead of passing
  // the constructor.
  initial_map = Map::AsElementsKind(isolate, initial_map, to_kind);

  // If we don't care to track arrays of to_kind ElementsKind, then
  // don't emit a memento for them.
  Handle<AllocationSite> allocation_site;
  if (AllocationSite::ShouldTrack(to_kind)) {
    allocation_site = site;
  }

  Handle<JSArray> array = Handle<JSArray>::cast(factory->NewJSObjectFromMap(
      initial_map, AllocationType::kYoung, allocation_site));

  factory->NewJSArrayStorage(array, 0, 0, DONT_INITIALIZE_ARRAY_ELEMENTS);

  ElementsKind old_kind = array->GetElementsKind();
  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              ArrayConstructInitializeElements(array, &argv));
  if (!site.is_null()) {
    if ((old_kind != array->GetElementsKind() || !can_use_type_feedback ||
         !can_inline_array_constructor)) {
      // The arguments passed in caused a transition. This kind of complexity
      // can't be dealt with in the inlined optimized array constructor case.
      // We must mark the allocationsite as un-inlinable.
      site->SetDoNotInlineCall();
    }
  } else {
    if (old_kind != array->GetElementsKind() || !can_inline_array_constructor) {
      // We don't have an AllocationSite for this Array constructor invocation,
      // i.e. it might a call from Array#map or from an Array subclass, so we
      // just flip the bit on the global protector cell instead.
      // TODO(bmeurer): Find a better way to mark this. Global protectors
      // tend to back-fire over time...
      if (Protectors::IsArrayConstructorIntact(isolate)) {
        Protectors::InvalidateArrayConstructor(isolate);
      }
    }
  }

  return *array;
}

RUNTIME_FUNCTION(Runtime_NormalizeElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CHECK(!array->HasTypedArrayElements());
  CHECK(!array->IsJSGlobalProxy());
  JSObject::NormalizeElements(array);
  return *array;
}

// GrowArrayElements returns a sentinel Smi if the object was normalized or if
// the key is negative.
RUNTIME_FUNCTION(Runtime_GrowArrayElements) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(int, key, Int32, args[1]);

  if (key < 0) return Smi::kZero;

  uint32_t capacity = static_cast<uint32_t>(object->elements().length());
  uint32_t index = static_cast<uint32_t>(key);

  if (index >= capacity) {
    if (!object->GetElementsAccessor()->GrowCapacity(object, index)) {
      return Smi::kZero;
    }
  }

  return object->elements();
}

// ES6 22.1.2.2 Array.isArray
RUNTIME_FUNCTION(Runtime_ArrayIsArray) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Maybe<bool> result = Object::IsArray(object);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_IsArray) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj.IsJSArray());
}

RUNTIME_FUNCTION(Runtime_ArraySpeciesConstructor) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, original_array, 0);
  RETURN_RESULT_OR_FAILURE(
      isolate, Object::ArraySpeciesConstructor(isolate, original_array));
}

// ES7 22.1.3.11 Array.prototype.includes
RUNTIME_FUNCTION(Runtime_ArrayIncludes_Slow) {
  HandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, search_element, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, from_index, 2);

  // Let O be ? ToObject(this value).
  Handle<JSReceiver> object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, object,
      Object::ToObject(isolate, Handle<Object>(args[0], isolate)));

  // Let len be ? ToLength(? Get(O, "length")).
  int64_t len;
  {
    if (object->map().instance_type() == JS_ARRAY_TYPE) {
      uint32_t len32 = 0;
      bool success = JSArray::cast(*object).length().ToArrayLength(&len32);
      DCHECK(success);
      USE(success);
      len = len32;
    } else {
      Handle<Object> len_;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, len_,
          Object::GetProperty(isolate, object,
                              isolate->factory()->length_string()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, len_,
                                         Object::ToLength(isolate, len_));
      len = static_cast<int64_t>(len_->Number());
      DCHECK_EQ(len, len_->Number());
    }
  }

  if (len == 0) return ReadOnlyRoots(isolate).false_value();

  // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step
  // produces the value 0.)
  int64_t index = 0;
  if (!from_index->IsUndefined(isolate)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, from_index,
                                       Object::ToInteger(isolate, from_index));

    if (V8_LIKELY(from_index->IsSmi())) {
      int start_from = Smi::ToInt(*from_index);
      if (start_from < 0) {
        index = std::max<int64_t>(len + start_from, 0);
      } else {
        index = start_from;
      }
    } else {
      DCHECK(from_index->IsHeapNumber());
      double start_from = from_index->Number();
      if (start_from >= len) return ReadOnlyRoots(isolate).false_value();
      if (V8_LIKELY(std::isfinite(start_from))) {
        if (start_from < 0) {
          index = static_cast<int64_t>(std::max<double>(start_from + len, 0));
        } else {
          index = start_from;
        }
      }
    }

    DCHECK_GE(index, 0);
  }

  // If the receiver is not a special receiver type, and the length is a valid
  // element index, perform fast operation tailored to specific ElementsKinds.
  if (!object->map().IsSpecialReceiverMap() &&
      len <= JSObject::kMaxElementCount &&
      JSObject::PrototypeHasNoElements(isolate, JSObject::cast(*object))) {
    Handle<JSObject> obj = Handle<JSObject>::cast(object);
    ElementsAccessor* elements = obj->GetElementsAccessor();
    Maybe<bool> result = elements->IncludesValue(isolate, obj, search_element,
                                                 static_cast<uint32_t>(index),
                                                 static_cast<uint32_t>(len));
    MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
    return *isolate->factory()->ToBoolean(result.FromJust());
  }

  // Otherwise, perform slow lookups for special receiver types.
  for (; index < len; ++index) {
    HandleScope iteration_hs(isolate);

    // Let elementK be the result of ? Get(O, ! ToString(k)).
    Handle<Object> element_k;
    {
      Handle<Object> index_obj = isolate->factory()->NewNumberFromInt64(index);
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, object, index_obj, &success);
      DCHECK(success);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_k,
                                         Object::GetProperty(&it));
    }

    // If SameValueZero(searchElement, elementK) is true, return true.
    if (search_element->SameValueZero(*element_k)) {
      return ReadOnlyRoots(isolate).true_value();
    }
  }
  return ReadOnlyRoots(isolate).false_value();
}

RUNTIME_FUNCTION(Runtime_ArrayIndexOf) {
  HandleScope hs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, search_element, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, from_index, 2);

  // Let O be ? ToObject(this value).
  Handle<JSReceiver> object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, object,
      Object::ToObject(isolate, args.at(0), "Array.prototype.indexOf"));

  // Let len be ? ToLength(? Get(O, "length")).
  int64_t len;
  {
    if (object->IsJSArray()) {
      uint32_t len32 = 0;
      bool success = JSArray::cast(*object).length().ToArrayLength(&len32);
      DCHECK(success);
      USE(success);
      len = len32;
    } else {
      Handle<Object> len_;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, len_,
          Object::GetProperty(isolate, object,
                              isolate->factory()->length_string()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, len_,
                                         Object::ToLength(isolate, len_));
      len = static_cast<int64_t>(len_->Number());
      DCHECK_EQ(len, len_->Number());
    }
  }

  if (len == 0) return Smi::FromInt(-1);

  // Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step
  // produces the value 0.)
  int64_t start_from;
  {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, from_index,
                                       Object::ToInteger(isolate, from_index));
    double fp = from_index->Number();
    if (fp > len) return Smi::FromInt(-1);
    if (V8_LIKELY(fp >=
                  static_cast<double>(std::numeric_limits<int64_t>::min()))) {
      DCHECK(fp < std::numeric_limits<int64_t>::max());
      start_from = static_cast<int64_t>(fp);
    } else {
      start_from = std::numeric_limits<int64_t>::min();
    }
  }

  int64_t index;
  if (start_from >= 0) {
    index = start_from;
  } else {
    index = len + start_from;
    if (index < 0) {
      index = 0;
    }
  }

  // If the receiver is not a special receiver type, and the length fits
  // uint32_t, perform fast operation tailored to specific ElementsKinds.
  if (!object->map().IsSpecialReceiverMap() && len <= kMaxUInt32 &&
      JSObject::PrototypeHasNoElements(isolate, JSObject::cast(*object))) {
    Handle<JSObject> obj = Handle<JSObject>::cast(object);
    ElementsAccessor* elements = obj->GetElementsAccessor();
    Maybe<int64_t> result = elements->IndexOfValue(isolate, obj, search_element,
                                                   static_cast<uint32_t>(index),
                                                   static_cast<uint32_t>(len));
    MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
    return *isolate->factory()->NewNumberFromInt64(result.FromJust());
  }

  // Otherwise, perform slow lookups for special receiver types
  for (; index < len; ++index) {
    HandleScope iteration_hs(isolate);
    // Let elementK be the result of ? Get(O, ! ToString(k)).
    Handle<Object> element_k;
    {
      Handle<Object> index_obj = isolate->factory()->NewNumberFromInt64(index);
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, object, index_obj, &success);
      DCHECK(success);
      Maybe<bool> present = JSReceiver::HasProperty(&it);
      MAYBE_RETURN(present, ReadOnlyRoots(isolate).exception());
      if (!present.FromJust()) continue;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_k,
                                         Object::GetProperty(&it));
      if (search_element->StrictEquals(*element_k)) {
        return *index_obj;
      }
    }
  }
  return Smi::FromInt(-1);
}

}  // namespace internal
}  // namespace v8
