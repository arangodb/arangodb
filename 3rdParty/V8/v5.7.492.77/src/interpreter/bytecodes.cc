// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecodes.h"

#include <iomanip>

#include "src/base/bits.h"
#include "src/interpreter/bytecode-traits.h"

namespace v8 {
namespace internal {
namespace interpreter {

// clang-format off
const OperandType* const Bytecodes::kOperandTypes[] = {
#define ENTRY(Name, ...) BytecodeTraits<__VA_ARGS__>::kOperandTypes,
  BYTECODE_LIST(ENTRY)
#undef ENTRY
};

const OperandTypeInfo* const Bytecodes::kOperandTypeInfos[] = {
#define ENTRY(Name, ...) BytecodeTraits<__VA_ARGS__>::kOperandTypeInfos,
  BYTECODE_LIST(ENTRY)
#undef ENTRY
};

const int Bytecodes::kOperandCount[] = {
#define ENTRY(Name, ...) BytecodeTraits<__VA_ARGS__>::kOperandCount,
  BYTECODE_LIST(ENTRY)
#undef ENTRY
};

const AccumulatorUse Bytecodes::kAccumulatorUse[] = {
#define ENTRY(Name, ...) BytecodeTraits<__VA_ARGS__>::kAccumulatorUse,
  BYTECODE_LIST(ENTRY)
#undef ENTRY
};

const int Bytecodes::kBytecodeSizes[][3] = {
#define ENTRY(Name, ...)                            \
  { BytecodeTraits<__VA_ARGS__>::kSingleScaleSize,  \
    BytecodeTraits<__VA_ARGS__>::kDoubleScaleSize,  \
    BytecodeTraits<__VA_ARGS__>::kQuadrupleScaleSize },
  BYTECODE_LIST(ENTRY)
#undef ENTRY
};

const OperandSize* const Bytecodes::kOperandSizes[][3] = {
#define ENTRY(Name, ...)                                    \
  { BytecodeTraits<__VA_ARGS__>::kSingleScaleOperandSizes,  \
    BytecodeTraits<__VA_ARGS__>::kDoubleScaleOperandSizes,  \
    BytecodeTraits<__VA_ARGS__>::kQuadrupleScaleOperandSizes },
  BYTECODE_LIST(ENTRY)
#undef ENTRY
};
// clang-format on

// static
const char* Bytecodes::ToString(Bytecode bytecode) {
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return #Name;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return "";
}

// static
std::string Bytecodes::ToString(Bytecode bytecode, OperandScale operand_scale) {
  static const char kSeparator = '.';

  std::string value(ToString(bytecode));
  if (operand_scale > OperandScale::kSingle) {
    Bytecode prefix_bytecode = OperandScaleToPrefixBytecode(operand_scale);
    std::string suffix = ToString(prefix_bytecode);
    return value.append(1, kSeparator).append(suffix);
  } else {
    return value;
  }
}

// static
Bytecode Bytecodes::GetDebugBreak(Bytecode bytecode) {
  DCHECK(!IsDebugBreak(bytecode));
  if (bytecode == Bytecode::kWide) {
    return Bytecode::kDebugBreakWide;
  }
  if (bytecode == Bytecode::kExtraWide) {
    return Bytecode::kDebugBreakExtraWide;
  }
  int bytecode_size = Size(bytecode, OperandScale::kSingle);
#define RETURN_IF_DEBUG_BREAK_SIZE_MATCHES(Name)                         \
  if (bytecode_size == Size(Bytecode::k##Name, OperandScale::kSingle)) { \
    return Bytecode::k##Name;                                            \
  }
  DEBUG_BREAK_PLAIN_BYTECODE_LIST(RETURN_IF_DEBUG_BREAK_SIZE_MATCHES)
#undef RETURN_IF_DEBUG_BREAK_SIZE_MATCHES
  UNREACHABLE();
  return Bytecode::kIllegal;
}

// static
int Bytecodes::GetOperandOffset(Bytecode bytecode, int i,
                                OperandScale operand_scale) {
  DCHECK_LT(i, Bytecodes::NumberOfOperands(bytecode));
  // TODO(oth): restore this to a statically determined constant.
  int offset = 1;
  for (int operand_index = 0; operand_index < i; ++operand_index) {
    OperandSize operand_size =
        GetOperandSize(bytecode, operand_index, operand_scale);
    offset += static_cast<int>(operand_size);
  }
  return offset;
}

// static
Bytecode Bytecodes::GetJumpWithoutToBoolean(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kJumpIfToBooleanTrue:
      return Bytecode::kJumpIfTrue;
    case Bytecode::kJumpIfToBooleanFalse:
      return Bytecode::kJumpIfFalse;
    case Bytecode::kJumpIfToBooleanTrueConstant:
      return Bytecode::kJumpIfTrueConstant;
    case Bytecode::kJumpIfToBooleanFalseConstant:
      return Bytecode::kJumpIfFalseConstant;
    default:
      break;
  }
  UNREACHABLE();
  return Bytecode::kIllegal;
}

// static
bool Bytecodes::IsDebugBreak(Bytecode bytecode) {
  switch (bytecode) {
#define CASE(Name, ...) case Bytecode::k##Name:
    DEBUG_BREAK_BYTECODE_LIST(CASE);
#undef CASE
    return true;
    default:
      break;
  }
  return false;
}

// static
bool Bytecodes::IsRegisterOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return true;
    REGISTER_OPERAND_TYPE_LIST(CASE)
#undef CASE
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    break;
    NON_REGISTER_OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  return false;
}

// static
bool Bytecodes::IsRegisterInputOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return true;
    REGISTER_INPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    break;
    NON_REGISTER_OPERAND_TYPE_LIST(CASE)
    REGISTER_OUTPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  return false;
}

// static
bool Bytecodes::IsRegisterOutputOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return true;
    REGISTER_OUTPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    break;
    NON_REGISTER_OPERAND_TYPE_LIST(CASE)
    REGISTER_INPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  return false;
}

