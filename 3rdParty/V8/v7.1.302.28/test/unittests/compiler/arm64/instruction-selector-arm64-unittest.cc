// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects-inl.h"
#include "test/unittests/compiler/instruction-selector-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

template <typename T>
struct MachInst {
  T constructor;
  const char* constructor_name;
  ArchOpcode arch_opcode;
  MachineType machine_type;
};

typedef MachInst<Node* (RawMachineAssembler::*)(Node*)> MachInst1;
typedef MachInst<Node* (RawMachineAssembler::*)(Node*, Node*)> MachInst2;


template <typename T>
std::ostream& operator<<(std::ostream& os, const MachInst<T>& mi) {
  return os << mi.constructor_name;
}


struct Shift {
  MachInst2 mi;
  AddressingMode mode;
};


std::ostream& operator<<(std::ostream& os, const Shift& shift) {
  return os << shift.mi;
}


// Helper to build Int32Constant or Int64Constant depending on the given
// machine type.
Node* BuildConstant(InstructionSelectorTest::StreamBuilder& m, MachineType type,
                    int64_t value) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return m.Int32Constant(static_cast<int32_t>(value));
      break;

    case MachineRepresentation::kWord64:
      return m.Int64Constant(value);
      break;

    default:
      UNIMPLEMENTED();
  }
  return NULL;
}


// ARM64 logical instructions.
const MachInst2 kLogicalInstructions[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64And32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64And, "Word64And", kArm64And,
     MachineType::Int64()},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArm64Or32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64Or, "Word64Or", kArm64Or,
     MachineType::Int64()},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArm64Eor32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64Xor, "Word64Xor", kArm64Eor,
     MachineType::Int64()}};


// ARM64 logical immediates: contiguous set bits, rotated about a power of two
// sized block. The block is then duplicated across the word. Below is a random
// subset of the 32-bit immediates.
const uint32_t kLogical32Immediates[] = {
    0x00000002, 0x00000003, 0x00000070, 0x00000080, 0x00000100, 0x000001C0,
    0x00000300, 0x000007E0, 0x00003FFC, 0x00007FC0, 0x0003C000, 0x0003F000,
    0x0003FFC0, 0x0003FFF8, 0x0007FF00, 0x0007FFE0, 0x000E0000, 0x001E0000,
    0x001FFFFC, 0x003F0000, 0x003F8000, 0x00780000, 0x007FC000, 0x00FF0000,
    0x01800000, 0x01800180, 0x01F801F8, 0x03FE0000, 0x03FFFFC0, 0x03FFFFFC,
    0x06000000, 0x07FC0000, 0x07FFC000, 0x07FFFFC0, 0x07FFFFE0, 0x0FFE0FFE,
    0x0FFFF800, 0x0FFFFFF0, 0x0FFFFFFF, 0x18001800, 0x1F001F00, 0x1F801F80,
    0x30303030, 0x3FF03FF0, 0x3FF83FF8, 0x3FFF0000, 0x3FFF8000, 0x3FFFFFC0,
    0x70007000, 0x7F7F7F7F, 0x7FC00000, 0x7FFFFFC0, 0x8000001F, 0x800001FF,
    0x81818181, 0x9FFF9FFF, 0xC00007FF, 0xC0FFFFFF, 0xDDDDDDDD, 0xE00001FF,
    0xE00003FF, 0xE007FFFF, 0xEFFFEFFF, 0xF000003F, 0xF001F001, 0xF3FFF3FF,
    0xF800001F, 0xF80FFFFF, 0xF87FF87F, 0xFBFBFBFB, 0xFC00001F, 0xFC0000FF,
    0xFC0001FF, 0xFC03FC03, 0xFE0001FF, 0xFF000001, 0xFF03FF03, 0xFF800000,
    0xFF800FFF, 0xFF801FFF, 0xFF87FFFF, 0xFFC0003F, 0xFFC007FF, 0xFFCFFFCF,
    0xFFE00003, 0xFFE1FFFF, 0xFFF0001F, 0xFFF07FFF, 0xFFF80007, 0xFFF87FFF,
    0xFFFC00FF, 0xFFFE07FF, 0xFFFF00FF, 0xFFFFC001, 0xFFFFF007, 0xFFFFF3FF,
    0xFFFFF807, 0xFFFFF9FF, 0xFFFFFC0F, 0xFFFFFEFF};

// Random subset of 64-bit logical immediates.
const uint64_t kLogical64Immediates[] = {
    0x0000000000000001, 0x0000000000000002, 0x0000000000000003,
    0x0000000000000070, 0x0000000000000080, 0x0000000000000100,
    0x00000000000001C0, 0x0000000000000300, 0x0000000000000600,
    0x00000000000007E0, 0x0000000000003FFC, 0x0000000000007FC0,
    0x0000000600000000, 0x0000003FFFFFFFFC, 0x000000F000000000,
    0x000001F800000000, 0x0003FC0000000000, 0x0003FC000003FC00,
    0x0003FFFFFFC00000, 0x0003FFFFFFFFFFC0, 0x0006000000060000,
    0x003FFFFFFFFC0000, 0x0180018001800180, 0x01F801F801F801F8,
    0x0600000000000000, 0x1000000010000000, 0x1000100010001000,
    0x1010101010101010, 0x1111111111111111, 0x1F001F001F001F00,
    0x1F1F1F1F1F1F1F1F, 0x1FFFFFFFFFFFFFFE, 0x3FFC3FFC3FFC3FFC,
    0x5555555555555555, 0x7F7F7F7F7F7F7F7F, 0x8000000000000000,
    0x8000001F8000001F, 0x8181818181818181, 0x9999999999999999,
    0x9FFF9FFF9FFF9FFF, 0xAAAAAAAAAAAAAAAA, 0xDDDDDDDDDDDDDDDD,
    0xE0000000000001FF, 0xF800000000000000, 0xF8000000000001FF,
    0xF807F807F807F807, 0xFEFEFEFEFEFEFEFE, 0xFFFEFFFEFFFEFFFE,
    0xFFFFF807FFFFF807, 0xFFFFF9FFFFFFF9FF, 0xFFFFFC0FFFFFFC0F,
    0xFFFFFC0FFFFFFFFF, 0xFFFFFEFFFFFFFEFF, 0xFFFFFEFFFFFFFFFF,
    0xFFFFFF8000000000, 0xFFFFFFFEFFFFFFFE, 0xFFFFFFFFEFFFFFFF,
    0xFFFFFFFFF9FFFFFF, 0xFFFFFFFFFF800000, 0xFFFFFFFFFFFFC0FF,
    0xFFFFFFFFFFFFFFFE};

// ARM64 arithmetic instructions.
struct AddSub {
  MachInst2 mi;
  ArchOpcode negate_arch_opcode;
};


std::ostream& operator<<(std::ostream& os, const AddSub& op) {
  return os << op.mi;
}


const AddSub kAddSubInstructions[] = {
    {{&RawMachineAssembler::Int32Add, "Int32Add", kArm64Add32,
      MachineType::Int32()},
     kArm64Sub32},
    {{&RawMachineAssembler::Int64Add, "Int64Add", kArm64Add,
      MachineType::Int64()},
     kArm64Sub},
    {{&RawMachineAssembler::Int32Sub, "Int32Sub", kArm64Sub32,
      MachineType::Int32()},
     kArm64Add32},
    {{&RawMachineAssembler::Int64Sub, "Int64Sub", kArm64Sub,
      MachineType::Int64()},
     kArm64Add}};


// ARM64 Add/Sub immediates: 12-bit immediate optionally shifted by 12.
// Below is a combination of a random subset and some edge values.
const int32_t kAddSubImmediates[] = {
    0,        1,        69,       493,      599,      701,      719,
    768,      818,      842,      945,      1246,     1286,     1429,
    1669,     2171,     2179,     2182,     2254,     2334,     2338,
    2343,     2396,     2449,     2610,     2732,     2855,     2876,
    2944,     3377,     3458,     3475,     3476,     3540,     3574,
    3601,     3813,     3871,     3917,     4095,     4096,     16384,
    364544,   462848,   970752,   1523712,  1863680,  2363392,  3219456,
    3280896,  4247552,  4526080,  4575232,  4960256,  5505024,  5894144,
    6004736,  6193152,  6385664,  6795264,  7114752,  7233536,  7348224,
    7499776,  7573504,  7729152,  8634368,  8937472,  9465856,  10354688,
    10682368, 11059200, 11460608, 13168640, 13176832, 14336000, 15028224,
    15597568, 15892480, 16773120};


// ARM64 flag setting data processing instructions.
const MachInst2 kDPFlagSetInstructions[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64Tst32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32Add, "Int32Add", kArm64Cmn32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32Sub, "Int32Sub", kArm64Cmp32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64And, "Word64And", kArm64Tst,
     MachineType::Int64()}};


// ARM64 arithmetic with overflow instructions.
const MachInst2 kOvfAddSubInstructions[] = {
    {&RawMachineAssembler::Int32AddWithOverflow, "Int32AddWithOverflow",
     kArm64Add32, MachineType::Int32()},
    {&RawMachineAssembler::Int32SubWithOverflow, "Int32SubWithOverflow",
     kArm64Sub32, MachineType::Int32()},
    {&RawMachineAssembler::Int64AddWithOverflow, "Int64AddWithOverflow",
     kArm64Add, MachineType::Int64()},
    {&RawMachineAssembler::Int64SubWithOverflow, "Int64SubWithOverflow",
     kArm64Sub, MachineType::Int64()}};

// ARM64 shift instructions.
const Shift kShiftInstructions[] = {
    {{&RawMachineAssembler::Word32Shl, "Word32Shl", kArm64Lsl32,
      MachineType::Int32()},
     kMode_Operand2_R_LSL_I},
    {{&RawMachineAssembler::Word64Shl, "Word64Shl", kArm64Lsl,
      MachineType::Int64()},
     kMode_Operand2_R_LSL_I},
    {{&RawMachineAssembler::Word32Shr, "Word32Shr", kArm64Lsr32,
      MachineType::Int32()},
     kMode_Operand2_R_LSR_I},
    {{&RawMachineAssembler::Word64Shr, "Word64Shr", kArm64Lsr,
      MachineType::Int64()},
     kMode_Operand2_R_LSR_I},
    {{&RawMachineAssembler::Word32Sar, "Word32Sar", kArm64Asr32,
      MachineType::Int32()},
     kMode_Operand2_R_ASR_I},
    {{&RawMachineAssembler::Word64Sar, "Word64Sar", kArm64Asr,
      MachineType::Int64()},
     kMode_Operand2_R_ASR_I},
    {{&RawMachineAssembler::Word32Ror, "Word32Ror", kArm64Ror32,
      MachineType::Int32()},
     kMode_Operand2_R_ROR_I},
    {{&RawMachineAssembler::Word64Ror, "Word64Ror", kArm64Ror,
      MachineType::Int64()},
     kMode_Operand2_R_ROR_I}};


// ARM64 Mul/Div instructions.
const MachInst2 kMulDivInstructions[] = {
    {&RawMachineAssembler::Int32Mul, "Int32Mul", kArm64Mul32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int64Mul, "Int64Mul", kArm64Mul,
     MachineType::Int64()},
    {&RawMachineAssembler::Int32Div, "Int32Div", kArm64Idiv32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int64Div, "Int64Div", kArm64Idiv,
     MachineType::Int64()},
    {&RawMachineAssembler::Uint32Div, "Uint32Div", kArm64Udiv32,
     MachineType::Int32()},
    {&RawMachineAssembler::Uint64Div, "Uint64Div", kArm64Udiv,
     MachineType::Int64()}};


// ARM64 FP arithmetic instructions.
const MachInst2 kFPArithInstructions[] = {
    {&RawMachineAssembler::Float64Add, "Float64Add", kArm64Float64Add,
     MachineType::Float64()},
    {&RawMachineAssembler::Float64Sub, "Float64Sub", kArm64Float64Sub,
     MachineType::Float64()},
    {&RawMachineAssembler::Float64Mul, "Float64Mul", kArm64Float64Mul,
     MachineType::Float64()},
    {&RawMachineAssembler::Float64Div, "Float64Div", kArm64Float64Div,
     MachineType::Float64()}};


