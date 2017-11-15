// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_BUILDER_H_
#define V8_COMPILER_ACCESS_BUILDER_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/simplified-operator.h"
#include "src/elements-kind.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// This access builder provides a set of static methods constructing commonly
// used FieldAccess and ElementAccess descriptors. These descriptors serve as
// parameters to simplified load/store operators.
class V8_EXPORT_PRIVATE AccessBuilder final
    : public NON_EXPORTED_BASE(AllStatic) {
 public:
  // ===========================================================================
  // Access to external values (based on external references).

  // Provides access to a double field identified by an external reference.
  static FieldAccess ForExternalDoubleValue();

  // Provides access to a tagged field identified by an external reference.
  static FieldAccess ForExternalTaggedValue();

  // Provides access to an uint8 field identified by an external reference.
  static FieldAccess ForExternalUint8Value();

  // ===========================================================================
  // Access to heap object fields and elements (based on tagged pointer).

  // Provides access to HeapObject::map() field.
  static FieldAccess ForMap();

  // Provides access to HeapNumber::value() field.
  static FieldAccess ForHeapNumberValue();

  // Provides access to JSObject::properties() field.
  static FieldAccess ForJSObjectProperties();

  // Provides access to JSObject::elements() field.
  static FieldAccess ForJSObjectElements();

  // Provides access to JSObject inobject property fields.
  static FieldAccess ForJSObjectInObjectProperty(Handle<Map> map, int index);
  static FieldAccess ForJSObjectOffset(
      int offset, WriteBarrierKind write_barrier_kind = kFullWriteBarrier);

  // Provides access to JSFunction::prototype_or_initial_map() field.
  static FieldAccess ForJSFunctionPrototypeOrInitialMap();

  // Provides access to JSFunction::context() field.
  static FieldAccess ForJSFunctionContext();

  // Provides access to JSFunction::shared() field.
  static FieldAccess ForJSFunctionSharedFunctionInfo();

  // Provides access to JSFunction::literals() field.
  static FieldAccess ForJSFunctionLiterals();

  // Provides access to JSFunction::code() field.
  static FieldAccess ForJSFunctionCodeEntry();

  // Provides access to JSFunction::next_function_link() field.
  static FieldAccess ForJSFunctionNextFunctionLink();

  // Provides access to JSGeneratorObject::context() field.
  static FieldAccess ForJSGeneratorObjectContext();

  // Provides access to JSGeneratorObject::continuation() field.
  static FieldAccess ForJSGeneratorObjectContinuation();

  // Provides access to JSGeneratorObject::input_or_debug_pos() field.
  static FieldAccess ForJSGeneratorObjectInputOrDebugPos();

  // Provides access to JSGeneratorObject::register_file() field.
  static FieldAccess ForJSGeneratorObjectRegisterFile();

  // Provides access to JSGeneratorObject::resume_mode() field.
  static FieldAccess ForJSGeneratorObjectResumeMode();

  // Provides access to JSArray::length() field.
  static FieldAccess ForJSArrayLength(ElementsKind elements_kind);

  // Provides access to JSArrayBuffer::backing_store() field.
  static FieldAccess ForJSArrayBufferBackingStore();

  // Provides access to JSArrayBuffer::bit_field() field.
  static FieldAccess ForJSArrayBufferBitField();

  // Provides access to JSArrayBufferView::buffer() field.
  static FieldAccess ForJSArrayBufferViewBuffer();

  // Provides access to JSArrayBufferView::byteLength() field.
  static FieldAccess ForJSArrayBufferViewByteLength();

  // Provides access to JSArrayBufferView::byteOffset() field.
  static FieldAccess ForJSArrayBufferViewByteOffset();

  // Provides access to JSTypedArray::length() field.
  static FieldAccess ForJSTypedArrayLength();

  // Provides access to JSDate::value() field.
  static FieldAccess ForJSDateValue();

  // Provides access to JSDate fields.
  static FieldAccess ForJSDateField(JSDate::FieldIndex index);

  // Provides access to JSIteratorResult::done() field.
  static FieldAccess ForJSIteratorResultDone();

  // Provides access to JSIteratorResult::value() field.
  static FieldAccess ForJSIteratorResultValue();

  // Provides access to JSRegExp::flags() field.
  static FieldAccess ForJSRegExpFlags();

  // Provides access to JSRegExp::source() field.
  static FieldAccess ForJSRegExpSource();

  // Provides access to FixedArray::length() field.
  static FieldAccess ForFixedArrayLength();

  // Provides access to FixedTypedArrayBase::base_pointer() field.
  static FieldAccess ForFixedTypedArrayBaseBasePointer();

  // Provides access to FixedTypedArrayBase::external_pointer() field.
  static FieldAccess ForFixedTypedArrayBaseExternalPointer();

  // Provides access to DescriptorArray::enum_cache() field.
  static FieldAccess ForDescriptorArrayEnumCache();

  // Provides access to DescriptorArray::enum_cache_bridge_cache() field.
  static FieldAccess ForDescriptorArrayEnumCacheBridgeCache();

  // Provides access to Map::bit_field() byte.
  static FieldAccess ForMapBitField();

  // Provides access to Map::bit_field3() field.
  static FieldAccess ForMapBitField3();

  // Provides access to Map::descriptors() field.
  static FieldAccess ForMapDescriptors();

  // Provides access to Map::instance_type() field.
  static FieldAccess ForMapInstanceType();

  // Provides access to Map::prototype() field.
  static FieldAccess ForMapPrototype();

  // Provides access to Module::regular_exports() field.
  static FieldAccess ForModuleRegularExports();

  // Provides access to Module::regular_imports() field.
  static FieldAccess ForModuleRegularImports();

  // Provides access to Name::hash_field() field.
  static FieldAccess ForNameHashField();

  // Provides access to String::length() field.
  static FieldAccess ForStringLength();

  // Provides access to ConsString::first() field.
  static FieldAccess ForConsStringFirst();

  // Provides access to ConsString::second() field.
  static FieldAccess ForConsStringSecond();

  // Provides access to SlicedString::offset() field.
  static FieldAccess ForSlicedStringOffset();

  // Provides access to SlicedString::parent() field.
  static FieldAccess ForSlicedStringParent();

  // Provides access to ExternalString::resource_data() field.
  static FieldAccess ForExternalStringResourceData();

  // Provides access to ExternalOneByteString characters.
  static ElementAccess ForExternalOneByteStringCharacter();

  // Provides access to ExternalTwoByteString characters.
  static ElementAccess ForExternalTwoByteStringCharacter();

  // Provides access to SeqOneByteString characters.
  static ElementAccess ForSeqOneByteStringCharacter();

  // Provides access to SeqTwoByteString characters.
  static ElementAccess ForSeqTwoByteStringCharacter();

  // Provides access to JSGlobalObject::global_proxy() field.
  static FieldAccess ForJSGlobalObjectGlobalProxy();

  // Provides access to JSGlobalObject::native_context() field.
  static FieldAccess ForJSGlobalObjectNativeContext();

  // Provides access to JSArrayIterator::object() field.
  static FieldAccess ForJSArrayIteratorObject();

  // Provides access to JSArrayIterator::index() field.
  static FieldAccess ForJSArrayIteratorIndex(InstanceType type = JS_OBJECT_TYPE,
                                             ElementsKind kind = NO_ELEMENTS);

  // Provides access to JSArrayIterator::object_map() field.
  static FieldAccess ForJSArrayIteratorObjectMap();

  // Provides access to JSStringIterator::string() field.
  static FieldAccess ForJSStringIteratorString();

  // Provides access to JSStringIterator::index() field.
  static FieldAccess ForJSStringIteratorIndex();

  // Provides access to JSValue::value() field.
  static FieldAccess ForValue();

  // Provides access to Cell::value() field.
  static FieldAccess ForCellValue();

  // Provides access to arguments object fields.
  static FieldAccess ForArgumentsLength();
  static FieldAccess ForArgumentsCallee();

  // Provides access to FixedArray slots.
  static FieldAccess ForFixedArraySlot(
      size_t index, WriteBarrierKind write_barrier_kind = kFullWriteBarrier);

  // Provides access to Context slots.
  static FieldAccess ForContextSlot(size_t index);

  // Provides access to ContextExtension fields.
  static FieldAccess ForContextExtensionScopeInfo();
  static FieldAccess ForContextExtensionExtension();

  // Provides access to FixedArray elements.
  static ElementAccess ForFixedArrayElement();
  static ElementAccess ForFixedArrayElement(ElementsKind kind);

  // Provides access to FixedDoubleArray elements.
  static ElementAccess ForFixedDoubleArrayElement();

  // Provides access to Fixed{type}TypedArray and External{type}Array elements.
  static ElementAccess ForTypedArrayElement(ExternalArrayType type,
                                            bool is_external);

  // Provides access to HashTable fields.
  static FieldAccess ForHashTableBaseNumberOfElements();
  static FieldAccess ForHashTableBaseNumberOfDeletedElement();
  static FieldAccess ForHashTableBaseCapacity();

  // Provides access to Dictionary fields.
  static FieldAccess ForDictionaryMaxNumberKey();
  static FieldAccess ForDictionaryNextEnumerationIndex();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AccessBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ACCESS_BUILDER_H_
