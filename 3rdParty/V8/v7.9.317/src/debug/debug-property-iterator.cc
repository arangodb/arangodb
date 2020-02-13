// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-property-iterator.h"

#include "src/api/api-inl.h"
#include "src/base/flags.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/keys.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/property-details.h"

namespace v8 {

std::unique_ptr<debug::PropertyIterator> debug::PropertyIterator::Create(
    v8::Local<v8::Object> v8_object) {
  internal::Isolate* isolate =
      reinterpret_cast<internal::Isolate*>(v8_object->GetIsolate());
  return std::unique_ptr<debug::PropertyIterator>(
      new internal::DebugPropertyIterator(isolate,
                                          Utils::OpenHandle(*v8_object)));
}

namespace internal {

DebugPropertyIterator::DebugPropertyIterator(Isolate* isolate,
                                             Handle<JSReceiver> receiver)
    : isolate_(isolate),
      prototype_iterator_(isolate, receiver, kStartAtReceiver,
                          PrototypeIterator::END_AT_NULL) {
  if (receiver->IsJSProxy()) {
    is_own_ = false;
    prototype_iterator_.AdvanceIgnoringProxies();
  }
  if (prototype_iterator_.IsAtEnd()) return;
  FillKeysForCurrentPrototypeAndStage();
  if (should_move_to_next_stage()) Advance();
}

bool DebugPropertyIterator::Done() const {
  return prototype_iterator_.IsAtEnd();
}

void DebugPropertyIterator::Advance() {
  ++current_key_index_;
  calculated_native_accessor_flags_ = false;
  while (should_move_to_next_stage()) {
    switch (stage_) {
      case Stage::kExoticIndices:
        stage_ = Stage::kEnumerableStrings;
        break;
      case Stage::kEnumerableStrings:
        stage_ = Stage::kAllProperties;
        break;
      case Stage::kAllProperties:
        stage_ = kExoticIndices;
        is_own_ = false;
        prototype_iterator_.AdvanceIgnoringProxies();
        break;
    }
    FillKeysForCurrentPrototypeAndStage();
  }
}

bool DebugPropertyIterator::is_native_accessor() {
  if (stage_ == kExoticIndices) return false;
  CalculateNativeAccessorFlags();
  return native_accessor_flags_;
}

bool DebugPropertyIterator::has_native_getter() {
  if (stage_ == kExoticIndices) return false;
  CalculateNativeAccessorFlags();
  return native_accessor_flags_ &
         static_cast<int>(debug::NativeAccessorType::HasGetter);
}

bool DebugPropertyIterator::has_native_setter() {
  if (stage_ == kExoticIndices) return false;
  CalculateNativeAccessorFlags();
  return native_accessor_flags_ &
         static_cast<int>(debug::NativeAccessorType::HasSetter);
}

Handle<Name> DebugPropertyIterator::raw_name() const {
  DCHECK(!Done());
  if (stage_ == kExoticIndices) {
    return isolate_->factory()->Uint32ToString(current_key_index_);
  } else {
    return Handle<Name>::cast(
        FixedArray::get(*keys_, current_key_index_, isolate_));
  }
}

v8::Local<v8::Name> DebugPropertyIterator::name() const {
  return Utils::ToLocal(raw_name());
}

v8::Maybe<v8::PropertyAttribute> DebugPropertyIterator::attributes() {
  Handle<JSReceiver> receiver =
      PrototypeIterator::GetCurrent<JSReceiver>(prototype_iterator_);
  auto result = JSReceiver::GetPropertyAttributes(receiver, raw_name());
  if (result.IsNothing()) return Nothing<v8::PropertyAttribute>();
  DCHECK(result.FromJust() != ABSENT);
  return Just(static_cast<v8::PropertyAttribute>(result.FromJust()));
}

v8::Maybe<v8::debug::PropertyDescriptor> DebugPropertyIterator::descriptor() {
  Handle<JSReceiver> receiver =
      PrototypeIterator::GetCurrent<JSReceiver>(prototype_iterator_);

  PropertyDescriptor descriptor;
  Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
      isolate_, receiver, raw_name(), &descriptor);
  if (did_get_descriptor.IsNothing()) {
    return Nothing<v8::debug::PropertyDescriptor>();
  }
  DCHECK(did_get_descriptor.FromJust());
  return Just(v8::debug::PropertyDescriptor{
      descriptor.enumerable(), descriptor.has_enumerable(),
      descriptor.configurable(), descriptor.has_configurable(),
      descriptor.writable(), descriptor.has_writable(),
      descriptor.has_value() ? Utils::ToLocal(descriptor.value())
                             : v8::Local<v8::Value>(),
      descriptor.has_get() ? Utils::ToLocal(descriptor.get())
                           : v8::Local<v8::Value>(),
      descriptor.has_set() ? Utils::ToLocal(descriptor.set())
                           : v8::Local<v8::Value>(),
  });
}

bool DebugPropertyIterator::is_own() { return is_own_; }

bool DebugPropertyIterator::is_array_index() {
  if (stage_ == kExoticIndices) return true;
  uint32_t index = 0;
  return raw_name()->AsArrayIndex(&index);
}

void DebugPropertyIterator::FillKeysForCurrentPrototypeAndStage() {
  current_key_index_ = 0;
  exotic_length_ = 0;
  keys_ = Handle<FixedArray>::null();
  if (prototype_iterator_.IsAtEnd()) return;
  Handle<JSReceiver> receiver =
      PrototypeIterator::GetCurrent<JSReceiver>(prototype_iterator_);
  bool has_exotic_indices = receiver->IsJSTypedArray();
  if (stage_ == kExoticIndices) {
    if (!has_exotic_indices) return;
    Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(receiver);
    if (typed_array->WasDetached()) {
      exotic_length_ = 0;
    } else {
      // TODO(bmeurer, v8:4153): Change this to size_t later.
      exotic_length_ = static_cast<uint32_t>(typed_array->length());
    }
    return;
  }
  bool skip_indices = has_exotic_indices;
  PropertyFilter filter =
      stage_ == kEnumerableStrings ? ENUMERABLE_STRINGS : ALL_PROPERTIES;
  if (!KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly, filter,
                               GetKeysConversion::kConvertToString, false,
                               skip_indices)
           .ToHandle(&keys_)) {
    keys_ = Handle<FixedArray>::null();
  }
}

