// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return cp; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {r2, r3, r4, r5, r6};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

const Register FastNewFunctionContextDescriptor::FunctionRegister() {
  return r3;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return r2; }

const Register LoadDescriptor::ReceiverRegister() { return r3; }
const Register LoadDescriptor::NameRegister() { return r4; }
const Register LoadDescriptor::SlotRegister() { return r2; }

const Register LoadWithVectorDescriptor::VectorRegister() { return r5; }

const Register LoadICProtoArrayDescriptor::HandlerRegister() { return r6; }

const Register StoreDescriptor::ReceiverRegister() { return r3; }
const Register StoreDescriptor::NameRegister() { return r4; }
const Register StoreDescriptor::ValueRegister() { return r2; }
const Register StoreDescriptor::SlotRegister() { return r6; }

const Register StoreWithVectorDescriptor::VectorRegister() { return r5; }

const Register StoreTransitionDescriptor::SlotRegister() { return r6; }
const Register StoreTransitionDescriptor::VectorRegister() { return r5; }
const Register StoreTransitionDescriptor::MapRegister() { return r7; }

const Register StringCompareDescriptor::LeftRegister() { return r3; }
const Register StringCompareDescriptor::RightRegister() { return r2; }

const Register ApiGetterDescriptor::HolderRegister() { return r2; }
const Register ApiGetterDescriptor::CallbackRegister() { return r5; }

const Register MathPowTaggedDescriptor::exponent() { return r4; }

const Register MathPowIntegerDescriptor::exponent() {
  return MathPowTaggedDescriptor::exponent();
}

const Register GrowArrayElementsDescriptor::ObjectRegister() { return r2; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return r5; }

void FastNewClosureDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r4, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewRestParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewSloppyArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewStrictArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

// static
const Register TypeConversionDescriptor::ArgumentRegister() { return r2; }

void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastCloneRegExpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r5, r4, r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastCloneShallowArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r5, r4, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastCloneShallowObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r5, r4, r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CreateAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CreateWeakCellDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4, r5, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionWithFeedbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2, r5, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r3 : the function to call
  // r4 : feedback vector
  // r5 : slot in feedback vector (Smi, for RecordCallTarget)
  // r6 : new target (for IsSuperConstructorCall)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {r2, r3, r6, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r3 : the target to call
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r3 : the target to call
  // r5 : the new target
  // r4 : allocation site or undefined
  Register registers[] = {r3, r5, r2, r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // r2 : number of arguments
  // r3 : the target to call
  // r5 : the new target
  Register registers[] = {r3, r5, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void TransitionElementsKindDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r2, r3};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr, nullptr);
}

#define SIMD128_ALLOC_DESC(TYPE, Type, type, lane_count, lane_type) \
  void Allocate##Type##Descriptor::InitializePlatformSpecific(      \
      CallInterfaceDescriptorData* data) {                          \
    data->InitializePlatformSpecific(0, nullptr, nullptr);          \
  }
SIMD128_TYPES(SIMD128_ALLOC_DESC)
#undef SIMD128_ALLOC_DESC

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r2 -- number of arguments
  // r3 -- function
  // r4 -- allocation site with elements kind
  Register registers[] = {r3, r4, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r2 -- number of arguments
  // r3 -- function
  // r4 -- allocation site with elements kind
  Register registers[] = {r3, r4, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers, NULL);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (constructor pointer, and single argument)
  Register registers[] = {r3, r4, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void VarArgFunctionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // stack param count needs (arg count)
  Register registers[] = {r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpWithAllocationSiteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4, r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // register state
  // r3 -- lhs
  // r2 -- rhs
  // r6 -- slot id
  // r5 -- vector
  Register registers[] = {r3, r2, r6, r5};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CountOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r4};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StringAddDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {r3, r2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void KeyedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r4,  // key
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void NamedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r4,  // name
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallHandlerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // receiver
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r3,  // JSFunction
      r5,  // the new target
      r2,  // actual number of arguments
      r4,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // callee
      r6,  // call_data
      r4,  // holder
      r3,  // api_function_address
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterDispatchDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // argument count (not including receiver)
      r4,  // address of first argument
      r3   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // argument count (not including receiver)
      r5,  // new target
      r3,  // constructor to call
      r4,  // allocation site feedback if available, undefined otherwise
      r6   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsAndConstructArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // argument count (not including receiver)
      r3,  // target to call checked to be Array function
      r4,  // allocation site feedback if available, undefined otherwise
      r5   // address of the first argument
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterCEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // argument count (argc)
      r4,  // address of first argument (argv)
      r3   // the runtime function to call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      r2,  // the value to pass to the generator
      r3,  // the JSGeneratorObject to resume
      r4   // the resume mode (tagged)
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
