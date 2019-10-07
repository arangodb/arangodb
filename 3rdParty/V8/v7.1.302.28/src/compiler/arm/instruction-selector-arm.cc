// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/base/bits.h"
#include "src/compiler/instruction-selector-impl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// Adds Arm-specific methods for generating InstructionOperands.
class ArmOperandGenerator : public OperandGenerator {
 public:
  explicit ArmOperandGenerator(InstructionSelector* selector)
      : OperandGenerator(selector) {}

  bool CanBeImmediate(int32_t value) const {
    return Assembler::ImmediateFitsAddrMode1Instruction(value);
  }

  bool CanBeImmediate(uint32_t value) const {
    return CanBeImmediate(bit_cast<int32_t>(value));
  }

  bool CanBeImmediate(Node* node, InstructionCode opcode) {
    Int32Matcher m(node);
    if (!m.HasValue()) return false;
    int32_t value = m.Value();
    switch (ArchOpcodeField::decode(opcode)) {
      case kArmAnd:
      case kArmMov:
      case kArmMvn:
      case kArmBic:
        return CanBeImmediate(value) || CanBeImmediate(~value);

      case kArmAdd:
      case kArmSub:
      case kArmCmp:
      case kArmCmn:
        return CanBeImmediate(value) || CanBeImmediate(-value);

      case kArmTst:
      case kArmTeq:
      case kArmOrr:
      case kArmEor:
      case kArmRsb:
        return CanBeImmediate(value);

      case kArmVldrF32:
      case kArmVstrF32:
      case kArmVldrF64:
      case kArmVstrF64:
        return value >= -1020 && value <= 1020 && (value % 4) == 0;

      case kArmLdrb:
      case kArmLdrsb:
      case kArmStrb:
      case kArmLdr:
      case kArmStr:
        return value >= -4095 && value <= 4095;

      case kArmLdrh:
      case kArmLdrsh:
      case kArmStrh:
        return value >= -255 && value <= 255;

      default:
        break;
    }
    return false;
  }

  // Use the stack pointer if the node is LoadStackPointer, otherwise assign a
  // register.
  InstructionOperand UseRegisterOrStackPointer(Node* node) {
    if (node->opcode() == IrOpcode::kLoadStackPointer) {
      return LocationOperand(LocationOperand::EXPLICIT,
                             LocationOperand::REGISTER,
                             MachineRepresentation::kWord32, sp.code());
    }
    return UseRegister(node);
  }
};


namespace {

void VisitRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)));
}

void VisitRRR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  ArmOperandGenerator g(selector);
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}

void VisitRRRShuffle(InstructionSelector* selector, ArchOpcode opcode,
                     Node* node) {
  ArmOperandGenerator g(selector);
  // Swap inputs to save an instruction in the CodeGenerator for High ops.
  if (opcode == kArmS32x4ZipRight || opcode == kArmS32x4UnzipRight ||
      opcode == kArmS32x4TransposeRight || opcode == kArmS16x8ZipRight ||
      opcode == kArmS16x8UnzipRight || opcode == kArmS16x8TransposeRight ||
      opcode == kArmS8x16ZipRight || opcode == kArmS8x16UnzipRight ||
      opcode == kArmS8x16TransposeRight) {
    Node* in0 = node->InputAt(0);
    Node* in1 = node->InputAt(1);
    node->ReplaceInput(0, in1);
    node->ReplaceInput(1, in0);
  }
  // Use DefineSameAsFirst for binary ops that clobber their inputs, e.g. the
  // NEON vzip, vuzp, and vtrn instructions.
  selector->Emit(opcode, g.DefineSameAsFirst(node),
                 g.UseRegister(node->InputAt(0)),
                 g.UseRegister(node->InputAt(1)));
}

void VisitRRI(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  ArmOperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm));
}

void VisitRRIR(InstructionSelector* selector, ArchOpcode opcode, Node* node) {
  ArmOperandGenerator g(selector);
  int32_t imm = OpParameter<int32_t>(node->op());
  selector->Emit(opcode, g.DefineAsRegister(node),
                 g.UseRegister(node->InputAt(0)), g.UseImmediate(imm),
                 g.UseRegister(node->InputAt(1)));
}

template <IrOpcode::Value kOpcode, int kImmMin, int kImmMax,
          AddressingMode kImmMode, AddressingMode kRegMode>
bool TryMatchShift(InstructionSelector* selector,
                   InstructionCode* opcode_return, Node* node,
                   InstructionOperand* value_return,
                   InstructionOperand* shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() == kOpcode) {
    Int32BinopMatcher m(node);
    *value_return = g.UseRegister(m.left().node());
    if (m.right().IsInRange(kImmMin, kImmMax)) {
      *opcode_return |= AddressingModeField::encode(kImmMode);
      *shift_return = g.UseImmediate(m.right().node());
    } else {
      *opcode_return |= AddressingModeField::encode(kRegMode);
      *shift_return = g.UseRegister(m.right().node());
    }
    return true;
  }
  return false;
}

template <IrOpcode::Value kOpcode, int kImmMin, int kImmMax,
          AddressingMode kImmMode>
bool TryMatchShiftImmediate(InstructionSelector* selector,
                            InstructionCode* opcode_return, Node* node,
                            InstructionOperand* value_return,
                            InstructionOperand* shift_return) {
  ArmOperandGenerator g(selector);
  if (node->opcode() == kOpcode) {
    Int32BinopMatcher m(node);
    if (m.right().IsInRange(kImmMin, kImmMax)) {
      *opcode_return |= AddressingModeField::encode(kImmMode);
      *value_return = g.UseRegister(m.left().node());
      *shift_return = g.UseImmediate(m.right().node());
      return true;
    }
  }
  return false;
}

bool TryMatchROR(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Ror, 1, 31, kMode_Operand2_R_ROR_I,
                       kMode_Operand2_R_ROR_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchASR(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Sar, 1, 32, kMode_Operand2_R_ASR_I,
                       kMode_Operand2_R_ASR_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchLSL(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Shl, 0, 31, kMode_Operand2_R_LSL_I,
                       kMode_Operand2_R_LSL_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}

bool TryMatchLSLImmediate(InstructionSelector* selector,
                          InstructionCode* opcode_return, Node* node,
                          InstructionOperand* value_return,
                          InstructionOperand* shift_return) {
  return TryMatchShiftImmediate<IrOpcode::kWord32Shl, 0, 31,
                                kMode_Operand2_R_LSL_I>(
      selector, opcode_return, node, value_return, shift_return);
}

bool TryMatchLSR(InstructionSelector* selector, InstructionCode* opcode_return,
                 Node* node, InstructionOperand* value_return,
                 InstructionOperand* shift_return) {
  return TryMatchShift<IrOpcode::kWord32Shr, 1, 32, kMode_Operand2_R_LSR_I,
                       kMode_Operand2_R_LSR_R>(selector, opcode_return, node,
                                               value_return, shift_return);
}


bool TryMatchShift(InstructionSelector* selector,
                   InstructionCode* opcode_return, Node* node,
                   InstructionOperand* value_return,
                   InstructionOperand* shift_return) {
  return (
      TryMatchASR(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchLSL(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchLSR(selector, opcode_return, node, value_return, shift_return) ||
      TryMatchROR(selector, opcode_return, node, value_return, shift_return));
}


bool TryMatchImmediateOrShift(InstructionSelector* selector,
                              InstructionCode* opcode_return, Node* node,
                              size_t* input_count_return,
                              InstructionOperand* inputs) {
  ArmOperandGenerator g(selector);
  if (g.CanBeImmediate(node, *opcode_return)) {
    *opcode_return |= AddressingModeField::encode(kMode_Operand2_I);
    inputs[0] = g.UseImmediate(node);
    *input_count_return = 1;
    return true;
  }
  if (TryMatchShift(selector, opcode_return, node, &inputs[0], &inputs[1])) {
    *input_count_return = 2;
    return true;
  }
  return false;
}


void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, InstructionCode reverse_opcode,
                FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  if (m.left().node() == m.right().node()) {
    // If both inputs refer to the same operand, enforce allocating a register
    // for both of them to ensure that we don't end up generating code like
    // this:
    //
    //   mov r0, r1, asr #16
    //   adds r0, r0, r1, asr #16
    //   bvs label
    InstructionOperand const input = g.UseRegister(m.left().node());
    opcode |= AddressingModeField::encode(kMode_Operand2_R);
    inputs[input_count++] = input;
    inputs[input_count++] = input;
  } else if (TryMatchImmediateOrShift(selector, &opcode, m.right().node(),
                                      &input_count, &inputs[1])) {
    inputs[0] = g.UseRegister(m.left().node());
    input_count++;
  } else if (TryMatchImmediateOrShift(selector, &reverse_opcode,
                                      m.left().node(), &input_count,
                                      &inputs[1])) {
    inputs[0] = g.UseRegister(m.right().node());
    opcode = reverse_opcode;
    input_count++;
  } else {
    opcode |= AddressingModeField::encode(kMode_Operand2_R);
    inputs[input_count++] = g.UseRegister(m.left().node());
    inputs[input_count++] = g.UseRegister(m.right().node());
  }

  outputs[output_count++] = g.DefineAsRegister(node);

  DCHECK_NE(0u, input_count);
  DCHECK_EQ(1u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);
  DCHECK_NE(kMode_None, AddressingModeField::decode(opcode));

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}


void VisitBinop(InstructionSelector* selector, Node* node,
                InstructionCode opcode, InstructionCode reverse_opcode) {
  FlagsContinuation cont;
  VisitBinop(selector, node, opcode, reverse_opcode, &cont);
}


void EmitDiv(InstructionSelector* selector, ArchOpcode div_opcode,
             ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode,
             InstructionOperand result_operand, InstructionOperand left_operand,
             InstructionOperand right_operand) {
  ArmOperandGenerator g(selector);
  if (selector->IsSupported(SUDIV)) {
    selector->Emit(div_opcode, result_operand, left_operand, right_operand);
    return;
  }
  InstructionOperand left_double_operand = g.TempDoubleRegister();
  InstructionOperand right_double_operand = g.TempDoubleRegister();
  InstructionOperand result_double_operand = g.TempDoubleRegister();
  selector->Emit(f64i32_opcode, left_double_operand, left_operand);
  selector->Emit(f64i32_opcode, right_double_operand, right_operand);
  selector->Emit(kArmVdivF64, result_double_operand, left_double_operand,
                 right_double_operand);
  selector->Emit(i32f64_opcode, result_operand, result_double_operand);
}


void VisitDiv(InstructionSelector* selector, Node* node, ArchOpcode div_opcode,
              ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  EmitDiv(selector, div_opcode, f64i32_opcode, i32f64_opcode,
          g.DefineAsRegister(node), g.UseRegister(m.left().node()),
          g.UseRegister(m.right().node()));
}


void VisitMod(InstructionSelector* selector, Node* node, ArchOpcode div_opcode,
              ArchOpcode f64i32_opcode, ArchOpcode i32f64_opcode) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand div_operand = g.TempRegister();
  InstructionOperand result_operand = g.DefineAsRegister(node);
  InstructionOperand left_operand = g.UseRegister(m.left().node());
  InstructionOperand right_operand = g.UseRegister(m.right().node());
  EmitDiv(selector, div_opcode, f64i32_opcode, i32f64_opcode, div_operand,
          left_operand, right_operand);
  if (selector->IsSupported(ARMv7)) {
    selector->Emit(kArmMls, result_operand, div_operand, right_operand,
                   left_operand);
  } else {
    InstructionOperand mul_operand = g.TempRegister();
    selector->Emit(kArmMul, mul_operand, div_operand, right_operand);
    selector->Emit(kArmSub | AddressingModeField::encode(kMode_Operand2_R),
                   result_operand, left_operand, mul_operand);
  }
}

void EmitLoad(InstructionSelector* selector, InstructionCode opcode,
              InstructionOperand* output, Node* base, Node* index) {
  ArmOperandGenerator g(selector);
  InstructionOperand inputs[3];
  size_t input_count = 2;

  inputs[0] = g.UseRegister(base);
  if (g.CanBeImmediate(index, opcode)) {
    inputs[1] = g.UseImmediate(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RI);
  } else if ((opcode == kArmLdr) &&
             TryMatchLSLImmediate(selector, &opcode, index, &inputs[1],
                                  &inputs[2])) {
    input_count = 3;
  } else {
    inputs[1] = g.UseRegister(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RR);
  }
  selector->Emit(opcode, 1, output, input_count, inputs);
}

void EmitStore(InstructionSelector* selector, InstructionCode opcode,
               size_t input_count, InstructionOperand* inputs,
               Node* index) {
  ArmOperandGenerator g(selector);

  if (g.CanBeImmediate(index, opcode)) {
    inputs[input_count++] = g.UseImmediate(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RI);
  } else if ((opcode == kArmStr) &&
             TryMatchLSLImmediate(selector, &opcode, index, &inputs[2],
                                  &inputs[3])) {
    input_count = 4;
  } else {
    inputs[input_count++] = g.UseRegister(index);
    opcode |= AddressingModeField::encode(kMode_Offset_RR);
  }
  selector->Emit(opcode, 0, nullptr, input_count, inputs);
}

void VisitPairAtomicBinOp(InstructionSelector* selector, Node* node,
                          ArchOpcode opcode) {
  ArmOperandGenerator g(selector);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  Node* value_high = node->InputAt(3);
  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[] = {
      g.UseUniqueRegister(value), g.UseUniqueRegister(value_high),
      g.UseUniqueRegister(base), g.UseUniqueRegister(index)};
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  if (projection1) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r2),
                                    g.DefineAsFixed(projection1, r3)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(r6),
                                  g.TempRegister(r7), g.TempRegister()};
    selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                   arraysize(temps), temps);
  } else if (projection0) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r2)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(r6),
                                  g.TempRegister(r7), g.TempRegister(),
                                  g.TempRegister(r3)};
    selector->Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
                   arraysize(temps), temps);
  } else {
    InstructionOperand temps[] = {g.TempRegister(),   g.TempRegister(r6),
                                  g.TempRegister(r7), g.TempRegister(),
                                  g.TempRegister(r2), g.TempRegister(r3)};
    selector->Emit(code, 0, nullptr, arraysize(inputs), inputs,
                   arraysize(temps), temps);
  }
}

}  // namespace

