// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODES_H_
#define V8_INTERPRETER_BYTECODES_H_

#include <cstdint>
#include <iosfwd>
#include <string>

#include "src/globals.h"
#include "src/interpreter/bytecode-operands.h"

// This interface and it's implementation are independent of the
// libv8_base library as they are used by the interpreter and the
// standalone mkpeephole table generator program.

namespace v8 {
namespace internal {
namespace interpreter {

// The list of bytecodes which are interpreted by the interpreter.
// Format is V(<bytecode>, <accumulator_use>, <operands>).
#define BYTECODE_LIST(V)                                                       \
  /* Extended width operands */                                                \
  V(Wide, AccumulatorUse::kNone)                                               \
  V(ExtraWide, AccumulatorUse::kNone)                                          \
                                                                               \
  /* Loading the accumulator */                                                \
  V(LdaZero, AccumulatorUse::kWrite)                                           \
  V(LdaSmi, AccumulatorUse::kWrite, OperandType::kImm)                         \
  V(LdaUndefined, AccumulatorUse::kWrite)                                      \
  V(LdaNull, AccumulatorUse::kWrite)                                           \
  V(LdaTheHole, AccumulatorUse::kWrite)                                        \
  V(LdaTrue, AccumulatorUse::kWrite)                                           \
  V(LdaFalse, AccumulatorUse::kWrite)                                          \
  V(LdaConstant, AccumulatorUse::kWrite, OperandType::kIdx)                    \
                                                                               \
  /* Globals */                                                                \
  V(LdaGlobal, AccumulatorUse::kWrite, OperandType::kIdx, OperandType::kIdx)   \
  V(LdaGlobalInsideTypeof, AccumulatorUse::kWrite, OperandType::kIdx,          \
    OperandType::kIdx)                                                         \
  V(StaGlobalSloppy, AccumulatorUse::kRead, OperandType::kIdx,                 \
    OperandType::kIdx)                                                         \
  V(StaGlobalStrict, AccumulatorUse::kRead, OperandType::kIdx,                 \
    OperandType::kIdx)                                                         \
                                                                               \
  /* Context operations */                                                     \
  V(PushContext, AccumulatorUse::kRead, OperandType::kRegOut)                  \
  V(PopContext, AccumulatorUse::kNone, OperandType::kReg)                      \
  V(LdaContextSlot, AccumulatorUse::kWrite, OperandType::kReg,                 \
    OperandType::kIdx, OperandType::kUImm)                                     \
  V(LdaCurrentContextSlot, AccumulatorUse::kWrite, OperandType::kIdx)          \
  V(StaContextSlot, AccumulatorUse::kRead, OperandType::kReg,                  \
    OperandType::kIdx, OperandType::kUImm)                                     \
  V(StaCurrentContextSlot, AccumulatorUse::kRead, OperandType::kIdx)           \
                                                                               \
  /* Load-Store lookup slots */                                                \
  V(LdaLookupSlot, AccumulatorUse::kWrite, OperandType::kIdx)                  \
  V(LdaLookupContextSlot, AccumulatorUse::kWrite, OperandType::kIdx,           \
    OperandType::kIdx, OperandType::kUImm)                                     \
  V(LdaLookupGlobalSlot, AccumulatorUse::kWrite, OperandType::kIdx,            \
    OperandType::kIdx, OperandType::kUImm)                                     \
  V(LdaLookupSlotInsideTypeof, AccumulatorUse::kWrite, OperandType::kIdx)      \
  V(LdaLookupContextSlotInsideTypeof, AccumulatorUse::kWrite,                  \
    OperandType::kIdx, OperandType::kIdx, OperandType::kUImm)                  \
  V(LdaLookupGlobalSlotInsideTypeof, AccumulatorUse::kWrite,                   \
    OperandType::kIdx, OperandType::kIdx, OperandType::kUImm)                  \
  V(StaLookupSlotSloppy, AccumulatorUse::kReadWrite, OperandType::kIdx)        \
  V(StaLookupSlotStrict, AccumulatorUse::kReadWrite, OperandType::kIdx)        \
                                                                               \
  /* Register-accumulator transfers */                                         \
  V(Ldar, AccumulatorUse::kWrite, OperandType::kReg)                           \
  V(Star, AccumulatorUse::kRead, OperandType::kRegOut)                         \
                                                                               \
  /* Register-register transfers */                                            \
  V(Mov, AccumulatorUse::kNone, OperandType::kReg, OperandType::kRegOut)       \
                                                                               \
  /* Property loads (LoadIC) operations */                                     \
  V(LdaNamedProperty, AccumulatorUse::kWrite, OperandType::kReg,               \
    OperandType::kIdx, OperandType::kIdx)                                      \
  V(LdaKeyedProperty, AccumulatorUse::kReadWrite, OperandType::kReg,           \
    OperandType::kIdx)                                                         \
                                                                               \
  /* Operations on module variables */                                         \
  V(LdaModuleVariable, AccumulatorUse::kWrite, OperandType::kImm,              \
    OperandType::kUImm)                                                        \
  V(StaModuleVariable, AccumulatorUse::kRead, OperandType::kImm,               \
    OperandType::kUImm)                                                        \
                                                                               \
  /* Propery stores (StoreIC) operations */                                    \
  V(StaNamedPropertySloppy, AccumulatorUse::kRead, OperandType::kReg,          \
    OperandType::kIdx, OperandType::kIdx)                                      \
  V(StaNamedPropertyStrict, AccumulatorUse::kRead, OperandType::kReg,          \
    OperandType::kIdx, OperandType::kIdx)                                      \
  V(StaKeyedPropertySloppy, AccumulatorUse::kRead, OperandType::kReg,          \
    OperandType::kReg, OperandType::kIdx)                                      \
  V(StaKeyedPropertyStrict, AccumulatorUse::kRead, OperandType::kReg,          \
    OperandType::kReg, OperandType::kIdx)                                      \
  V(StaDataPropertyInLiteral, AccumulatorUse::kRead, OperandType::kReg,        \
    OperandType::kReg, OperandType::kFlag8, OperandType::kIdx)                 \
                                                                               \
  /* Binary Operators */                                                       \
  V(Add, AccumulatorUse::kReadWrite, OperandType::kReg, OperandType::kIdx)     \
  V(Sub, AccumulatorUse::kReadWrite, OperandType::kReg, OperandType::kIdx)     \
  V(Mul, AccumulatorUse::kReadWrite, OperandType::kReg, OperandType::kIdx)     \
  V(Div, AccumulatorUse::kReadWrite, OperandType::kReg, OperandType::kIdx)     \
  V(Mod, AccumulatorUse::kReadWrite, OperandType::kReg, OperandType::kIdx)     \
  V(BitwiseOr, AccumulatorUse::kReadWrite, OperandType::kReg,                  \
    OperandType::kIdx)                                                         \
  V(BitwiseXor, AccumulatorUse::kReadWrite, OperandType::kReg,                 \
    OperandType::kIdx)                                                         \
  V(BitwiseAnd, AccumulatorUse::kReadWrite, OperandType::kReg,                 \
    OperandType::kIdx)                                                         \
  V(ShiftLeft, AccumulatorUse::kReadWrite, OperandType::kReg,                  \
    OperandType::kIdx)                                                         \
  V(ShiftRight, AccumulatorUse::kReadWrite, OperandType::kReg,                 \
    OperandType::kIdx)                                                         \
  V(ShiftRightLogical, AccumulatorUse::kReadWrite, OperandType::kReg,          \
    OperandType::kIdx)                                                         \
                                                                               \
  /* Binary operators with immediate operands */                               \
  V(AddSmi, AccumulatorUse::kWrite, OperandType::kImm, OperandType::kReg,      \
    OperandType::kIdx)                                                         \
  V(SubSmi, AccumulatorUse::kWrite, OperandType::kImm, OperandType::kReg,      \
    OperandType::kIdx)                                                         \
  V(BitwiseOrSmi, AccumulatorUse::kWrite, OperandType::kImm,                   \
    OperandType::kReg, OperandType::kIdx)                                      \
  V(BitwiseAndSmi, AccumulatorUse::kWrite, OperandType::kImm,                  \
    OperandType::kReg, OperandType::kIdx)                                      \
  V(ShiftLeftSmi, AccumulatorUse::kWrite, OperandType::kImm,                   \
    OperandType::kReg, OperandType::kIdx)                                      \
  V(ShiftRightSmi, AccumulatorUse::kWrite, OperandType::kImm,                  \
    OperandType::kReg, OperandType::kIdx)                                      \
                                                                               \
  /* Unary Operators */                                                        \
  V(Inc, AccumulatorUse::kReadWrite, OperandType::kIdx)                        \
  V(Dec, AccumulatorUse::kReadWrite, OperandType::kIdx)                        \
  V(ToBooleanLogicalNot, AccumulatorUse::kReadWrite)                           \
  V(LogicalNot, AccumulatorUse::kReadWrite)                                    \
  V(TypeOf, AccumulatorUse::kReadWrite)                                        \
  V(DeletePropertyStrict, AccumulatorUse::kReadWrite, OperandType::kReg)       \
  V(DeletePropertySloppy, AccumulatorUse::kReadWrite, OperandType::kReg)       \
                                                                               \
  /* GetSuperConstructor operator */                                           \
  V(GetSuperConstructor, AccumulatorUse::kRead, OperandType::kRegOut)          \
                                                                               \
  /* Call operations */                                                        \
  V(Call, AccumulatorUse::kWrite, OperandType::kReg, OperandType::kRegList,    \
    OperandType::kRegCount, OperandType::kIdx)                                 \
  V(CallProperty, AccumulatorUse::kWrite, OperandType::kReg,                   \
    OperandType::kRegList, OperandType::kRegCount, OperandType::kIdx)          \
  V(TailCall, AccumulatorUse::kWrite, OperandType::kReg,                       \
    OperandType::kRegList, OperandType::kRegCount, OperandType::kIdx)          \
  V(CallRuntime, AccumulatorUse::kWrite, OperandType::kRuntimeId,              \
    OperandType::kRegList, OperandType::kRegCount)                             \
  V(CallRuntimeForPair, AccumulatorUse::kNone, OperandType::kRuntimeId,        \
    OperandType::kRegList, OperandType::kRegCount, OperandType::kRegOutPair)   \
  V(CallJSRuntime, AccumulatorUse::kWrite, OperandType::kIdx,                  \
    OperandType::kRegList, OperandType::kRegCount)                             \
                                                                               \
  /* Intrinsics */                                                             \
  V(InvokeIntrinsic, AccumulatorUse::kWrite, OperandType::kIntrinsicId,        \
    OperandType::kRegList, OperandType::kRegCount)                             \
                                                                               \
  /* New operators */                                                          \
  V(New, AccumulatorUse::kReadWrite, OperandType::kReg, OperandType::kRegList, \
    OperandType::kRegCount, OperandType::kIdx)                                 \
  V(NewWithSpread, AccumulatorUse::kWrite, OperandType::kRegList,              \
    OperandType::kRegCount)                                                    \
                                                                               \
  /* Test Operators */                                                         \
  V(TestEqual, AccumulatorUse::kReadWrite, OperandType::kReg,                  \
    OperandType::kIdx)                                                         \
  V(TestNotEqual, AccumulatorUse::kReadWrite, OperandType::kReg,               \
    OperandType::kIdx)                                                         \
  V(TestEqualStrict, AccumulatorUse::kReadWrite, OperandType::kReg,            \
    OperandType::kIdx)                                                         \
  V(TestLessThan, AccumulatorUse::kReadWrite, OperandType::kReg,               \
    OperandType::kIdx)                                                         \
  V(TestGreaterThan, AccumulatorUse::kReadWrite, OperandType::kReg,            \
    OperandType::kIdx)                                                         \
  V(TestLessThanOrEqual, AccumulatorUse::kReadWrite, OperandType::kReg,        \
    OperandType::kIdx)                                                         \
  V(TestGreaterThanOrEqual, AccumulatorUse::kReadWrite, OperandType::kReg,     \
    OperandType::kIdx)                                                         \
  V(TestInstanceOf, AccumulatorUse::kReadWrite, OperandType::kReg)             \
  V(TestIn, AccumulatorUse::kReadWrite, OperandType::kReg)                     \
                                                                               \
  /* TestEqual with Null or Undefined */                                       \
  V(TestUndetectable, AccumulatorUse::kWrite, OperandType::kReg)               \
  V(TestNull, AccumulatorUse::kWrite, OperandType::kReg)                       \
  V(TestUndefined, AccumulatorUse::kWrite, OperandType::kReg)                  \
                                                                               \
  /* Cast operators */                                                         \
  V(ToName, AccumulatorUse::kRead, OperandType::kRegOut)                       \
  V(ToNumber, AccumulatorUse::kRead, OperandType::kRegOut)                     \
  V(ToObject, AccumulatorUse::kRead, OperandType::kRegOut)                     \
                                                                               \
  /* Literals */                                                               \
  V(CreateRegExpLiteral, AccumulatorUse::kWrite, OperandType::kIdx,            \
    OperandType::kIdx, OperandType::kFlag8)                                    \
  V(CreateArrayLiteral, AccumulatorUse::kWrite, OperandType::kIdx,             \
    OperandType::kIdx, OperandType::kFlag8)                                    \
  V(CreateObjectLiteral, AccumulatorUse::kNone, OperandType::kIdx,             \
    OperandType::kIdx, OperandType::kFlag8, OperandType::kRegOut)              \
                                                                               \
  /* Closure allocation */                                                     \
  V(CreateClosure, AccumulatorUse::kWrite, OperandType::kIdx,                  \
    OperandType::kIdx, OperandType::kFlag8)                                    \
                                                                               \
  /* Context allocation */                                                     \
  V(CreateBlockContext, AccumulatorUse::kReadWrite, OperandType::kIdx)         \
  V(CreateCatchContext, AccumulatorUse::kReadWrite, OperandType::kReg,         \
    OperandType::kIdx, OperandType::kIdx)                                      \
  V(CreateFunctionContext, AccumulatorUse::kWrite, OperandType::kUImm)         \
  V(CreateEvalContext, AccumulatorUse::kWrite, OperandType::kUImm)             \
  V(CreateWithContext, AccumulatorUse::kReadWrite, OperandType::kReg,          \
    OperandType::kIdx)                                                         \
                                                                               \
  /* Arguments allocation */                                                   \
  V(CreateMappedArguments, AccumulatorUse::kWrite)                             \
  V(CreateUnmappedArguments, AccumulatorUse::kWrite)                           \
  V(CreateRestParameter, AccumulatorUse::kWrite)                               \
                                                                               \
  /* Control Flow -- carefully ordered for efficient checks */                 \
  /* - [Unconditional jumps] */                                                \
  V(JumpLoop, AccumulatorUse::kNone, OperandType::kImm, OperandType::kImm)     \
  /* - [Forward jumps] */                                                      \
  V(Jump, AccumulatorUse::kNone, OperandType::kImm)                            \
  /* - [Start constant jumps] */                                               \
  V(JumpConstant, AccumulatorUse::kNone, OperandType::kIdx)                    \
  /* - [Conditional jumps] */                                                  \
  /* - [Conditional constant jumps] */                                         \
  V(JumpIfNullConstant, AccumulatorUse::kRead, OperandType::kIdx)              \
  V(JumpIfUndefinedConstant, AccumulatorUse::kRead, OperandType::kIdx)         \
  V(JumpIfTrueConstant, AccumulatorUse::kRead, OperandType::kIdx)              \
  V(JumpIfFalseConstant, AccumulatorUse::kRead, OperandType::kIdx)             \
  V(JumpIfJSReceiverConstant, AccumulatorUse::kRead, OperandType::kIdx)        \
  V(JumpIfNotHoleConstant, AccumulatorUse::kRead, OperandType::kIdx)           \
  /* - [Start ToBoolean jumps] */                                              \
  V(JumpIfToBooleanTrueConstant, AccumulatorUse::kRead, OperandType::kIdx)     \
  V(JumpIfToBooleanFalseConstant, AccumulatorUse::kRead, OperandType::kIdx)    \
  /* - [End constant jumps] */                                                 \
  /* - [Conditional immediate jumps] */                                        \
  V(JumpIfToBooleanTrue, AccumulatorUse::kRead, OperandType::kImm)             \
  V(JumpIfToBooleanFalse, AccumulatorUse::kRead, OperandType::kImm)            \
  /* - [End ToBoolean jumps] */                                                \
  V(JumpIfTrue, AccumulatorUse::kRead, OperandType::kImm)                      \
  V(JumpIfFalse, AccumulatorUse::kRead, OperandType::kImm)                     \
  V(JumpIfNull, AccumulatorUse::kRead, OperandType::kImm)                      \
  V(JumpIfUndefined, AccumulatorUse::kRead, OperandType::kImm)                 \
  V(JumpIfJSReceiver, AccumulatorUse::kRead, OperandType::kImm)                \
  V(JumpIfNotHole, AccumulatorUse::kRead, OperandType::kImm)                   \
                                                                               \
  /* Complex flow control For..in */                                           \
  V(ForInPrepare, AccumulatorUse::kNone, OperandType::kReg,                    \
    OperandType::kRegOutTriple)                                                \
  V(ForInContinue, AccumulatorUse::kWrite, OperandType::kReg,                  \
    OperandType::kReg)                                                         \
  V(ForInNext, AccumulatorUse::kWrite, OperandType::kReg, OperandType::kReg,   \
    OperandType::kRegPair, OperandType::kIdx)                                  \
  V(ForInStep, AccumulatorUse::kWrite, OperandType::kReg)                      \
                                                                               \
  /* Perform a stack guard check */                                            \
  V(StackCheck, AccumulatorUse::kNone)                                         \
                                                                               \
  /* Update the pending message */                                             \
  V(SetPendingMessage, AccumulatorUse::kReadWrite)                             \
                                                                               \
  /* Non-local flow control */                                                 \
  V(Throw, AccumulatorUse::kRead)                                              \
  V(ReThrow, AccumulatorUse::kRead)                                            \
  V(Return, AccumulatorUse::kRead)                                             \
                                                                               \
  /* Generators */                                                             \
  V(SuspendGenerator, AccumulatorUse::kRead, OperandType::kReg)                \
  V(ResumeGenerator, AccumulatorUse::kWrite, OperandType::kReg)                \
                                                                               \
  /* Debugger */                                                               \
  V(Debugger, AccumulatorUse::kNone)                                           \
                                                                               \
  /* Debug Breakpoints - one for each possible size of unscaled bytecodes */   \
  /* and one for each operand widening prefix bytecode                    */   \
  V(DebugBreak0, AccumulatorUse::kRead)                                        \
  V(DebugBreak1, AccumulatorUse::kRead, OperandType::kReg)                     \
  V(DebugBreak2, AccumulatorUse::kRead, OperandType::kReg, OperandType::kReg)  \
  V(DebugBreak3, AccumulatorUse::kRead, OperandType::kReg, OperandType::kReg,  \
    OperandType::kReg)                                                         \
  V(DebugBreak4, AccumulatorUse::kRead, OperandType::kReg, OperandType::kReg,  \
    OperandType::kReg, OperandType::kReg)                                      \
  V(DebugBreak5, AccumulatorUse::kRead, OperandType::kRuntimeId,               \
    OperandType::kReg, OperandType::kReg)                                      \
  V(DebugBreak6, AccumulatorUse::kRead, OperandType::kRuntimeId,               \
    OperandType::kReg, OperandType::kReg, OperandType::kReg)                   \
  V(DebugBreakWide, AccumulatorUse::kRead)                                     \
  V(DebugBreakExtraWide, AccumulatorUse::kRead)                                \
                                                                               \
  /* Illegal bytecode (terminates execution) */                                \
  V(Illegal, AccumulatorUse::kNone)                                            \
                                                                               \
  /* No operation (used to maintain source positions for peephole */           \
  /* eliminated bytecodes). */                                                 \
  V(Nop, AccumulatorUse::kNone)

// List of debug break bytecodes.
#define DEBUG_BREAK_PLAIN_BYTECODE_LIST(V) \
  V(DebugBreak0)                           \
  V(DebugBreak1)                           \
  V(DebugBreak2)                           \
  V(DebugBreak3)                           \
  V(DebugBreak4)                           \
  V(DebugBreak5)                           \
  V(DebugBreak6)

#define DEBUG_BREAK_PREFIX_BYTECODE_LIST(V) \
  V(DebugBreakWide)                         \
  V(DebugBreakExtraWide)

#define DEBUG_BREAK_BYTECODE_LIST(V) \
  DEBUG_BREAK_PLAIN_BYTECODE_LIST(V) \
  DEBUG_BREAK_PREFIX_BYTECODE_LIST(V)

// Lists of jump bytecodes.

#define JUMP_UNCONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  V(JumpLoop)                                         \
  V(Jump)

#define JUMP_UNCONDITIONAL_CONSTANT_BYTECODE_LIST(V) V(JumpConstant)

#define JUMP_TOBOOLEAN_CONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  V(JumpIfToBooleanTrue)                                      \
  V(JumpIfToBooleanFalse)

#define JUMP_TOBOOLEAN_CONDITIONAL_CONSTANT_BYTECODE_LIST(V) \
  V(JumpIfToBooleanTrueConstant)                             \
  V(JumpIfToBooleanFalseConstant)

#define JUMP_CONDITIONAL_IMMEDIATE_BYTECODE_LIST(V)     \
  JUMP_TOBOOLEAN_CONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  V(JumpIfTrue)                                         \
  V(JumpIfFalse)                                        \
  V(JumpIfNull)                                         \
  V(JumpIfUndefined)                                    \
  V(JumpIfJSReceiver)                                   \
  V(JumpIfNotHole)

#define JUMP_CONDITIONAL_CONSTANT_BYTECODE_LIST(V)     \
  JUMP_TOBOOLEAN_CONDITIONAL_CONSTANT_BYTECODE_LIST(V) \
  V(JumpIfNullConstant)                                \
  V(JumpIfUndefinedConstant)                           \
  V(JumpIfTrueConstant)                                \
  V(JumpIfFalseConstant)                               \
  V(JumpIfJSReceiverConstant)                          \
  V(JumpIfNotHoleConstant)

#define JUMP_CONSTANT_BYTECODE_LIST(V)         \
  JUMP_UNCONDITIONAL_CONSTANT_BYTECODE_LIST(V) \
  JUMP_CONDITIONAL_CONSTANT_BYTECODE_LIST(V)

#define JUMP_IMMEDIATE_BYTECODE_LIST(V)         \
  JUMP_UNCONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  JUMP_CONDITIONAL_IMMEDIATE_BYTECODE_LIST(V)

#define JUMP_TO_BOOLEAN_BYTECODE_LIST(V)                \
  JUMP_TOBOOLEAN_CONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  JUMP_TOBOOLEAN_CONDITIONAL_CONSTANT_BYTECODE_LIST(V)

#define JUMP_UNCONDITIONAL_BYTECODE_LIST(V)     \
  JUMP_UNCONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  JUMP_UNCONDITIONAL_CONSTANT_BYTECODE_LIST(V)

#define JUMP_CONDITIONAL_BYTECODE_LIST(V)     \
  JUMP_CONDITIONAL_IMMEDIATE_BYTECODE_LIST(V) \
  JUMP_CONDITIONAL_CONSTANT_BYTECODE_LIST(V)

#define JUMP_FORWARD_BYTECODE_LIST(V) \
  V(Jump)                             \
  V(JumpConstant)                     \
  JUMP_CONDITIONAL_BYTECODE_LIST(V)

#define JUMP_BYTECODE_LIST(V)   \
  JUMP_FORWARD_BYTECODE_LIST(V) \
  V(JumpLoop)

// Enumeration of interpreter bytecodes.
enum class Bytecode : uint8_t {
#define DECLARE_BYTECODE(Name, ...) k##Name,
  BYTECODE_LIST(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
#define COUNT_BYTECODE(x, ...) +1
  // The COUNT_BYTECODE macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last real bytecode.
  kLast = -1 BYTECODE_LIST(COUNT_BYTECODE)
#undef COUNT_BYTECODE
};

class V8_EXPORT_PRIVATE Bytecodes final {
 public:
  //  The maximum number of operands a bytecode may have.
  static const int kMaxOperands = 4;