// static
bool Bytecodes::IsStarLookahead(Bytecode bytecode, OperandScale operand_scale) {
  if (operand_scale == OperandScale::kSingle) {
    switch (bytecode) {
      case Bytecode::kLdaZero:
      case Bytecode::kLdaSmi:
      case Bytecode::kLdaNull:
      case Bytecode::kLdaTheHole:
      case Bytecode::kLdaConstant:
      case Bytecode::kLdaUndefined:
      case Bytecode::kLdaGlobal:
      case Bytecode::kLdaNamedProperty:
      case Bytecode::kLdaKeyedProperty:
      case Bytecode::kLdaContextSlot:
      case Bytecode::kLdaCurrentContextSlot:
      case Bytecode::kAdd:
      case Bytecode::kSub:
      case Bytecode::kMul:
      case Bytecode::kAddSmi:
      case Bytecode::kSubSmi:
      case Bytecode::kInc:
      case Bytecode::kDec:
      case Bytecode::kTypeOf:
      case Bytecode::kCall:
      case Bytecode::kCallProperty:
      case Bytecode::kNew:
        return true;
      default:
        return false;
    }
  }
  return false;
}

// static
bool Bytecodes::IsBytecodeWithScalableOperands(Bytecode bytecode) {
  for (int i = 0; i < NumberOfOperands(bytecode); i++) {
    if (OperandIsScalable(bytecode, i)) return true;
  }
  return false;
}

// static
bool Bytecodes::IsUnsignedOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return OperandTraits<OperandType::k##Name>::TypeInfoTraits::kIsUnsigned;
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
}

// static
OperandSize Bytecodes::SizeOfOperand(OperandType operand_type,
                                     OperandScale operand_scale) {
  DCHECK_LE(operand_type, OperandType::kLast);
  DCHECK_GE(operand_scale, OperandScale::kSingle);
  DCHECK_LE(operand_scale, OperandScale::kLast);
  STATIC_ASSERT(static_cast<int>(OperandScale::kQuadruple) == 4 &&
                OperandScale::kLast == OperandScale::kQuadruple);
  int scale_index = static_cast<int>(operand_scale) >> 1;
  // clang-format off
  static const OperandSize kOperandSizes[][3] = {
#define ENTRY(Name, ...)                                \
  { OperandScaler<OperandType::k##Name,                 \
                 OperandScale::kSingle>::kOperandSize,  \
    OperandScaler<OperandType::k##Name,                 \
                 OperandScale::kDouble>::kOperandSize,  \
    OperandScaler<OperandType::k##Name,                 \
                 OperandScale::kQuadruple>::kOperandSize },
    OPERAND_TYPE_LIST(ENTRY)
#undef ENTRY
  };
  // clang-format on
  return kOperandSizes[static_cast<size_t>(operand_type)][scale_index];
}

// static
bool Bytecodes::BytecodeHasHandler(Bytecode bytecode,
                                   OperandScale operand_scale) {
  return operand_scale == OperandScale::kSingle ||
         Bytecodes::IsBytecodeWithScalableOperands(bytecode);
}

std::ostream& operator<<(std::ostream& os, const Bytecode& bytecode) {
  return os << Bytecodes::ToString(bytecode);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