void InstructionSelector::VisitStackSlot(Node* node) {
  StackSlotRepresentation rep = StackSlotRepresentationOf(node->op());
  int slot = frame_->AllocateSpillSlot(rep.size());
  OperandGenerator g(this);

  Emit(kArchStackSlot, g.DefineAsRegister(node),
       sequence()->AddImmediate(Constant(slot)), 0, nullptr);
}

void InstructionSelector::VisitDebugAbort(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArchDebugAbort, g.NoOutput(), g.UseFixed(node->InputAt(0), r1));
}

void InstructionSelector::VisitLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionCode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kFloat32:
      opcode = kArmVldrF32;
      break;
    case MachineRepresentation::kFloat64:
      opcode = kArmVldrF64;
      break;
    case MachineRepresentation::kBit:  // Fall through.
    case MachineRepresentation::kWord8:
      opcode = load_rep.IsUnsigned() ? kArmLdrb : kArmLdrsb;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsUnsigned() ? kArmLdrh : kArmLdrsh;
      break;
    case MachineRepresentation::kTaggedSigned:   // Fall through.
    case MachineRepresentation::kTaggedPointer:  // Fall through.
    case MachineRepresentation::kTagged:  // Fall through.
    case MachineRepresentation::kWord32:
      opcode = kArmLdr;
      break;
    case MachineRepresentation::kSimd128:
      opcode = kArmVld1S128;
      break;
    case MachineRepresentation::kWord64:   // Fall through.
    case MachineRepresentation::kNone:
      UNREACHABLE();
      return;
  }
  if (node->opcode() == IrOpcode::kPoisonedLoad) {
    CHECK_NE(poisoning_level_, PoisoningMitigationLevel::kDontPoison);
    opcode |= MiscField::encode(kMemoryAccessPoisoned);
  }

  InstructionOperand output = g.DefineAsRegister(node);
  EmitLoad(this, opcode, &output, base, index);
}

void InstructionSelector::VisitPoisonedLoad(Node* node) { VisitLoad(node); }

void InstructionSelector::VisitProtectedLoad(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitStore(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  StoreRepresentation store_rep = StoreRepresentationOf(node->op());
  WriteBarrierKind write_barrier_kind = store_rep.write_barrier_kind();
  MachineRepresentation rep = store_rep.representation();

  if (write_barrier_kind != kNoWriteBarrier) {
    DCHECK(CanBeTaggedPointer(rep));
    AddressingMode addressing_mode;
    InstructionOperand inputs[3];
    size_t input_count = 0;
    inputs[input_count++] = g.UseUniqueRegister(base);
    // OutOfLineRecordWrite uses the index in an 'add' instruction as well as
    // for the store itself, so we must check compatibility with both.
    if (g.CanBeImmediate(index, kArmAdd) && g.CanBeImmediate(index, kArmStr)) {
      inputs[input_count++] = g.UseImmediate(index);
      addressing_mode = kMode_Offset_RI;
    } else {
      inputs[input_count++] = g.UseUniqueRegister(index);
      addressing_mode = kMode_Offset_RR;
    }
    inputs[input_count++] = g.UseUniqueRegister(value);
    RecordWriteMode record_write_mode = RecordWriteMode::kValueIsAny;
    switch (write_barrier_kind) {
      case kNoWriteBarrier:
        UNREACHABLE();
        break;
      case kMapWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsMap;
        break;
      case kPointerWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsPointer;
        break;
      case kFullWriteBarrier:
        record_write_mode = RecordWriteMode::kValueIsAny;
        break;
    }
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    size_t const temp_count = arraysize(temps);
    InstructionCode code = kArchStoreWithWriteBarrier;
    code |= AddressingModeField::encode(addressing_mode);
    code |= MiscField::encode(static_cast<int>(record_write_mode));
    Emit(code, 0, nullptr, input_count, inputs, temp_count, temps);
  } else {
    InstructionCode opcode = kArchNop;
    switch (rep) {
      case MachineRepresentation::kFloat32:
        opcode = kArmVstrF32;
        break;
      case MachineRepresentation::kFloat64:
        opcode = kArmVstrF64;
        break;
      case MachineRepresentation::kBit:  // Fall through.
      case MachineRepresentation::kWord8:
        opcode = kArmStrb;
        break;
      case MachineRepresentation::kWord16:
        opcode = kArmStrh;
        break;
      case MachineRepresentation::kTaggedSigned:   // Fall through.
      case MachineRepresentation::kTaggedPointer:  // Fall through.
      case MachineRepresentation::kTagged:  // Fall through.
      case MachineRepresentation::kWord32:
        opcode = kArmStr;
        break;
      case MachineRepresentation::kSimd128:
        opcode = kArmVst1S128;
        break;
      case MachineRepresentation::kWord64:   // Fall through.
      case MachineRepresentation::kNone:
        UNREACHABLE();
        return;
    }

    InstructionOperand inputs[4];
    size_t input_count = 0;
    inputs[input_count++] = g.UseRegister(value);
    inputs[input_count++] = g.UseRegister(base);
    EmitStore(this, opcode, input_count, inputs, index);
  }
}

void InstructionSelector::VisitProtectedStore(Node* node) {
  // TODO(eholk)
  UNIMPLEMENTED();
}

void InstructionSelector::VisitUnalignedLoad(Node* node) {
  MachineRepresentation load_rep =
      LoadRepresentationOf(node->op()).representation();
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);

  InstructionCode opcode = kArmLdr;
  // Only floating point loads need to be specially handled; integer loads
  // support unaligned access. We support unaligned FP loads by loading to
  // integer registers first, then moving to the destination FP register. If
  // NEON is supported, we use the vld1.8 instruction.
  switch (load_rep) {
    case MachineRepresentation::kFloat32: {
      InstructionOperand temp = g.TempRegister();
      EmitLoad(this, opcode, &temp, base, index);
      Emit(kArmVmovF32U32, g.DefineAsRegister(node), temp);
      return;
    }
    case MachineRepresentation::kFloat64:
    case MachineRepresentation::kSimd128: {
      // Compute the address of the least-significant byte of the FP value.
      // We assume that the base node is unlikely to be an encodable immediate
      // or the result of a shift operation, so only consider the addressing
      // mode that should be used for the index node.
      InstructionCode add_opcode = kArmAdd;
      InstructionOperand inputs[3];
      inputs[0] = g.UseRegister(base);

      size_t input_count;
      if (TryMatchImmediateOrShift(this, &add_opcode, index, &input_count,
                                   &inputs[1])) {
        // input_count has been set by TryMatchImmediateOrShift(), so
        // increment it to account for the base register in inputs[0].
        input_count++;
      } else {
        add_opcode |= AddressingModeField::encode(kMode_Operand2_R);
        inputs[1] = g.UseRegister(index);
        input_count = 2;  // Base register and index.
      }

      InstructionOperand addr = g.TempRegister();
      Emit(add_opcode, 1, &addr, input_count, inputs);

      if (CpuFeatures::IsSupported(NEON)) {
        // With NEON we can load directly from the calculated address.
        InstructionCode op = load_rep == MachineRepresentation::kFloat64
                                 ? kArmVld1F64
                                 : kArmVld1S128;
        op |= AddressingModeField::encode(kMode_Operand2_R);
        Emit(op, g.DefineAsRegister(node), addr);
      } else {
        DCHECK_NE(MachineRepresentation::kSimd128, load_rep);
        // Load both halves and move to an FP register.
        InstructionOperand fp_lo = g.TempRegister();
        InstructionOperand fp_hi = g.TempRegister();
        opcode |= AddressingModeField::encode(kMode_Offset_RI);
        Emit(opcode, fp_lo, addr, g.TempImmediate(0));
        Emit(opcode, fp_hi, addr, g.TempImmediate(4));
        Emit(kArmVmovF64U32U32, g.DefineAsRegister(node), fp_lo, fp_hi);
      }
      return;
    }
    default:
      // All other cases should support unaligned accesses.
      UNREACHABLE();
      return;
  }
}

void InstructionSelector::VisitUnalignedStore(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);

  InstructionOperand inputs[4];
  size_t input_count = 0;

  UnalignedStoreRepresentation store_rep =
      UnalignedStoreRepresentationOf(node->op());

  // Only floating point stores need to be specially handled; integer stores
  // support unaligned access. We support unaligned FP stores by moving the
  // value to integer registers first, then storing to the destination address.
  // If NEON is supported, we use the vst1.8 instruction.
  switch (store_rep) {
    case MachineRepresentation::kFloat32: {
      inputs[input_count++] = g.TempRegister();
      Emit(kArmVmovU32F32, inputs[0], g.UseRegister(value));
      inputs[input_count++] = g.UseRegister(base);
      EmitStore(this, kArmStr, input_count, inputs, index);
      return;
    }
    case MachineRepresentation::kFloat64:
    case MachineRepresentation::kSimd128: {
      if (CpuFeatures::IsSupported(NEON)) {
        InstructionOperand address = g.TempRegister();
        {
          // First we have to calculate the actual address.
          InstructionCode add_opcode = kArmAdd;
          InstructionOperand inputs[3];
          inputs[0] = g.UseRegister(base);

          size_t input_count;
          if (TryMatchImmediateOrShift(this, &add_opcode, index, &input_count,
                                       &inputs[1])) {
            // input_count has been set by TryMatchImmediateOrShift(), so
            // increment it to account for the base register in inputs[0].
            input_count++;
          } else {
            add_opcode |= AddressingModeField::encode(kMode_Operand2_R);
            inputs[1] = g.UseRegister(index);
            input_count = 2;  // Base register and index.
          }

          Emit(add_opcode, 1, &address, input_count, inputs);
        }

        inputs[input_count++] = g.UseRegister(value);
        inputs[input_count++] = address;
        InstructionCode op = store_rep == MachineRepresentation::kFloat64
                                 ? kArmVst1F64
                                 : kArmVst1S128;
        op |= AddressingModeField::encode(kMode_Operand2_R);
        Emit(op, 0, nullptr, input_count, inputs);
      } else {
        DCHECK_NE(MachineRepresentation::kSimd128, store_rep);
        // Store a 64-bit floating point value using two 32-bit integer stores.
        // Computing the store address here would require three live temporary
        // registers (fp<63:32>, fp<31:0>, address), so compute base + 4 after
        // storing the least-significant half of the value.

        // First, move the 64-bit FP value into two temporary integer registers.
        InstructionOperand fp[] = {g.TempRegister(), g.TempRegister()};
        inputs[input_count++] = g.UseRegister(value);
        Emit(kArmVmovU32U32F64, arraysize(fp), fp, input_count, inputs);

        // Store the least-significant half.
        inputs[0] = fp[0];  // Low 32-bits of FP value.
        inputs[input_count++] =
            g.UseRegister(base);  // First store base address.
        EmitStore(this, kArmStr, input_count, inputs, index);

        // Store the most-significant half.
        InstructionOperand base4 = g.TempRegister();
        Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_I), base4,
             g.UseRegister(base), g.TempImmediate(4));  // Compute base + 4.
        inputs[0] = fp[1];  // High 32-bits of FP value.
        inputs[1] = base4;  // Second store base + 4 address.
        EmitStore(this, kArmStr, input_count, inputs, index);
      }
      return;
    }
    default:
      // All other cases should support unaligned accesses.
      UNREACHABLE();
      return;
  }
}