  // Returns string representation of |bytecode|.
  static const char* ToString(Bytecode bytecode);

  // Returns string representation of |bytecode|.
  static std::string ToString(Bytecode bytecode, OperandScale operand_scale);

  // Returns byte value of bytecode.
  static uint8_t ToByte(Bytecode bytecode) {
    DCHECK_LE(bytecode, Bytecode::kLast);
    return static_cast<uint8_t>(bytecode);
  }

  // Returns bytecode for |value|.
  static Bytecode FromByte(uint8_t value) {
    Bytecode bytecode = static_cast<Bytecode>(value);
    DCHECK(bytecode <= Bytecode::kLast);
    return bytecode;
  }

  // Returns the prefix bytecode representing an operand scale to be
  // applied to a a bytecode.
  static Bytecode OperandScaleToPrefixBytecode(OperandScale operand_scale) {
    switch (operand_scale) {
      case OperandScale::kQuadruple:
        return Bytecode::kExtraWide;
      case OperandScale::kDouble:
        return Bytecode::kWide;
      default:
        UNREACHABLE();
        return Bytecode::kIllegal;
    }
  }

  // Returns true if the operand scale requires a prefix bytecode.
  static bool OperandScaleRequiresPrefixBytecode(OperandScale operand_scale) {
    return operand_scale != OperandScale::kSingle;
  }