bool DebugPropertyIterator::should_move_to_next_stage() const {
  if (prototype_iterator_.IsAtEnd()) return false;
  if (stage_ == kExoticIndices) return current_key_index_ >= exotic_length_;
  return keys_.is_null() ||
         current_key_index_ >= static_cast<uint32_t>(keys_->length());
}

namespace {
base::Flags<debug::NativeAccessorType, int> GetNativeAccessorDescriptorInternal(
    Handle<JSReceiver> object, Handle<Name> name) {
  uint32_t index;
  if (name->AsArrayIndex(&index)) return debug::NativeAccessorType::None;
  LookupIterator it =
      LookupIterator(object->GetIsolate(), object, name, LookupIterator::OWN);
  if (!it.IsFound()) return debug::NativeAccessorType::None;
  if (it.state() != LookupIterator::ACCESSOR) {
    return debug::NativeAccessorType::None;
  }
  Handle<Object> structure = it.GetAccessors();
  if (!structure->IsAccessorInfo()) return debug::NativeAccessorType::None;
  auto isolate = object->GetIsolate();
  base::Flags<debug::NativeAccessorType, int> result;
#define IS_BUILTIN_ACESSOR(_, name, ...)                    \
  if (*structure == *isolate->factory()->name##_accessor()) \
    return debug::NativeAccessorType::None;
  ACCESSOR_INFO_LIST_GENERATOR(IS_BUILTIN_ACESSOR, /* not used */)
#undef IS_BUILTIN_ACESSOR
  Handle<AccessorInfo> accessor_info = Handle<AccessorInfo>::cast(structure);
  if (accessor_info->getter() != Object()) {
    result |= debug::NativeAccessorType::HasGetter;
  }
  if (accessor_info->setter() != Object()) {
    result |= debug::NativeAccessorType::HasSetter;
  }
  return result;
}
}  // anonymous namespace

void DebugPropertyIterator::CalculateNativeAccessorFlags() {
  if (calculated_native_accessor_flags_) return;
  Handle<JSReceiver> receiver =
      PrototypeIterator::GetCurrent<JSReceiver>(prototype_iterator_);
  native_accessor_flags_ =
      GetNativeAccessorDescriptorInternal(receiver, raw_name());
  calculated_native_accessor_flags_ = true;
}
}  // namespace internal
}  // namespace v8