namespace {

void EmitBic(InstructionSelector* selector, Node* node, Node* left,
             Node* right) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmBic;
  InstructionOperand value_operand;
  InstructionOperand shift_operand;
  if (TryMatchShift(selector, &opcode, right, &value_operand, &shift_operand)) {
    selector->Emit(opcode, g.DefineAsRegister(node), g.UseRegister(left),
                   value_operand, shift_operand);
    return;
  }
  selector->Emit(opcode | AddressingModeField::encode(kMode_Operand2_R),
                 g.DefineAsRegister(node), g.UseRegister(left),
                 g.UseRegister(right));
}


void EmitUbfx(InstructionSelector* selector, Node* node, Node* left,
              uint32_t lsb, uint32_t width) {
  DCHECK_LE(1u, width);
  DCHECK_LE(width, 32u - lsb);
  ArmOperandGenerator g(selector);
  selector->Emit(kArmUbfx, g.DefineAsRegister(node), g.UseRegister(left),
                 g.TempImmediate(lsb), g.TempImmediate(width));
}

}  // namespace


void InstructionSelector::VisitWord32And(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.left().IsWord32Xor() && CanCover(node, m.left().node())) {
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().Is(-1)) {
      EmitBic(this, node, m.right().node(), mleft.left().node());
      return;
    }
  }
  if (m.right().IsWord32Xor() && CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    if (mright.right().Is(-1)) {
      EmitBic(this, node, m.left().node(), mright.left().node());
      return;
    }
  }
  if (m.right().HasValue()) {
    uint32_t const value = m.right().Value();
    uint32_t width = base::bits::CountPopulation(value);
    uint32_t leading_zeros = base::bits::CountLeadingZeros32(value);

    // Try to merge SHR operations on the left hand input into this AND.
    if (m.left().IsWord32Shr()) {
      Int32BinopMatcher mshr(m.left().node());
      if (mshr.right().HasValue()) {
        uint32_t const shift = mshr.right().Value();

        if (((shift == 8) || (shift == 16) || (shift == 24)) &&
            (value == 0xFF)) {
          // Merge SHR into AND by emitting a UXTB instruction with a
          // bytewise rotation.
          Emit(kArmUxtb, g.DefineAsRegister(m.node()),
               g.UseRegister(mshr.left().node()),
               g.TempImmediate(mshr.right().Value()));
          return;
        } else if (((shift == 8) || (shift == 16)) && (value == 0xFFFF)) {
          // Merge SHR into AND by emitting a UXTH instruction with a
          // bytewise rotation.
          Emit(kArmUxth, g.DefineAsRegister(m.node()),
               g.UseRegister(mshr.left().node()),
               g.TempImmediate(mshr.right().Value()));
          return;
        } else if (IsSupported(ARMv7) && (width != 0) &&
                   ((leading_zeros + width) == 32)) {
          // Merge Shr into And by emitting a UBFX instruction.
          DCHECK_EQ(0u, base::bits::CountTrailingZeros32(value));
          if ((1 <= shift) && (shift <= 31)) {
            // UBFX cannot extract bits past the register size, however since
            // shifting the original value would have introduced some zeros we
            // can still use UBFX with a smaller mask and the remaining bits
            // will be zeros.
            EmitUbfx(this, node, mshr.left().node(), shift,
                     std::min(width, 32 - shift));
            return;
          }
        }
      }
    } else if (value == 0xFFFF) {
      // Emit UXTH for this AND. We don't bother testing for UXTB, as it's no
      // better than AND 0xFF for this operation.
      Emit(kArmUxth, g.DefineAsRegister(m.node()),
           g.UseRegister(m.left().node()), g.TempImmediate(0));
      return;
    }
    if (g.CanBeImmediate(~value)) {
      // Emit BIC for this AND by inverting the immediate value first.
      Emit(kArmBic | AddressingModeField::encode(kMode_Operand2_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.TempImmediate(~value));
      return;
    }
    if (!g.CanBeImmediate(value) && IsSupported(ARMv7)) {
      // If value has 9 to 23 contiguous set bits, and has the lsb set, we can
      // replace this AND with UBFX. Other contiguous bit patterns have already
      // been handled by BIC or will be handled by AND.
      if ((width != 0) && ((leading_zeros + width) == 32) &&
          (9 <= leading_zeros) && (leading_zeros <= 23)) {
        DCHECK_EQ(0u, base::bits::CountTrailingZeros32(value));
        EmitUbfx(this, node, m.left().node(), 0, width);
        return;
      }

      width = 32 - width;
      leading_zeros = base::bits::CountLeadingZeros32(~value);
      uint32_t lsb = base::bits::CountTrailingZeros32(~value);
      if ((leading_zeros + width + lsb) == 32) {
        // This AND can be replaced with BFC.
        Emit(kArmBfc, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
             g.TempImmediate(lsb), g.TempImmediate(width));
        return;
      }
    }
  }
  VisitBinop(this, node, kArmAnd, kArmAnd);
}


void InstructionSelector::VisitWord32Or(Node* node) {
  VisitBinop(this, node, kArmOrr, kArmOrr);
}


void InstructionSelector::VisitWord32Xor(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().Is(-1)) {
    InstructionCode opcode = kArmMvn;
    InstructionOperand value_operand;
    InstructionOperand shift_operand;
    if (TryMatchShift(this, &opcode, m.left().node(), &value_operand,
                      &shift_operand)) {
      Emit(opcode, g.DefineAsRegister(node), value_operand, shift_operand);
      return;
    }
    Emit(opcode | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineAsRegister(node), g.UseRegister(m.left().node()));
    return;
  }
  VisitBinop(this, node, kArmEor, kArmEor);
}


namespace {

template <typename TryMatchShift>
void VisitShift(InstructionSelector* selector, Node* node,
                TryMatchShift try_match_shift, FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  InstructionCode opcode = kArmMov;
  InstructionOperand inputs[2];
  size_t input_count = 2;
  InstructionOperand outputs[1];
  size_t output_count = 0;

  CHECK(try_match_shift(selector, &opcode, node, &inputs[0], &inputs[1]));

  outputs[output_count++] = g.DefineAsRegister(node);

  DCHECK_NE(0u, input_count);
  DCHECK_NE(0u, output_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);
  DCHECK_NE(kMode_None, AddressingModeField::decode(opcode));

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}


template <typename TryMatchShift>
void VisitShift(InstructionSelector* selector, Node* node,
                              TryMatchShift try_match_shift) {
  FlagsContinuation cont;
  VisitShift(selector, node, try_match_shift, &cont);
}

}  // namespace


void InstructionSelector::VisitWord32Shl(Node* node) {
  VisitShift(this, node, TryMatchLSL);
}


void InstructionSelector::VisitWord32Shr(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (IsSupported(ARMv7) && m.left().IsWord32And() &&
      m.right().IsInRange(0, 31)) {
    uint32_t lsb = m.right().Value();
    Int32BinopMatcher mleft(m.left().node());
    if (mleft.right().HasValue()) {
      uint32_t value = (mleft.right().Value() >> lsb) << lsb;
      uint32_t width = base::bits::CountPopulation(value);
      uint32_t msb = base::bits::CountLeadingZeros32(value);
      if (msb + width + lsb == 32) {
        DCHECK_EQ(lsb, base::bits::CountTrailingZeros32(value));
        return EmitUbfx(this, node, mleft.left().node(), lsb, width);
      }
    }
  }
  VisitShift(this, node, TryMatchLSR);
}


void InstructionSelector::VisitWord32Sar(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (CanCover(m.node(), m.left().node()) && m.left().IsWord32Shl()) {
    Int32BinopMatcher mleft(m.left().node());
    if (m.right().HasValue() && mleft.right().HasValue()) {
      uint32_t sar = m.right().Value();
      uint32_t shl = mleft.right().Value();
      if ((sar == shl) && (sar == 16)) {
        Emit(kArmSxth, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(0));
        return;
      } else if ((sar == shl) && (sar == 24)) {
        Emit(kArmSxtb, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(0));
        return;
      } else if (IsSupported(ARMv7) && (sar >= shl)) {
        Emit(kArmSbfx, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()), g.TempImmediate(sar - shl),
             g.TempImmediate(32 - sar));
        return;
      }
    }
  }
  VisitShift(this, node, TryMatchASR);
}

void InstructionSelector::VisitInt32PairAdd(Node* node) {
  ArmOperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // registers.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kArmAddPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairSub(Node* node) {
  ArmOperandGenerator g(this);

  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    // We use UseUniqueRegister here to avoid register sharing with the output
    // register.
    InstructionOperand inputs[] = {
        g.UseRegister(node->InputAt(0)), g.UseUniqueRegister(node->InputAt(1)),
        g.UseRegister(node->InputAt(2)), g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kArmSubPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kArmSub | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

void InstructionSelector::VisitInt32PairMul(Node* node) {
  ArmOperandGenerator g(this);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                   g.UseUniqueRegister(node->InputAt(1)),
                                   g.UseUniqueRegister(node->InputAt(2)),
                                   g.UseUniqueRegister(node->InputAt(3))};

    InstructionOperand outputs[] = {
        g.DefineAsRegister(node),
        g.DefineAsRegister(NodeProperties::FindProjection(node, 1))};

    Emit(kArmMulPair, 2, outputs, 4, inputs);
  } else {
    // The high word of the result is not used, so we emit the standard 32 bit
    // instruction.
    Emit(kArmMul | AddressingModeField::encode(kMode_Operand2_R),
         g.DefineSameAsFirst(node), g.UseRegister(node->InputAt(0)),
         g.UseRegister(node->InputAt(2)));
  }
}

namespace {
// Shared routine for multiple shift operations.
void VisitWord32PairShift(InstructionSelector* selector, InstructionCode opcode,
                          Node* node) {
  ArmOperandGenerator g(selector);
  // We use g.UseUniqueRegister here to guarantee that there is
  // no register aliasing of input registers with output registers.
  Int32Matcher m(node->InputAt(2));
  InstructionOperand shift_operand;
  if (m.HasValue()) {
    shift_operand = g.UseImmediate(m.node());
  } else {
    shift_operand = g.UseUniqueRegister(m.node());
  }

  InstructionOperand inputs[] = {g.UseUniqueRegister(node->InputAt(0)),
                                 g.UseUniqueRegister(node->InputAt(1)),
                                 shift_operand};

  Node* projection1 = NodeProperties::FindProjection(node, 1);

  InstructionOperand outputs[2];
  InstructionOperand temps[1];
  int32_t output_count = 0;
  int32_t temp_count = 0;

  outputs[output_count++] = g.DefineAsRegister(node);
  if (projection1) {
    outputs[output_count++] = g.DefineAsRegister(projection1);
  } else {
    temps[temp_count++] = g.TempRegister();
  }

  selector->Emit(opcode, output_count, outputs, 3, inputs, temp_count, temps);
}
}  // namespace
void InstructionSelector::VisitWord32PairShl(Node* node) {
  VisitWord32PairShift(this, kArmLslPair, node);
}