  // Returns the scaling applied to scalable operands if bytecode is
  // is a scaling prefix.
  static OperandScale PrefixBytecodeToOperandScale(Bytecode bytecode) {
    switch (bytecode) {
      case Bytecode::kExtraWide:
      case Bytecode::kDebugBreakExtraWide:
        return OperandScale::kQuadruple;
      case Bytecode::kWide:
      case Bytecode::kDebugBreakWide:
        return OperandScale::kDouble;
      default:
        UNREACHABLE();
        return OperandScale::kSingle;
    }
  }

  // Returns how accumulator is used by |bytecode|.
  static AccumulatorUse GetAccumulatorUse(Bytecode bytecode) {
    DCHECK(bytecode <= Bytecode::kLast);
    return kAccumulatorUse[static_cast<size_t>(bytecode)];
  }

  // Returns true if |bytecode| reads the accumulator.
  static bool ReadsAccumulator(Bytecode bytecode) {
    return BytecodeOperands::ReadsAccumulator(GetAccumulatorUse(bytecode));
  }

  // Returns true if |bytecode| writes the accumulator.
  static bool WritesAccumulator(Bytecode bytecode) {
    return BytecodeOperands::WritesAccumulator(GetAccumulatorUse(bytecode));
  }

  // Return true if |bytecode| writes the accumulator with a boolean value.
  static bool WritesBooleanToAccumulator(Bytecode bytecode) {
    switch (bytecode) {
      case Bytecode::kLdaTrue:
      case Bytecode::kLdaFalse:
      case Bytecode::kToBooleanLogicalNot:
      case Bytecode::kLogicalNot:
      case Bytecode::kTestEqual:
      case Bytecode::kTestNotEqual:
      case Bytecode::kTestEqualStrict:
      case Bytecode::kTestLessThan:
      case Bytecode::kTestLessThanOrEqual:
      case Bytecode::kTestGreaterThan:
      case Bytecode::kTestGreaterThanOrEqual:
      case Bytecode::kTestInstanceOf:
      case Bytecode::kTestIn:
      case Bytecode::kTestUndetectable:
      case Bytecode::kForInContinue:
      case Bytecode::kTestUndefined:
      case Bytecode::kTestNull:
        return true;
      default:
        return false;
    }
  }