struct FPCmp {
  MachInst2 mi;
  FlagsCondition cond;
  FlagsCondition commuted_cond;
};


std::ostream& operator<<(std::ostream& os, const FPCmp& cmp) {
  return os << cmp.mi;
}


// ARM64 FP comparison instructions.
const FPCmp kFPCmpInstructions[] = {
    {{&RawMachineAssembler::Float64Equal, "Float64Equal", kArm64Float64Cmp,
      MachineType::Float64()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Float64LessThan, "Float64LessThan",
      kArm64Float64Cmp, MachineType::Float64()},
     kFloatLessThan,
     kFloatGreaterThan},
    {{&RawMachineAssembler::Float64LessThanOrEqual, "Float64LessThanOrEqual",
      kArm64Float64Cmp, MachineType::Float64()},
     kFloatLessThanOrEqual,
     kFloatGreaterThanOrEqual},
    {{&RawMachineAssembler::Float32Equal, "Float32Equal", kArm64Float32Cmp,
      MachineType::Float32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Float32LessThan, "Float32LessThan",
      kArm64Float32Cmp, MachineType::Float32()},
     kFloatLessThan,
     kFloatGreaterThan},
    {{&RawMachineAssembler::Float32LessThanOrEqual, "Float32LessThanOrEqual",
      kArm64Float32Cmp, MachineType::Float32()},
     kFloatLessThanOrEqual,
     kFloatGreaterThanOrEqual}};


struct Conversion {
  // The machine_type field in MachInst1 represents the destination type.
  MachInst1 mi;
  MachineType src_machine_type;
};


std::ostream& operator<<(std::ostream& os, const Conversion& conv) {
  return os << conv.mi;
}


// ARM64 type conversion instructions.
const Conversion kConversionInstructions[] = {
    {{&RawMachineAssembler::ChangeFloat32ToFloat64, "ChangeFloat32ToFloat64",
      kArm64Float32ToFloat64, MachineType::Float64()},
     MachineType::Float32()},
    {{&RawMachineAssembler::TruncateFloat64ToFloat32,
      "TruncateFloat64ToFloat32", kArm64Float64ToFloat32,
      MachineType::Float32()},
     MachineType::Float64()},
    {{&RawMachineAssembler::ChangeInt32ToInt64, "ChangeInt32ToInt64",
      kArm64Sxtw, MachineType::Int64()},
     MachineType::Int32()},
    {{&RawMachineAssembler::ChangeUint32ToUint64, "ChangeUint32ToUint64",
      kArm64Mov32, MachineType::Uint64()},
     MachineType::Uint32()},
    {{&RawMachineAssembler::TruncateInt64ToInt32, "TruncateInt64ToInt32",
      kArchNop, MachineType::Int32()},
     MachineType::Int64()},
    {{&RawMachineAssembler::ChangeInt32ToFloat64, "ChangeInt32ToFloat64",
      kArm64Int32ToFloat64, MachineType::Float64()},
     MachineType::Int32()},
    {{&RawMachineAssembler::ChangeUint32ToFloat64, "ChangeUint32ToFloat64",
      kArm64Uint32ToFloat64, MachineType::Float64()},
     MachineType::Uint32()},
    {{&RawMachineAssembler::ChangeFloat64ToInt32, "ChangeFloat64ToInt32",
      kArm64Float64ToInt32, MachineType::Int32()},
     MachineType::Float64()},
    {{&RawMachineAssembler::ChangeFloat64ToUint32, "ChangeFloat64ToUint32",
      kArm64Float64ToUint32, MachineType::Uint32()},
     MachineType::Float64()}};

// ARM64 instructions that clear the top 32 bits of the destination.
const MachInst2 kCanElideChangeUint32ToUint64[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64And32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArm64Or32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArm64Eor32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Shl, "Word32Shl", kArm64Lsl32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Shr, "Word32Shr", kArm64Lsr32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Sar, "Word32Sar", kArm64Asr32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Ror, "Word32Ror", kArm64Ror32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Int32Add, "Int32Add", kArm64Add32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32AddWithOverflow, "Int32AddWithOverflow",
     kArm64Add32, MachineType::Int32()},
    {&RawMachineAssembler::Int32Sub, "Int32Sub", kArm64Sub32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32SubWithOverflow, "Int32SubWithOverflow",
     kArm64Sub32, MachineType::Int32()},
    {&RawMachineAssembler::Int32Mul, "Int32Mul", kArm64Mul32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32Div, "Int32Div", kArm64Idiv32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32Mod, "Int32Mod", kArm64Imod32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32LessThan, "Int32LessThan", kArm64Cmp32,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
     kArm64Cmp32, MachineType::Int32()},
    {&RawMachineAssembler::Uint32Div, "Uint32Div", kArm64Udiv32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kArm64Cmp32,
     MachineType::Uint32()},
    {&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
     kArm64Cmp32, MachineType::Uint32()},
    {&RawMachineAssembler::Uint32Mod, "Uint32Mod", kArm64Umod32,
     MachineType::Uint32()},
};

}  // namespace


// -----------------------------------------------------------------------------
// Logical instructions.


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorLogicalTest;