void InstructionSelector::VisitWord32PairShr(Node* node) {
  VisitWord32PairShift(this, kArmLsrPair, node);
}

void InstructionSelector::VisitWord32PairSar(Node* node) {
  VisitWord32PairShift(this, kArmAsrPair, node);
}

void InstructionSelector::VisitWord32Ror(Node* node) {
  VisitShift(this, node, TryMatchROR);
}

void InstructionSelector::VisitWord32Ctz(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBits(Node* node) {
  DCHECK(IsSupported(ARMv7));
  VisitRR(this, kArmRbit, node);
}

void InstructionSelector::VisitWord64ReverseBytes(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitWord32ReverseBytes(Node* node) {
  VisitRR(this, kArmRev, node);
}

void InstructionSelector::VisitWord32Popcnt(Node* node) { UNREACHABLE(); }

void InstructionSelector::VisitSpeculationFence(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmDsbIsb, g.NoOutput());
}

void InstructionSelector::VisitInt32Add(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (CanCover(node, m.left().node())) {
    switch (m.left().opcode()) {
      case IrOpcode::kInt32Mul: {
        Int32BinopMatcher mleft(m.left().node());
        Emit(kArmMla, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseRegister(mleft.right().node()),
             g.UseRegister(m.right().node()));
        return;
      }
      case IrOpcode::kInt32MulHigh: {
        Int32BinopMatcher mleft(m.left().node());
        Emit(kArmSmmla, g.DefineAsRegister(node),
             g.UseRegister(mleft.left().node()),
             g.UseRegister(mleft.right().node()),
             g.UseRegister(m.right().node()));
        return;
      }
      case IrOpcode::kWord32And: {
        Int32BinopMatcher mleft(m.left().node());
        if (mleft.right().Is(0xFF)) {
          Emit(kArmUxtab, g.DefineAsRegister(node),
               g.UseRegister(m.right().node()),
               g.UseRegister(mleft.left().node()), g.TempImmediate(0));
          return;
        } else if (mleft.right().Is(0xFFFF)) {
          Emit(kArmUxtah, g.DefineAsRegister(node),
               g.UseRegister(m.right().node()),
               g.UseRegister(mleft.left().node()), g.TempImmediate(0));
          return;
        }
        break;
      }
      case IrOpcode::kWord32Sar: {
        Int32BinopMatcher mleft(m.left().node());
        if (CanCover(mleft.node(), mleft.left().node()) &&
            mleft.left().IsWord32Shl()) {
          Int32BinopMatcher mleftleft(mleft.left().node());
          if (mleft.right().Is(24) && mleftleft.right().Is(24)) {
            Emit(kArmSxtab, g.DefineAsRegister(node),
                 g.UseRegister(m.right().node()),
                 g.UseRegister(mleftleft.left().node()), g.TempImmediate(0));
            return;
          } else if (mleft.right().Is(16) && mleftleft.right().Is(16)) {
            Emit(kArmSxtah, g.DefineAsRegister(node),
                 g.UseRegister(m.right().node()),
                 g.UseRegister(mleftleft.left().node()), g.TempImmediate(0));
            return;
          }
        }
        break;
      }
      default:
        break;
    }
  }
  if (CanCover(node, m.right().node())) {
    switch (m.right().opcode()) {
      case IrOpcode::kInt32Mul: {
        Int32BinopMatcher mright(m.right().node());
        Emit(kArmMla, g.DefineAsRegister(node),
             g.UseRegister(mright.left().node()),
             g.UseRegister(mright.right().node()),
             g.UseRegister(m.left().node()));
        return;
      }
      case IrOpcode::kInt32MulHigh: {
        Int32BinopMatcher mright(m.right().node());
        Emit(kArmSmmla, g.DefineAsRegister(node),
             g.UseRegister(mright.left().node()),
             g.UseRegister(mright.right().node()),
             g.UseRegister(m.left().node()));
        return;
      }
      case IrOpcode::kWord32And: {
        Int32BinopMatcher mright(m.right().node());
        if (mright.right().Is(0xFF)) {
          Emit(kArmUxtab, g.DefineAsRegister(node),
               g.UseRegister(m.left().node()),
               g.UseRegister(mright.left().node()), g.TempImmediate(0));
          return;
        } else if (mright.right().Is(0xFFFF)) {
          Emit(kArmUxtah, g.DefineAsRegister(node),
               g.UseRegister(m.left().node()),
               g.UseRegister(mright.left().node()), g.TempImmediate(0));
          return;
        }
        break;
      }
      case IrOpcode::kWord32Sar: {
        Int32BinopMatcher mright(m.right().node());
        if (CanCover(mright.node(), mright.left().node()) &&
            mright.left().IsWord32Shl()) {
          Int32BinopMatcher mrightleft(mright.left().node());
          if (mright.right().Is(24) && mrightleft.right().Is(24)) {
            Emit(kArmSxtab, g.DefineAsRegister(node),
                 g.UseRegister(m.left().node()),
                 g.UseRegister(mrightleft.left().node()), g.TempImmediate(0));
            return;
          } else if (mright.right().Is(16) && mrightleft.right().Is(16)) {
            Emit(kArmSxtah, g.DefineAsRegister(node),
                 g.UseRegister(m.left().node()),
                 g.UseRegister(mrightleft.left().node()), g.TempImmediate(0));
            return;
          }
        }
        break;
      }
      default:
        break;
    }
  }
  VisitBinop(this, node, kArmAdd, kArmAdd);
}


void InstructionSelector::VisitInt32Sub(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (IsSupported(ARMv7) && m.right().IsInt32Mul() &&
      CanCover(node, m.right().node())) {
    Int32BinopMatcher mright(m.right().node());
    Emit(kArmMls, g.DefineAsRegister(node), g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()), g.UseRegister(m.left().node()));
    return;
  }
  VisitBinop(this, node, kArmSub, kArmRsb);
}

namespace {

void EmitInt32MulWithOverflow(InstructionSelector* selector, Node* node,
                              FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand result_operand = g.DefineAsRegister(node);
  InstructionOperand temp_operand = g.TempRegister();
  InstructionOperand outputs[] = {result_operand, temp_operand};
  InstructionOperand inputs[] = {g.UseRegister(m.left().node()),
                                 g.UseRegister(m.right().node())};
  selector->Emit(kArmSmull, 2, outputs, 2, inputs);

  // result operand needs shift operator.
  InstructionOperand shift_31 = g.UseImmediate(31);
  InstructionCode opcode =
      kArmCmp | AddressingModeField::encode(kMode_Operand2_R_ASR_I);
  selector->EmitWithContinuation(opcode, temp_operand, result_operand, shift_31,
                                 cont);
}

}  // namespace

void InstructionSelector::VisitInt32Mul(Node* node) {
  ArmOperandGenerator g(this);
  Int32BinopMatcher m(node);
  if (m.right().HasValue() && m.right().Value() > 0) {
    int32_t value = m.right().Value();
    if (base::bits::IsPowerOfTwo(value - 1)) {
      Emit(kArmAdd | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value - 1)));
      return;
    }
    if (value < kMaxInt && base::bits::IsPowerOfTwo(value + 1)) {
      Emit(kArmRsb | AddressingModeField::encode(kMode_Operand2_R_LSL_I),
           g.DefineAsRegister(node), g.UseRegister(m.left().node()),
           g.UseRegister(m.left().node()),
           g.TempImmediate(WhichPowerOf2(value + 1)));
      return;
    }
  }
  VisitRRR(this, kArmMul, node);
}

void InstructionSelector::VisitUint32MulHigh(Node* node) {
  ArmOperandGenerator g(this);
  InstructionOperand outputs[] = {g.TempRegister(), g.DefineAsRegister(node)};
  InstructionOperand inputs[] = {g.UseRegister(node->InputAt(0)),
                                 g.UseRegister(node->InputAt(1))};
  Emit(kArmUmull, arraysize(outputs), outputs, arraysize(inputs), inputs);
}


void InstructionSelector::VisitInt32Div(Node* node) {
  VisitDiv(this, node, kArmSdiv, kArmVcvtF64S32, kArmVcvtS32F64);
}


void InstructionSelector::VisitUint32Div(Node* node) {
  VisitDiv(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}


void InstructionSelector::VisitInt32Mod(Node* node) {
  VisitMod(this, node, kArmSdiv, kArmVcvtF64S32, kArmVcvtS32F64);
}


void InstructionSelector::VisitUint32Mod(Node* node) {
  VisitMod(this, node, kArmUdiv, kArmVcvtF64U32, kArmVcvtU32F64);
}

#define RR_OP_LIST(V)                                \
  V(Word32Clz, kArmClz)                              \
  V(ChangeFloat32ToFloat64, kArmVcvtF64F32)          \
  V(RoundInt32ToFloat32, kArmVcvtF32S32)             \
  V(RoundUint32ToFloat32, kArmVcvtF32U32)            \
  V(ChangeInt32ToFloat64, kArmVcvtF64S32)            \
  V(ChangeUint32ToFloat64, kArmVcvtF64U32)           \
  V(TruncateFloat32ToInt32, kArmVcvtS32F32)          \
  V(TruncateFloat32ToUint32, kArmVcvtU32F32)         \
  V(ChangeFloat64ToInt32, kArmVcvtS32F64)            \
  V(ChangeFloat64ToUint32, kArmVcvtU32F64)           \
  V(TruncateFloat64ToUint32, kArmVcvtU32F64)         \
  V(TruncateFloat64ToFloat32, kArmVcvtF32F64)        \
  V(TruncateFloat64ToWord32, kArchTruncateDoubleToI) \
  V(RoundFloat64ToInt32, kArmVcvtS32F64)             \
  V(BitcastFloat32ToInt32, kArmVmovU32F32)           \
  V(BitcastInt32ToFloat32, kArmVmovF32U32)           \
  V(Float64ExtractLowWord32, kArmVmovLowU32F64)      \
  V(Float64ExtractHighWord32, kArmVmovHighU32F64)    \
  V(Float64SilenceNaN, kArmFloat64SilenceNaN)        \
  V(Float32Abs, kArmVabsF32)                         \
  V(Float64Abs, kArmVabsF64)                         \
  V(Float32Neg, kArmVnegF32)                         \
  V(Float64Neg, kArmVnegF64)                         \
  V(Float32Sqrt, kArmVsqrtF32)                       \
  V(Float64Sqrt, kArmVsqrtF64)

#define RR_OP_LIST_V8(V)                 \
  V(Float32RoundDown, kArmVrintmF32)     \
  V(Float64RoundDown, kArmVrintmF64)     \
  V(Float32RoundUp, kArmVrintpF32)       \
  V(Float64RoundUp, kArmVrintpF64)       \
  V(Float32RoundTruncate, kArmVrintzF32) \
  V(Float64RoundTruncate, kArmVrintzF64) \
  V(Float64RoundTiesAway, kArmVrintaF64) \
  V(Float32RoundTiesEven, kArmVrintnF32) \
  V(Float64RoundTiesEven, kArmVrintnF64)

#define RRR_OP_LIST(V)          \
  V(Int32MulHigh, kArmSmmul)    \
  V(Float32Mul, kArmVmulF32)    \
  V(Float64Mul, kArmVmulF64)    \
  V(Float32Div, kArmVdivF32)    \
  V(Float64Div, kArmVdivF64)    \
  V(Float32Max, kArmFloat32Max) \
  V(Float64Max, kArmFloat64Max) \
  V(Float32Min, kArmFloat32Min) \
  V(Float64Min, kArmFloat64Min)

#define RR_VISITOR(Name, opcode)                      \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, opcode, node);                      \
  }
