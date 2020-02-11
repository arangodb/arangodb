// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_API_CALLBACKS_H_
#define V8_OBJECTS_API_CALLBACKS_H_

#include "src/objects/struct.h"
#include "torque-generated/class-definitions-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// An accessor must have a getter, but can have no setter.
//
// When setting a property, V8 searches accessors in prototypes.
// If an accessor was found and it does not have a setter,
// the request is ignored.
//
// If the accessor in the prototype has the READ_ONLY property attribute, then
// a new value is added to the derived object when the property is set.
// This shadows the accessor in the prototype.
class AccessorInfo : public Struct {
 public:
  DECL_ACCESSORS(name, Name)
  DECL_INT_ACCESSORS(flags)
  DECL_ACCESSORS(expected_receiver_type, Object)
  // This directly points at a foreign C function to be used from the runtime.
  DECL_ACCESSORS(getter, Object)
  inline bool has_getter();
  DECL_ACCESSORS(setter, Object)
  inline bool has_setter();
  // This either points at the same as above, or a trampoline in case we are
  // running with the simulator. Use these entries from generated code.
  DECL_ACCESSORS(js_getter, Object)
  DECL_ACCESSORS(data, Object)

  static Address redirect(Address address, AccessorComponent component);
  Address redirected_getter() const;

  // Dispatched behavior.
  DECL_PRINTER(AccessorInfo)

  DECL_BOOLEAN_ACCESSORS(all_can_read)
  DECL_BOOLEAN_ACCESSORS(all_can_write)
  DECL_BOOLEAN_ACCESSORS(is_special_data_property)
  DECL_BOOLEAN_ACCESSORS(replace_on_access)
  DECL_BOOLEAN_ACCESSORS(is_sloppy)

  inline SideEffectType getter_side_effect_type() const;
  inline void set_getter_side_effect_type(SideEffectType type);

  inline SideEffectType setter_side_effect_type() const;
  inline void set_setter_side_effect_type(SideEffectType type);

  // The property attributes used when an API object template is instantiated
  // for the first time. Changing of this value afterwards does not affect
  // the actual attributes of a property.
  inline PropertyAttributes initial_property_attributes() const;
  inline void set_initial_property_attributes(PropertyAttributes attributes);

  // Checks whether the given receiver is compatible with this accessor.
  static bool IsCompatibleReceiverMap(Handle<AccessorInfo> info,
                                      Handle<Map> map);
  inline bool IsCompatibleReceiver(Object receiver);

  DECL_CAST(AccessorInfo)

  // Dispatched behavior.
  DECL_VERIFIER(AccessorInfo)

  // Append all descriptors to the array that are not already there.
  // Return number added.
  static int AppendUnique(Isolate* isolate, Handle<Object> descriptors,
                          Handle<FixedArray> array, int valid_descriptors);

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_ACCESSOR_INFO_FIELDS)

 private:
  inline bool HasExpectedReceiverType();

// Bit positions in |flags|.
#define ACCESSOR_INFO_FLAGS_BIT_FIELDS(V, _)                           \
  V(AllCanReadBit, bool, 1, _)                                         \
  V(AllCanWriteBit, bool, 1, _)                                        \
  V(IsSpecialDataPropertyBit, bool, 1, _)                              \
  V(IsSloppyBit, bool, 1, _)                                           \
  V(ReplaceOnAccessBit, bool, 1, _)                                    \
  V(GetterSideEffectTypeBits, SideEffectType, 2, _)                    \
  /* We could save a bit from setter side-effect type, if necessary */ \
  V(SetterSideEffectTypeBits, SideEffectType, 2, _)                    \
  V(InitialAttributesBits, PropertyAttributes, 3, _)

  DEFINE_BIT_FIELDS(ACCESSOR_INFO_FLAGS_BIT_FIELDS)
#undef ACCESSOR_INFO_FLAGS_BIT_FIELDS

  OBJECT_CONSTRUCTORS(AccessorInfo, Struct);
};

class AccessCheckInfo
    : public TorqueGeneratedAccessCheckInfo<AccessCheckInfo, Struct> {
 public:
  // Dispatched behavior.
  DECL_PRINTER(AccessCheckInfo)

  static AccessCheckInfo Get(Isolate* isolate, Handle<JSObject> receiver);

  TQ_OBJECT_CONSTRUCTORS(AccessCheckInfo)
};

class InterceptorInfo
    : public TorqueGeneratedInterceptorInfo<InterceptorInfo, Struct> {
 public:
  DECL_BOOLEAN_ACCESSORS(can_intercept_symbols)
  DECL_BOOLEAN_ACCESSORS(all_can_read)
  DECL_BOOLEAN_ACCESSORS(non_masking)
  DECL_BOOLEAN_ACCESSORS(is_named)
  DECL_BOOLEAN_ACCESSORS(has_no_side_effect)

  inline int flags() const;
  inline void set_flags(int flags);

  // Dispatched behavior.
  DECL_PRINTER(InterceptorInfo)

  static const int kCanInterceptSymbolsBit = 0;
  static const int kAllCanReadBit = 1;
  static const int kNonMasking = 2;
  static const int kNamed = 3;
  static const int kHasNoSideEffect = 4;

  TQ_OBJECT_CONSTRUCTORS(InterceptorInfo)
};

class CallHandlerInfo
    : public TorqueGeneratedCallHandlerInfo<CallHandlerInfo, Struct> {
 public:
  inline bool IsSideEffectFreeCallHandlerInfo() const;
  inline bool IsSideEffectCallHandlerInfo() const;
  inline void SetNextCallHasNoSideEffect();
  // Returns whether or not the next call can be side effect free.
  // Calling this will change the state back to having a side effect.
  inline bool NextCallHasNoSideEffect();

  // Dispatched behavior.
  DECL_PRINTER(CallHandlerInfo)
  DECL_VERIFIER(CallHandlerInfo)

  Address redirected_callback() const;

  TQ_OBJECT_CONSTRUCTORS(CallHandlerInfo)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_API_CALLBACKS_H_