TEST_P(InstructionSelectorLogicalTest, Parameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorLogicalTest, Immediate) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  if (type == MachineType::Int32()) {
    // Immediate on the right.
    TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
      StreamBuilder m(this, type, type);
      m.Return((m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }

    // Immediate on the left; all logical ops should commute.
    TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
      StreamBuilder m(this, type, type);
      m.Return((m.*dpi.constructor)(m.Int32Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  } else if (type == MachineType::Int64()) {
    // Immediate on the right.
    TRACED_FOREACH(int64_t, imm, kLogical64Immediates) {
      StreamBuilder m(this, type, type);
      m.Return((m.*dpi.constructor)(m.Parameter(0), m.Int64Constant(imm)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }

    // Immediate on the left; all logical ops should commute.
    TRACED_FOREACH(int64_t, imm, kLogical64Immediates) {
      StreamBuilder m(this, type, type);
      m.Return((m.*dpi.constructor)(m.Int64Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_P(InstructionSelectorLogicalTest, ShiftByImmediate) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test 64-bit shifted operands with 64-bit instructions.
    if (shift.mi.machine_type != type) continue;

    TRACED_FORRANGE(int, imm, 0, ((type == MachineType::Int32()) ? 31 : 63)) {
      StreamBuilder m(this, type, type, type);
      m.Return((m.*dpi.constructor)(
          m.Parameter(0),
          (m.*shift.mi.constructor)(m.Parameter(1),
                                    BuildConstant(m, type, imm))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }

    TRACED_FORRANGE(int, imm, 0, ((type == MachineType::Int32()) ? 31 : 63)) {
      StreamBuilder m(this, type, type, type);
      m.Return((m.*dpi.constructor)(
          (m.*shift.mi.constructor)(m.Parameter(1),
                                    BuildConstant(m, type, imm)),
          m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorLogicalTest,
                        ::testing::ValuesIn(kLogicalInstructions));


// -----------------------------------------------------------------------------
// Add and Sub instructions.

typedef InstructionSelectorTestWithParam<AddSub> InstructionSelectorAddSubTest;


TEST_P(InstructionSelectorAddSubTest, Parameter) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorAddSubTest, ImmediateOnRight) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    m.Return(
        (m.*dpi.mi.constructor)(m.Parameter(0), BuildConstant(m, type, imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorAddSubTest, NegImmediateOnRight) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    if (imm == 0) continue;
    StreamBuilder m(this, type, type);
    m.Return(
        (m.*dpi.mi.constructor)(m.Parameter(0), BuildConstant(m, type, -imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.negate_arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorAddSubTest, ShiftByImmediateOnRight) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test 64-bit shifted operands with 64-bit instructions.
    if (shift.mi.machine_type != type) continue;

    if ((shift.mi.arch_opcode == kArm64Ror32) ||
        (shift.mi.arch_opcode == kArm64Ror)) {
      // Not supported by add/sub instructions.
      continue;
    }

    TRACED_FORRANGE(int, imm, 0, ((type == MachineType::Int32()) ? 31 : 63)) {
      StreamBuilder m(this, type, type, type);
      m.Return((m.*dpi.mi.constructor)(
          m.Parameter(0),
          (m.*shift.mi.constructor)(m.Parameter(1),
                                    BuildConstant(m, type, imm))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_P(InstructionSelectorAddSubTest, UnsignedExtendByte) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.mi.constructor)(
      m.Parameter(0), m.Word32And(m.Parameter(1), m.Int32Constant(0xFF))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R_UXTB, s[0]->addressing_mode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorAddSubTest, UnsignedExtendHalfword) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.mi.constructor)(
      m.Parameter(0), m.Word32And(m.Parameter(1), m.Int32Constant(0xFFFF))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R_UXTH, s[0]->addressing_mode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorAddSubTest, SignedExtendByte) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.mi.constructor)(
      m.Parameter(0),
      m.Word32Sar(m.Word32Shl(m.Parameter(1), m.Int32Constant(24)),
                  m.Int32Constant(24))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorAddSubTest, SignedExtendHalfword) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.mi.constructor)(
      m.Parameter(0),
      m.Word32Sar(m.Word32Shl(m.Parameter(1), m.Int32Constant(16)),
                  m.Int32Constant(16))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R_SXTH, s[0]->addressing_mode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorAddSubTest,
                        ::testing::ValuesIn(kAddSubInstructions));


TEST_F(InstructionSelectorTest, AddImmediateOnLeft) {
  {
    // 32-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Int32Add(m.Int32Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
  {
    // 64-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
      m.Return(m.Int64Add(m.Int64Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_F(InstructionSelectorTest, SubZeroOnLeft) {
  {
    // 32-bit subtract.
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Int32Sub(m.Int32Constant(0), m.Parameter(0)));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Sub32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(0)->IsImmediate());
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(0)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    // 64-bit subtract.
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(m.Int64Sub(m.Int64Constant(0), m.Parameter(0)));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Sub, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(0)->IsImmediate());
    EXPECT_EQ(0, s.ToInt64(s[0]->InputAt(0)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, SubZeroOnLeftWithShift) {
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    {
      // Test 32-bit operations. Ignore ROR shifts, as subtract does not
      // support them.
      if ((shift.mi.machine_type != MachineType::Int32()) ||
          (shift.mi.arch_opcode == kArm64Ror32) ||
          (shift.mi.arch_opcode == kArm64Ror))
        continue;

      TRACED_FORRANGE(int, imm, -32, 63) {
        StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                        MachineType::Int32());
        m.Return(m.Int32Sub(
            m.Int32Constant(0),
            (m.*shift.mi.constructor)(m.Parameter(1), m.Int32Constant(imm))));
        Stream s = m.Build();

        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(kArm64Sub32, s[0]->arch_opcode());
        ASSERT_EQ(3U, s[0]->InputCount());
        EXPECT_TRUE(s[0]->InputAt(0)->IsImmediate());
        EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(0)));
        EXPECT_EQ(shift.mode, s[0]->addressing_mode());
        EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt32(s[0]->InputAt(2)));
        EXPECT_EQ(1U, s[0]->OutputCount());
      }
    }
    {
      // Test 64-bit operations. Ignore ROR shifts, as subtract does not
      // support them.
      if ((shift.mi.machine_type != MachineType::Int64()) ||
          (shift.mi.arch_opcode == kArm64Ror32) ||
          (shift.mi.arch_opcode == kArm64Ror))
        continue;

      TRACED_FORRANGE(int, imm, -32, 127) {
        StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                        MachineType::Int64());
        m.Return(m.Int64Sub(
            m.Int64Constant(0),
            (m.*shift.mi.constructor)(m.Parameter(1), m.Int64Constant(imm))));
        Stream s = m.Build();

        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(kArm64Sub, s[0]->arch_opcode());
        ASSERT_EQ(3U, s[0]->InputCount());
        EXPECT_TRUE(s[0]->InputAt(0)->IsImmediate());
        EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(0)));
        EXPECT_EQ(shift.mode, s[0]->addressing_mode());
        EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt32(s[0]->InputAt(2)));
        EXPECT_EQ(1U, s[0]->OutputCount());
      }
    }
  }
}


TEST_F(InstructionSelectorTest, AddNegImmediateOnLeft) {
  {
    // 32-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      if (imm == 0) continue;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Int32Add(m.Int32Constant(-imm), m.Parameter(0)));
      Stream s = m.Build();

      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Sub32, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
  {
    // 64-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      if (imm == 0) continue;
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
      m.Return(m.Int64Add(m.Int64Constant(-imm), m.Parameter(0)));
      Stream s = m.Build();

      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Sub, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_F(InstructionSelectorTest, AddShiftByImmediateOnLeft) {
  // 32-bit add.
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test relevant shifted operands.
    if (shift.mi.machine_type != MachineType::Int32()) continue;
    if (shift.mi.arch_opcode == kArm64Ror32) continue;

    // The available shift operand range is `0 <= imm < 32`, but we also test
    // that immediates outside this range are handled properly (modulo-32).
    TRACED_FORRANGE(int, imm, -32, 63) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return((m.Int32Add)(
          (m.*shift.mi.constructor)(m.Parameter(1), m.Int32Constant(imm)),
          m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }

  // 64-bit add.
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test relevant shifted operands.
    if (shift.mi.machine_type != MachineType::Int64()) continue;
    if (shift.mi.arch_opcode == kArm64Ror) continue;

    // The available shift operand range is `0 <= imm < 64`, but we also test
    // that immediates outside this range are handled properly (modulo-64).
    TRACED_FORRANGE(int, imm, -64, 127) {
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                      MachineType::Int64());
      m.Return((m.Int64Add)(
          (m.*shift.mi.constructor)(m.Parameter(1), m.Int64Constant(imm)),
          m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_F(InstructionSelectorTest, AddUnsignedExtendByteOnLeft) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Int32Add(m.Word32And(m.Parameter(0), m.Int32Constant(0xFF)),
                        m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int32(),
                    MachineType::Int64());
    m.Return(m.Int64Add(m.Word32And(m.Parameter(0), m.Int32Constant(0xFF)),
                        m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, AddUnsignedExtendHalfwordOnLeft) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Int32Add(m.Word32And(m.Parameter(0), m.Int32Constant(0xFFFF)),
                        m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int32(),
                    MachineType::Int64());
    m.Return(m.Int64Add(m.Word32And(m.Parameter(0), m.Int32Constant(0xFFFF)),
                        m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, AddSignedExtendByteOnLeft) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Add(m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(24)),
                               m.Int32Constant(24)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int32(),
                    MachineType::Int64());
    m.Return(
        m.Int64Add(m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(24)),
                               m.Int32Constant(24)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, AddSignedExtendHalfwordOnLeft) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Add(m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(16)),
                               m.Int32Constant(16)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int32(),
                    MachineType::Int64());
    m.Return(
        m.Int64Add(m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(16)),
                               m.Int32Constant(16)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


// -----------------------------------------------------------------------------
// Data processing controlled branches.


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorDPFlagSetTest;


TEST_P(InstructionSelectorDPFlagSetTest, BranchWithParameters) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  RawMachineLabel a, b;
  m.Branch((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kNotEqual, s[0]->flags_condition());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorDPFlagSetTest,
                        ::testing::ValuesIn(kDPFlagSetInstructions));


TEST_F(InstructionSelectorTest, Word32AndBranchWithImmediateOnRight) {
  TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation(static_cast<uint32_t>(imm)) == 1) continue;

    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32And(m.Parameter(0), m.Int32Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithImmediateOnRight) {
  TRACED_FOREACH(int64_t, imm, kLogical64Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation(static_cast<uint64_t>(imm)) == 1) continue;

    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    m.Branch(m.Word64And(m.Parameter(0), m.Int64Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, AddBranchWithImmediateOnRight) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Int32Add(m.Parameter(0), m.Int32Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, SubBranchWithImmediateOnRight) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Int32Sub(m.Parameter(0), m.Int32Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ((imm == 0) ? kArm64CompareAndBranch32 : kArm64Cmp32,
              s[0]->arch_opcode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word32AndBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation(static_cast<uint32_t>(imm)) == 1) continue;

    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32And(m.Int32Constant(imm), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    ASSERT_LE(1U, s[0]->InputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int64_t, imm, kLogical64Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation(static_cast<uint64_t>(imm)) == 1) continue;

    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    m.Branch(m.Word64And(m.Int64Constant(imm), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    ASSERT_LE(1U, s[0]->InputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, AddBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Int32Add(m.Int32Constant(imm), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
    ASSERT_LE(1U, s[0]->InputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}

struct TestAndBranch {
  MachInst<std::function<Node*(InstructionSelectorTest::StreamBuilder&, Node*,
                               uint32_t mask)>>
      mi;
  FlagsCondition cond;
};

std::ostream& operator<<(std::ostream& os, const TestAndBranch& tb) {
  return os << tb.mi;
}

const TestAndBranch kTestAndBranchMatchers32[] = {
    // Branch on the result of Word32And directly.
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x, uint32_t mask)
          -> Node* { return m.Word32And(x, m.Int32Constant(mask)); },
      "if (x and mask)", kArm64TestAndBranch32, MachineType::Int32()},
     kNotEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32BinaryNot(m.Word32And(x, m.Int32Constant(mask)));
      },
      "if not (x and mask)", kArm64TestAndBranch32, MachineType::Int32()},
     kEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x, uint32_t mask)
          -> Node* { return m.Word32And(m.Int32Constant(mask), x); },
      "if (mask and x)", kArm64TestAndBranch32, MachineType::Int32()},
     kNotEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32BinaryNot(m.Word32And(m.Int32Constant(mask), x));
      },
      "if not (mask and x)", kArm64TestAndBranch32, MachineType::Int32()},
     kEqual},
    // Branch on the result of '(x and mask) == mask'. This tests that a bit is
    // set rather than cleared which is why conditions are inverted.
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32Equal(m.Word32And(x, m.Int32Constant(mask)),
                             m.Int32Constant(mask));
      },
      "if ((x and mask) == mask)", kArm64TestAndBranch32, MachineType::Int32()},
     kNotEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32BinaryNot(m.Word32Equal(
            m.Word32And(x, m.Int32Constant(mask)), m.Int32Constant(mask)));
      },
      "if ((x and mask) != mask)", kArm64TestAndBranch32, MachineType::Int32()},
     kEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32Equal(m.Int32Constant(mask),
                             m.Word32And(x, m.Int32Constant(mask)));
      },
      "if (mask == (x and mask))", kArm64TestAndBranch32, MachineType::Int32()},
     kNotEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32BinaryNot(m.Word32Equal(
            m.Int32Constant(mask), m.Word32And(x, m.Int32Constant(mask))));
      },
      "if (mask != (x and mask))", kArm64TestAndBranch32, MachineType::Int32()},
     kEqual},
    // Same as above but swap 'mask' and 'x'.
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32Equal(m.Word32And(m.Int32Constant(mask), x),
                             m.Int32Constant(mask));
      },
      "if ((mask and x) == mask)", kArm64TestAndBranch32, MachineType::Int32()},
     kNotEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32BinaryNot(m.Word32Equal(
            m.Word32And(m.Int32Constant(mask), x), m.Int32Constant(mask)));
      },
      "if ((mask and x) != mask)", kArm64TestAndBranch32, MachineType::Int32()},
     kEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32Equal(m.Int32Constant(mask),
                             m.Word32And(m.Int32Constant(mask), x));
      },
      "if (mask == (mask and x))", kArm64TestAndBranch32, MachineType::Int32()},
     kNotEqual},
    {{[](InstructionSelectorTest::StreamBuilder& m, Node* x,
         uint32_t mask) -> Node* {
        return m.Word32BinaryNot(m.Word32Equal(
            m.Int32Constant(mask), m.Word32And(m.Int32Constant(mask), x)));
      },
      "if (mask != (mask and x))", kArm64TestAndBranch32, MachineType::Int32()},
     kEqual}};

typedef InstructionSelectorTestWithParam<TestAndBranch>
    InstructionSelectorTestAndBranchTest;

TEST_P(InstructionSelectorTestAndBranchTest, TestAndBranch32) {
  const TestAndBranch inst = GetParam();
  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(inst.mi.constructor(m, m.Parameter(0), mask), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(inst.cond, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorTestAndBranchTest,
                        ::testing::ValuesIn(kTestAndBranchMatchers32));

TEST_F(InstructionSelectorTest, Word64AndBranchWithOneBitMaskOnRight) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = 1L << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    m.Branch(m.Word64And(m.Parameter(0), m.Int64Constant(mask)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithOneBitMaskOnLeft) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = 1L << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    m.Branch(m.Word64And(m.Int64Constant(mask), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }
}

TEST_F(InstructionSelectorTest, TestAndBranch64EqualWhenCanCoverFalse) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = uint64_t{1} << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b, c;
    Node* n = m.Word64And(m.Parameter(0), m.Int64Constant(mask));
    m.Branch(m.Word64Equal(n, m.Int64Constant(0)), &a, &b);
    m.Bind(&a);
    m.Branch(m.Word64Equal(n, m.Int64Constant(3)), &b, &c);
    m.Bind(&c);
    m.Return(m.Int64Constant(1));
    m.Bind(&b);
    m.Return(m.Int64Constant(0));

    Stream s = m.Build();
    ASSERT_EQ(3U, s.size());
    EXPECT_EQ(kArm64And, s[0]->arch_opcode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
    EXPECT_EQ(kArm64TestAndBranch, s[1]->arch_opcode());
    EXPECT_EQ(kEqual, s[1]->flags_condition());
    EXPECT_EQ(kArm64Cmp, s[2]->arch_opcode());
    EXPECT_EQ(kEqual, s[2]->flags_condition());
    EXPECT_EQ(2U, s[0]->InputCount());
  }
}

TEST_F(InstructionSelectorTest, TestAndBranch64AndWhenCanCoverFalse) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = uint64_t{1} << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b, c;
    m.Branch(m.Word64And(m.Parameter(0), m.Int64Constant(mask)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int64Constant(1));
    m.Bind(&b);
    m.Return(m.Int64Constant(0));

    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(4U, s[0]->InputCount());
  }
}

TEST_F(InstructionSelectorTest, TestAndBranch32AndWhenCanCoverFalse) {
  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = uint32_t{1} << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b, c;
    m.Branch(m.Word32And(m.Parameter(0), m.Int32Constant(mask)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));

    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(4U, s[0]->InputCount());
  }
}

TEST_F(InstructionSelectorTest, Word32EqualZeroAndBranchWithOneBitMask) {
  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32Equal(m.Word32And(m.Int32Constant(mask), m.Parameter(0)),
                           m.Int32Constant(0)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }

  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(
        m.Word32NotEqual(m.Word32And(m.Int32Constant(mask), m.Parameter(0)),
                         m.Int32Constant(0)),
        &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }
}

TEST_F(InstructionSelectorTest, Word64EqualZeroAndBranchWithOneBitMask) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = uint64_t{1} << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    m.Branch(m.Word64Equal(m.Word64And(m.Int64Constant(mask), m.Parameter(0)),
                           m.Int64Constant(0)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int64Constant(1));
    m.Bind(&b);
    m.Return(m.Int64Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }

  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = uint64_t{1} << bit;
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    m.Branch(
        m.Word64NotEqual(m.Word64And(m.Int64Constant(mask), m.Parameter(0)),
                         m.Int64Constant(0)),
        &a, &b);
    m.Bind(&a);
    m.Return(m.Int64Constant(1));
    m.Bind(&b);
    m.Return(m.Int64Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64TestAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }
}

TEST_F(InstructionSelectorTest, CompareAgainstZeroAndBranch) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* p0 = m.Parameter(0);
    m.Branch(p0, &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64CompareAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  }

  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* p0 = m.Parameter(0);
    m.Branch(m.Word32BinaryNot(p0), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64CompareAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  }
}

TEST_F(InstructionSelectorTest, EqualZeroAndBranch) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* p0 = m.Parameter(0);
    m.Branch(m.Word32Equal(p0, m.Int32Constant(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64CompareAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  }

  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* p0 = m.Parameter(0);
    m.Branch(m.Word32NotEqual(p0, m.Int32Constant(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64CompareAndBranch32, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  }

  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    Node* p0 = m.Parameter(0);
    m.Branch(m.Word64Equal(p0, m.Int64Constant(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int64Constant(1));
    m.Bind(&b);
    m.Return(m.Int64Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64CompareAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  }

  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    RawMachineLabel a, b;
    Node* p0 = m.Parameter(0);
    m.Branch(m.Word64NotEqual(p0, m.Int64Constant(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int64Constant(1));
    m.Bind(&b);
    m.Return(m.Int64Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64CompareAndBranch, s[0]->arch_opcode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  }
}

// -----------------------------------------------------------------------------
// Add and subtract instructions with overflow.


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorOvfAddSubTest;


TEST_P(InstructionSelectorOvfAddSubTest, OvfParameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return(
      m.Projection(1, (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_LE(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorOvfAddSubTest, OvfImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    m.Return(m.Projection(
        1, (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorOvfAddSubTest, ValParameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return(
      m.Projection(0, (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_LE(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}


TEST_P(InstructionSelectorOvfAddSubTest, ValImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    m.Return(m.Projection(
        0, (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_P(InstructionSelectorOvfAddSubTest, BothParameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
  Stream s = m.Build();
  ASSERT_LE(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(2U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorOvfAddSubTest, BothImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();
    ASSERT_LE(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorOvfAddSubTest, BranchWithParameters) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  RawMachineLabel a, b;
  Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Branch(m.Projection(1, n), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(0));
  m.Bind(&b);
  m.Return(m.Projection(0, n));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorOvfAddSubTest, BranchWithImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    RawMachineLabel a, b;
    Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm));
    m.Branch(m.Projection(1, n), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(0));
    m.Bind(&b);
    m.Return(m.Projection(0, n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorOvfAddSubTest, RORShift) {
  // ADD and SUB do not support ROR shifts, make sure we do not try
  // to merge them into the ADD/SUB instruction.
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  auto rotate = &RawMachineAssembler::Word64Ror;
  ArchOpcode rotate_opcode = kArm64Ror;
  if (type == MachineType::Int32()) {
    rotate = &RawMachineAssembler::Word32Ror;
    rotate_opcode = kArm64Ror32;
  }
  TRACED_FORRANGE(int32_t, imm, -32, 63) {
    StreamBuilder m(this, type, type, type);
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r = (m.*rotate)(p1, m.Int32Constant(imm));
    m.Return((m.*dpi.constructor)(p0, r));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(rotate_opcode, s[0]->arch_opcode());
    EXPECT_EQ(dpi.arch_opcode, s[1]->arch_opcode());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorOvfAddSubTest,
                        ::testing::ValuesIn(kOvfAddSubInstructions));


TEST_F(InstructionSelectorTest, OvfFlagAddImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        1, m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0))));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, OvfValAddImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        0, m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0))));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_F(InstructionSelectorTest, OvfBothAddImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* n = m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();

    ASSERT_LE(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, OvfBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* n = m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0));
    m.Branch(m.Projection(1, n), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(0));
    m.Bind(&b);
    m.Return(m.Projection(0, n));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    ASSERT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


// -----------------------------------------------------------------------------
// Shift instructions.


typedef InstructionSelectorTestWithParam<Shift> InstructionSelectorShiftTest;


TEST_P(InstructionSelectorShiftTest, Parameter) {
  const Shift shift = GetParam();
  const MachineType type = shift.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*shift.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(shift.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorShiftTest, Immediate) {
  const Shift shift = GetParam();
  const MachineType type = shift.mi.machine_type;
  TRACED_FORRANGE(int32_t, imm, 0,
                  ((1 << ElementSizeLog2Of(type.representation())) * 8) - 1) {
    StreamBuilder m(this, type, type);
    m.Return((m.*shift.mi.constructor)(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(shift.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorShiftTest,
                        ::testing::ValuesIn(kShiftInstructions));


TEST_F(InstructionSelectorTest, Word64ShlWithChangeInt32ToInt64) {
  TRACED_FORRANGE(int64_t, x, 32, 63) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const n = m.Word64Shl(m.ChangeInt32ToInt64(p0), m.Int64Constant(x));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Lsl, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(x, s.ToInt64(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word64ShlWithChangeUint32ToUint64) {
  TRACED_FORRANGE(int64_t, x, 32, 63) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Uint32());
    Node* const p0 = m.Parameter(0);
    Node* const n = m.Word64Shl(m.ChangeUint32ToUint64(p0), m.Int64Constant(x));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Lsl, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(x, s.ToInt64(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, TruncateInt64ToInt32WithWord64Sar) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int64());
  Node* const p = m.Parameter(0);
  Node* const t = m.TruncateInt64ToInt32(m.Word64Sar(p, m.Int64Constant(32)));
  m.Return(t);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Asr, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(32, s.ToInt64(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, TruncateInt64ToInt32WithWord64Shr) {
  TRACED_FORRANGE(int64_t, x, 32, 63) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int64());
    Node* const p = m.Parameter(0);
    Node* const t = m.TruncateInt64ToInt32(m.Word64Shr(p, m.Int64Constant(x)));
    m.Return(t);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Lsr, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(x, s.ToInt64(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


// -----------------------------------------------------------------------------
// Mul and Div instructions.


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorMulDivTest;


TEST_P(InstructionSelectorMulDivTest, Parameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorMulDivTest,
                        ::testing::ValuesIn(kMulDivInstructions));


namespace {

struct MulDPInst {
  const char* mul_constructor_name;
  Node* (RawMachineAssembler::*mul_constructor)(Node*, Node*);
  Node* (RawMachineAssembler::*add_constructor)(Node*, Node*);
  Node* (RawMachineAssembler::*sub_constructor)(Node*, Node*);
  ArchOpcode add_arch_opcode;
  ArchOpcode sub_arch_opcode;
  ArchOpcode neg_arch_opcode;
  MachineType machine_type;
};


std::ostream& operator<<(std::ostream& os, const MulDPInst& inst) {
  return os << inst.mul_constructor_name;
}

}  // namespace


static const MulDPInst kMulDPInstructions[] = {
    {"Int32Mul", &RawMachineAssembler::Int32Mul, &RawMachineAssembler::Int32Add,
     &RawMachineAssembler::Int32Sub, kArm64Madd32, kArm64Msub32, kArm64Mneg32,
     MachineType::Int32()},
    {"Int64Mul", &RawMachineAssembler::Int64Mul, &RawMachineAssembler::Int64Add,
     &RawMachineAssembler::Int64Sub, kArm64Madd, kArm64Msub, kArm64Mneg,
     MachineType::Int64()}};


typedef InstructionSelectorTestWithParam<MulDPInst>
    InstructionSelectorIntDPWithIntMulTest;


TEST_P(InstructionSelectorIntDPWithIntMulTest, AddWithMul) {
  const MulDPInst mdpi = GetParam();
  const MachineType type = mdpi.machine_type;
  {
    StreamBuilder m(this, type, type, type, type);
    Node* n = (m.*mdpi.mul_constructor)(m.Parameter(1), m.Parameter(2));
    m.Return((m.*mdpi.add_constructor)(m.Parameter(0), n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.add_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type, type);
    Node* n = (m.*mdpi.mul_constructor)(m.Parameter(0), m.Parameter(1));
    m.Return((m.*mdpi.add_constructor)(n, m.Parameter(2)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.add_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorIntDPWithIntMulTest, SubWithMul) {
  const MulDPInst mdpi = GetParam();
  const MachineType type = mdpi.machine_type;
  {
    StreamBuilder m(this, type, type, type, type);
    Node* n = (m.*mdpi.mul_constructor)(m.Parameter(1), m.Parameter(2));
    m.Return((m.*mdpi.sub_constructor)(m.Parameter(0), n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.sub_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorIntDPWithIntMulTest, NegativeMul) {
  const MulDPInst mdpi = GetParam();
  const MachineType type = mdpi.machine_type;
  {
    StreamBuilder m(this, type, type, type);
    Node* n =
        (m.*mdpi.sub_constructor)(BuildConstant(m, type, 0), m.Parameter(0));
    m.Return((m.*mdpi.mul_constructor)(n, m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.neg_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type);
    Node* n =
        (m.*mdpi.sub_constructor)(BuildConstant(m, type, 0), m.Parameter(1));
    m.Return((m.*mdpi.mul_constructor)(m.Parameter(0), n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.neg_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorIntDPWithIntMulTest,
                        ::testing::ValuesIn(kMulDPInstructions));


TEST_F(InstructionSelectorTest, Int32MulWithImmediate) {
  // x * (2^k + 1) -> x + (x << k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Int32Mul(m.Parameter(0), m.Int32Constant((1 << k) + 1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // (2^k + 1) * x -> x + (x << k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Int32Mul(m.Int32Constant((1 << k) + 1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // x * (2^k + 1) + c -> x + (x << k) + c
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Add(m.Int32Mul(m.Parameter(0), m.Int32Constant((1 << k) + 1)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add32, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // (2^k + 1) * x + c -> x + (x << k) + c
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Add(m.Int32Mul(m.Int32Constant((1 << k) + 1), m.Parameter(0)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add32, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c + x * (2^k + 1) -> c + x + (x << k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Add(m.Parameter(0),
                   m.Int32Mul(m.Parameter(1), m.Int32Constant((1 << k) + 1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add32, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c + (2^k + 1) * x -> c + x + (x << k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Add(m.Parameter(0),
                   m.Int32Mul(m.Int32Constant((1 << k) + 1), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add32, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c - x * (2^k + 1) -> c - x + (x << k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Sub(m.Parameter(0),
                   m.Int32Mul(m.Parameter(1), m.Int32Constant((1 << k) + 1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Sub32, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c - (2^k + 1) * x -> c - x + (x << k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(
        m.Int32Sub(m.Parameter(0),
                   m.Int32Mul(m.Int32Constant((1 << k) + 1), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Sub32, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Int64MulWithImmediate) {
  // x * (2^k + 1) -> x + (x << k)
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    m.Return(m.Int64Mul(m.Parameter(0), m.Int64Constant((1L << k) + 1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // (2^k + 1) * x -> x + (x << k)
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    m.Return(m.Int64Mul(m.Int64Constant((1L << k) + 1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // x * (2^k + 1) + c -> x + (x << k) + c
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(
        m.Int64Add(m.Int64Mul(m.Parameter(0), m.Int64Constant((1L << k) + 1)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // (2^k + 1) * x + c -> x + (x << k) + c
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(
        m.Int64Add(m.Int64Mul(m.Int64Constant((1L << k) + 1), m.Parameter(0)),
                   m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c + x * (2^k + 1) -> c + x + (x << k)
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(
        m.Int64Add(m.Parameter(0),
                   m.Int64Mul(m.Parameter(1), m.Int64Constant((1L << k) + 1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c + (2^k + 1) * x -> c + x + (x << k)
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(
        m.Int64Add(m.Parameter(0),
                   m.Int64Mul(m.Int64Constant((1L << k) + 1), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Add, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c - x * (2^k + 1) -> c - x + (x << k)
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(
        m.Int64Sub(m.Parameter(0),
                   m.Int64Mul(m.Parameter(1), m.Int64Constant((1L << k) + 1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Sub, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // c - (2^k + 1) * x -> c - x + (x << k)
  TRACED_FORRANGE(int64_t, k, 1, 62) {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64(),
                    MachineType::Int64());
    m.Return(
        m.Int64Sub(m.Parameter(0),
                   m.Int64Mul(m.Int64Constant((1L << k) + 1), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
    EXPECT_EQ(kArm64Sub, s[1]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


// -----------------------------------------------------------------------------
// Floating point instructions.

typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorFPArithTest;


TEST_P(InstructionSelectorFPArithTest, Parameter) {
  const MachInst2 fpa = GetParam();
  StreamBuilder m(this, fpa.machine_type, fpa.machine_type, fpa.machine_type);
  m.Return((m.*fpa.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(fpa.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorFPArithTest,
                        ::testing::ValuesIn(kFPArithInstructions));


typedef InstructionSelectorTestWithParam<FPCmp> InstructionSelectorFPCmpTest;


TEST_P(InstructionSelectorFPCmpTest, Parameter) {
  const FPCmp cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), cmp.mi.machine_type,
                  cmp.mi.machine_type);
  m.Return((m.*cmp.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(cmp.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.cond, s[0]->flags_condition());
}


TEST_P(InstructionSelectorFPCmpTest, WithImmediateZeroOnRight) {
  const FPCmp cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), cmp.mi.machine_type);
  if (cmp.mi.machine_type == MachineType::Float64()) {
    m.Return((m.*cmp.mi.constructor)(m.Parameter(0), m.Float64Constant(0.0)));
  } else {
    m.Return((m.*cmp.mi.constructor)(m.Parameter(0), m.Float32Constant(0.0f)));
  }
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(cmp.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.cond, s[0]->flags_condition());
}


TEST_P(InstructionSelectorFPCmpTest, WithImmediateZeroOnLeft) {
  const FPCmp cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), cmp.mi.machine_type);
  if (cmp.mi.machine_type == MachineType::Float64()) {
    m.Return((m.*cmp.mi.constructor)(m.Float64Constant(0.0), m.Parameter(0)));
  } else {
    m.Return((m.*cmp.mi.constructor)(m.Float32Constant(0.0f), m.Parameter(0)));
  }
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(cmp.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.commuted_cond, s[0]->flags_condition());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorFPCmpTest,
                        ::testing::ValuesIn(kFPCmpInstructions));


// -----------------------------------------------------------------------------
// Conversions.

typedef InstructionSelectorTestWithParam<Conversion>
    InstructionSelectorConversionTest;


TEST_P(InstructionSelectorConversionTest, Parameter) {
  const Conversion conv = GetParam();
  StreamBuilder m(this, conv.mi.machine_type, conv.src_machine_type);
  m.Return((m.*conv.mi.constructor)(m.Parameter(0)));
  Stream s = m.Build();
  if (conv.mi.arch_opcode == kArchNop) {
    ASSERT_EQ(0U, s.size());
    return;
  }
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(conv.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorConversionTest,
                        ::testing::ValuesIn(kConversionInstructions));

typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorElidedChangeUint32ToUint64Test;

TEST_P(InstructionSelectorElidedChangeUint32ToUint64Test, Parameter) {
  const MachInst2 binop = GetParam();
  StreamBuilder m(this, MachineType::Uint64(), binop.machine_type,
                  binop.machine_type);
  m.Return(m.ChangeUint32ToUint64(
      (m.*binop.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  // Make sure the `ChangeUint32ToUint64` node turned into a no-op.
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(binop.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorElidedChangeUint32ToUint64Test,
                        ::testing::ValuesIn(kCanElideChangeUint32ToUint64));

TEST_F(InstructionSelectorTest, ChangeUint32ToUint64AfterLoad) {
  // For each case, make sure the `ChangeUint32ToUint64` node turned into a
  // no-op.

  // Ldrb
  {
    StreamBuilder m(this, MachineType::Uint64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeUint32ToUint64(
        m.Load(MachineType::Uint8(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrb, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // Ldrh
  {
    StreamBuilder m(this, MachineType::Uint64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeUint32ToUint64(
        m.Load(MachineType::Uint16(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrh, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // LdrW
  {
    StreamBuilder m(this, MachineType::Uint64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeUint32ToUint64(
        m.Load(MachineType::Uint32(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64LdrW, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

TEST_F(InstructionSelectorTest, ChangeInt32ToInt64AfterLoad) {
  // For each case, test that the conversion is merged into the load
  // operation.
  // ChangeInt32ToInt64(Load_Uint8) -> Ldrb
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeInt32ToInt64(
        m.Load(MachineType::Uint8(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrb, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // ChangeInt32ToInt64(Load_Int8) -> Ldrsb
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeInt32ToInt64(
        m.Load(MachineType::Int8(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrsb, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // ChangeInt32ToInt64(Load_Uint16) -> Ldrh
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeInt32ToInt64(
        m.Load(MachineType::Uint16(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrh, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // ChangeInt32ToInt64(Load_Int16) -> Ldrsh
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeInt32ToInt64(
        m.Load(MachineType::Int16(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrsh, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // ChangeInt32ToInt64(Load_Uint32) -> Ldrsw
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeInt32ToInt64(
        m.Load(MachineType::Uint32(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrsw, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // ChangeInt32ToInt64(Load_Int32) -> Ldrsw
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                    MachineType::Int32());
    m.Return(m.ChangeInt32ToInt64(
        m.Load(MachineType::Int32(), m.Parameter(0), m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldrsw, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

// -----------------------------------------------------------------------------
// Memory access instructions.


namespace {

struct MemoryAccess {
  MachineType type;
  ArchOpcode ldr_opcode;
  ArchOpcode str_opcode;
  const int32_t immediates[20];
};


std::ostream& operator<<(std::ostream& os, const MemoryAccess& memacc) {
  return os << memacc.type;
}

}  // namespace


static const MemoryAccess kMemoryAccesses[] = {
    {MachineType::Int8(),
     kArm64Ldrsb,
     kArm64Strb,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 257, 258, 1000, 1001, 2121,
      2442, 4093, 4094, 4095}},
    {MachineType::Uint8(),
     kArm64Ldrb,
     kArm64Strb,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 257, 258, 1000, 1001, 2121,
      2442, 4093, 4094, 4095}},
    {MachineType::Int16(),
     kArm64Ldrsh,
     kArm64Strh,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 258, 260, 4096, 4098, 4100,
      4242, 6786, 8188, 8190}},
    {MachineType::Uint16(),
     kArm64Ldrh,
     kArm64Strh,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 258, 260, 4096, 4098, 4100,
      4242, 6786, 8188, 8190}},
    {MachineType::Int32(),
     kArm64LdrW,
     kArm64StrW,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 260, 4096, 4100, 8192, 8196,
      3276, 3280, 16376, 16380}},
    {MachineType::Uint32(),
     kArm64LdrW,
     kArm64StrW,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 260, 4096, 4100, 8192, 8196,
      3276, 3280, 16376, 16380}},
    {MachineType::Int64(),
     kArm64Ldr,
     kArm64Str,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 264, 4096, 4104, 8192, 8200,
      16384, 16392, 32752, 32760}},
    {MachineType::Uint64(),
     kArm64Ldr,
     kArm64Str,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 264, 4096, 4104, 8192, 8200,
      16384, 16392, 32752, 32760}},
    {MachineType::Float32(),
     kArm64LdrS,
     kArm64StrS,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 260, 4096, 4100, 8192, 8196,
      3276, 3280, 16376, 16380}},
    {MachineType::Float64(),
     kArm64LdrD,
     kArm64StrD,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 264, 4096, 4104, 8192, 8200,
      16384, 16392, 32752, 32760}}};


typedef InstructionSelectorTestWithParam<MemoryAccess>
    InstructionSelectorMemoryAccessTest;


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, MachineType::Pointer(),
                  MachineType::Int32());
  m.Return(m.Load(memacc.type, m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, memacc.type, MachineType::Pointer());
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                  MachineType::Int32(), memacc.type);
  m.Store(memacc.type.representation(), m.Parameter(0), m.Parameter(1),
          m.Parameter(2), kNoWriteBarrier);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
  EXPECT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(0U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    memacc.type);
    m.Store(memacc.type.representation(), m.Parameter(0),
            m.Int32Constant(index), m.Parameter(1), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(2)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

TEST_P(InstructionSelectorMemoryAccessTest, StoreZero) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
    m.Store(memacc.type.representation(), m.Parameter(0),
            m.Int32Constant(index), m.Int32Constant(0), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(2)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(0)->kind());
    EXPECT_EQ(0, s.ToInt64(s[0]->InputAt(0)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

TEST_P(InstructionSelectorMemoryAccessTest, LoadWithShiftedIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FORRANGE(int, immediate_shift, 0, 4) {
    // 32 bit shift
    {
      StreamBuilder m(this, memacc.type, MachineType::Pointer(),
                      MachineType::Int32());
      Node* const index =
          m.Word32Shl(m.Parameter(1), m.Int32Constant(immediate_shift));
      m.Return(m.Load(memacc.type, m.Parameter(0), index));
      Stream s = m.Build();
      if (immediate_shift == ElementSizeLog2Of(memacc.type.representation())) {
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
        EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
        EXPECT_EQ(3U, s[0]->InputCount());
        EXPECT_EQ(1U, s[0]->OutputCount());
      } else {
        // Make sure we haven't merged the shift into the load instruction.
        ASSERT_NE(1U, s.size());
        EXPECT_NE(memacc.ldr_opcode, s[0]->arch_opcode());
        EXPECT_NE(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
      }
    }
    // 64 bit shift
    {
      StreamBuilder m(this, memacc.type, MachineType::Pointer(),
                      MachineType::Int64());
      Node* const index =
          m.Word64Shl(m.Parameter(1), m.Int64Constant(immediate_shift));
      m.Return(m.Load(memacc.type, m.Parameter(0), index));
      Stream s = m.Build();
      if (immediate_shift == ElementSizeLog2Of(memacc.type.representation())) {
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
        EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
        EXPECT_EQ(3U, s[0]->InputCount());
        EXPECT_EQ(1U, s[0]->OutputCount());
      } else {
        // Make sure we haven't merged the shift into the load instruction.
        ASSERT_NE(1U, s.size());
        EXPECT_NE(memacc.ldr_opcode, s[0]->arch_opcode());
        EXPECT_NE(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
      }
    }
  }
}

TEST_P(InstructionSelectorMemoryAccessTest, StoreWithShiftedIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FORRANGE(int, immediate_shift, 0, 4) {
    // 32 bit shift
    {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                      MachineType::Int32(), memacc.type);
      Node* const index =
          m.Word32Shl(m.Parameter(1), m.Int32Constant(immediate_shift));
      m.Store(memacc.type.representation(), m.Parameter(0), index,
              m.Parameter(2), kNoWriteBarrier);
      m.Return(m.Int32Constant(0));
      Stream s = m.Build();
      if (immediate_shift == ElementSizeLog2Of(memacc.type.representation())) {
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
        EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
        EXPECT_EQ(4U, s[0]->InputCount());
        EXPECT_EQ(0U, s[0]->OutputCount());
      } else {
        // Make sure we haven't merged the shift into the store instruction.
        ASSERT_NE(1U, s.size());
        EXPECT_NE(memacc.str_opcode, s[0]->arch_opcode());
        EXPECT_NE(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
      }
    }
    // 64 bit shift
    {
      StreamBuilder m(this, MachineType::Int64(), MachineType::Pointer(),
                      MachineType::Int64(), memacc.type);
      Node* const index =
          m.Word64Shl(m.Parameter(1), m.Int64Constant(immediate_shift));
      m.Store(memacc.type.representation(), m.Parameter(0), index,
              m.Parameter(2), kNoWriteBarrier);
      m.Return(m.Int64Constant(0));
      Stream s = m.Build();
      if (immediate_shift == ElementSizeLog2Of(memacc.type.representation())) {
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
        EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
        EXPECT_EQ(4U, s[0]->InputCount());
        EXPECT_EQ(0U, s[0]->OutputCount());
      } else {
        // Make sure we haven't merged the shift into the store instruction.
        ASSERT_NE(1U, s.size());
        EXPECT_NE(memacc.str_opcode, s[0]->arch_opcode());
        EXPECT_NE(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessTest,
                        ::testing::ValuesIn(kMemoryAccesses));


// -----------------------------------------------------------------------------
// Comparison instructions.

static const MachInst2 kComparisonInstructions[] = {
    {&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64Equal, "Word64Equal", kArm64Cmp,
     MachineType::Int64()},
};


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorComparisonTest;


TEST_P(InstructionSelectorComparisonTest, WithParameters) {
  const MachInst2 cmp = GetParam();
  const MachineType type = cmp.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*cmp.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(cmp.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kEqual, s[0]->flags_condition());
}


TEST_P(InstructionSelectorComparisonTest, WithImmediate) {
  const MachInst2 cmp = GetParam();
  const MachineType type = cmp.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    // Compare with 0 are turned into tst instruction.
    if (imm == 0) continue;
    StreamBuilder m(this, type, type);
    m.Return((m.*cmp.constructor)(m.Parameter(0), BuildConstant(m, type, imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(cmp.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    // Compare with 0 are turned into tst instruction.
    if (imm == 0) continue;
    StreamBuilder m(this, type, type);
    m.Return((m.*cmp.constructor)(BuildConstant(m, type, imm), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(cmp.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorComparisonTest,
                        ::testing::ValuesIn(kComparisonInstructions));


TEST_F(InstructionSelectorTest, Word32EqualWithZero) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Parameter(0), m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Int32Constant(0), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word64EqualWithZero) {
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    m.Return(m.Word64Equal(m.Parameter(0), m.Int64Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    m.Return(m.Word64Equal(m.Int64Constant(0), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithWord32Shift) {
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Skip non 32-bit shifts or ror operations.
    if (shift.mi.machine_type != MachineType::Int32() ||
        shift.mi.arch_opcode == kArm64Ror32) {
      continue;
    }

    TRACED_FORRANGE(int32_t, imm, -32, 63) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      Node* const p0 = m.Parameter(0);
      Node* const p1 = m.Parameter(1);
      Node* r = (m.*shift.mi.constructor)(p1, m.Int32Constant(imm));
      m.Return(m.Word32Equal(p0, r));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
      EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
      EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt32(s[0]->InputAt(2)));
      ASSERT_EQ(1U, s[0]->OutputCount());
    }
    TRACED_FORRANGE(int32_t, imm, -32, 63) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      Node* const p0 = m.Parameter(0);
      Node* const p1 = m.Parameter(1);
      Node* r = (m.*shift.mi.constructor)(p1, m.Int32Constant(imm));
      m.Return(m.Word32Equal(r, p0));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
      EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
      EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt32(s[0]->InputAt(2)));
      ASSERT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithUnsignedExtendByte) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r = m.Word32And(p1, m.Int32Constant(0xFF));
    m.Return(m.Word32Equal(p0, r));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r = m.Word32And(p1, m.Int32Constant(0xFF));
    m.Return(m.Word32Equal(r, p0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithUnsignedExtendHalfword) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r = m.Word32And(p1, m.Int32Constant(0xFFFF));
    m.Return(m.Word32Equal(p0, r));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r = m.Word32And(p1, m.Int32Constant(0xFFFF));
    m.Return(m.Word32Equal(r, p0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_UXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithSignedExtendByte) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r =
        m.Word32Sar(m.Word32Shl(p1, m.Int32Constant(24)), m.Int32Constant(24));
    m.Return(m.Word32Equal(p0, r));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r =
        m.Word32Sar(m.Word32Shl(p1, m.Int32Constant(24)), m.Int32Constant(24));
    m.Return(m.Word32Equal(r, p0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithSignedExtendHalfword) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r =
        m.Word32Sar(m.Word32Shl(p1, m.Int32Constant(16)), m.Int32Constant(16));
    m.Return(m.Word32Equal(p0, r));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* r =
        m.Word32Sar(m.Word32Shl(p1, m.Int32Constant(16)), m.Int32Constant(16));
    m.Return(m.Word32Equal(r, p0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_SXTH, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualZeroWithWord32Equal) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    m.Return(m.Word32Equal(m.Word32Equal(p0, p1), m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    m.Return(m.Word32Equal(m.Int32Constant(0), m.Word32Equal(p0, p1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}

namespace {

struct IntegerCmp {
  MachInst2 mi;
  FlagsCondition cond;
  FlagsCondition commuted_cond;
};


std::ostream& operator<<(std::ostream& os, const IntegerCmp& cmp) {
  return os << cmp.mi;
}


// ARM64 32-bit integer comparison instructions.
const IntegerCmp kIntegerCmpInstructions[] = {
    {{&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32,
      MachineType::Int32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Int32LessThan, "Int32LessThan", kArm64Cmp32,
      MachineType::Int32()},
     kSignedLessThan,
     kSignedGreaterThan},
    {{&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
      kArm64Cmp32, MachineType::Int32()},
     kSignedLessThanOrEqual,
     kSignedGreaterThanOrEqual},
    {{&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kArm64Cmp32,
      MachineType::Uint32()},
     kUnsignedLessThan,
     kUnsignedGreaterThan},
    {{&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
      kArm64Cmp32, MachineType::Uint32()},
     kUnsignedLessThanOrEqual,
     kUnsignedGreaterThanOrEqual}};

const IntegerCmp kIntegerCmpEqualityInstructions[] = {
    {{&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32,
      MachineType::Int32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kArm64Cmp32,
      MachineType::Int32()},
     kNotEqual,
     kNotEqual}};
}  // namespace


TEST_F(InstructionSelectorTest, Word32CompareNegateWithWord32Shift) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpEqualityInstructions) {
    TRACED_FOREACH(Shift, shift, kShiftInstructions) {
      // Test 32-bit operations. Ignore ROR shifts, as compare-negate does not
      // support them.
      if (shift.mi.machine_type != MachineType::Int32() ||
          shift.mi.arch_opcode == kArm64Ror32) {
        continue;
      }

      TRACED_FORRANGE(int32_t, imm, -32, 63) {
        StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                        MachineType::Int32());
        Node* const p0 = m.Parameter(0);
        Node* const p1 = m.Parameter(1);
        Node* r = (m.*shift.mi.constructor)(p1, m.Int32Constant(imm));
        m.Return(
            (m.*cmp.mi.constructor)(p0, m.Int32Sub(m.Int32Constant(0), r)));
        Stream s = m.Build();
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
        EXPECT_EQ(3U, s[0]->InputCount());
        EXPECT_EQ(shift.mode, s[0]->addressing_mode());
        EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt32(s[0]->InputAt(2)));
        EXPECT_EQ(1U, s[0]->OutputCount());
        EXPECT_EQ(kFlags_set, s[0]->flags_mode());
        EXPECT_EQ(cmp.cond, s[0]->flags_condition());
      }
    }
  }
}

TEST_F(InstructionSelectorTest, CmpWithImmediateOnLeft) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpInstructions) {
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      // kEqual and kNotEqual trigger the cbz/cbnz optimization, which
      // is tested elsewhere.
      if (cmp.cond == kEqual || cmp.cond == kNotEqual) continue;
      // For signed less than or equal to zero, we generate TBNZ.
      if (cmp.cond == kSignedLessThanOrEqual && imm == 0) continue;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      Node* const p0 = m.Parameter(0);
      m.Return((m.*cmp.mi.constructor)(m.Int32Constant(imm), p0));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
      ASSERT_LE(2U, s[0]->InputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(cmp.commuted_cond, s[0]->flags_condition());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    }
  }
}

TEST_F(InstructionSelectorTest, CmnWithImmediateOnLeft) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpEqualityInstructions) {
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      // kEqual and kNotEqual trigger the cbz/cbnz optimization, which
      // is tested elsewhere.
      if (cmp.cond == kEqual || cmp.cond == kNotEqual) continue;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      Node* sub = m.Int32Sub(m.Int32Constant(0), m.Parameter(0));
      m.Return((m.*cmp.mi.constructor)(m.Int32Constant(imm), sub));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
      ASSERT_LE(2U, s[0]->InputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(cmp.cond, s[0]->flags_condition());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    }
  }
}

TEST_F(InstructionSelectorTest, CmpSignedExtendByteOnLeft) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* extend = m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(24)),
                               m.Int32Constant(24));
    m.Return((m.*cmp.mi.constructor)(extend, m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.commuted_cond, s[0]->flags_condition());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
  }
}

TEST_F(InstructionSelectorTest, CmnSignedExtendByteOnLeft) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpEqualityInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* sub = m.Int32Sub(m.Int32Constant(0), m.Parameter(0));
    Node* extend = m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(24)),
                               m.Int32Constant(24));
    m.Return((m.*cmp.mi.constructor)(extend, sub));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
  }
}

TEST_F(InstructionSelectorTest, CmpShiftByImmediateOnLeft) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpInstructions) {
    TRACED_FOREACH(Shift, shift, kShiftInstructions) {
      // Only test relevant shifted operands.
      if (shift.mi.machine_type != MachineType::Int32()) continue;

      // The available shift operand range is `0 <= imm < 32`, but we also test
      // that immediates outside this range are handled properly (modulo-32).
      TRACED_FORRANGE(int, imm, -32, 63) {
        StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                        MachineType::Int32());
        m.Return((m.*cmp.mi.constructor)(
            (m.*shift.mi.constructor)(m.Parameter(1), m.Int32Constant(imm)),
            m.Parameter(0)));
        Stream s = m.Build();
        // Cmp does not support ROR shifts.
        if (shift.mi.arch_opcode == kArm64Ror32) {
          ASSERT_EQ(2U, s.size());
          continue;
        }
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
        EXPECT_EQ(shift.mode, s[0]->addressing_mode());
        EXPECT_EQ(3U, s[0]->InputCount());
        EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt64(s[0]->InputAt(2)));
        EXPECT_EQ(1U, s[0]->OutputCount());
        EXPECT_EQ(kFlags_set, s[0]->flags_mode());
        EXPECT_EQ(cmp.commuted_cond, s[0]->flags_condition());
      }
    }
  }
}

TEST_F(InstructionSelectorTest, CmnShiftByImmediateOnLeft) {
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpEqualityInstructions) {
    TRACED_FOREACH(Shift, shift, kShiftInstructions) {
      // Only test relevant shifted operands.
      if (shift.mi.machine_type != MachineType::Int32()) continue;

      // The available shift operand range is `0 <= imm < 32`, but we also test
      // that immediates outside this range are handled properly (modulo-32).
      TRACED_FORRANGE(int, imm, -32, 63) {
        StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                        MachineType::Int32());
        Node* sub = m.Int32Sub(m.Int32Constant(0), m.Parameter(0));
        m.Return((m.*cmp.mi.constructor)(
            (m.*shift.mi.constructor)(m.Parameter(1), m.Int32Constant(imm)),
            sub));
        Stream s = m.Build();
        // Cmn does not support ROR shifts.
        if (shift.mi.arch_opcode == kArm64Ror32) {
          ASSERT_EQ(2U, s.size());
          continue;
        }
        ASSERT_EQ(1U, s.size());
        EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
        EXPECT_EQ(shift.mode, s[0]->addressing_mode());
        EXPECT_EQ(3U, s[0]->InputCount());
        EXPECT_EQ(0x3F & imm, 0x3F & s.ToInt64(s[0]->InputAt(2)));
        EXPECT_EQ(1U, s[0]->OutputCount());
        EXPECT_EQ(kFlags_set, s[0]->flags_mode());
        EXPECT_EQ(cmp.cond, s[0]->flags_condition());
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Flag-setting add and and instructions.

const IntegerCmp kBinopCmpZeroRightInstructions[] = {
    {{&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32,
      MachineType::Int32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kArm64Cmp32,
      MachineType::Int32()},
     kNotEqual,
     kNotEqual},
    {{&RawMachineAssembler::Int32LessThan, "Int32LessThan", kArm64Cmp32,
      MachineType::Int32()},
     kNegative,
     kNegative},
    {{&RawMachineAssembler::Int32GreaterThanOrEqual, "Int32GreaterThanOrEqual",
      kArm64Cmp32, MachineType::Int32()},
     kPositiveOrZero,
     kPositiveOrZero},
    {{&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
      kArm64Cmp32, MachineType::Int32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Uint32GreaterThan, "Uint32GreaterThan", kArm64Cmp32,
      MachineType::Int32()},
     kNotEqual,
     kNotEqual}};

const IntegerCmp kBinopCmpZeroLeftInstructions[] = {
    {{&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32,
      MachineType::Int32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kArm64Cmp32,
      MachineType::Int32()},
     kNotEqual,
     kNotEqual},
    {{&RawMachineAssembler::Int32GreaterThan, "Int32GreaterThan", kArm64Cmp32,
      MachineType::Int32()},
     kNegative,
     kNegative},
    {{&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
      kArm64Cmp32, MachineType::Int32()},
     kPositiveOrZero,
     kPositiveOrZero},
    {{&RawMachineAssembler::Uint32GreaterThanOrEqual,
      "Uint32GreaterThanOrEqual", kArm64Cmp32, MachineType::Int32()},
     kEqual,
     kEqual},
    {{&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kArm64Cmp32,
      MachineType::Int32()},
     kNotEqual,
     kNotEqual}};

struct FlagSettingInst {
  MachInst2 mi;
  ArchOpcode no_output_opcode;
};

std::ostream& operator<<(std::ostream& os, const FlagSettingInst& inst) {
  return os << inst.mi.constructor_name;
}

const FlagSettingInst kFlagSettingInstructions[] = {
    {{&RawMachineAssembler::Int32Add, "Int32Add", kArm64Add32,
      MachineType::Int32()},
     kArm64Cmn32},
    {{&RawMachineAssembler::Word32And, "Word32And", kArm64And32,
      MachineType::Int32()},
     kArm64Tst32}};

typedef InstructionSelectorTestWithParam<FlagSettingInst>
    InstructionSelectorFlagSettingTest;

TEST_P(InstructionSelectorFlagSettingTest, CmpZeroRight) {
  const FlagSettingInst inst = GetParam();
  // Add with single user : a cmp instruction.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* binop = (m.*inst.mi.constructor)(m.Parameter(0), m.Parameter(1));
    m.Return((m.*cmp.mi.constructor)(binop, m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CmpZeroLeft) {
  const FlagSettingInst inst = GetParam();
  // Test a cmp with zero on the left-hand side.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroLeftInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* binop = (m.*inst.mi.constructor)(m.Parameter(0), m.Parameter(1));
    m.Return((m.*cmp.mi.constructor)(m.Int32Constant(0), binop));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CmpZeroOnlyUserInBasicBlock) {
  const FlagSettingInst inst = GetParam();
  // Binop with additional users, but in a different basic block.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* binop = (m.*inst.mi.constructor)(m.Parameter(0), m.Parameter(1));
    Node* comp = (m.*cmp.mi.constructor)(binop, m.Int32Constant(0));
    m.Branch(m.Parameter(0), &a, &b);
    m.Bind(&a);
    m.Return(binop);
    m.Bind(&b);
    m.Return(comp);
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());  // Flag-setting instruction and branch.
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(inst.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, ShiftedOperand) {
  const FlagSettingInst inst = GetParam();
  // Like the test above, but with a shifted input to the binary operator.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* imm = m.Int32Constant(5);
    Node* shift = m.Word32Shl(m.Parameter(1), imm);
    Node* binop = (m.*inst.mi.constructor)(m.Parameter(0), shift);
    Node* comp = (m.*cmp.mi.constructor)(binop, m.Int32Constant(0));
    m.Branch(m.Parameter(0), &a, &b);
    m.Bind(&a);
    m.Return(binop);
    m.Bind(&b);
    m.Return(comp);
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());  // Flag-setting instruction and branch.
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(inst.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(5, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, UsersInSameBasicBlock) {
  const FlagSettingInst inst = GetParam();
  // Binop with additional users, in the same basic block. We need to make sure
  // we don't try to optimise this case.
  TRACED_FOREACH(IntegerCmp, cmp, kIntegerCmpInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* binop = (m.*inst.mi.constructor)(m.Parameter(0), m.Parameter(1));
    Node* mul = m.Int32Mul(m.Parameter(0), binop);
    Node* comp = (m.*cmp.mi.constructor)(binop, m.Int32Constant(0));
    m.Branch(m.Parameter(0), &a, &b);
    m.Bind(&a);
    m.Return(mul);
    m.Bind(&b);
    m.Return(comp);
    Stream s = m.Build();
    ASSERT_EQ(4U, s.size());  // Includes the compare and branch instruction.
    EXPECT_EQ(inst.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
    EXPECT_EQ(kArm64Mul32, s[1]->arch_opcode());
    EXPECT_EQ(kArm64Cmp32, s[2]->arch_opcode());
    EXPECT_EQ(kFlags_set, s[2]->flags_mode());
    EXPECT_EQ(cmp.cond, s[2]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CommuteImmediate) {
  const FlagSettingInst inst = GetParam();
  // Immediate on left hand side of the binary operator.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    // 3 can be an immediate on both arithmetic and logical instructions.
    Node* imm = m.Int32Constant(3);
    Node* binop = (m.*inst.mi.constructor)(imm, m.Parameter(0));
    Node* comp = (m.*cmp.mi.constructor)(binop, m.Int32Constant(0));
    m.Return(comp);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(3, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CommuteShift) {
  const FlagSettingInst inst = GetParam();
  // Left-hand side operand shifted by immediate.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    TRACED_FOREACH(Shift, shift, kShiftInstructions) {
      // Only test relevant shifted operands.
      if (shift.mi.machine_type != MachineType::Int32()) continue;

      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      Node* imm = m.Int32Constant(5);
      Node* shifted_operand = (m.*shift.mi.constructor)(m.Parameter(0), imm);
      Node* binop = (m.*inst.mi.constructor)(shifted_operand, m.Parameter(1));
      Node* comp = (m.*cmp.mi.constructor)(binop, m.Int32Constant(0));
      m.Return(comp);
      Stream s = m.Build();
      // Cmn does not support ROR shifts.
      if (inst.no_output_opcode == kArm64Cmn32 &&
          shift.mi.arch_opcode == kArm64Ror32) {
        ASSERT_EQ(2U, s.size());
        continue;
      }
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(5, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(cmp.cond, s[0]->flags_condition());
    }
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorFlagSettingTest,
                        ::testing::ValuesIn(kFlagSettingInstructions));

TEST_F(InstructionSelectorTest, TstInvalidImmediate) {
  // Make sure we do not generate an invalid immediate for TST.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    // 5 is not a valid constant for TST.
    Node* imm = m.Int32Constant(5);
    Node* binop = m.Word32And(imm, m.Parameter(0));
    Node* comp = (m.*cmp.mi.constructor)(binop, m.Int32Constant(0));
    m.Return(comp);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    EXPECT_NE(InstructionOperand::IMMEDIATE, s[0]->InputAt(0)->kind());
    EXPECT_NE(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
  }
}

TEST_F(InstructionSelectorTest, CommuteAddsExtend) {
  // Extended left-hand side operand.
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* extend = m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(24)),
                               m.Int32Constant(24));
    Node* binop = m.Int32Add(extend, m.Parameter(1));
    m.Return((m.*cmp.mi.constructor)(binop, m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.cond, s[0]->flags_condition());
    EXPECT_EQ(kMode_Operand2_R_SXTB, s[0]->addressing_mode());
  }
}

// -----------------------------------------------------------------------------
// Miscellaneous


static const MachInst2 kLogicalWithNotRHSs[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64Bic32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64And, "Word64And", kArm64Bic,
     MachineType::Int64()},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArm64Orn32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64Or, "Word64Or", kArm64Orn,
     MachineType::Int64()},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArm64Eon32,
     MachineType::Int32()},
    {&RawMachineAssembler::Word64Xor, "Word64Xor", kArm64Eon,
     MachineType::Int64()}};


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorLogicalWithNotRHSTest;


TEST_P(InstructionSelectorLogicalWithNotRHSTest, Parameter) {
  const MachInst2 inst = GetParam();
  const MachineType type = inst.machine_type;
  // Test cases where RHS is Xor(x, -1).
  {
    StreamBuilder m(this, type, type, type);
    if (type == MachineType::Int32()) {
      m.Return((m.*inst.constructor)(
          m.Parameter(0), m.Word32Xor(m.Parameter(1), m.Int32Constant(-1))));
    } else {
      ASSERT_EQ(MachineType::Int64(), type);
      m.Return((m.*inst.constructor)(
          m.Parameter(0), m.Word64Xor(m.Parameter(1), m.Int64Constant(-1))));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type);
    if (type == MachineType::Int32()) {
      m.Return((m.*inst.constructor)(
          m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)), m.Parameter(1)));
    } else {
      ASSERT_EQ(MachineType::Int64(), type);
      m.Return((m.*inst.constructor)(
          m.Word64Xor(m.Parameter(0), m.Int64Constant(-1)), m.Parameter(1)));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // Test cases where RHS is Not(x).
  {
    StreamBuilder m(this, type, type, type);
    if (type == MachineType::Int32()) {
      m.Return((m.*inst.constructor)(m.Parameter(0),
                                     m.Word32BitwiseNot(m.Parameter(1))));
    } else {
      ASSERT_EQ(MachineType::Int64(), type);
      m.Return(
          (m.*inst.constructor)(m.Parameter(0), m.Word64Not(m.Parameter(1))));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type);
    if (type == MachineType::Int32()) {
      m.Return((m.*inst.constructor)(m.Word32BitwiseNot(m.Parameter(0)),
                                     m.Parameter(1)));
    } else {
      ASSERT_EQ(MachineType::Int64(), type);
      m.Return(
          (m.*inst.constructor)(m.Word64Not(m.Parameter(0)), m.Parameter(1)));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorLogicalWithNotRHSTest,
                        ::testing::ValuesIn(kLogicalWithNotRHSs));

TEST_F(InstructionSelectorTest, Word32BitwiseNotWithParameter) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
  m.Return(m.Word32BitwiseNot(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Not32, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Word64NotWithParameter) {
  StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
  m.Return(m.Word64Not(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Not, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Word32XorMinusOneWithParameter) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not32, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Xor(m.Int32Constant(-1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not32, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word64XorMinusOneWithParameter) {
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    m.Return(m.Word64Xor(m.Parameter(0), m.Int64Constant(-1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
    m.Return(m.Word64Xor(m.Int64Constant(-1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32ShrWithWord32AndWithImmediate) {
  // The available shift operand range is `0 <= imm < 32`, but we also test
  // that immediates outside this range are handled properly (modulo-32).
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t jnk = rng()->NextInt();
      jnk = (lsb > 0) ? (jnk >> (32 - lsb)) : 0;
      uint32_t msk = ((0xFFFFFFFFu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Shr(m.Word32And(m.Parameter(0), m.Int32Constant(msk)),
                           m.Int32Constant(shift)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t jnk = rng()->NextInt();
      jnk = (lsb > 0) ? (jnk >> (32 - lsb)) : 0;
      uint32_t msk = ((0xFFFFFFFFu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Shr(m.Word32And(m.Int32Constant(msk), m.Parameter(0)),
                           m.Int32Constant(shift)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word64ShrWithWord64AndWithImmediate) {
  // The available shift operand range is `0 <= imm < 64`, but we also test
  // that immediates outside this range are handled properly (modulo-64).
  TRACED_FORRANGE(int32_t, shift, -64, 127) {
    int32_t lsb = shift & 0x3F;
    TRACED_FORRANGE(int32_t, width, 1, 64 - lsb) {
      uint64_t jnk = rng()->NextInt64();
      jnk = (lsb > 0) ? (jnk >> (64 - lsb)) : 0;
      uint64_t msk =
          ((uint64_t{0xFFFFFFFFFFFFFFFF} >> (64 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
      m.Return(m.Word64Shr(m.Word64And(m.Parameter(0), m.Int64Constant(msk)),
                           m.Int64Constant(shift)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, shift, -64, 127) {
    int32_t lsb = shift & 0x3F;
    TRACED_FORRANGE(int32_t, width, 1, 64 - lsb) {
      uint64_t jnk = rng()->NextInt64();
      jnk = (lsb > 0) ? (jnk >> (64 - lsb)) : 0;
      uint64_t msk =
          ((uint64_t{0xFFFFFFFFFFFFFFFF} >> (64 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
      m.Return(m.Word64Shr(m.Word64And(m.Int64Constant(msk), m.Parameter(0)),
                           m.Int64Constant(shift)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word32AndWithImmediateWithWord32Shr) {
  // The available shift operand range is `0 <= imm < 32`, but we also test
  // that immediates outside this range are handled properly (modulo-32).
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 31) {
      uint32_t msk = (1 << width) - 1;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32And(m.Word32Shr(m.Parameter(0), m.Int32Constant(shift)),
                           m.Int32Constant(msk)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      int32_t actual_width = (lsb + width > 32) ? (32 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 31) {
      uint32_t msk = (1 << width) - 1;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(
          m.Word32And(m.Int32Constant(msk),
                      m.Word32Shr(m.Parameter(0), m.Int32Constant(shift))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      int32_t actual_width = (lsb + width > 32) ? (32 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word64AndWithImmediateWithWord64Shr) {
  // The available shift operand range is `0 <= imm < 64`, but we also test
  // that immediates outside this range are handled properly (modulo-64).
  TRACED_FORRANGE(int64_t, shift, -64, 127) {
    int64_t lsb = shift & 0x3F;
    TRACED_FORRANGE(int64_t, width, 1, 63) {
      uint64_t msk = (uint64_t{1} << width) - 1;
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
      m.Return(m.Word64And(m.Word64Shr(m.Parameter(0), m.Int64Constant(shift)),
                           m.Int64Constant(msk)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      int64_t actual_width = (lsb + width > 64) ? (64 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int64_t, shift, -64, 127) {
    int64_t lsb = shift & 0x3F;
    TRACED_FORRANGE(int64_t, width, 1, 63) {
      uint64_t msk = (uint64_t{1} << width) - 1;
      StreamBuilder m(this, MachineType::Int64(), MachineType::Int64());
      m.Return(
          m.Word64And(m.Int64Constant(msk),
                      m.Word64Shr(m.Parameter(0), m.Int64Constant(shift))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      int64_t actual_width = (lsb + width > 64) ? (64 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Int32MulHighWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Int32MulHigh(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArm64Smull, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArm64Asr, s[1]->arch_opcode());
  ASSERT_EQ(2U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(32, s.ToInt64(s[1]->InputAt(1)));
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[1]->Output()));
}


TEST_F(InstructionSelectorTest, Int32MulHighWithSar) {
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const n = m.Word32Sar(m.Int32MulHigh(p0, p1), m.Int32Constant(shift));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Smull, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kArm64Asr, s[1]->arch_opcode());
    ASSERT_EQ(2U, s[1]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
    EXPECT_EQ((shift & 0x1F) + 32, s.ToInt64(s[1]->InputAt(1)));
    ASSERT_EQ(1U, s[1]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[1]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Int32MulHighWithAdd) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const a = m.Int32Add(m.Int32MulHigh(p0, p1), p0);
  // Test only one shift constant here, as we're only interested in it being a
  // 32-bit operation; the shift amount is irrelevant.
  Node* const n = m.Word32Sar(a, m.Int32Constant(1));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArm64Smull, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArm64Add, s[1]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R_ASR_I, s[1]->addressing_mode());
  ASSERT_EQ(3U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(1)));
  EXPECT_EQ(32, s.ToInt64(s[1]->InputAt(2)));
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArm64Asr32, s[2]->arch_opcode());
  ASSERT_EQ(2U, s[2]->InputCount());
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(1, s.ToInt64(s[2]->InputAt(1)));
  ASSERT_EQ(1U, s[2]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[2]->Output()));
}


TEST_F(InstructionSelectorTest, Uint32MulHighWithShr) {
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const n =
        m.Word32Shr(m.Uint32MulHigh(p0, p1), m.Int32Constant(shift));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(kArm64Umull, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kArm64Lsr, s[1]->arch_opcode());
    ASSERT_EQ(2U, s[1]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
    EXPECT_EQ((shift & 0x1F) + 32, s.ToInt64(s[1]->InputAt(1)));
    ASSERT_EQ(1U, s[1]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[1]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word32SarWithWord32Shl) {
  TRACED_FORRANGE(int32_t, shift, 1, 31) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(shift)),
                                m.Int32Constant(shift));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Sbfx32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  TRACED_FORRANGE(int32_t, shift, 1, 31) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(shift + 32)),
                                m.Int32Constant(shift + 64));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Sbfx32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word32ShrWithWord32Shl) {
  TRACED_FORRANGE(int32_t, shift, 1, 31) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32Shr(m.Word32Shl(p0, m.Int32Constant(shift)),
                                m.Int32Constant(shift));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  TRACED_FORRANGE(int32_t, shift, 1, 31) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32Shr(m.Word32Shl(p0, m.Int32Constant(shift + 32)),
                                m.Int32Constant(shift + 64));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word32ShlWithWord32And) {
  TRACED_FORRANGE(int32_t, shift, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r =
        m.Word32Shl(m.Word32And(p0, m.Int32Constant((1 << (31 - shift)) - 1)),
                    m.Int32Constant(shift));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ubfiz32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  TRACED_FORRANGE(int32_t, shift, 0, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r =
        m.Word32Shl(m.Word32And(p0, m.Int32Constant((1 << (31 - shift)) - 1)),
                    m.Int32Constant(shift + 1));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Lsl32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word32Clz) {
  StreamBuilder m(this, MachineType::Uint32(), MachineType::Uint32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Word32Clz(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Clz32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float32Abs) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float32Abs(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Float32Abs, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float64Abs) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float64Abs(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Float64Abs, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float64Max) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Float64Max(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Float64Max, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float64Min) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Float64Min(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Float64Min, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Float32Neg) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  // Don't use m.Float32Neg() as that generates an explicit sub.
  Node* const n = m.AddNode(m.machine()->Float32Neg(), m.Parameter(0));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Float32Neg, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Float64Neg) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  // Don't use m.Float64Neg() as that generates an explicit sub.
  Node* const n = m.AddNode(m.machine()->Float64Neg(), m.Parameter(0));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Float64Neg, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, LoadAndShiftRight) {
  {
    int32_t immediates[] = {-256, -255, -3,   -2,   -1,    0,    1,
                            2,    3,    255,  256,  260,   4096, 4100,
                            8192, 8196, 3276, 3280, 16376, 16380};
    TRACED_FOREACH(int32_t, index, immediates) {
      StreamBuilder m(this, MachineType::Uint64(), MachineType::Pointer());
      Node* const load = m.Load(MachineType::Uint64(), m.Parameter(0),
                                m.Int32Constant(index - 4));
      Node* const sar = m.Word64Sar(load, m.Int32Constant(32));
      // Make sure we don't fold the shift into the following add:
      m.Return(m.Int64Add(sar, m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(2U, s.size());
      EXPECT_EQ(kArm64Ldrsw, s[0]->arch_opcode());
      EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
      EXPECT_EQ(2U, s[0]->InputCount());
      EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
      ASSERT_EQ(1U, s[0]->OutputCount());
    }
  }
}

TEST_F(InstructionSelectorTest, CompareAgainstZero32) {
  TRACED_FOREACH(IntegerCmp, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const param = m.Parameter(0);
    RawMachineLabel a, b;
    m.Branch((m.*cmp.mi.constructor)(param, m.Int32Constant(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(s.ToVreg(param), s.ToVreg(s[0]->InputAt(0)));
    if (cmp.cond == kNegative || cmp.cond == kPositiveOrZero) {
      EXPECT_EQ(kArm64TestAndBranch32, s[0]->arch_opcode());
      EXPECT_EQ(4U, s[0]->InputCount());  // The labels are also inputs.
      EXPECT_EQ((cmp.cond == kNegative) ? kNotEqual : kEqual,
                s[0]->flags_condition());
      EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(31, s.ToInt32(s[0]->InputAt(1)));
    } else {
      EXPECT_EQ(kArm64CompareAndBranch32, s[0]->arch_opcode());
      EXPECT_EQ(3U, s[0]->InputCount());  // The labels are also inputs.
      EXPECT_EQ(cmp.cond, s[0]->flags_condition());
    }
  }
}

TEST_F(InstructionSelectorTest, CompareFloat64HighLessThanZero64) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64());
  Node* const param = m.Parameter(0);
  Node* const high = m.Float64ExtractHighWord32(param);
  RawMachineLabel a, b;
  m.Branch(m.Int32LessThan(high, m.Int32Constant(0)), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArm64U64MoveFloat64, s[0]->arch_opcode());
  EXPECT_EQ(kArm64TestAndBranch, s[1]->arch_opcode());
  EXPECT_EQ(kNotEqual, s[1]->flags_condition());
  EXPECT_EQ(4U, s[1]->InputCount());
  EXPECT_EQ(InstructionOperand::IMMEDIATE, s[1]->InputAt(1)->kind());
  EXPECT_EQ(63, s.ToInt32(s[1]->InputAt(1)));
}

TEST_F(InstructionSelectorTest, CompareFloat64HighGreaterThanOrEqualZero64) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64());
  Node* const param = m.Parameter(0);
  Node* const high = m.Float64ExtractHighWord32(param);
  RawMachineLabel a, b;
  m.Branch(m.Int32GreaterThanOrEqual(high, m.Int32Constant(0)), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArm64U64MoveFloat64, s[0]->arch_opcode());
  EXPECT_EQ(kArm64TestAndBranch, s[1]->arch_opcode());
  EXPECT_EQ(kEqual, s[1]->flags_condition());
  EXPECT_EQ(4U, s[1]->InputCount());
  EXPECT_EQ(InstructionOperand::IMMEDIATE, s[1]->InputAt(1)->kind());
  EXPECT_EQ(63, s.ToInt32(s[1]->InputAt(1)));
}

TEST_F(InstructionSelectorTest, SpeculationFence) {
  StreamBuilder m(this, MachineType::Int32());
  m.SpeculationFence();
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64DsbIsb, s[0]->arch_opcode());
}

TEST_F(InstructionSelectorTest, StackCheck0) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
  Node* const sp = m.LoadStackPointer();
  Node* const stack_limit = m.Load(MachineType::Int64(), m.Parameter(0));
  Node* const interrupt = m.UintPtrLessThan(sp, stack_limit);

  RawMachineLabel if_true, if_false;
  m.Branch(interrupt, &if_true, &if_false);

  m.Bind(&if_true);
  m.Return(m.Int32Constant(1));

  m.Bind(&if_false);
  m.Return(m.Int32Constant(0));

  Stream s = m.Build();

  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArm64Ldr, s[0]->arch_opcode());
  EXPECT_EQ(kArm64Cmp, s[1]->arch_opcode());
  EXPECT_EQ(4U, s[1]->InputCount());
  EXPECT_EQ(0U, s[1]->OutputCount());
}

TEST_F(InstructionSelectorTest, StackCheck1) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
  Node* const sp = m.LoadStackPointer();
  Node* const stack_limit = m.Load(MachineType::Int64(), m.Parameter(0));
  Node* const sp_within_limit = m.UintPtrLessThan(stack_limit, sp);

  RawMachineLabel if_true, if_false;
  m.Branch(sp_within_limit, &if_true, &if_false);

  m.Bind(&if_true);
  m.Return(m.Int32Constant(1));

  m.Bind(&if_false);
  m.Return(m.Int32Constant(0));

  Stream s = m.Build();

  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArm64Ldr, s[0]->arch_opcode());
  EXPECT_EQ(kArm64Cmp, s[1]->arch_opcode());
  EXPECT_EQ(4U, s[1]->InputCount());
  EXPECT_EQ(0U, s[1]->OutputCount());
}

TEST_F(InstructionSelectorTest, ExternalReferenceLoad1) {
  // Test offsets we can use kMode_Root for.
  const int64_t kOffsets[] = {0, 1, 4, INT32_MIN, INT32_MAX};
  TRACED_FOREACH(int64_t, offset, kOffsets) {
    StreamBuilder m(this, MachineType::Int64());
    ExternalReference reference = bit_cast<ExternalReference>(
        reinterpret_cast<intptr_t>(isolate()->heap()->roots_array_start()) +
        offset + kRootRegisterBias);
    Node* const value =
        m.Load(MachineType::Int64(), m.ExternalConstant(reference));
    m.Return(value);

    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Ldr, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Root, s[0]->addressing_mode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(s.ToInt64(s[0]->InputAt(0)), offset);
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

TEST_F(InstructionSelectorTest, ExternalReferenceLoad2) {
  // Offset too large, we cannot use kMode_Root.
  StreamBuilder m(this, MachineType::Int64());
  int64_t offset = 0x100000000;
  ExternalReference reference = bit_cast<ExternalReference>(
      reinterpret_cast<intptr_t>(isolate()->heap()->roots_array_start()) +
      offset + kRootRegisterBias);
  Node* const value =
      m.Load(MachineType::Int64(), m.ExternalConstant(reference));
  m.Return(value);

  Stream s = m.Build();

  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Ldr, s[0]->arch_opcode());
  EXPECT_NE(kMode_Root, s[0]->addressing_mode());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