RR_OP_LIST(RR_VISITOR)
#undef RR_VISITOR
#undef RR_OP_LIST

#define RR_VISITOR_V8(Name, opcode)                   \
  void InstructionSelector::Visit##Name(Node* node) { \
    DCHECK(CpuFeatures::IsSupported(ARMv8));          \
    VisitRR(this, opcode, node);                      \
  }
RR_OP_LIST_V8(RR_VISITOR_V8)
#undef RR_VISITOR_V8
#undef RR_OP_LIST_V8

#define RRR_VISITOR(Name, opcode)                     \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRR(this, opcode, node);                     \
  }
RRR_OP_LIST(RRR_VISITOR)
#undef RRR_VISITOR
#undef RRR_OP_LIST

void InstructionSelector::VisitFloat32Add(Node* node) {
  ArmOperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.left().IsFloat32Mul() && CanCover(node, m.left().node())) {
    Float32BinopMatcher mleft(m.left().node());
    Emit(kArmVmlaF32, g.DefineSameAsFirst(node),
         g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()));
    return;
  }
  if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
    Float32BinopMatcher mright(m.right().node());
    Emit(kArmVmlaF32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVaddF32, node);
}


void InstructionSelector::VisitFloat64Add(Node* node) {
  ArmOperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.left().IsFloat64Mul() && CanCover(node, m.left().node())) {
    Float64BinopMatcher mleft(m.left().node());
    Emit(kArmVmlaF64, g.DefineSameAsFirst(node),
         g.UseRegister(m.right().node()), g.UseRegister(mleft.left().node()),
         g.UseRegister(mleft.right().node()));
    return;
  }
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    Float64BinopMatcher mright(m.right().node());
    Emit(kArmVmlaF64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVaddF64, node);
}

void InstructionSelector::VisitFloat32Sub(Node* node) {
  ArmOperandGenerator g(this);
  Float32BinopMatcher m(node);
  if (m.right().IsFloat32Mul() && CanCover(node, m.right().node())) {
    Float32BinopMatcher mright(m.right().node());
    Emit(kArmVmlsF32, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVsubF32, node);
}

void InstructionSelector::VisitFloat64Sub(Node* node) {
  ArmOperandGenerator g(this);
  Float64BinopMatcher m(node);
  if (m.right().IsFloat64Mul() && CanCover(node, m.right().node())) {
    Float64BinopMatcher mright(m.right().node());
    Emit(kArmVmlsF64, g.DefineSameAsFirst(node), g.UseRegister(m.left().node()),
         g.UseRegister(mright.left().node()),
         g.UseRegister(mright.right().node()));
    return;
  }
  VisitRRR(this, kArmVsubF64, node);
}

void InstructionSelector::VisitFloat64Mod(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmVmodF64, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Binop(Node* node,
                                                   InstructionCode opcode) {
  ArmOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0),
       g.UseFixed(node->InputAt(1), d1))
      ->MarkAsCall();
}

void InstructionSelector::VisitFloat64Ieee754Unop(Node* node,
                                                  InstructionCode opcode) {
  ArmOperandGenerator g(this);
  Emit(opcode, g.DefineAsFixed(node, d0), g.UseFixed(node->InputAt(0), d0))
      ->MarkAsCall();
}

void InstructionSelector::EmitPrepareArguments(
    ZoneVector<PushParameter>* arguments, const CallDescriptor* call_descriptor,
    Node* node) {
  ArmOperandGenerator g(this);

  // Prepare for C function call.
  if (call_descriptor->IsCFunctionCall()) {
    Emit(kArchPrepareCallCFunction | MiscField::encode(static_cast<int>(
                                         call_descriptor->ParameterCount())),
         0, nullptr, 0, nullptr);

    // Poke any stack arguments.
    for (size_t n = 0; n < arguments->size(); ++n) {
      PushParameter input = (*arguments)[n];
      if (input.node) {
        int slot = static_cast<int>(n);
        Emit(kArmPoke | MiscField::encode(slot), g.NoOutput(),
             g.UseRegister(input.node));
      }
    }
  } else {
    // Push any stack arguments.
    for (PushParameter input : base::Reversed(*arguments)) {
      // Skip any alignment holes in pushed nodes.
      if (input.node == nullptr) continue;
      Emit(kArmPush, g.NoOutput(), g.UseRegister(input.node));
    }
  }
}

void InstructionSelector::EmitPrepareResults(
    ZoneVector<PushParameter>* results, const CallDescriptor* call_descriptor,
    Node* node) {
  ArmOperandGenerator g(this);

  int reverse_slot = 0;
  for (PushParameter output : *results) {
    if (!output.location.IsCallerFrameSlot()) continue;
    // Skip any alignment holes in nodes.
    if (output.node != nullptr) {
      DCHECK(!call_descriptor->IsCFunctionCall());
      if (output.location.GetType() == MachineType::Float32()) {
        MarkAsFloat32(output.node);
      } else if (output.location.GetType() == MachineType::Float64()) {
        MarkAsFloat64(output.node);
      }
      Emit(kArmPeek, g.DefineAsRegister(output.node),
           g.UseImmediate(reverse_slot));
    }
    reverse_slot += output.location.GetSizeInPointers();
  }
}

bool InstructionSelector::IsTailCallAddressImmediate() { return false; }

int InstructionSelector::GetTempsCountForTailCallFromJSFunction() { return 3; }

namespace {

// Shared routine for multiple compare operations.
void VisitCompare(InstructionSelector* selector, InstructionCode opcode,
                  InstructionOperand left, InstructionOperand right,
                  FlagsContinuation* cont) {
  selector->EmitWithContinuation(opcode, left, right, cont);
}


// Shared routine for multiple float32 compare operations.
void VisitFloat32Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Float32BinopMatcher m(node);
  if (m.right().Is(0.0f)) {
    VisitCompare(selector, kArmVcmpF32, g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()), cont);
  } else if (m.left().Is(0.0f)) {
    cont->Commute();
    VisitCompare(selector, kArmVcmpF32, g.UseRegister(m.right().node()),
                 g.UseImmediate(m.left().node()), cont);
  } else {
    VisitCompare(selector, kArmVcmpF32, g.UseRegister(m.left().node()),
                 g.UseRegister(m.right().node()), cont);
  }
}


// Shared routine for multiple float64 compare operations.
void VisitFloat64Compare(InstructionSelector* selector, Node* node,
                         FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Float64BinopMatcher m(node);
  if (m.right().Is(0.0)) {
    VisitCompare(selector, kArmVcmpF64, g.UseRegister(m.left().node()),
                 g.UseImmediate(m.right().node()), cont);
  } else if (m.left().Is(0.0)) {
    cont->Commute();
    VisitCompare(selector, kArmVcmpF64, g.UseRegister(m.right().node()),
                 g.UseImmediate(m.left().node()), cont);
  } else {
    VisitCompare(selector, kArmVcmpF64, g.UseRegister(m.left().node()),
                 g.UseRegister(m.right().node()), cont);
  }
}

// Check whether we can convert:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>.
// We only generate conditions <cond'> that are a combination of the N
// and Z flags. This avoids the need to make this function dependent on
// the flag-setting operation.
bool CanUseFlagSettingBinop(FlagsCondition cond) {
  switch (cond) {
    case kEqual:
    case kNotEqual:
    case kSignedLessThan:
    case kSignedGreaterThanOrEqual:
    case kUnsignedLessThanOrEqual:  // x <= 0 -> x == 0
    case kUnsignedGreaterThan:      // x > 0 -> x != 0
      return true;
    default:
      return false;
  }
}

// Map <cond> to <cond'> so that the following transformation is possible:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>.
FlagsCondition MapForFlagSettingBinop(FlagsCondition cond) {
  DCHECK(CanUseFlagSettingBinop(cond));
  switch (cond) {
    case kEqual:
    case kNotEqual:
      return cond;
    case kSignedLessThan:
      return kNegative;
    case kSignedGreaterThanOrEqual:
      return kPositiveOrZero;
    case kUnsignedLessThanOrEqual:  // x <= 0 -> x == 0
      return kEqual;
    case kUnsignedGreaterThan:  // x > 0 -> x != 0
      return kNotEqual;
    default:
      UNREACHABLE();
  }
}

// Check if we can perform the transformation:
// ((a <op> b) cmp 0), b.<cond>
// to:
// (a <ops> b), b.<cond'>
// where <ops> is the flag setting version of <op>, and if so,
// updates {node}, {opcode} and {cont} accordingly.
void MaybeReplaceCmpZeroWithFlagSettingBinop(InstructionSelector* selector,
                                             Node** node, Node* binop,
                                             InstructionCode* opcode,
                                             FlagsCondition cond,
                                             FlagsContinuation* cont) {
  InstructionCode binop_opcode;
  InstructionCode no_output_opcode;
  switch (binop->opcode()) {
    case IrOpcode::kInt32Add:
      binop_opcode = kArmAdd;
      no_output_opcode = kArmCmn;
      break;
    case IrOpcode::kWord32And:
      binop_opcode = kArmAnd;
      no_output_opcode = kArmTst;
      break;
    case IrOpcode::kWord32Or:
      binop_opcode = kArmOrr;
      no_output_opcode = kArmOrr;
      break;
    case IrOpcode::kWord32Xor:
      binop_opcode = kArmEor;
      no_output_opcode = kArmTeq;
      break;
    default:
      UNREACHABLE();
      return;
  }
  if (selector->CanCover(*node, binop)) {
    // The comparison is the only user of {node}.
    cont->Overwrite(MapForFlagSettingBinop(cond));
    *opcode = no_output_opcode;
    *node = binop;
  } else if (selector->IsOnlyUserOfNodeInSameBlock(*node, binop)) {
    // We can also handle the case where the {node} and the comparison are in
    // the same basic block, and the comparison is the only user of {node} in
    // this basic block ({node} has users in other basic blocks).
    cont->Overwrite(MapForFlagSettingBinop(cond));
    *opcode = binop_opcode;
    *node = binop;
  }
}

// Shared routine for multiple word compare operations.
void VisitWordCompare(InstructionSelector* selector, Node* node,
                      InstructionCode opcode, FlagsContinuation* cont) {
  ArmOperandGenerator g(selector);
  Int32BinopMatcher m(node);
  InstructionOperand inputs[3];
  size_t input_count = 0;
  InstructionOperand outputs[2];
  size_t output_count = 0;
  bool has_result = (opcode != kArmCmp) && (opcode != kArmCmn) &&
                    (opcode != kArmTst) && (opcode != kArmTeq);

  if (TryMatchImmediateOrShift(selector, &opcode, m.right().node(),
                               &input_count, &inputs[1])) {
    inputs[0] = g.UseRegisterOrStackPointer(m.left().node());
    input_count++;
  } else if (TryMatchImmediateOrShift(selector, &opcode, m.left().node(),
                                      &input_count, &inputs[1])) {
    if (!node->op()->HasProperty(Operator::kCommutative)) cont->Commute();
    inputs[0] = g.UseRegisterOrStackPointer(m.right().node());
    input_count++;
  } else {
    opcode |= AddressingModeField::encode(kMode_Operand2_R);
    inputs[input_count++] = g.UseRegisterOrStackPointer(m.left().node());
    inputs[input_count++] = g.UseRegisterOrStackPointer(m.right().node());
  }

  if (has_result) {
    if (cont->IsDeoptimize()) {
      // If we can deoptimize as a result of the binop, we need to make sure
      // that the deopt inputs are not overwritten by the binop result. One way
      // to achieve that is to declare the output register as same-as-first.
      outputs[output_count++] = g.DefineSameAsFirst(node);
    } else {
      outputs[output_count++] = g.DefineAsRegister(node);
    }
  }

  DCHECK_NE(0u, input_count);
  DCHECK_GE(arraysize(inputs), input_count);
  DCHECK_GE(arraysize(outputs), output_count);

  selector->EmitWithContinuation(opcode, output_count, outputs, input_count,
                                 inputs, cont);
}