  // Return true if |bytecode| is an accumulator load without effects,
  // e.g. LdaConstant, LdaTrue, Ldar.
  static constexpr bool IsAccumulatorLoadWithoutEffects(Bytecode bytecode) {
    return bytecode == Bytecode::kLdar || bytecode == Bytecode::kLdaZero ||
           bytecode == Bytecode::kLdaSmi || bytecode == Bytecode::kLdaNull ||
           bytecode == Bytecode::kLdaTrue || bytecode == Bytecode::kLdaFalse ||
           bytecode == Bytecode::kLdaUndefined ||
           bytecode == Bytecode::kLdaTheHole ||
           bytecode == Bytecode::kLdaConstant ||
           bytecode == Bytecode::kLdaContextSlot ||
           bytecode == Bytecode::kLdaCurrentContextSlot;
  }

  // Return true if |bytecode| is a register load without effects,
  // e.g. Mov, Star.
  static constexpr bool IsRegisterLoadWithoutEffects(Bytecode bytecode) {
    return bytecode == Bytecode::kMov || bytecode == Bytecode::kPopContext ||
           bytecode == Bytecode::kPushContext || bytecode == Bytecode::kStar;
  }

  // Returns true if the bytecode is a conditional jump taking
  // an immediate byte operand (OperandType::kImm).
  static constexpr bool IsConditionalJumpImmediate(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpIfToBooleanTrue &&
           bytecode <= Bytecode::kJumpIfNotHole;
  }

