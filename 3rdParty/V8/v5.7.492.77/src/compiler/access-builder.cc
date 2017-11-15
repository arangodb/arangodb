// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"

#include "src/compiler/type-cache.h"
#include "src/contexts.h"
#include "src/frames.h"
#include "src/handles-inl.h"
#include "src/heap/heap.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
FieldAccess AccessBuilder::ForExternalDoubleValue() {
  FieldAccess access = {kUntaggedBase,       0,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Number(),      MachineType::Float64(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForExternalTaggedValue() {
  FieldAccess access = {kUntaggedBase,       0,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Any(),         MachineType::AnyTagged(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForExternalUint8Value() {
  FieldAccess access = {kUntaggedBase,           0,
                        MaybeHandle<Name>(),     MaybeHandle<Map>(),
                        TypeCache::Get().kUint8, MachineType::Uint8(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMap() {
  FieldAccess access = {kTaggedBase,           HeapObject::kMapOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kMapWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForHeapNumberValue() {
  FieldAccess access = {
      kTaggedBase,        HeapNumber::kValueOffset,  MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get().kFloat64, MachineType::Float64(),
      kNoWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSObjectProperties() {
  FieldAccess access = {kTaggedBase,         JSObject::kPropertiesOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSObjectElements() {
  FieldAccess access = {kTaggedBase,         JSObject::kElementsOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSObjectInObjectProperty(Handle<Map> map,
                                                       int index) {
  int const offset = map->GetInObjectPropertyOffset(index);
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectOffset(
    int offset, WriteBarrierKind write_barrier_kind) {
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        write_barrier_kind};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionPrototypeOrInitialMap() {
  FieldAccess access = {
      kTaggedBase,         JSFunction::kPrototypeOrInitialMapOffset,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Any(),         MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionContext() {
  FieldAccess access = {kTaggedBase,         JSFunction::kContextOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::AnyTagged(),
                        kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSFunctionSharedFunctionInfo() {
  FieldAccess access = {
      kTaggedBase,           JSFunction::kSharedFunctionInfoOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionLiterals() {
  FieldAccess access = {kTaggedBase,         JSFunction::kLiteralsOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionCodeEntry() {
  FieldAccess access = {kTaggedBase,           JSFunction::kCodeEntryOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::Pointer(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionNextFunctionLink() {
  FieldAccess access = {
      kTaggedBase,         JSFunction::kNextFunctionLinkOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Any(),         MachineType::AnyTagged(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContext() {
  FieldAccess access = {kTaggedBase,         JSGeneratorObject::kContextOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContinuation() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kContinuationOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectInputOrDebugPos() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kInputOrDebugPosOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectRegisterFile() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kRegisterFileOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Internal(),    MachineType::AnyTagged(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectResumeMode() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kResumeModeOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayLength(ElementsKind elements_kind) {
  TypeCache const& type_cache = TypeCache::Get();
  FieldAccess access = {kTaggedBase,
                        JSArray::kLengthOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        type_cache.kJSArrayLengthType,
                        MachineType::TaggedSigned(),
                        kFullWriteBarrier};
  if (IsFastDoubleElementsKind(elements_kind)) {
    access.type = type_cache.kFixedDoubleArrayLengthType;
    access.write_barrier_kind = kNoWriteBarrier;
  } else if (IsFastElementsKind(elements_kind)) {
    access.type = type_cache.kFixedArrayLengthType;
    access.write_barrier_kind = kNoWriteBarrier;
  }
  return access;
}


// static
FieldAccess AccessBuilder::ForJSArrayBufferBackingStore() {
  FieldAccess access = {
      kTaggedBase,           JSArrayBuffer::kBackingStoreOffset,
      MaybeHandle<Name>(),   MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::Pointer(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferBitField() {
  FieldAccess access = {
      kTaggedBase,        JSArrayBuffer::kBitFieldOffset, MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get().kUint8,        MachineType::Uint32(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewBuffer() {
  FieldAccess access = {kTaggedBase,           JSArrayBufferView::kBufferOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewByteLength() {
  FieldAccess access = {kTaggedBase,
                        JSArrayBufferView::kByteLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kPositiveInteger,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewByteOffset() {
  FieldAccess access = {kTaggedBase,
                        JSArrayBufferView::kByteOffsetOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kPositiveInteger,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayLength() {
  FieldAccess access = {kTaggedBase,
                        JSTypedArray::kLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kJSTypedArrayLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDateValue() {
  FieldAccess access = {kTaggedBase,
                        JSDate::kValueOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kJSDateValueType,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDateField(JSDate::FieldIndex index) {
  FieldAccess access = {
      kTaggedBase,         JSDate::kValueOffset + index * kPointerSize,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Number(),      MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSIteratorResultDone() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kDoneOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSIteratorResultValue() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kValueOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSRegExpFlags() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kFlagsOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSRegExpSource() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kSourceOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForFixedArrayLength() {
  FieldAccess access = {kTaggedBase,
                        FixedArray::kLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kFixedArrayLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedTypedArrayBaseBasePointer() {
  FieldAccess access = {
      kTaggedBase,           FixedTypedArrayBase::kBasePointerOffset,
      MaybeHandle<Name>(),   MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::AnyTagged(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedTypedArrayBaseExternalPointer() {
  FieldAccess access = {kTaggedBase,
                        FixedTypedArrayBase::kExternalPointerOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        Type::ExternalPointer(),
                        MachineType::Pointer(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCache() {
  FieldAccess access = {
      kTaggedBase,           DescriptorArray::kEnumCacheOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCacheBridgeCache() {
  FieldAccess access = {
      kTaggedBase,           DescriptorArray::kEnumCacheBridgeCacheOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapBitField() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitFieldOffset,    Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get().kUint8, MachineType::Uint8(),
      kNoWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapBitField3() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitField3Offset,   Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get().kInt32, MachineType::Int32(),
      kNoWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapDescriptors() {
  FieldAccess access = {kTaggedBase,           Map::kDescriptorsOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapInstanceType() {
  FieldAccess access = {
      kTaggedBase,        Map::kInstanceTypeOffset, Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get().kUint8,  MachineType::Uint8(),
      kNoWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapPrototype() {
  FieldAccess access = {kTaggedBase,         Map::kPrototypeOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Any(),         MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularExports() {
  FieldAccess access = {kTaggedBase,           Module::kRegularExportsOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularImports() {
  FieldAccess access = {kTaggedBase,           Module::kRegularImportsOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForNameHashField() {
  FieldAccess access = {kTaggedBase,      Name::kHashFieldOffset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Internal(), MachineType::Uint32(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForStringLength() {
  FieldAccess access = {kTaggedBase,
                        String::kLengthOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kStringLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringFirst() {
  FieldAccess access = {kTaggedBase,         ConsString::kFirstOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringSecond() {
  FieldAccess access = {kTaggedBase,         ConsString::kSecondOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringOffset() {
  FieldAccess access = {kTaggedBase,         SlicedString::kOffsetOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringParent() {
  FieldAccess access = {kTaggedBase,         SlicedString::kParentOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForExternalStringResourceData() {
  FieldAccess access = {kTaggedBase,
                        ExternalString::kResourceDataOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        Type::ExternalPointer(),
                        MachineType::Pointer(),
                        kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForExternalOneByteStringCharacter() {
  ElementAccess access = {kUntaggedBase, 0, TypeCache::Get().kUint8,
                          MachineType::Uint8(), kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForExternalTwoByteStringCharacter() {
  ElementAccess access = {kUntaggedBase, 0, TypeCache::Get().kUint16,
                          MachineType::Uint16(), kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForSeqOneByteStringCharacter() {
  ElementAccess access = {kTaggedBase, SeqOneByteString::kHeaderSize,
                          TypeCache::Get().kUint8, MachineType::Uint8(),
                          kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForSeqTwoByteStringCharacter() {
  ElementAccess access = {kTaggedBase, SeqTwoByteString::kHeaderSize,
                          TypeCache::Get().kUint16, MachineType::Uint16(),
                          kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGlobalObjectGlobalProxy() {
  FieldAccess access = {kTaggedBase,         JSGlobalObject::kGlobalProxyOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Receiver(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGlobalObjectNativeContext() {
  FieldAccess access = {
      kTaggedBase,         JSGlobalObject::kNativeContextOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Internal(),    MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorObject() {
  FieldAccess access = {kTaggedBase,
                        JSArrayIterator::kIteratedObjectOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        Type::ReceiverOrUndefined(),
                        MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorIndex(InstanceType instance_type,
                                                   ElementsKind elements_kind) {
  // In generic case, cap to 2^53-1 (per ToLength() in spec) via
  // kPositiveSafeInteger
  FieldAccess access = {kTaggedBase,
                        JSArrayIterator::kNextIndexOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kPositiveSafeInteger,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier};
  if (instance_type == JS_ARRAY_TYPE) {
    if (IsFastDoubleElementsKind(elements_kind)) {
      access.type = TypeCache::Get().kFixedDoubleArrayLengthType;
      access.machine_type = MachineType::TaggedSigned();
      access.write_barrier_kind = kNoWriteBarrier;
    } else if (IsFastElementsKind(elements_kind)) {
      access.type = TypeCache::Get().kFixedArrayLengthType;
      access.machine_type = MachineType::TaggedSigned();
      access.write_barrier_kind = kNoWriteBarrier;
    } else {
      access.type = TypeCache::Get().kJSArrayLengthType;
    }
  } else if (instance_type == JS_TYPED_ARRAY_TYPE) {
    access.type = TypeCache::Get().kJSTypedArrayLengthType;
    access.machine_type = MachineType::TaggedSigned();
    access.write_barrier_kind = kNoWriteBarrier;
  }
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorObjectMap() {
  FieldAccess access = {
      kTaggedBase,           JSArrayIterator::kIteratedObjectMapOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorString() {
  FieldAccess access = {kTaggedBase,         JSStringIterator::kStringOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorIndex() {
  FieldAccess access = {kTaggedBase,
                        JSStringIterator::kNextIndexOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get().kStringLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForValue() {
  FieldAccess access = {kTaggedBase,         JSValue::kValueOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForArgumentsLength() {
  FieldAccess access = {kTaggedBase,         JSArgumentsObject::kLengthOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForArgumentsCallee() {
  FieldAccess access = {
      kTaggedBase,         JSSloppyArgumentsObject::kCalleeOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kPointerWriteBarrier};
  return access;
}


// static
FieldAccess AccessBuilder::ForFixedArraySlot(
    size_t index, WriteBarrierKind write_barrier_kind) {
  int offset = FixedArray::OffsetOfElementAt(static_cast<int>(index));
  FieldAccess access = {kTaggedBase,         offset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        write_barrier_kind};
  return access;
}


// static
FieldAccess AccessBuilder::ForCellValue() {
  FieldAccess access = {kTaggedBase,      Cell::kValueOffset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlot(size_t index) {
  int offset = Context::kHeaderSize + static_cast<int>(index) * kPointerSize;
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,      offset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextExtensionScopeInfo() {
  FieldAccess access = {
      kTaggedBase,           ContextExtension::kScopeInfoOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextExtensionExtension() {
  FieldAccess access = {kTaggedBase,      ContextExtension::kExtensionOffset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedArrayElement() {
  ElementAccess access = {kTaggedBase, FixedArray::kHeaderSize, Type::Any(),
                          MachineType::AnyTagged(), kFullWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedArrayElement(ElementsKind kind) {
  ElementAccess access = {kTaggedBase, FixedArray::kHeaderSize, Type::Any(),
                          MachineType::AnyTagged(), kFullWriteBarrier};
  switch (kind) {
    case FAST_SMI_ELEMENTS:
      access.type = Type::SignedSmall();
      access.machine_type = MachineType::TaggedSigned();
      access.write_barrier_kind = kNoWriteBarrier;
      break;
    case FAST_HOLEY_SMI_ELEMENTS:
      access.type = TypeCache::Get().kHoleySmi;
      break;
    case FAST_ELEMENTS:
      access.type = Type::NonInternal();
      break;
    case FAST_HOLEY_ELEMENTS:
      break;
    case FAST_DOUBLE_ELEMENTS:
      access.type = Type::Number();
      access.write_barrier_kind = kNoWriteBarrier;
      access.machine_type = MachineType::Float64();
      break;
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      access.type = Type::Number();
      access.write_barrier_kind = kNoWriteBarrier;
      access.machine_type = MachineType::Float64();
      break;
    default:
      UNREACHABLE();
      break;
  }
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedDoubleArrayElement() {
  ElementAccess access = {kTaggedBase, FixedDoubleArray::kHeaderSize,
                          TypeCache::Get().kFloat64, MachineType::Float64(),
                          kNoWriteBarrier};
  return access;
}


// static
ElementAccess AccessBuilder::ForTypedArrayElement(ExternalArrayType type,
                                                  bool is_external) {
  BaseTaggedness taggedness = is_external ? kUntaggedBase : kTaggedBase;
  int header_size = is_external ? 0 : FixedTypedArrayBase::kDataOffset;
  switch (type) {
    case kExternalInt8Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              MachineType::Int8(), kNoWriteBarrier};
      return access;
    }
    case kExternalUint8Array:
    case kExternalUint8ClampedArray: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              MachineType::Uint8(), kNoWriteBarrier};
      return access;
    }
    case kExternalInt16Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              MachineType::Int16(), kNoWriteBarrier};
      return access;
    }
    case kExternalUint16Array: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              MachineType::Uint16(), kNoWriteBarrier};
      return access;
    }
    case kExternalInt32Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              MachineType::Int32(), kNoWriteBarrier};
      return access;
    }
    case kExternalUint32Array: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              MachineType::Uint32(), kNoWriteBarrier};
      return access;
    }
    case kExternalFloat32Array: {
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              MachineType::Float32(), kNoWriteBarrier};
      return access;
    }
    case kExternalFloat64Array: {
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              MachineType::Float64(), kNoWriteBarrier};
      return access;
    }
  }
  UNREACHABLE();
  ElementAccess access = {kUntaggedBase, 0, Type::None(), MachineType::None(),
                          kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForHashTableBaseNumberOfElements() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(HashTableBase::kNumberOfElementsIndex),
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForHashTableBaseNumberOfDeletedElement() {
  FieldAccess access = {
      kTaggedBase, FixedArray::OffsetOfElementAt(
                       HashTableBase::kNumberOfDeletedElementsIndex),
      MaybeHandle<Name>(), MaybeHandle<Map>(), Type::SignedSmall(),
      MachineType::TaggedSigned(), kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForHashTableBaseCapacity() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(HashTableBase::kCapacityIndex),
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForDictionaryMaxNumberKey() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(NameDictionary::kMaxNumberKeyIndex),
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
      Type::Any(),
      MachineType::AnyTagged(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForDictionaryNextEnumerationIndex() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(NameDictionary::kNextEnumerationIndexIndex),
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