void VisitWordCompare(InstructionSelector* selector, Node* node,
                      FlagsContinuation* cont) {
  InstructionCode opcode = kArmCmp;
  Int32BinopMatcher m(node);

  FlagsCondition cond = cont->condition();
  if (m.right().Is(0) && (m.left().IsInt32Add() || m.left().IsWord32Or() ||
                          m.left().IsWord32And() || m.left().IsWord32Xor())) {
    // Emit flag setting instructions for comparisons against zero.
    if (CanUseFlagSettingBinop(cond)) {
      Node* binop = m.left().node();
      MaybeReplaceCmpZeroWithFlagSettingBinop(selector, &node, binop, &opcode,
                                              cond, cont);
    }
  } else if (m.left().Is(0) &&
             (m.right().IsInt32Add() || m.right().IsWord32Or() ||
              m.right().IsWord32And() || m.right().IsWord32Xor())) {
    // Same as above, but we need to commute the condition before we
    // continue with the rest of the checks.
    cond = CommuteFlagsCondition(cond);
    if (CanUseFlagSettingBinop(cond)) {
      Node* binop = m.right().node();
      MaybeReplaceCmpZeroWithFlagSettingBinop(selector, &node, binop, &opcode,
                                              cond, cont);
    }
  }

  VisitWordCompare(selector, node, opcode, cont);
}

}  // namespace

// Shared routine for word comparisons against zero.
void InstructionSelector::VisitWordCompareZero(Node* user, Node* value,
                                               FlagsContinuation* cont) {
  // Try to combine with comparisons against 0 by simply inverting the branch.
  while (value->opcode() == IrOpcode::kWord32Equal && CanCover(user, value)) {
    Int32BinopMatcher m(value);
    if (!m.right().Is(0)) break;

    user = value;
    value = m.left().node();
    cont->Negate();
  }

  if (CanCover(user, value)) {
    switch (value->opcode()) {
      case IrOpcode::kWord32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kInt32LessThan:
        cont->OverwriteAndNegateIfEqual(kSignedLessThan);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kInt32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kSignedLessThanOrEqual);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kUint32LessThan:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThan);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kUint32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kUnsignedLessThanOrEqual);
        return VisitWordCompare(this, value, cont);
      case IrOpcode::kFloat32Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThan:
        cont->OverwriteAndNegateIfEqual(kFloatLessThan);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat32LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
        return VisitFloat32Compare(this, value, cont);
      case IrOpcode::kFloat64Equal:
        cont->OverwriteAndNegateIfEqual(kEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThan:
        cont->OverwriteAndNegateIfEqual(kFloatLessThan);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kFloat64LessThanOrEqual:
        cont->OverwriteAndNegateIfEqual(kFloatLessThanOrEqual);
        return VisitFloat64Compare(this, value, cont);
      case IrOpcode::kProjection:
        // Check if this is the overflow output projection of an
        // <Operation>WithOverflow node.
        if (ProjectionIndexOf(value->op()) == 1u) {
          // We cannot combine the <Operation>WithOverflow with this branch
          // unless the 0th projection (the use of the actual value of the
          // <Operation> is either nullptr, which means there's no use of the
          // actual value, or was already defined, which means it is scheduled
          // *AFTER* this branch).
          Node* const node = value->InputAt(0);
          Node* const result = NodeProperties::FindProjection(node, 0);
          if (!result || IsDefined(result)) {
            switch (node->opcode()) {
              case IrOpcode::kInt32AddWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kArmAdd, kArmAdd, cont);
              case IrOpcode::kInt32SubWithOverflow:
                cont->OverwriteAndNegateIfEqual(kOverflow);
                return VisitBinop(this, node, kArmSub, kArmRsb, cont);
              case IrOpcode::kInt32MulWithOverflow:
                // ARM doesn't set the overflow flag for multiplication, so we
                // need to test on kNotEqual. Here is the code sequence used:
                //   smull resultlow, resulthigh, left, right
                //   cmp resulthigh, Operand(resultlow, ASR, 31)
                cont->OverwriteAndNegateIfEqual(kNotEqual);
                return EmitInt32MulWithOverflow(this, node, cont);
              default:
                break;
            }
          }
        }
        break;
      case IrOpcode::kInt32Add:
        return VisitWordCompare(this, value, kArmCmn, cont);
      case IrOpcode::kInt32Sub:
        return VisitWordCompare(this, value, kArmCmp, cont);
      case IrOpcode::kWord32And:
        return VisitWordCompare(this, value, kArmTst, cont);
      case IrOpcode::kWord32Or:
        return VisitBinop(this, value, kArmOrr, kArmOrr, cont);
      case IrOpcode::kWord32Xor:
        return VisitWordCompare(this, value, kArmTeq, cont);
      case IrOpcode::kWord32Sar:
        return VisitShift(this, value, TryMatchASR, cont);
      case IrOpcode::kWord32Shl:
        return VisitShift(this, value, TryMatchLSL, cont);
      case IrOpcode::kWord32Shr:
        return VisitShift(this, value, TryMatchLSR, cont);
      case IrOpcode::kWord32Ror:
        return VisitShift(this, value, TryMatchROR, cont);
      default:
        break;
    }
  }

  if (user->opcode() == IrOpcode::kWord32Equal) {
    return VisitWordCompare(this, user, cont);
  }

  // Continuation could not be combined with a compare, emit compare against 0.
  ArmOperandGenerator g(this);
  InstructionCode const opcode =
      kArmTst | AddressingModeField::encode(kMode_Operand2_R);
  InstructionOperand const value_operand = g.UseRegister(value);
  EmitWithContinuation(opcode, value_operand, value_operand, cont);
}

void InstructionSelector::VisitSwitch(Node* node, const SwitchInfo& sw) {
  ArmOperandGenerator g(this);
  InstructionOperand value_operand = g.UseRegister(node->InputAt(0));

  // Emit either ArchTableSwitch or ArchLookupSwitch.
  if (enable_switch_jump_table_ == kEnableSwitchJumpTable) {
    static const size_t kMaxTableSwitchValueRange = 2 << 16;
    size_t table_space_cost = 4 + sw.value_range();
    size_t table_time_cost = 3;
    size_t lookup_space_cost = 3 + 2 * sw.case_count();
    size_t lookup_time_cost = sw.case_count();
    if (sw.case_count() > 0 &&
        table_space_cost + 3 * table_time_cost <=
            lookup_space_cost + 3 * lookup_time_cost &&
        sw.min_value() > std::numeric_limits<int32_t>::min() &&
        sw.value_range() <= kMaxTableSwitchValueRange) {
      InstructionOperand index_operand = value_operand;
      if (sw.min_value()) {
        index_operand = g.TempRegister();
        Emit(kArmSub | AddressingModeField::encode(kMode_Operand2_I),
             index_operand, value_operand, g.TempImmediate(sw.min_value()));
      }
      // Generate a table lookup.
      return EmitTableSwitch(sw, index_operand);
    }
  }

  // Generate a tree of conditional jumps.
  return EmitBinarySearchSwitch(sw, value_operand);
}

void InstructionSelector::VisitWord32Equal(Node* const node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  Int32BinopMatcher m(node);
  if (m.right().Is(0)) {
    return VisitWordCompareZero(m.node(), m.left().node(), &cont);
  }
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kSignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kSignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kUnsignedLessThan, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitUint32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kUnsignedLessThanOrEqual, node);
  VisitWordCompare(this, node, &cont);
}


void InstructionSelector::VisitInt32AddWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmAdd, kArmAdd, &cont);
}

void InstructionSelector::VisitInt32SubWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    FlagsContinuation cont = FlagsContinuation::ForSet(kOverflow, ovf);
    return VisitBinop(this, node, kArmSub, kArmRsb, &cont);
  }
  FlagsContinuation cont;
  VisitBinop(this, node, kArmSub, kArmRsb, &cont);
}

void InstructionSelector::VisitInt32MulWithOverflow(Node* node) {
  if (Node* ovf = NodeProperties::FindProjection(node, 1)) {
    // ARM doesn't set the overflow flag for multiplication, so we need to test
    // on kNotEqual. Here is the code sequence used:
    //   smull resultlow, resulthigh, left, right
    //   cmp resulthigh, Operand(resultlow, ASR, 31)
    FlagsContinuation cont = FlagsContinuation::ForSet(kNotEqual, ovf);
    return EmitInt32MulWithOverflow(this, node, &cont);
  }
  FlagsContinuation cont;
  EmitInt32MulWithOverflow(this, node, &cont);
}

void InstructionSelector::VisitFloat32Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat32LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat32Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64Equal(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kEqual, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThan(Node* node) {
  FlagsContinuation cont = FlagsContinuation::ForSet(kFloatLessThan, node);
  VisitFloat64Compare(this, node, &cont);
}


void InstructionSelector::VisitFloat64LessThanOrEqual(Node* node) {
  FlagsContinuation cont =
      FlagsContinuation::ForSet(kFloatLessThanOrEqual, node);
  VisitFloat64Compare(this, node, &cont);
}

void InstructionSelector::VisitFloat64InsertLowWord32(Node* node) {
  ArmOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertHighWord32 &&
      CanCover(node, left)) {
    left = left->InputAt(1);
    Emit(kArmVmovF64U32U32, g.DefineAsRegister(node), g.UseRegister(right),
         g.UseRegister(left));
    return;
  }
  Emit(kArmVmovLowF64U32, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}


void InstructionSelector::VisitFloat64InsertHighWord32(Node* node) {
  ArmOperandGenerator g(this);
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (left->opcode() == IrOpcode::kFloat64InsertLowWord32 &&
      CanCover(node, left)) {
    left = left->InputAt(1);
    Emit(kArmVmovF64U32U32, g.DefineAsRegister(node), g.UseRegister(left),
         g.UseRegister(right));
    return;
  }
  Emit(kArmVmovHighF64U32, g.DefineSameAsFirst(node), g.UseRegister(left),
       g.UseRegister(right));
}

void InstructionSelector::VisitWord32AtomicLoad(Node* node) {
  LoadRepresentation load_rep = LoadRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  ArchOpcode opcode = kArchNop;
  switch (load_rep.representation()) {
    case MachineRepresentation::kWord8:
      opcode =
          load_rep.IsSigned() ? kWord32AtomicLoadInt8 : kWord32AtomicLoadUint8;
      break;
    case MachineRepresentation::kWord16:
      opcode = load_rep.IsSigned() ? kWord32AtomicLoadInt16
                                   : kWord32AtomicLoadUint16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kWord32AtomicLoadWord32;
      break;
    default:
      UNREACHABLE();
      return;
  }
  Emit(opcode | AddressingModeField::encode(kMode_Offset_RR),
       g.DefineAsRegister(node), g.UseRegister(base), g.UseRegister(index));
}

void InstructionSelector::VisitWord32AtomicStore(Node* node) {
  MachineRepresentation rep = AtomicStoreRepresentationOf(node->op());
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
  switch (rep) {
    case MachineRepresentation::kWord8:
      opcode = kWord32AtomicStoreWord8;
      break;
    case MachineRepresentation::kWord16:
      opcode = kWord32AtomicStoreWord16;
      break;
    case MachineRepresentation::kWord32:
      opcode = kWord32AtomicStoreWord32;
      break;
    default:
      UNREACHABLE();
      return;
  }

  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseUniqueRegister(base);
  inputs[input_count++] = g.UseUniqueRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, nullptr, input_count, inputs);
}

void InstructionSelector::VisitWord32AtomicExchange(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kWord32AtomicExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kWord32AtomicExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kWord32AtomicExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }

  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseRegister(base);
  inputs[input_count++] = g.UseRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, arraysize(temps), temps);
}

void InstructionSelector::VisitWord32AtomicCompareExchange(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* old_value = node->InputAt(2);
  Node* new_value = node->InputAt(3);
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = kWord32AtomicCompareExchangeInt8;
  } else if (type == MachineType::Uint8()) {
    opcode = kWord32AtomicCompareExchangeUint8;
  } else if (type == MachineType::Int16()) {
    opcode = kWord32AtomicCompareExchangeInt16;
  } else if (type == MachineType::Uint16()) {
    opcode = kWord32AtomicCompareExchangeUint16;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = kWord32AtomicCompareExchangeWord32;
  } else {
    UNREACHABLE();
    return;
  }

  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[4];
  size_t input_count = 0;
  inputs[input_count++] = g.UseRegister(base);
  inputs[input_count++] = g.UseRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(old_value);
  inputs[input_count++] = g.UseUniqueRegister(new_value);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, arraysize(temps), temps);
}