  // Returns true if the bytecode is a conditional jump taking
  // a constant pool entry (OperandType::kIdx).
  static constexpr bool IsConditionalJumpConstant(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpIfNullConstant &&
           bytecode <= Bytecode::kJumpIfToBooleanFalseConstant;
  }

  // Returns true if the bytecode is a conditional jump taking
  // any kind of operand.
  static constexpr bool IsConditionalJump(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpIfNullConstant &&
           bytecode <= Bytecode::kJumpIfNotHole;
  }

  // Returns true if the bytecode is an unconditional jump.
  static constexpr bool IsUnconditionalJump(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpLoop &&
           bytecode <= Bytecode::kJumpConstant;
  }

  // Returns true if the bytecode is a jump or a conditional jump taking
  // an immediate byte operand (OperandType::kImm).
  static constexpr bool IsJumpImmediate(Bytecode bytecode) {
    return bytecode == Bytecode::kJump || bytecode == Bytecode::kJumpLoop ||
           IsConditionalJumpImmediate(bytecode);
  }

  // Returns true if the bytecode is a jump or conditional jump taking a
  // constant pool entry (OperandType::kIdx).
  static constexpr bool IsJumpConstant(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpConstant &&
           bytecode <= Bytecode::kJumpIfToBooleanFalseConstant;
  }

