// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_INL_H_
#define V8_API_INL_H_

#include "src/api.h"
#include "src/objects-inl.h"
#include "src/objects/stack-frame-info.h"

namespace v8 {

template <typename T>
inline T ToCData(v8::internal::Object* obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == v8::internal::Smi::kZero) return nullptr;
  return reinterpret_cast<T>(
      v8::internal::Foreign::cast(obj)->foreign_address());
}

template <>
inline v8::internal::Address ToCData(v8::internal::Object* obj) {
  if (obj == v8::internal::Smi::kZero) return v8::internal::kNullAddress;
  return v8::internal::Foreign::cast(obj)->foreign_address();
}

template <typename T>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, T obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == nullptr) return handle(v8::internal::Smi::kZero, isolate);
  return isolate->factory()->NewForeign(
      reinterpret_cast<v8::internal::Address>(obj));
}

template <>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, v8::internal::Address obj) {
  if (obj == v8::internal::kNullAddress) {
    return handle(v8::internal::Smi::kZero, isolate);
  }
  return isolate->factory()->NewForeign(obj);
}

template <class From, class To>
inline Local<To> Utils::Convert(v8::internal::Handle<From> obj) {
  DCHECK(obj.is_null() || (obj->IsSmi() || !obj->IsTheHole()));
  return Local<To>(reinterpret_cast<To*>(obj.location()));
}

// Implementations of ToLocal

#define MAKE_TO_LOCAL(Name, From, To)                                       \
  Local<v8::To> Utils::Name(v8::internal::Handle<v8::internal::From> obj) { \
    return Convert<v8::internal::From, v8::To>(obj);                        \
  }

#define MAKE_TO_LOCAL_TYPED_ARRAY(Type, typeName, TYPE, ctype)        \
  Local<v8::Type##Array> Utils::ToLocal##Type##Array(                 \
      v8::internal::Handle<v8::internal::JSTypedArray> obj) {         \
    DCHECK(obj->type() == v8::internal::kExternal##Type##Array);      \
    return Convert<v8::internal::JSTypedArray, v8::Type##Array>(obj); \
  }

MAKE_TO_LOCAL(ToLocal, Context, Context)
MAKE_TO_LOCAL(ToLocal, Object, Value)
MAKE_TO_LOCAL(ToLocal, Module, Module)
MAKE_TO_LOCAL(ToLocal, Name, Name)
MAKE_TO_LOCAL(ToLocal, String, String)
MAKE_TO_LOCAL(ToLocal, Symbol, Symbol)
MAKE_TO_LOCAL(ToLocal, JSRegExp, RegExp)
MAKE_TO_LOCAL(ToLocal, JSReceiver, Object)
MAKE_TO_LOCAL(ToLocal, JSObject, Object)
MAKE_TO_LOCAL(ToLocal, JSFunction, Function)
MAKE_TO_LOCAL(ToLocal, JSArray, Array)
MAKE_TO_LOCAL(ToLocal, JSMap, Map)
MAKE_TO_LOCAL(ToLocal, JSSet, Set)
MAKE_TO_LOCAL(ToLocal, JSProxy, Proxy)
MAKE_TO_LOCAL(ToLocal, JSArrayBuffer, ArrayBuffer)
MAKE_TO_LOCAL(ToLocal, JSArrayBufferView, ArrayBufferView)
MAKE_TO_LOCAL(ToLocal, JSDataView, DataView)
MAKE_TO_LOCAL(ToLocal, JSTypedArray, TypedArray)
MAKE_TO_LOCAL(ToLocalShared, JSArrayBuffer, SharedArrayBuffer)

TYPED_ARRAYS(MAKE_TO_LOCAL_TYPED_ARRAY)

MAKE_TO_LOCAL(ToLocal, FunctionTemplateInfo, FunctionTemplate)
MAKE_TO_LOCAL(ToLocal, ObjectTemplateInfo, ObjectTemplate)
MAKE_TO_LOCAL(SignatureToLocal, FunctionTemplateInfo, Signature)
MAKE_TO_LOCAL(AccessorSignatureToLocal, FunctionTemplateInfo, AccessorSignature)
MAKE_TO_LOCAL(MessageToLocal, Object, Message)
MAKE_TO_LOCAL(PromiseToLocal, JSObject, Promise)
MAKE_TO_LOCAL(StackTraceToLocal, FixedArray, StackTrace)
MAKE_TO_LOCAL(StackFrameToLocal, StackFrameInfo, StackFrame)
MAKE_TO_LOCAL(NumberToLocal, Object, Number)
MAKE_TO_LOCAL(IntegerToLocal, Object, Integer)
MAKE_TO_LOCAL(Uint32ToLocal, Object, Uint32)
MAKE_TO_LOCAL(ToLocal, BigInt, BigInt);
MAKE_TO_LOCAL(ExternalToLocal, JSObject, External)
MAKE_TO_LOCAL(CallableToLocal, JSReceiver, Function)
MAKE_TO_LOCAL(ToLocalPrimitive, Object, Primitive)
MAKE_TO_LOCAL(ToLocal, FixedArray, PrimitiveArray)
MAKE_TO_LOCAL(ScriptOrModuleToLocal, Script, ScriptOrModule)

#undef MAKE_TO_LOCAL_TYPED_ARRAY
#undef MAKE_TO_LOCAL

// Implementations of OpenHandle

#define MAKE_OPEN_HANDLE(From, To)                                             \
  v8::internal::Handle<v8::internal::To> Utils::OpenHandle(                    \
      const v8::From* that, bool allow_empty_handle) {                         \
    DCHECK(allow_empty_handle || that != nullptr);                             \
    DCHECK(that == nullptr ||                                                  \
           (*reinterpret_cast<v8::internal::Object* const*>(that))->Is##To()); \
    return v8::internal::Handle<v8::internal::To>(                             \
        reinterpret_cast<v8::internal::To**>(const_cast<v8::From*>(that)));    \
  }

OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

#undef MAKE_OPEN_HANDLE
#undef OPEN_HANDLE_LIST

namespace internal {

Handle<Context> HandleScopeImplementer::MicrotaskContext() {
  if (microtask_context_) return Handle<Context>(microtask_context_, isolate_);
  return Handle<Context>::null();
}

Handle<Context> HandleScopeImplementer::LastEnteredContext() {
  if (entered_contexts_.empty()) return Handle<Context>::null();
  return Handle<Context>(entered_contexts_.back(), isolate_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_API_INL_H_