void InstructionSelector::VisitWord32AtomicBinaryOperation(
    Node* node, ArchOpcode int8_op, ArchOpcode uint8_op, ArchOpcode int16_op,
    ArchOpcode uint16_op, ArchOpcode word32_op) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  ArchOpcode opcode = kArchNop;
  MachineType type = AtomicOpType(node->op());
  if (type == MachineType::Int8()) {
    opcode = int8_op;
  } else if (type == MachineType::Uint8()) {
    opcode = uint8_op;
  } else if (type == MachineType::Int16()) {
    opcode = int16_op;
  } else if (type == MachineType::Uint16()) {
    opcode = uint16_op;
  } else if (type == MachineType::Int32() || type == MachineType::Uint32()) {
    opcode = word32_op;
  } else {
    UNREACHABLE();
    return;
  }

  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[3];
  size_t input_count = 0;
  inputs[input_count++] = g.UseRegister(base);
  inputs[input_count++] = g.UseRegister(index);
  inputs[input_count++] = g.UseUniqueRegister(value);
  InstructionOperand outputs[1];
  outputs[0] = g.DefineAsRegister(node);
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                g.TempRegister()};
  InstructionCode code = opcode | AddressingModeField::encode(addressing_mode);
  Emit(code, 1, outputs, input_count, inputs, arraysize(temps), temps);
}

#define VISIT_ATOMIC_BINOP(op)                                   \
  void InstructionSelector::VisitWord32Atomic##op(Node* node) {  \
    VisitWord32AtomicBinaryOperation(                            \
        node, kWord32Atomic##op##Int8, kWord32Atomic##op##Uint8, \
        kWord32Atomic##op##Int16, kWord32Atomic##op##Uint16,     \
        kWord32Atomic##op##Word32);                              \
  }
VISIT_ATOMIC_BINOP(Add)
VISIT_ATOMIC_BINOP(Sub)
VISIT_ATOMIC_BINOP(And)
VISIT_ATOMIC_BINOP(Or)
VISIT_ATOMIC_BINOP(Xor)
#undef VISIT_ATOMIC_BINOP

void InstructionSelector::VisitWord32AtomicPairLoad(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionCode code =
      kArmWord32AtomicPairLoad | AddressingModeField::encode(addressing_mode);
  InstructionOperand inputs[] = {g.UseUniqueRegister(base),
                                 g.UseUniqueRegister(index)};
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r0),
                                    g.DefineAsFixed(projection1, r1)};
    InstructionOperand temps[] = {g.TempRegister()};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else if (projection0) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r0)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(r1)};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else {
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(r0),
                                  g.TempRegister(r1)};
    Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
  }
}

void InstructionSelector::VisitWord32AtomicPairStore(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value_low = node->InputAt(2);
  Node* value_high = node->InputAt(3);
  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[] = {
      g.UseUniqueRegister(base), g.UseUniqueRegister(index),
      g.UseFixed(value_low, r2), g.UseFixed(value_high, r3)};
  InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(r0),
                                g.TempRegister(r1)};
  InstructionCode code =
      kArmWord32AtomicPairStore | AddressingModeField::encode(addressing_mode);
  Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
}

void InstructionSelector::VisitWord32AtomicPairAdd(Node* node) {
  VisitPairAtomicBinOp(this, node, kArmWord32AtomicPairAdd);
}

void InstructionSelector::VisitWord32AtomicPairSub(Node* node) {
  VisitPairAtomicBinOp(this, node, kArmWord32AtomicPairSub);
}

void InstructionSelector::VisitWord32AtomicPairAnd(Node* node) {
  VisitPairAtomicBinOp(this, node, kArmWord32AtomicPairAnd);
}

void InstructionSelector::VisitWord32AtomicPairOr(Node* node) {
  VisitPairAtomicBinOp(this, node, kArmWord32AtomicPairOr);
}

void InstructionSelector::VisitWord32AtomicPairXor(Node* node) {
  VisitPairAtomicBinOp(this, node, kArmWord32AtomicPairXor);
}

void InstructionSelector::VisitWord32AtomicPairExchange(Node* node) {
  ArmOperandGenerator g(this);
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  Node* value_high = node->InputAt(3);
  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[] = {
      g.UseFixed(value, r0), g.UseFixed(value_high, r1),
      g.UseUniqueRegister(base), g.UseUniqueRegister(index)};
  InstructionCode code = kArmWord32AtomicPairExchange |
                         AddressingModeField::encode(addressing_mode);
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r6),
                                    g.DefineAsFixed(projection1, r7)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else if (projection0) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r6)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                  g.TempRegister(r7)};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else {
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                  g.TempRegister(r6), g.TempRegister(r7)};
    Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
  }
}

void InstructionSelector::VisitWord32AtomicPairCompareExchange(Node* node) {
  ArmOperandGenerator g(this);
  AddressingMode addressing_mode = kMode_Offset_RR;
  InstructionOperand inputs[] = {g.UseFixed(node->InputAt(2), r4),
                                 g.UseFixed(node->InputAt(3), r5),
                                 g.UseFixed(node->InputAt(4), r8),
                                 g.UseFixed(node->InputAt(5), r9),
                                 g.UseUniqueRegister(node->InputAt(0)),
                                 g.UseUniqueRegister(node->InputAt(1))};
  InstructionCode code = kArmWord32AtomicPairCompareExchange |
                         AddressingModeField::encode(addressing_mode);
  Node* projection0 = NodeProperties::FindProjection(node, 0);
  Node* projection1 = NodeProperties::FindProjection(node, 1);
  if (projection1) {
    InstructionOperand outputs[] = {g.DefineAsFixed(projection0, r2),
                                    g.DefineAsFixed(projection1, r3)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister()};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else if (projection0) {
    InstructionOperand outputs[] = {
        g.DefineAsFixed(NodeProperties::FindProjection(node, 0), r2)};
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                  g.TempRegister(r3)};
    Emit(code, arraysize(outputs), outputs, arraysize(inputs), inputs,
         arraysize(temps), temps);
  } else {
    InstructionOperand temps[] = {g.TempRegister(), g.TempRegister(),
                                  g.TempRegister(r2), g.TempRegister(r3)};
    Emit(code, 0, nullptr, arraysize(inputs), inputs, arraysize(temps), temps);
  }
}

#define SIMD_TYPE_LIST(V) \
  V(F32x4)                \
  V(I32x4)                \
  V(I16x8)                \
  V(I8x16)

#define SIMD_UNOP_LIST(V)                               \
  V(F32x4SConvertI32x4, kArmF32x4SConvertI32x4)         \
  V(F32x4UConvertI32x4, kArmF32x4UConvertI32x4)         \
  V(F32x4Abs, kArmF32x4Abs)                             \
  V(F32x4Neg, kArmF32x4Neg)                             \
  V(F32x4RecipApprox, kArmF32x4RecipApprox)             \
  V(F32x4RecipSqrtApprox, kArmF32x4RecipSqrtApprox)     \
  V(I32x4SConvertF32x4, kArmI32x4SConvertF32x4)         \
  V(I32x4SConvertI16x8Low, kArmI32x4SConvertI16x8Low)   \
  V(I32x4SConvertI16x8High, kArmI32x4SConvertI16x8High) \
  V(I32x4Neg, kArmI32x4Neg)                             \
  V(I32x4UConvertF32x4, kArmI32x4UConvertF32x4)         \
  V(I32x4UConvertI16x8Low, kArmI32x4UConvertI16x8Low)   \
  V(I32x4UConvertI16x8High, kArmI32x4UConvertI16x8High) \
  V(I16x8SConvertI8x16Low, kArmI16x8SConvertI8x16Low)   \
  V(I16x8SConvertI8x16High, kArmI16x8SConvertI8x16High) \
  V(I16x8Neg, kArmI16x8Neg)                             \
  V(I16x8UConvertI8x16Low, kArmI16x8UConvertI8x16Low)   \
  V(I16x8UConvertI8x16High, kArmI16x8UConvertI8x16High) \
  V(I8x16Neg, kArmI8x16Neg)                             \
  V(S128Not, kArmS128Not)                               \
  V(S1x4AnyTrue, kArmS1x4AnyTrue)                       \
  V(S1x4AllTrue, kArmS1x4AllTrue)                       \
  V(S1x8AnyTrue, kArmS1x8AnyTrue)                       \
  V(S1x8AllTrue, kArmS1x8AllTrue)                       \
  V(S1x16AnyTrue, kArmS1x16AnyTrue)                     \
  V(S1x16AllTrue, kArmS1x16AllTrue)

#define SIMD_SHIFT_OP_LIST(V) \
  V(I32x4Shl)                 \
  V(I32x4ShrS)                \
  V(I32x4ShrU)                \
  V(I16x8Shl)                 \
  V(I16x8ShrS)                \
  V(I16x8ShrU)                \
  V(I8x16Shl)                 \
  V(I8x16ShrS)                \
  V(I8x16ShrU)

#define SIMD_BINOP_LIST(V)                      \
  V(F32x4Add, kArmF32x4Add)                     \
  V(F32x4AddHoriz, kArmF32x4AddHoriz)           \
  V(F32x4Sub, kArmF32x4Sub)                     \
  V(F32x4Mul, kArmF32x4Mul)                     \
  V(F32x4Min, kArmF32x4Min)                     \
  V(F32x4Max, kArmF32x4Max)                     \
  V(F32x4Eq, kArmF32x4Eq)                       \
  V(F32x4Ne, kArmF32x4Ne)                       \
  V(F32x4Lt, kArmF32x4Lt)                       \
  V(F32x4Le, kArmF32x4Le)                       \
  V(I32x4Add, kArmI32x4Add)                     \
  V(I32x4AddHoriz, kArmI32x4AddHoriz)           \
  V(I32x4Sub, kArmI32x4Sub)                     \
  V(I32x4Mul, kArmI32x4Mul)                     \
  V(I32x4MinS, kArmI32x4MinS)                   \
  V(I32x4MaxS, kArmI32x4MaxS)                   \
  V(I32x4Eq, kArmI32x4Eq)                       \
  V(I32x4Ne, kArmI32x4Ne)                       \
  V(I32x4GtS, kArmI32x4GtS)                     \
  V(I32x4GeS, kArmI32x4GeS)                     \
  V(I32x4MinU, kArmI32x4MinU)                   \
  V(I32x4MaxU, kArmI32x4MaxU)                   \
  V(I32x4GtU, kArmI32x4GtU)                     \
  V(I32x4GeU, kArmI32x4GeU)                     \
  V(I16x8SConvertI32x4, kArmI16x8SConvertI32x4) \
  V(I16x8Add, kArmI16x8Add)                     \
  V(I16x8AddSaturateS, kArmI16x8AddSaturateS)   \
  V(I16x8AddHoriz, kArmI16x8AddHoriz)           \
  V(I16x8Sub, kArmI16x8Sub)                     \
  V(I16x8SubSaturateS, kArmI16x8SubSaturateS)   \
  V(I16x8Mul, kArmI16x8Mul)                     \
  V(I16x8MinS, kArmI16x8MinS)                   \
  V(I16x8MaxS, kArmI16x8MaxS)                   \
  V(I16x8Eq, kArmI16x8Eq)                       \
  V(I16x8Ne, kArmI16x8Ne)                       \
  V(I16x8GtS, kArmI16x8GtS)                     \
  V(I16x8GeS, kArmI16x8GeS)                     \
  V(I16x8UConvertI32x4, kArmI16x8UConvertI32x4) \
  V(I16x8AddSaturateU, kArmI16x8AddSaturateU)   \
  V(I16x8SubSaturateU, kArmI16x8SubSaturateU)   \
  V(I16x8MinU, kArmI16x8MinU)                   \
  V(I16x8MaxU, kArmI16x8MaxU)                   \
  V(I16x8GtU, kArmI16x8GtU)                     \
  V(I16x8GeU, kArmI16x8GeU)                     \
  V(I8x16SConvertI16x8, kArmI8x16SConvertI16x8) \
  V(I8x16Add, kArmI8x16Add)                     \
  V(I8x16AddSaturateS, kArmI8x16AddSaturateS)   \
  V(I8x16Sub, kArmI8x16Sub)                     \
  V(I8x16SubSaturateS, kArmI8x16SubSaturateS)   \
  V(I8x16Mul, kArmI8x16Mul)                     \
  V(I8x16MinS, kArmI8x16MinS)                   \
  V(I8x16MaxS, kArmI8x16MaxS)                   \
  V(I8x16Eq, kArmI8x16Eq)                       \
  V(I8x16Ne, kArmI8x16Ne)                       \
  V(I8x16GtS, kArmI8x16GtS)                     \
  V(I8x16GeS, kArmI8x16GeS)                     \
  V(I8x16UConvertI16x8, kArmI8x16UConvertI16x8) \
  V(I8x16AddSaturateU, kArmI8x16AddSaturateU)   \
  V(I8x16SubSaturateU, kArmI8x16SubSaturateU)   \
  V(I8x16MinU, kArmI8x16MinU)                   \
  V(I8x16MaxU, kArmI8x16MaxU)                   \
  V(I8x16GtU, kArmI8x16GtU)                     \
  V(I8x16GeU, kArmI8x16GeU)                     \
  V(S128And, kArmS128And)                       \
  V(S128Or, kArmS128Or)                         \
  V(S128Xor, kArmS128Xor)