  // Returns true if the bytecode is a jump that internally coerces the
  // accumulator to a boolean.
  static constexpr bool IsJumpIfToBoolean(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpIfToBooleanTrueConstant &&
           bytecode <= Bytecode::kJumpIfToBooleanFalse;
  }

  // Returns true if the bytecode is a jump or conditional jump taking
  // any kind of operand.
  static constexpr bool IsJump(Bytecode bytecode) {
    return bytecode >= Bytecode::kJumpLoop &&
           bytecode <= Bytecode::kJumpIfNotHole;
  }

  // Returns true if the bytecode is a forward jump or conditional jump taking
  // any kind of operand.
  static constexpr bool IsForwardJump(Bytecode bytecode) {
    return bytecode >= Bytecode::kJump && bytecode <= Bytecode::kJumpIfNotHole;
  }

  // Returns true if the bytecode is a conditional jump, a jump, or a return.
  static constexpr bool IsJumpOrReturn(Bytecode bytecode) {
    return bytecode == Bytecode::kReturn || IsJump(bytecode);
  }

  // Return true if |bytecode| is a jump without effects,
  // e.g.  any jump excluding those that include type coercion like
  // JumpIfTrueToBoolean.
  static constexpr bool IsJumpWithoutEffects(Bytecode bytecode) {
    return IsJump(bytecode) && !IsJumpIfToBoolean(bytecode);
  }

