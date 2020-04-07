// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_BAILOUT_REASON_H_
#define V8_CODEGEN_BAILOUT_REASON_H_

#include <cstdint>

namespace v8 {
namespace internal {

#define ABORT_MESSAGES_LIST(V)                                                \
  V(kNoReason, "no reason")                                                   \
                                                                              \
  V(k32BitValueInRegisterIsNotZeroExtended,                                   \
    "32 bit value in register is not zero-extended")                          \
  V(kAPICallReturnedInvalidObject, "API call returned invalid object")        \
  V(kAllocatingNonEmptyPackedArray, "Allocating non-empty packed array")      \
  V(kAllocationIsNotDoubleAligned, "Allocation is not double aligned")        \
  V(kExpectedOptimizationSentinel,                                            \
    "Expected optimized code cell or optimization sentinel")                  \
  V(kExpectedUndefinedOrCell, "Expected undefined or cell in register")       \
  V(kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,                     \
    "The function_data field should be a BytecodeArray on interpreter entry") \
  V(kInputStringTooLong, "Input string too long")                             \
  V(kInvalidBytecode, "Invalid bytecode")                                     \
  V(kInvalidBytecodeAdvance, "Cannot advance current bytecode, ")             \
  V(kInvalidElementsKindForInternalPackedArray,                               \
    "Invalid ElementsKind for InternalPackedArray")                           \
  V(kInvalidHandleScopeLevel, "Invalid HandleScope level")                    \
  V(kInvalidJumpTableIndex, "Invalid jump table index")                       \
  V(kInvalidParametersAndRegistersInGenerator,                                \
    "invalid parameters and registers in generator")                          \
  V(kMissingBytecodeArray, "Missing bytecode array from function")            \
  V(kObjectNotTagged, "The object is not tagged")                             \
  V(kObjectTagged, "The object is tagged")                                    \
  V(kOffsetOutOfRange, "Offset out of range")                                 \
  V(kOperandIsASmi, "Operand is a smi")                                       \
  V(kOperandIsASmiAndNotABoundFunction,                                       \
    "Operand is a smi and not a bound function")                              \
  V(kOperandIsASmiAndNotAConstructor,                                         \
    "Operand is a smi and not a constructor")                                 \
  V(kOperandIsASmiAndNotAFunction, "Operand is a smi and not a function")     \
  V(kOperandIsASmiAndNotAGeneratorObject,                                     \
    "Operand is a smi and not a generator object")                            \
  V(kOperandIsNotABoundFunction, "Operand is not a bound function")           \
  V(kOperandIsNotAConstructor, "Operand is not a constructor")                \
  V(kOperandIsNotAFixedArray, "Operand is not a fixed array")                 \
  V(kOperandIsNotAFunction, "Operand is not a function")                      \
  V(kOperandIsNotAGeneratorObject, "Operand is not a generator object")       \
  V(kOperandIsNotASmi, "Operand is not a smi")                                \
  V(kPromiseAlreadySettled, "Promise already settled")                        \
  V(kReceivedInvalidReturnAddress, "Received invalid return address")         \
  V(kRegisterDidNotMatchExpectedRoot, "Register did not match expected root") \
  V(kReturnAddressNotFoundInFrame, "Return address not found in frame")       \
  V(kShouldNotDirectlyEnterOsrFunction,                                       \
    "Should not directly enter OSR-compiled function")                        \
  V(kStackAccessBelowStackPointer, "Stack access below stack pointer")        \
  V(kStackFrameTypesMustMatch, "Stack frame types must match")                \
  V(kUnalignedCellInWriteBarrier, "Unaligned cell in write barrier")          \
  V(kUnexpectedElementsKindInArrayConstructor,                                \
    "Unexpected ElementsKind in array constructor")                           \
  V(kUnexpectedFPCRMode, "Unexpected FPCR mode.")                             \
  V(kUnexpectedFunctionIDForInvokeIntrinsic,                                  \
    "Unexpected runtime function id for the InvokeIntrinsic bytecode")        \
  V(kUnexpectedInitialMapForArrayFunction,                                    \
    "Unexpected initial map for Array function")                              \
  V(kUnexpectedInitialMapForInternalArrayFunction,                            \
    "Unexpected initial map for InternalArray function")                      \
  V(kUnexpectedLevelAfterReturnFromApiCall,                                   \
    "Unexpected level after return from api call")                            \
  V(kUnexpectedNegativeValue, "Unexpected negative value")                    \
  V(kUnexpectedReturnFromFrameDropper,                                        \
    "Unexpectedly returned from dropping frames")                             \
  V(kUnexpectedReturnFromThrow, "Unexpectedly returned from a throw")         \
  V(kUnexpectedReturnFromWasmTrap,                                            \
    "Should not return after throwing a wasm trap")                           \
  V(kUnexpectedStackPointer, "The stack pointer is not the expected value")   \
  V(kUnexpectedValue, "Unexpected value")                                     \
  V(kUnsupportedModuleOperation, "Unsupported module operation")              \
  V(kUnsupportedNonPrimitiveCompare, "Unsupported non-primitive compare")     \
  V(kWrongAddressOrValuePassedToRecordWrite,                                  \
    "Wrong address or value passed to RecordWrite")                           \
  V(kWrongArgumentCountForInvokeIntrinsic,                                    \
    "Wrong number of arguments for intrinsic")                                \
  V(kWrongFunctionCodeStart, "Wrong value in code start register passed")     \
  V(kWrongFunctionContext, "Wrong context passed to function")                \
  V(kWrongNumberOfArgumentsForInternalPackedArray,                            \
    "Wrong number of arguments for InternalPackedArray")                      \
  V(kUnexpectedThreadInWasmSet, "thread_in_wasm flag was already set")        \
  V(kUnexpectedThreadInWasmUnset, "thread_in_wasm flag was not set")

#define BAILOUT_MESSAGES_LIST(V)                                            \
  V(kNoReason, "no reason")                                                 \
                                                                            \
  V(kBailedOutDueToDependencyChange, "Bailed out due to dependency change") \
  V(kCodeGenerationFailed, "Code generation failed")                        \
  V(kCyclicObjectStateDetectedInEscapeAnalysis,                             \
    "Cyclic object state detected by escape analysis")                      \
  V(kFunctionBeingDebugged, "Function is being debugged")                   \
  V(kGraphBuildingFailed, "Optimized graph construction failed")            \
  V(kFunctionTooBig, "Function is too big to be optimized")                 \
  V(kLiveEdit, "LiveEdit")                                                  \
  V(kNativeFunctionLiteral, "Native function literal")                      \
  V(kNotEnoughVirtualRegistersRegalloc,                                     \
    "Not enough virtual registers (regalloc)")                              \
  V(kOptimizationDisabled, "Optimization disabled")                         \
  V(kNeverOptimize, "Optimization is always disabled")

#define ERROR_MESSAGES_CONSTANTS(C, T) C,
enum class BailoutReason : uint8_t {
  BAILOUT_MESSAGES_LIST(ERROR_MESSAGES_CONSTANTS) kLastErrorMessage
};

enum class AbortReason : uint8_t {
  ABORT_MESSAGES_LIST(ERROR_MESSAGES_CONSTANTS) kLastErrorMessage
};
#undef ERROR_MESSAGES_CONSTANTS

const char* GetBailoutReason(BailoutReason reason);
const char* GetAbortReason(AbortReason reason);
bool IsValidAbortReason(int reason_id);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_BAILOUT_REASON_H_