void InstructionSelector::VisitS128Zero(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmS128Zero, g.DefineAsRegister(node), g.DefineAsRegister(node));
}

#define SIMD_VISIT_SPLAT(Type)                               \
  void InstructionSelector::Visit##Type##Splat(Node* node) { \
    VisitRR(this, kArm##Type##Splat, node);                  \
  }
SIMD_TYPE_LIST(SIMD_VISIT_SPLAT)
#undef SIMD_VISIT_SPLAT

#define SIMD_VISIT_EXTRACT_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ExtractLane(Node* node) { \
    VisitRRI(this, kArm##Type##ExtractLane, node);                 \
  }
SIMD_TYPE_LIST(SIMD_VISIT_EXTRACT_LANE)
#undef SIMD_VISIT_EXTRACT_LANE

#define SIMD_VISIT_REPLACE_LANE(Type)                              \
  void InstructionSelector::Visit##Type##ReplaceLane(Node* node) { \
    VisitRRIR(this, kArm##Type##ReplaceLane, node);                \
  }
SIMD_TYPE_LIST(SIMD_VISIT_REPLACE_LANE)
#undef SIMD_VISIT_REPLACE_LANE
#undef SIMD_TYPE_LIST

#define SIMD_VISIT_UNOP(Name, instruction)            \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRR(this, instruction, node);                 \
  }
SIMD_UNOP_LIST(SIMD_VISIT_UNOP)
#undef SIMD_VISIT_UNOP
#undef SIMD_UNOP_LIST

#define SIMD_VISIT_SHIFT_OP(Name)                     \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRI(this, kArm##Name, node);                 \
  }
SIMD_SHIFT_OP_LIST(SIMD_VISIT_SHIFT_OP)
#undef SIMD_VISIT_SHIFT_OP
#undef SIMD_SHIFT_OP_LIST

#define SIMD_VISIT_BINOP(Name, instruction)           \
  void InstructionSelector::Visit##Name(Node* node) { \
    VisitRRR(this, instruction, node);                \
  }
SIMD_BINOP_LIST(SIMD_VISIT_BINOP)
#undef SIMD_VISIT_BINOP
#undef SIMD_BINOP_LIST

void InstructionSelector::VisitS128Select(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmS128Select, g.DefineSameAsFirst(node),
       g.UseRegister(node->InputAt(0)), g.UseRegister(node->InputAt(1)),
       g.UseRegister(node->InputAt(2)));
}

namespace {

struct ShuffleEntry {
  uint8_t shuffle[kSimd128Size];
  ArchOpcode opcode;
};

static const ShuffleEntry arch_shuffles[] = {
    {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23},
     kArmS32x4ZipLeft},
    {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31},
     kArmS32x4ZipRight},
    {{0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27},
     kArmS32x4UnzipLeft},
    {{4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31},
     kArmS32x4UnzipRight},
    {{0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27},
     kArmS32x4TransposeLeft},
    {{4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 28, 29, 30, 31},
     kArmS32x4TransposeRight},
    {{4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11}, kArmS32x2Reverse},

    {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23},
     kArmS16x8ZipLeft},
    {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31},
     kArmS16x8ZipRight},
    {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29},
     kArmS16x8UnzipLeft},
    {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31},
     kArmS16x8UnzipRight},
    {{0, 1, 16, 17, 4, 5, 20, 21, 8, 9, 24, 25, 12, 13, 28, 29},
     kArmS16x8TransposeLeft},
    {{2, 3, 18, 19, 6, 7, 22, 23, 10, 11, 26, 27, 14, 15, 30, 31},
     kArmS16x8TransposeRight},
    {{6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9}, kArmS16x4Reverse},
    {{2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13}, kArmS16x2Reverse},

    {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23},
     kArmS8x16ZipLeft},
    {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31},
     kArmS8x16ZipRight},
    {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30},
     kArmS8x16UnzipLeft},
    {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31},
     kArmS8x16UnzipRight},
    {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30},
     kArmS8x16TransposeLeft},
    {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31},
     kArmS8x16TransposeRight},
    {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8}, kArmS8x8Reverse},
    {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12}, kArmS8x4Reverse},
    {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14}, kArmS8x2Reverse}};

bool TryMatchArchShuffle(const uint8_t* shuffle, const ShuffleEntry* table,
                         size_t num_entries, bool is_swizzle,
                         ArchOpcode* opcode) {
  uint8_t mask = is_swizzle ? kSimd128Size - 1 : 2 * kSimd128Size - 1;
  for (size_t i = 0; i < num_entries; ++i) {
    const ShuffleEntry& entry = table[i];
    int j = 0;
    for (; j < kSimd128Size; ++j) {
      if ((entry.shuffle[j] & mask) != (shuffle[j] & mask)) {
        break;
      }
    }
    if (j == kSimd128Size) {
      *opcode = entry.opcode;
      return true;
    }
  }
  return false;
}

void ArrangeShuffleTable(ArmOperandGenerator* g, Node* input0, Node* input1,
                         InstructionOperand* src0, InstructionOperand* src1) {
  if (input0 == input1) {
    // Unary, any q-register can be the table.
    *src0 = *src1 = g->UseRegister(input0);
  } else {
    // Binary, table registers must be consecutive.
    *src0 = g->UseFixed(input0, q0);
    *src1 = g->UseFixed(input1, q1);
  }
}

}  // namespace

void InstructionSelector::VisitS8x16Shuffle(Node* node) {
  uint8_t shuffle[kSimd128Size];
  bool is_swizzle;
  CanonicalizeShuffle(node, shuffle, &is_swizzle);
  Node* input0 = node->InputAt(0);
  Node* input1 = node->InputAt(1);
  uint8_t shuffle32x4[4];
  ArmOperandGenerator g(this);
  int index = 0;
  if (TryMatch32x4Shuffle(shuffle, shuffle32x4)) {
    if (TryMatchDup<4>(shuffle, &index)) {
      DCHECK_GT(4, index);
      Emit(kArmS128Dup, g.DefineAsRegister(node), g.UseRegister(input0),
           g.UseImmediate(Neon32), g.UseImmediate(index % 4));
    } else if (TryMatchIdentity(shuffle)) {
      EmitIdentity(node);
    } else {
      // 32x4 shuffles are implemented as s-register moves. To simplify these,
      // make sure the destination is distinct from both sources.
      InstructionOperand src0 = g.UseUniqueRegister(input0);
      InstructionOperand src1 = is_swizzle ? src0 : g.UseUniqueRegister(input1);
      Emit(kArmS32x4Shuffle, g.DefineAsRegister(node), src0, src1,
           g.UseImmediate(Pack4Lanes(shuffle32x4)));
    }
    return;
  }
  if (TryMatchDup<8>(shuffle, &index)) {
    DCHECK_GT(8, index);
    Emit(kArmS128Dup, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseImmediate(Neon16), g.UseImmediate(index % 8));
    return;
  }
  if (TryMatchDup<16>(shuffle, &index)) {
    DCHECK_GT(16, index);
    Emit(kArmS128Dup, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseImmediate(Neon8), g.UseImmediate(index % 16));
    return;
  }
  ArchOpcode opcode;
  if (TryMatchArchShuffle(shuffle, arch_shuffles, arraysize(arch_shuffles),
                          is_swizzle, &opcode)) {
    VisitRRRShuffle(this, opcode, node);
    return;
  }
  uint8_t offset;
  if (TryMatchConcat(shuffle, &offset)) {
    Emit(kArmS8x16Concat, g.DefineAsRegister(node), g.UseRegister(input0),
         g.UseRegister(input1), g.UseImmediate(offset));
    return;
  }
  // Code generator uses vtbl, arrange sources to form a valid lookup table.
  InstructionOperand src0, src1;
  ArrangeShuffleTable(&g, input0, input1, &src0, &src1);
  Emit(kArmS8x16Shuffle, g.DefineAsRegister(node), src0, src1,
       g.UseImmediate(Pack4Lanes(shuffle)),
       g.UseImmediate(Pack4Lanes(shuffle + 4)),
       g.UseImmediate(Pack4Lanes(shuffle + 8)),
       g.UseImmediate(Pack4Lanes(shuffle + 12)));
}

void InstructionSelector::VisitSignExtendWord8ToInt32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmSxtb, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0));
}

void InstructionSelector::VisitSignExtendWord16ToInt32(Node* node) {
  ArmOperandGenerator g(this);
  Emit(kArmSxth, g.DefineAsRegister(node), g.UseRegister(node->InputAt(0)),
       g.TempImmediate(0));
}

void InstructionSelector::VisitInt32AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

void InstructionSelector::VisitInt64AbsWithOverflow(Node* node) {
  UNREACHABLE();
}

// static
MachineOperatorBuilder::Flags
InstructionSelector::SupportedMachineOperatorFlags() {
  MachineOperatorBuilder::Flags flags =
      MachineOperatorBuilder::kSpeculationFence;
  if (CpuFeatures::IsSupported(SUDIV)) {
    // The sdiv and udiv instructions correctly return 0 if the divisor is 0,
    // but the fall-back implementation does not.
    flags |= MachineOperatorBuilder::kInt32DivIsSafe |
             MachineOperatorBuilder::kUint32DivIsSafe;
  }
  if (CpuFeatures::IsSupported(ARMv7)) {
    flags |= MachineOperatorBuilder::kWord32ReverseBits;
  }
  if (CpuFeatures::IsSupported(ARMv8)) {
    flags |= MachineOperatorBuilder::kFloat32RoundDown |
             MachineOperatorBuilder::kFloat64RoundDown |
             MachineOperatorBuilder::kFloat32RoundUp |
             MachineOperatorBuilder::kFloat64RoundUp |
             MachineOperatorBuilder::kFloat32RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTruncate |
             MachineOperatorBuilder::kFloat64RoundTiesAway |
             MachineOperatorBuilder::kFloat32RoundTiesEven |
             MachineOperatorBuilder::kFloat64RoundTiesEven;
  }
  return flags;
}

// static
MachineOperatorBuilder::AlignmentRequirements
InstructionSelector::AlignmentRequirements() {
  EnumSet<MachineRepresentation> req_aligned;
  req_aligned.Add(MachineRepresentation::kFloat32);
  req_aligned.Add(MachineRepresentation::kFloat64);
  return MachineOperatorBuilder::AlignmentRequirements::
      SomeUnalignedAccessUnsupported(req_aligned, req_aligned);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