  // Returns true if |bytecode| has no effects. These bytecodes only manipulate
  // interpreter frame state and will never throw.
  static constexpr bool IsWithoutExternalSideEffects(Bytecode bytecode) {
    return (IsAccumulatorLoadWithoutEffects(bytecode) ||
            IsRegisterLoadWithoutEffects(bytecode) ||
            bytecode == Bytecode::kNop || IsJumpWithoutEffects(bytecode));
  }

  // Returns true if the bytecode is Ldar or Star.
  static constexpr bool IsLdarOrStar(Bytecode bytecode) {
    return bytecode == Bytecode::kLdar || bytecode == Bytecode::kStar;
  }

  // Returns true if |bytecode| puts a name in the accumulator.
  static constexpr bool PutsNameInAccumulator(Bytecode bytecode) {
    return bytecode == Bytecode::kTypeOf;
  }

  // Returns true if the bytecode is a call or a constructor call.
  static constexpr bool IsCallOrNew(Bytecode bytecode) {
    return bytecode == Bytecode::kCall || bytecode == Bytecode::kCallProperty ||
           bytecode == Bytecode::kTailCall || bytecode == Bytecode::kNew;
  }

  // Returns true if the bytecode is a call to the runtime.
  static constexpr bool IsCallRuntime(Bytecode bytecode) {
    return bytecode == Bytecode::kCallRuntime ||
           bytecode == Bytecode::kCallRuntimeForPair ||
           bytecode == Bytecode::kInvokeIntrinsic;
  }

  // Returns true if the bytecode is a scaling prefix bytecode.
  static constexpr bool IsPrefixScalingBytecode(Bytecode bytecode) {
    return bytecode == Bytecode::kExtraWide || bytecode == Bytecode::kWide ||
           bytecode == Bytecode::kDebugBreakExtraWide ||
           bytecode == Bytecode::kDebugBreakWide;
  }

  // Returns the number of values which |bytecode| returns.
  static constexpr size_t ReturnCount(Bytecode bytecode) {
    return bytecode == Bytecode::kReturn ? 1 : 0;
  }

  // Returns the number of operands expected by |bytecode|.
  static int NumberOfOperands(Bytecode bytecode) {
    DCHECK(bytecode <= Bytecode::kLast);
    return kOperandCount[static_cast<size_t>(bytecode)];
  }

  // Returns the i-th operand of |bytecode|.
  static OperandType GetOperandType(Bytecode bytecode, int i) {
    DCHECK_LE(bytecode, Bytecode::kLast);
    DCHECK_LT(i, NumberOfOperands(bytecode));
    DCHECK_GE(i, 0);
    return GetOperandTypes(bytecode)[i];
  }

  // Returns a pointer to an array of operand types terminated in
  // OperandType::kNone.
  static const OperandType* GetOperandTypes(Bytecode bytecode) {
    DCHECK(bytecode <= Bytecode::kLast);
    return kOperandTypes[static_cast<size_t>(bytecode)];
  }

  static bool OperandIsScalableSignedByte(Bytecode bytecode,
                                          int operand_index) {
    DCHECK(bytecode <= Bytecode::kLast);
    return kOperandTypeInfos[static_cast<size_t>(bytecode)][operand_index] ==
           OperandTypeInfo::kScalableSignedByte;
  }

  static bool OperandIsScalableUnsignedByte(Bytecode bytecode,
                                            int operand_index) {
    DCHECK(bytecode <= Bytecode::kLast);
    return kOperandTypeInfos[static_cast<size_t>(bytecode)][operand_index] ==
           OperandTypeInfo::kScalableUnsignedByte;
  }

  static bool OperandIsScalable(Bytecode bytecode, int operand_index) {
    return OperandIsScalableSignedByte(bytecode, operand_index) ||
           OperandIsScalableUnsignedByte(bytecode, operand_index);
  }

  // Returns true if the bytecode has wider operand forms.
  static bool IsBytecodeWithScalableOperands(Bytecode bytecode);

  // Returns the size of the i-th operand of |bytecode|.
  static OperandSize GetOperandSize(Bytecode bytecode, int i,
                                    OperandScale operand_scale) {
    CHECK_LT(i, NumberOfOperands(bytecode));
    return GetOperandSizes(bytecode, operand_scale)[i];
  }

