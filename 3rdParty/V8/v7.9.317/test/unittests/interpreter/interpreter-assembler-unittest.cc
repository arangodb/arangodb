// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/interpreter/interpreter-assembler-unittest.h"

#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;
using ::testing::Eq;
using v8::internal::compiler::Node;

namespace c = v8::internal::compiler;

namespace v8 {
namespace internal {
namespace interpreter {
namespace interpreter_assembler_unittest {

InterpreterAssemblerTestState::InterpreterAssemblerTestState(
    InterpreterAssemblerTest* test, Bytecode bytecode)
    : compiler::CodeAssemblerState(
          test->isolate(), test->zone(), InterpreterDispatchDescriptor{},
          Code::BYTECODE_HANDLER, Bytecodes::ToString(bytecode),
          PoisoningMitigationLevel::kPoisonCriticalOnly) {}

const interpreter::Bytecode kBytecodes[] = {
#define DEFINE_BYTECODE(Name, ...) interpreter::Bytecode::k##Name,
    BYTECODE_LIST(DEFINE_BYTECODE)
#undef DEFINE_BYTECODE
};


InterpreterAssemblerTest::InterpreterAssemblerForTest::
    ~InterpreterAssemblerForTest() {
  // Tests don't necessarily read and write accumulator but
  // InterpreterAssembler checks accumulator uses.
  if (Bytecodes::ReadsAccumulator(bytecode())) {
    GetAccumulator();
  }
  if (Bytecodes::WritesAccumulator(bytecode())) {
    SetAccumulator(nullptr);
  }
}

Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsLoad(
    const Matcher<c::LoadRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher,
    LoadSensitivity needs_poisoning) {
  CHECK_NE(LoadSensitivity::kUnsafe, needs_poisoning);
  CHECK_NE(PoisoningMitigationLevel::kPoisonAll, poisoning_level());
  if (poisoning_level() == PoisoningMitigationLevel::kPoisonCriticalOnly &&
      needs_poisoning == LoadSensitivity::kCritical) {
    return ::i::compiler::IsPoisonedLoad(rep_matcher, base_matcher,
                                         index_matcher, _, _);
  }
  return ::i::compiler::IsLoad(rep_matcher, base_matcher, index_matcher, _, _);
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsLoadFromObject(
    const Matcher<c::LoadRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher) {
  CHECK_NE(PoisoningMitigationLevel::kPoisonAll, poisoning_level());
  return ::i::compiler::IsLoadFromObject(rep_matcher, base_matcher,
                                         index_matcher, _, _);
}

Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsStore(
    const Matcher<c::StoreRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher,
    const Matcher<Node*>& value_matcher) {
  return ::i::compiler::IsStore(rep_matcher, base_matcher, index_matcher,
                                value_matcher, _, _);
}

Matcher<Node*> InterpreterAssemblerTest::InterpreterAssemblerForTest::IsWordNot(
    const Matcher<Node*>& value_matcher) {
  return kSystemPointerSize == 8
             ? IsWord64Xor(value_matcher, c::IsInt64Constant(-1))
             : IsWord32Xor(value_matcher, c::IsInt32Constant(-1));
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedByteOperand(
    int offset, LoadSensitivity needs_poisoning) {
  return IsLoad(
      MachineType::Uint8(),
      c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
      c::IsIntPtrAdd(
          c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
          c::IsIntPtrConstant(offset)),
      needs_poisoning);
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedByteOperand(
    int offset, LoadSensitivity needs_poisoning) {
  return IsLoad(
      MachineType::Int8(),
      c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
      c::IsIntPtrAdd(
          c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
          c::IsIntPtrConstant(offset)),
      needs_poisoning);
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedShortOperand(
    int offset, LoadSensitivity needs_poisoning) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Uint16(),
        c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        c::IsIntPtrAdd(
            c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
            c::IsIntPtrConstant(offset)),
        needs_poisoning);
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    const int kMsbOffset = 1;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    const int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[2];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          MachineType::Uint8(),
          c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          c::IsIntPtrAdd(
              c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
              c::IsIntPtrConstant(offset + kMsbOffset + kStep * i)),
          needs_poisoning);
    }
    return c::IsWord32Or(
        c::IsWord32Shl(bytes[0], c::IsInt32Constant(kBitsPerByte)), bytes[1]);
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedShortOperand(
    int offset, LoadSensitivity needs_poisoning) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Int16(),
        c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        c::IsIntPtrAdd(
            c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
            c::IsIntPtrConstant(offset)),
        needs_poisoning);
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    const int kMsbOffset = 1;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    const int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[2];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          (i == 0) ? MachineType::Int8() : MachineType::Uint8(),
          c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          c::IsIntPtrAdd(
              c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
              c::IsIntPtrConstant(offset + kMsbOffset + kStep * i)),
          needs_poisoning);
    }
    return c::IsWord32Or(
        c::IsWord32Shl(bytes[0], c::IsInt32Constant(kBitsPerByte)), bytes[1]);
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedQuadOperand(
    int offset, LoadSensitivity needs_poisoning) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Uint32(),
        c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        c::IsIntPtrAdd(
            c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
            c::IsIntPtrConstant(offset)),
        needs_poisoning);
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    const int kMsbOffset = 3;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    const int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[4];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          MachineType::Uint8(),
          c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          c::IsIntPtrAdd(
              c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
              c::IsIntPtrConstant(offset + kMsbOffset + kStep * i)),
          needs_poisoning);
    }
    return c::IsWord32Or(
        c::IsWord32Shl(bytes[0], c::IsInt32Constant(3 * kBitsPerByte)),
        c::IsWord32Or(
            c::IsWord32Shl(bytes[1], c::IsInt32Constant(2 * kBitsPerByte)),
            c::IsWord32Or(
                c::IsWord32Shl(bytes[2], c::IsInt32Constant(1 * kBitsPerByte)),
                bytes[3])));
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedQuadOperand(
    int offset, LoadSensitivity needs_poisoning) {
  if (TargetSupportsUnalignedAccess()) {
    return IsLoad(
        MachineType::Int32(),
        c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
        c::IsIntPtrAdd(
            c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
            c::IsIntPtrConstant(offset)),
        needs_poisoning);
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    const int kStep = -1;
    int kMsbOffset = 3;
#elif V8_TARGET_BIG_ENDIAN
    const int kStep = 1;
    int kMsbOffset = 0;
#else
#error "Unknown Architecture"
#endif
    Matcher<Node*> bytes[4];
    for (int i = 0; i < static_cast<int>(arraysize(bytes)); i++) {
      bytes[i] = IsLoad(
          (i == 0) ? MachineType::Int8() : MachineType::Uint8(),
          c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
          c::IsIntPtrAdd(
              c::IsParameter(InterpreterDispatchDescriptor::kBytecodeOffset),
              c::IsIntPtrConstant(offset + kMsbOffset + kStep * i)),
          needs_poisoning);
    }
    return c::IsWord32Or(
        c::IsWord32Shl(bytes[0], c::IsInt32Constant(3 * kBitsPerByte)),
        c::IsWord32Or(
            c::IsWord32Shl(bytes[1], c::IsInt32Constant(2 * kBitsPerByte)),
            c::IsWord32Or(
                c::IsWord32Shl(bytes[2], c::IsInt32Constant(1 * kBitsPerByte)),
                bytes[3])));
  }
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsSignedOperand(
    int offset, OperandSize operand_size, LoadSensitivity needs_poisoning) {
  switch (operand_size) {
    case OperandSize::kByte:
      return IsSignedByteOperand(offset, needs_poisoning);
    case OperandSize::kShort:
      return IsSignedShortOperand(offset, needs_poisoning);
    case OperandSize::kQuad:
      return IsSignedQuadOperand(offset, needs_poisoning);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return nullptr;
}

Matcher<Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsUnsignedOperand(
    int offset, OperandSize operand_size, LoadSensitivity needs_poisoning) {
  switch (operand_size) {
    case OperandSize::kByte:
      return IsUnsignedByteOperand(offset, needs_poisoning);
    case OperandSize::kShort:
      return IsUnsignedShortOperand(offset, needs_poisoning);
    case OperandSize::kQuad:
      return IsUnsignedQuadOperand(offset, needs_poisoning);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return nullptr;
}

Matcher<compiler::Node*>
InterpreterAssemblerTest::InterpreterAssemblerForTest::IsLoadRegisterOperand(
    int offset, OperandSize operand_size) {
  Matcher<compiler::Node*> reg_operand = IsChangeInt32ToIntPtr(
      IsSignedOperand(offset, operand_size, LoadSensitivity::kSafe));
  return IsBitcastWordToTagged(IsLoad(
      MachineType::Pointer(), c::IsLoadParentFramePointer(),
      c::IsWordShl(reg_operand, c::IsIntPtrConstant(kSystemPointerSizeLog2)),
      LoadSensitivity::kCritical));
}

TARGET_TEST_F(InterpreterAssemblerTest, BytecodeOperand) {
  static const OperandScale kOperandScales[] = {
      OperandScale::kSingle, OperandScale::kDouble, OperandScale::kQuadruple};
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    TRACED_FOREACH(interpreter::OperandScale, operand_scale, kOperandScales) {
      InterpreterAssemblerTestState state(this, bytecode);
      InterpreterAssemblerForTest m(&state, bytecode, operand_scale);
      int number_of_operands =
          interpreter::Bytecodes::NumberOfOperands(bytecode);
      for (int i = 0; i < number_of_operands; i++) {
        int offset = interpreter::Bytecodes::GetOperandOffset(bytecode, i,
                                                              operand_scale);
        OperandType operand_type =
            interpreter::Bytecodes::GetOperandType(bytecode, i);
        OperandSize operand_size =
            Bytecodes::SizeOfOperand(operand_type, operand_scale);
        switch (interpreter::Bytecodes::GetOperandType(bytecode, i)) {
          case interpreter::OperandType::kRegCount:
            EXPECT_THAT(m.BytecodeOperandCount(i),
                        m.IsUnsignedOperand(offset, operand_size,
                                            LoadSensitivity::kCritical));
            break;
          case interpreter::OperandType::kFlag8:
            EXPECT_THAT(m.BytecodeOperandFlag(i),
                        m.IsUnsignedOperand(offset, operand_size,
                                            LoadSensitivity::kCritical));
            break;
          case interpreter::OperandType::kIdx:
            EXPECT_THAT(m.BytecodeOperandIdx(i),
                        c::IsChangeUint32ToWord(m.IsUnsignedOperand(
                            offset, operand_size, LoadSensitivity::kCritical)));
            break;
          case interpreter::OperandType::kNativeContextIndex:
            EXPECT_THAT(m.BytecodeOperandNativeContextIndex(i),
                        c::IsChangeUint32ToWord(m.IsUnsignedOperand(
                            offset, operand_size, LoadSensitivity::kCritical)));
            break;
          case interpreter::OperandType::kUImm:
            EXPECT_THAT(m.BytecodeOperandUImm(i),
                        m.IsUnsignedOperand(offset, operand_size,
                                            LoadSensitivity::kCritical));
            break;
          case interpreter::OperandType::kImm: {
            EXPECT_THAT(m.BytecodeOperandImm(i),
                        m.IsSignedOperand(offset, operand_size,
                                          LoadSensitivity::kCritical));
            break;
          }
          case interpreter::OperandType::kRuntimeId:
            EXPECT_THAT(m.BytecodeOperandRuntimeId(i),
                        m.IsUnsignedOperand(offset, operand_size,
                                            LoadSensitivity::kCritical));
            break;
          case interpreter::OperandType::kIntrinsicId:
            EXPECT_THAT(m.BytecodeOperandIntrinsicId(i),
                        m.IsUnsignedOperand(offset, operand_size,
                                            LoadSensitivity::kCritical));
            break;
          case interpreter::OperandType::kRegList:
          case interpreter::OperandType::kReg:
          case interpreter::OperandType::kRegPair:
          case interpreter::OperandType::kRegOut:
          case interpreter::OperandType::kRegOutList:
          case interpreter::OperandType::kRegOutPair:
          case interpreter::OperandType::kRegOutTriple:
            EXPECT_THAT(m.LoadRegisterAtOperandIndex(i),
                        m.IsLoadRegisterOperand(offset, operand_size));
            break;
          case interpreter::OperandType::kNone:
            UNREACHABLE();
            break;
        }
      }
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, GetContext) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    EXPECT_THAT(
        m.GetContext(),
        IsBitcastWordToTagged(m.IsLoad(
            MachineType::Pointer(), c::IsLoadParentFramePointer(),
            c::IsIntPtrConstant(Register::current_context().ToOperand() *
                                kSystemPointerSize))));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadConstantPoolEntry) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    {
      TNode<IntPtrT> index = m.IntPtrConstant(2);
      TNode<Object> load_constant = m.LoadConstantPoolEntry(index);
      if (COMPRESS_POINTERS_BOOL && FLAG_turbo_decompression_elimination) {
        Matcher<Node*> constant_pool_matcher =
            IsChangeCompressedToTagged(m.IsLoadFromObject(
                MachineType::AnyCompressed(),
                c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
                c::IsIntPtrConstant(BytecodeArray::kConstantPoolOffset -
                                    kHeapObjectTag)));
        EXPECT_THAT(load_constant,
                    IsChangeCompressedToTagged(m.IsLoad(
                        MachineType::AnyCompressed(), constant_pool_matcher,
                        c::IsIntPtrConstant(FixedArray::OffsetOfElementAt(2) -
                                            kHeapObjectTag),
                        LoadSensitivity::kCritical)));
      } else {
        Matcher<Node*> constant_pool_matcher = m.IsLoadFromObject(
            MachineType::AnyTagged(),
            c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
            c::IsIntPtrConstant(BytecodeArray::kConstantPoolOffset -
                                kHeapObjectTag));
        EXPECT_THAT(
            load_constant,
            m.IsLoad(MachineType::AnyTagged(), constant_pool_matcher,
                     c::IsIntPtrConstant(FixedArray::OffsetOfElementAt(2) -
                                         kHeapObjectTag),
                     LoadSensitivity::kCritical));
      }
    }
    {
      Node* index = m.Parameter(2);
      TNode<Object> load_constant =
          m.LoadConstantPoolEntry(m.ReinterpretCast<IntPtrT>(index));
      if (COMPRESS_POINTERS_BOOL && FLAG_turbo_decompression_elimination) {
        Matcher<Node*> constant_pool_matcher =
            IsChangeCompressedToTagged(m.IsLoadFromObject(
                MachineType::AnyCompressed(),
                c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
                c::IsIntPtrConstant(BytecodeArray::kConstantPoolOffset -
                                    kHeapObjectTag)));
        EXPECT_THAT(
            load_constant,
            IsChangeCompressedToTagged(m.IsLoad(
                MachineType::AnyCompressed(), constant_pool_matcher,
                c::IsIntPtrAdd(
                    c::IsIntPtrConstant(FixedArray::kHeaderSize -
                                        kHeapObjectTag),
                    c::IsWordShl(index, c::IsIntPtrConstant(kTaggedSizeLog2))),
                LoadSensitivity::kCritical)));
      } else {
        Matcher<Node*> constant_pool_matcher = m.IsLoadFromObject(
            MachineType::AnyTagged(),
            c::IsParameter(InterpreterDispatchDescriptor::kBytecodeArray),
            c::IsIntPtrConstant(BytecodeArray::kConstantPoolOffset -
                                kHeapObjectTag));
        EXPECT_THAT(
            load_constant,
            m.IsLoad(
                MachineType::AnyTagged(), constant_pool_matcher,
                c::IsIntPtrAdd(
                    c::IsIntPtrConstant(FixedArray::kHeaderSize -
                                        kHeapObjectTag),
                    c::IsWordShl(index, c::IsIntPtrConstant(kTaggedSizeLog2))),
                LoadSensitivity::kCritical));
      }
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadObjectField) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    TNode<HeapObject> object =
        m.ReinterpretCast<HeapObject>(m.IntPtrConstant(0xDEADBEEF));
    int offset = 16;
    TNode<Object> load_field = m.LoadObjectField(object, offset);
    if (COMPRESS_POINTERS_BOOL && FLAG_turbo_decompression_elimination) {
      EXPECT_THAT(load_field,
                  IsChangeCompressedToTagged(m.IsLoadFromObject(
                      MachineType::AnyCompressed(), Eq(object),
                      c::IsIntPtrConstant(offset - kHeapObjectTag))));
    } else {
      EXPECT_THAT(
          load_field,
          m.IsLoadFromObject(MachineType::AnyTagged(), Eq(object),
                             c::IsIntPtrConstant(offset - kHeapObjectTag)));
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallRuntime2) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    TNode<Object> arg1 = m.ReinterpretCast<Object>(m.Int32Constant(2));
    TNode<Object> arg2 = m.ReinterpretCast<Object>(m.Int32Constant(3));
    TNode<Object> context = m.ReinterpretCast<Object>(m.Int32Constant(4));
    TNode<Object> call_runtime =
        m.CallRuntime(Runtime::kAdd, context, arg1, arg2);
    EXPECT_THAT(call_runtime,
                c::IsCall(_, _, Eq(arg1), Eq(arg2), _, c::IsInt32Constant(2),
                          Eq(context), _, _));
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, CallRuntime) {
  const int kResultSizes[] = {1, 2};
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    TRACED_FOREACH(int, result_size, kResultSizes) {
      if (Bytecodes::IsCallRuntime(bytecode)) {
        InterpreterAssemblerTestState state(this, bytecode);
        InterpreterAssemblerForTest m(&state, bytecode);
        Callable builtin =
            CodeFactory::InterpreterCEntry(isolate(), result_size);

        TNode<Uint32T> function_id = m.Uint32Constant(0);
        InterpreterAssembler::RegListNodePair registers(m.IntPtrConstant(1),
                                                        m.Int32Constant(2));
        TNode<Context> context = m.ReinterpretCast<Context>(m.Int32Constant(4));

        Matcher<Node*> function_table = c::IsExternalConstant(
            ExternalReference::runtime_function_table_address_for_unittests(
                isolate()));
        Matcher<Node*> function =
            c::IsIntPtrAdd(function_table,
                           c::IsChangeUint32ToWord(c::IsInt32Mul(
                               Eq(function_id),
                               c::IsInt32Constant(sizeof(Runtime::Function)))));
        Matcher<Node*> function_entry =
            m.IsLoad(MachineType::Pointer(), function,
                     c::IsIntPtrConstant(offsetof(Runtime::Function, entry)));

        Node* call_runtime =
            m.CallRuntimeN(function_id, context, registers, result_size);
        EXPECT_THAT(call_runtime,
                    c::IsCall(_, c::IsHeapConstant(builtin.code()),
                              Eq(registers.reg_count()),
                              Eq(registers.base_reg_location()), function_entry,
                              Eq(context), _, _));
      }
    }
  }
}

TARGET_TEST_F(InterpreterAssemblerTest, LoadFeedbackVector) {
  TRACED_FOREACH(interpreter::Bytecode, bytecode, kBytecodes) {
    InterpreterAssemblerTestState state(this, bytecode);
    InterpreterAssemblerForTest m(&state, bytecode);
    TNode<HeapObject> feedback_vector = m.LoadFeedbackVector();

    // Feedback vector is a phi node with two inputs. One of them is loading the
    // feedback vector and the other is undefined constant (when feedback
    // vectors aren't allocated). Find the input that loads feedback vector.
    CHECK_EQ(static_cast<Node*>(feedback_vector)->opcode(),
             i::compiler::IrOpcode::kPhi);
    Node* value0 =
        i::compiler::NodeProperties::GetValueInput(feedback_vector, 0);
    Node* value1 =
        i::compiler::NodeProperties::GetValueInput(feedback_vector, 1);
    Node* load_feedback_vector = value0;
    if (value0->opcode() == i::compiler::IrOpcode::kHeapConstant) {
      load_feedback_vector = value1;
    }

    Matcher<Node*> load_function_matcher = IsBitcastWordToTagged(
        m.IsLoad(MachineType::Pointer(), c::IsLoadParentFramePointer(),
                 c::IsIntPtrConstant(Register::function_closure().ToOperand() *
                                     kSystemPointerSize)));
    if (COMPRESS_POINTERS_BOOL && FLAG_turbo_decompression_elimination) {
      Matcher<Node*> load_vector_cell_matcher =
          IsChangeCompressedPointerToTaggedPointer(m.IsLoadFromObject(
              MachineType::CompressedPointer(), load_function_matcher,
              c::IsIntPtrConstant(JSFunction::kFeedbackCellOffset -
                                  kHeapObjectTag)));
      EXPECT_THAT(
          load_feedback_vector,
          IsChangeCompressedPointerToTaggedPointer(m.IsLoadFromObject(
              MachineType::CompressedPointer(), load_vector_cell_matcher,
              c::IsIntPtrConstant(Cell::kValueOffset - kHeapObjectTag))));
    } else {
      Matcher<Node*> load_vector_cell_matcher = m.IsLoadFromObject(
          MachineType::TaggedPointer(), load_function_matcher,
          c::IsIntPtrConstant(JSFunction::kFeedbackCellOffset -
                              kHeapObjectTag));
      EXPECT_THAT(
          load_feedback_vector,
          m.IsLoadFromObject(
              MachineType::TaggedPointer(), load_vector_cell_matcher,
              c::IsIntPtrConstant(Cell::kValueOffset - kHeapObjectTag)));
    }
  }
}

}  // namespace interpreter_assembler_unittest
}  // namespace interpreter
}  // namespace internal
}  // namespace v8