  // Returns the operand sizes of |bytecode| with scale |operand_scale|.
  static const OperandSize* GetOperandSizes(Bytecode bytecode,
                                            OperandScale operand_scale) {
    DCHECK(bytecode <= Bytecode::kLast);
    DCHECK_GE(operand_scale, OperandScale::kSingle);
    DCHECK_LE(operand_scale, OperandScale::kLast);
    STATIC_ASSERT(static_cast<int>(OperandScale::kQuadruple) == 4 &&
                  OperandScale::kLast == OperandScale::kQuadruple);
    int scale_index = static_cast<int>(operand_scale) >> 1;
    return kOperandSizes[static_cast<size_t>(bytecode)][scale_index];
  }

  // Returns the offset of the i-th operand of |bytecode| relative to the start
  // of the bytecode.
  static int GetOperandOffset(Bytecode bytecode, int i,
                              OperandScale operand_scale);

  // Returns the size of the bytecode including its operands for the
  // given |operand_scale|.
  static int Size(Bytecode bytecode, OperandScale operand_scale) {
    DCHECK(bytecode <= Bytecode::kLast);
    STATIC_ASSERT(static_cast<int>(OperandScale::kQuadruple) == 4 &&
                  OperandScale::kLast == OperandScale::kQuadruple);
    int scale_index = static_cast<int>(operand_scale) >> 1;
    return kBytecodeSizes[static_cast<size_t>(bytecode)][scale_index];
  }

  // Returns a debug break bytecode to replace |bytecode|.
  static Bytecode GetDebugBreak(Bytecode bytecode);

  // Returns the equivalent jump bytecode without the accumulator coercion.
  static Bytecode GetJumpWithoutToBoolean(Bytecode bytecode);

  // Returns true if the bytecode is a debug break.
  static bool IsDebugBreak(Bytecode bytecode);

  // Returns true if |operand_type| is any type of register operand.
  static bool IsRegisterOperandType(OperandType operand_type);

  // Returns true if |operand_type| represents a register used as an input.
  static bool IsRegisterInputOperandType(OperandType operand_type);

  // Returns true if |operand_type| represents a register used as an output.
  static bool IsRegisterOutputOperandType(OperandType operand_type);

  // Returns true if the handler for |bytecode| should look ahead and inline a
  // dispatch to a Star bytecode.
  static bool IsStarLookahead(Bytecode bytecode, OperandScale operand_scale);

  // Returns the number of registers represented by a register operand. For
  // instance, a RegPair represents two registers. Should not be called for
  // kRegList which has a variable number of registers based on the following
  // kRegCount operand.
  static int GetNumberOfRegistersRepresentedBy(OperandType operand_type) {
    switch (operand_type) {
      case OperandType::kReg:
      case OperandType::kRegOut:
        return 1;
      case OperandType::kRegPair:
      case OperandType::kRegOutPair:
        return 2;
      case OperandType::kRegOutTriple:
        return 3;
      case OperandType::kRegList:
        UNREACHABLE();
        return 0;
      default:
        return 0;
    }
    return 0;
  }

  // Returns the size of |operand| for |operand_scale|.
  static OperandSize SizeOfOperand(OperandType operand, OperandScale scale);

  // Returns true if |operand_type| is a runtime-id operand (kRuntimeId).
  static bool IsRuntimeIdOperandType(OperandType operand_type);

  // Returns true if |operand_type| is unsigned, false if signed.
  static bool IsUnsignedOperandType(OperandType operand_type);

  // Returns true if a handler is generated for a bytecode at a given
  // operand scale. All bytecodes have handlers at OperandScale::kSingle,
  // but only bytecodes with scalable operands have handlers with larger
  // OperandScale values.
  static bool BytecodeHasHandler(Bytecode bytecode, OperandScale operand_scale);

  // Return the operand scale required to hold a signed operand with |value|.
  static OperandScale ScaleForSignedOperand(int32_t value) {
    if (value >= kMinInt8 && value <= kMaxInt8) {
      return OperandScale::kSingle;
    } else if (value >= kMinInt16 && value <= kMaxInt16) {
      return OperandScale::kDouble;
    } else {
      return OperandScale::kQuadruple;
    }
  }

  // Return the operand scale required to hold an unsigned operand with |value|.
  static OperandScale ScaleForUnsignedOperand(uint32_t value) {
    if (value <= kMaxUInt8) {
      return OperandScale::kSingle;
    } else if (value <= kMaxUInt16) {
      return OperandScale::kDouble;
    } else {
      return OperandScale::kQuadruple;
    }
  }

  // Return the operand size required to hold an unsigned operand with |value|.
  static OperandSize SizeForUnsignedOperand(uint32_t value) {
    if (value <= kMaxUInt8) {
      return OperandSize::kByte;
    } else if (value <= kMaxUInt16) {
      return OperandSize::kShort;
    } else {
      return OperandSize::kQuad;
    }
  }

 private:
  static const OperandType* const kOperandTypes[];
  static const OperandTypeInfo* const kOperandTypeInfos[];
  static const int kOperandCount[];
  static const int kNumberOfRegisterOperands[];
  static const AccumulatorUse kAccumulatorUse[];
  static const bool kIsScalable[];
  static const int kBytecodeSizes[][3];
  static const OperandSize* const kOperandSizes[][3];
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const Bytecode& bytecode);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODES_H_
