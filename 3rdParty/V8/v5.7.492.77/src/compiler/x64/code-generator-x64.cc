// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include <limits>

#include "src/compilation-info.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/wasm/wasm-module.h"
#include "src/x64/assembler-x64.h"
#include "src/x64/macro-assembler-x64.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->

// Adds X64 specific methods for decoding operands.
class X64OperandConverter : public InstructionOperandConverter {
 public:
  X64OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  Immediate InputImmediate(size_t index) {
    return ToImmediate(instr_->InputAt(index));
  }

  Operand InputOperand(size_t index, int extra = 0) {
    return ToOperand(instr_->InputAt(index), extra);
  }

  Operand OutputOperand() { return ToOperand(instr_->Output()); }

  Immediate ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    if (constant.type() == Constant::kFloat64) {
      DCHECK_EQ(0, bit_cast<int64_t>(constant.ToFloat64()));
      return Immediate(0);
    }
    if (RelocInfo::IsWasmReference(constant.rmode())) {
      return Immediate(constant.ToInt32(), constant.rmode());
    }
    return Immediate(constant.ToInt32());
  }

  Operand ToOperand(InstructionOperand* op, int extra = 0) {
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToOperand(AllocatedOperand::cast(op)->index(), extra);
  }

  Operand SlotToOperand(int slot_index, int extra = 0) {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot_index);
    return Operand(offset.from_stack_pointer() ? rsp : rbp,
                   offset.offset() + extra);
  }

  static size_t NextOffset(size_t* offset) {
    size_t i = *offset;
    (*offset)++;
    return i;
  }

  static ScaleFactor ScaleFor(AddressingMode one, AddressingMode mode) {
    STATIC_ASSERT(0 == static_cast<int>(times_1));
    STATIC_ASSERT(1 == static_cast<int>(times_2));
    STATIC_ASSERT(2 == static_cast<int>(times_4));
    STATIC_ASSERT(3 == static_cast<int>(times_8));
    int scale = static_cast<int>(mode - one);
    DCHECK(scale >= 0 && scale < 4);
    return static_cast<ScaleFactor>(scale);
  }

  Operand MemoryOperand(size_t* offset) {
    AddressingMode mode = AddressingModeField::decode(instr_->opcode());
    switch (mode) {
      case kMode_MR: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = 0;
        return Operand(base, disp);
      }
      case kMode_MRI: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, disp);
      }
      case kMode_MR1:
      case kMode_MR2:
      case kMode_MR4:
      case kMode_MR8: {
        Register base = InputRegister(NextOffset(offset));
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_MR1, mode);
        int32_t disp = 0;
        return Operand(base, index, scale, disp);
      }
      case kMode_MR1I:
      case kMode_MR2I:
      case kMode_MR4I:
      case kMode_MR8I: {
        Register base = InputRegister(NextOffset(offset));
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_MR1I, mode);
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, index, scale, disp);
      }
      case kMode_M1: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = 0;
        return Operand(base, disp);
      }
      case kMode_M2:
        UNREACHABLE();  // Should use kModeMR with more compact encoding instead
        return Operand(no_reg, 0);
      case kMode_M4:
      case kMode_M8: {
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_M1, mode);
        int32_t disp = 0;
        return Operand(index, scale, disp);
      }
      case kMode_M1I:
      case kMode_M2I:
      case kMode_M4I:
      case kMode_M8I: {
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_M1I, mode);
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(index, scale, disp);
      }
      case kMode_Root: {
        Register base = kRootRegister;
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, disp);
      }
      case kMode_None:
        UNREACHABLE();
        return Operand(no_reg, 0);
    }
    UNREACHABLE();
    return Operand(no_reg, 0);
  }

  Operand MemoryOperand(size_t first_input = 0) {
    return MemoryOperand(&first_input);
  }
};


namespace {

bool HasImmediateInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsImmediate();
}


class OutOfLineLoadZero final : public OutOfLineCode {
 public:
  OutOfLineLoadZero(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final { __ xorl(result_, result_); }

 private:
  Register const result_;
};

class OutOfLineLoadFloat32NaN final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat32NaN(CodeGenerator* gen, XMMRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Xorps(result_, result_);
    __ Divss(result_, result_);
  }

 private:
  XMMRegister const result_;
};

class OutOfLineLoadFloat64NaN final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat64NaN(CodeGenerator* gen, XMMRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Xorpd(result_, result_);
    __ Divsd(result_, result_);
  }

 private:
  XMMRegister const result_;
};

class OutOfLineTruncateDoubleToI final : public OutOfLineCode {
 public:
  OutOfLineTruncateDoubleToI(CodeGenerator* gen, Register result,
                             XMMRegister input,
                             UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        result_(result),
        input_(input),
        unwinding_info_writer_(unwinding_info_writer) {}

  void Generate() final {
    __ subp(rsp, Immediate(kDoubleSize));
    unwinding_info_writer_->MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                      kDoubleSize);
    __ Movsd(MemOperand(rsp, 0), input_);
    __ SlowTruncateToI(result_, rsp, 0);
    __ addp(rsp, Immediate(kDoubleSize));
    unwinding_info_writer_->MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                      -kDoubleSize);
  }

 private:
  Register const result_;
  XMMRegister const input_;
  UnwindingInfoWriter* const unwinding_info_writer_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Operand operand,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        operand_(operand),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, zero,
                     exit());
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    __ leap(scratch1_, operand_);
    __ CallStub(&stub);
  }

 private:
  Register const object_;
  Operand const operand_;
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
};

class WasmOutOfLineTrap final : public OutOfLineCode {
 public:
  WasmOutOfLineTrap(CodeGenerator* gen, int pc, bool frame_elided,
                    int32_t position, Instruction* instr)
      : OutOfLineCode(gen),
        gen_(gen),
        pc_(pc),
        frame_elided_(frame_elided),
        position_(position),
        instr_(instr) {}

  // TODO(eholk): Refactor this method to take the code generator as a
  // parameter.
  void Generate() final {
    int current_pc = __ pc_offset();

    gen_->AddProtectedInstruction(pc_, current_pc);

    if (frame_elided_) {
      __ EnterFrame(StackFrame::WASM_COMPILED);
    }

    wasm::TrapReason trap_id = wasm::kTrapMemOutOfBounds;
    int trap_reason = wasm::WasmOpcodes::TrapReasonToMessageId(trap_id);
    __ Push(Smi::FromInt(trap_reason));
    __ Push(Smi::FromInt(position_));
    __ Move(rsi, gen_->isolate()->native_context());
    __ CallRuntime(Runtime::kThrowWasmError);

    if (instr_->reference_map() != nullptr) {
      gen_->RecordSafepoint(instr_->reference_map(), Safepoint::kSimple, 0,
                            Safepoint::kNoLazyDeopt);
    }
  }

 private:
  CodeGenerator* gen_;
  int pc_;
  bool frame_elided_;
  int32_t position_;
  Instruction* instr_;
};

void EmitOOLTrapIfNeeded(Zone* zone, CodeGenerator* codegen,
                         InstructionCode opcode, size_t input_count,
                         X64OperandConverter& i, int pc, Instruction* instr) {
  const X64MemoryProtection protection =
      static_cast<X64MemoryProtection>(MiscField::decode(opcode));
  if (protection == X64MemoryProtection::kProtected) {
    const bool frame_elided = !codegen->frame_access_state()->has_frame();
    const int32_t position = i.InputInt32(input_count - 1);
    new (zone) WasmOutOfLineTrap(codegen, pc, frame_elided, position, instr);
  }
}
}  // namespace


#define ASSEMBLE_UNOP(asm_instr)         \
  do {                                   \
    if (instr->Output()->IsRegister()) { \
      __ asm_instr(i.OutputRegister());  \
    } else {                             \
      __ asm_instr(i.OutputOperand());   \
    }                                    \
  } while (0)


#define ASSEMBLE_BINOP(asm_instr)                              \
  do {                                                         \
    if (HasImmediateInput(instr, 1)) {                         \
      if (instr->InputAt(0)->IsRegister()) {                   \
        __ asm_instr(i.InputRegister(0), i.InputImmediate(1)); \
      } else {                                                 \
        __ asm_instr(i.InputOperand(0), i.InputImmediate(1));  \
      }                                                        \
    } else {                                                   \
      if (instr->InputAt(1)->IsRegister()) {                   \
        __ asm_instr(i.InputRegister(0), i.InputRegister(1));  \
      } else {                                                 \
        __ asm_instr(i.InputRegister(0), i.InputOperand(1));   \
      }                                                        \
    }                                                          \
  } while (0)

#define ASSEMBLE_COMPARE(asm_instr)                                   \
  do {                                                                \
    if (AddressingModeField::decode(instr->opcode()) != kMode_None) { \
      size_t index = 0;                                               \
      Operand left = i.MemoryOperand(&index);                         \
      if (HasImmediateInput(instr, index)) {                          \
        __ asm_instr(left, i.InputImmediate(index));                  \
      } else {                                                        \
        __ asm_instr(left, i.InputRegister(index));                   \
      }                                                               \
    } else {                                                          \
      if (HasImmediateInput(instr, 1)) {                              \
        if (instr->InputAt(0)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputImmediate(1));      \
        } else {                                                      \
          __ asm_instr(i.InputOperand(0), i.InputImmediate(1));       \
        }                                                             \
      } else {                                                        \
        if (instr->InputAt(1)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputRegister(1));       \
        } else {                                                      \
          __ asm_instr(i.InputRegister(0), i.InputOperand(1));        \
        }                                                             \
      }                                                               \
    }                                                                 \
  } while (0)

#define ASSEMBLE_MULT(asm_instr)                              \
  do {                                                        \
    if (HasImmediateInput(instr, 1)) {                        \
      if (instr->InputAt(0)->IsRegister()) {                  \
        __ asm_instr(i.OutputRegister(), i.InputRegister(0),  \
                     i.InputImmediate(1));                    \
      } else {                                                \
        __ asm_instr(i.OutputRegister(), i.InputOperand(0),   \
                     i.InputImmediate(1));                    \
      }                                                       \
    } else {                                                  \
      if (instr->InputAt(1)->IsRegister()) {                  \
        __ asm_instr(i.OutputRegister(), i.InputRegister(1)); \
      } else {                                                \
        __ asm_instr(i.OutputRegister(), i.InputOperand(1));  \
      }                                                       \
    }                                                         \
  } while (0)


#define ASSEMBLE_SHIFT(asm_instr, width)                                   \
  do {                                                                     \
    if (HasImmediateInput(instr, 1)) {                                     \
      if (instr->Output()->IsRegister()) {                                 \
        __ asm_instr(i.OutputRegister(), Immediate(i.InputInt##width(1))); \
      } else {                                                             \
        __ asm_instr(i.OutputOperand(), Immediate(i.InputInt##width(1)));  \
      }                                                                    \
    } else {                                                               \
      if (instr->Output()->IsRegister()) {                                 \
        __ asm_instr##_cl(i.OutputRegister());                             \
      } else {                                                             \
        __ asm_instr##_cl(i.OutputOperand());                              \
      }                                                                    \
    }                                                                      \
  } while (0)


#define ASSEMBLE_MOVX(asm_instr)                            \
  do {                                                      \
    if (instr->addressing_mode() != kMode_None) {           \
      __ asm_instr(i.OutputRegister(), i.MemoryOperand());  \
    } else if (instr->InputAt(0)->IsRegister()) {           \
      __ asm_instr(i.OutputRegister(), i.InputRegister(0)); \
    } else {                                                \
      __ asm_instr(i.OutputRegister(), i.InputOperand(0));  \
    }                                                       \
  } while (0)

#define ASSEMBLE_SSE_BINOP(asm_instr)                                   \
  do {                                                                  \
    if (instr->InputAt(1)->IsFPRegister()) {                            \
      __ asm_instr(i.InputDoubleRegister(0), i.InputDoubleRegister(1)); \
    } else {                                                            \
      __ asm_instr(i.InputDoubleRegister(0), i.InputOperand(1));        \
    }                                                                   \
  } while (0)

#define ASSEMBLE_SSE_UNOP(asm_instr)                                    \
  do {                                                                  \
    if (instr->InputAt(0)->IsFPRegister()) {                            \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0)); \
    } else {                                                            \
      __ asm_instr(i.OutputDoubleRegister(), i.InputOperand(0));        \
    }                                                                   \
  } while (0)

#define ASSEMBLE_AVX_BINOP(asm_instr)                                  \
  do {                                                                 \
    CpuFeatureScope avx_scope(masm(), AVX);                            \
    if (instr->InputAt(1)->IsFPRegister()) {                           \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                   i.InputDoubleRegister(1));                          \
    } else {                                                           \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                   i.InputOperand(1));                                 \
    }                                                                  \
  } while (0)

#define ASSEMBLE_CHECKED_LOAD_FLOAT(asm_instr, OutOfLineLoadNaN)             \
  do {                                                                       \
    auto result = i.OutputDoubleRegister();                                  \
    auto buffer = i.InputRegister(0);                                        \
    auto index1 = i.InputRegister(1);                                        \
    auto index2 = i.InputUint32(2);                                          \
    OutOfLineCode* ool;                                                      \
    if (instr->InputAt(3)->IsRegister()) {                                   \
      auto length = i.InputRegister(3);                                      \
      DCHECK_EQ(0u, index2);                                                 \
      __ cmpl(index1, length);                                               \
      ool = new (zone()) OutOfLineLoadNaN(this, result);                     \
    } else {                                                                 \
      auto length = i.InputUint32(3);                                        \
      RelocInfo::Mode rmode = i.ToConstant(instr->InputAt(3)).rmode();       \
      DCHECK_LE(index2, length);                                             \
      __ cmpl(index1, Immediate(length - index2, rmode));                    \
      class OutOfLineLoadFloat final : public OutOfLineCode {                \
       public:                                                               \
        OutOfLineLoadFloat(CodeGenerator* gen, XMMRegister result,           \
                           Register buffer, Register index1, int32_t index2, \
                           int32_t length, RelocInfo::Mode rmode)            \
            : OutOfLineCode(gen),                                            \
              result_(result),                                               \
              buffer_(buffer),                                               \
              index1_(index1),                                               \
              index2_(index2),                                               \
              length_(length),                                               \
              rmode_(rmode) {}                                               \
                                                                             \
        void Generate() final {                                              \
          __ leal(kScratchRegister, Operand(index1_, index2_));              \
          __ Pcmpeqd(result_, result_);                                      \
          __ cmpl(kScratchRegister, Immediate(length_, rmode_));             \
          __ j(above_equal, exit());                                         \
          __ asm_instr(result_,                                              \
                       Operand(buffer_, kScratchRegister, times_1, 0));      \
        }                                                                    \
                                                                             \
       private:                                                              \
        XMMRegister const result_;                                           \
        Register const buffer_;                                              \
        Register const index1_;                                              \
        int32_t const index2_;                                               \
        int32_t const length_;                                               \
        RelocInfo::Mode rmode_;                                              \
      };                                                                     \
      ool = new (zone()) OutOfLineLoadFloat(this, result, buffer, index1,    \
                                            index2, length, rmode);          \
    }                                                                        \
    __ j(above_equal, ool->entry());                                         \
    __ asm_instr(result, Operand(buffer, index1, times_1, index2));          \
    __ bind(ool->exit());                                                    \
  } while (false)

#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr)                               \
  do {                                                                         \
    auto result = i.OutputRegister();                                          \
    auto buffer = i.InputRegister(0);                                          \
    auto index1 = i.InputRegister(1);                                          \
    auto index2 = i.InputUint32(2);                                            \
    OutOfLineCode* ool;                                                        \
    if (instr->InputAt(3)->IsRegister()) {                                     \
      auto length = i.InputRegister(3);                                        \
      DCHECK_EQ(0u, index2);                                                   \
      __ cmpl(index1, length);                                                 \
      ool = new (zone()) OutOfLineLoadZero(this, result);                      \
    } else {                                                                   \
      auto length = i.InputUint32(3);                                          \
      RelocInfo::Mode rmode = i.ToConstant(instr->InputAt(3)).rmode();         \
      DCHECK_LE(index2, length);                                               \
      __ cmpl(index1, Immediate(length - index2, rmode));                      \
      class OutOfLineLoadInteger final : public OutOfLineCode {                \
       public:                                                                 \
        OutOfLineLoadInteger(CodeGenerator* gen, Register result,              \
                             Register buffer, Register index1, int32_t index2, \
                             int32_t length, RelocInfo::Mode rmode)            \
            : OutOfLineCode(gen),                                              \
              result_(result),                                                 \
              buffer_(buffer),                                                 \
              index1_(index1),                                                 \
              index2_(index2),                                                 \
              length_(length),                                                 \
              rmode_(rmode) {}                                                 \
                                                                               \
        void Generate() final {                                                \
          Label oob;                                                           \
          __ leal(kScratchRegister, Operand(index1_, index2_));                \
          __ cmpl(kScratchRegister, Immediate(length_, rmode_));               \
          __ j(above_equal, &oob, Label::kNear);                               \
          __ asm_instr(result_,                                                \
                       Operand(buffer_, kScratchRegister, times_1, 0));        \
          __ jmp(exit());                                                      \
          __ bind(&oob);                                                       \
          __ xorl(result_, result_);                                           \
        }                                                                      \
                                                                               \
       private:                                                                \
        Register const result_;                                                \
        Register const buffer_;                                                \
        Register const index1_;                                                \
        int32_t const index2_;                                                 \
        int32_t const length_;                                                 \
        RelocInfo::Mode const rmode_;                                          \
      };                                                                       \
      ool = new (zone()) OutOfLineLoadInteger(this, result, buffer, index1,    \
                                              index2, length, rmode);          \
    }                                                                          \
    __ j(above_equal, ool->entry());                                           \
    __ asm_instr(result, Operand(buffer, index1, times_1, index2));            \
    __ bind(ool->exit());                                                      \
  } while (false)

#define ASSEMBLE_CHECKED_STORE_FLOAT(asm_instr)                              \
  do {                                                                       \
    auto buffer = i.InputRegister(0);                                        \
    auto index1 = i.InputRegister(1);                                        \
    auto index2 = i.InputUint32(2);                                          \
    auto value = i.InputDoubleRegister(4);                                   \
    if (instr->InputAt(3)->IsRegister()) {                                   \
      auto length = i.InputRegister(3);                                      \
      DCHECK_EQ(0u, index2);                                                 \
      Label done;                                                            \
      __ cmpl(index1, length);                                               \
      __ j(above_equal, &done, Label::kNear);                                \
      __ asm_instr(Operand(buffer, index1, times_1, index2), value);         \
      __ bind(&done);                                                        \
    } else {                                                                 \
      auto length = i.InputUint32(3);                                        \
      RelocInfo::Mode rmode = i.ToConstant(instr->InputAt(3)).rmode();       \
      DCHECK_LE(index2, length);                                             \
      __ cmpl(index1, Immediate(length - index2, rmode));                    \
      class OutOfLineStoreFloat final : public OutOfLineCode {               \
       public:                                                               \
        OutOfLineStoreFloat(CodeGenerator* gen, Register buffer,             \
                            Register index1, int32_t index2, int32_t length, \
                            XMMRegister value, RelocInfo::Mode rmode)        \
            : OutOfLineCode(gen),                                            \
              buffer_(buffer),                                               \
              index1_(index1),                                               \
              index2_(index2),                                               \
              length_(length),                                               \
              value_(value),                                                 \
              rmode_(rmode) {}                                               \
                                                                             \
        void Generate() final {                                              \
          __ leal(kScratchRegister, Operand(index1_, index2_));              \
          __ cmpl(kScratchRegister, Immediate(length_, rmode_));             \
          __ j(above_equal, exit());                                         \
          __ asm_instr(Operand(buffer_, kScratchRegister, times_1, 0),       \
                       value_);                                              \
        }                                                                    \
                                                                             \
       private:                                                              \
        Register const buffer_;                                              \
        Register const index1_;                                              \
        int32_t const index2_;                                               \
        int32_t const length_;                                               \
        XMMRegister const value_;                                            \
        RelocInfo::Mode rmode_;                                              \
      };                                                                     \
      auto ool = new (zone()) OutOfLineStoreFloat(                           \
          this, buffer, index1, index2, length, value, rmode);               \
      __ j(above_equal, ool->entry());                                       \
      __ asm_instr(Operand(buffer, index1, times_1, index2), value);         \
      __ bind(ool->exit());                                                  \
    }                                                                        \
  } while (false)

#define ASSEMBLE_CHECKED_STORE_INTEGER_IMPL(asm_instr, Value)                  \
  do {                                                                         \
    auto buffer = i.InputRegister(0);                                          \
    auto index1 = i.InputRegister(1);                                          \
    auto index2 = i.InputUint32(2);                                            \
    if (instr->InputAt(3)->IsRegister()) {                                     \
      auto length = i.InputRegister(3);                                        \
      DCHECK_EQ(0u, index2);                                                   \
      Label done;                                                              \
      __ cmpl(index1, length);                                                 \
      __ j(above_equal, &done, Label::kNear);                                  \
      __ asm_instr(Operand(buffer, index1, times_1, index2), value);           \
      __ bind(&done);                                                          \
    } else {                                                                   \
      auto length = i.InputUint32(3);                                          \
      RelocInfo::Mode rmode = i.ToConstant(instr->InputAt(3)).rmode();         \
      DCHECK_LE(index2, length);                                               \
      __ cmpl(index1, Immediate(length - index2, rmode));                      \
      class OutOfLineStoreInteger final : public OutOfLineCode {               \
       public:                                                                 \
        OutOfLineStoreInteger(CodeGenerator* gen, Register buffer,             \
                              Register index1, int32_t index2, int32_t length, \
                              Value value, RelocInfo::Mode rmode)              \
            : OutOfLineCode(gen),                                              \
              buffer_(buffer),                                                 \
              index1_(index1),                                                 \
              index2_(index2),                                                 \
              length_(length),                                                 \
              value_(value),                                                   \
              rmode_(rmode) {}                                                 \
                                                                               \
        void Generate() final {                                                \
          __ leal(kScratchRegister, Operand(index1_, index2_));                \
          __ cmpl(kScratchRegister, Immediate(length_, rmode_));               \
          __ j(above_equal, exit());                                           \
          __ asm_instr(Operand(buffer_, kScratchRegister, times_1, 0),         \
                       value_);                                                \
        }                                                                      \
                                                                               \
       private:                                                                \
        Register const buffer_;                                                \
        Register const index1_;                                                \
        int32_t const index2_;                                                 \
        int32_t const length_;                                                 \
        Value const value_;                                                    \
        RelocInfo::Mode rmode_;                                                \
      };                                                                       \
      auto ool = new (zone()) OutOfLineStoreInteger(                           \
          this, buffer, index1, index2, length, value, rmode);                 \
      __ j(above_equal, ool->entry());                                         \
      __ asm_instr(Operand(buffer, index1, times_1, index2), value);           \
      __ bind(ool->exit());                                                    \
    }                                                                          \
  } while (false)

#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr)                \
  do {                                                           \
    if (instr->InputAt(4)->IsRegister()) {                       \
      Register value = i.InputRegister(4);                       \
      ASSEMBLE_CHECKED_STORE_INTEGER_IMPL(asm_instr, Register);  \
    } else {                                                     \
      Immediate value = i.InputImmediate(4);                     \
      ASSEMBLE_CHECKED_STORE_INTEGER_IMPL(asm_instr, Immediate); \
    }                                                            \
  } while (false)

#define ASSEMBLE_IEEE754_BINOP(name)                                          \
  do {                                                                        \
    __ PrepareCallCFunction(2);                                               \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     2);                                                      \
  } while (false)

#define ASSEMBLE_IEEE754_UNOP(name)                                           \
  do {                                                                        \
    __ PrepareCallCFunction(1);                                               \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     1);                                                      \
  } while (false)

void CodeGenerator::AssembleDeconstructFrame() {
  unwinding_info_writer_.MarkFrameDeconstructed(__ pc_offset());
  __ movq(rsp, rbp);
  __ popq(rbp);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ movq(rbp, MemOperand(rbp, 0));
  }
  frame_access_state()->SetFrameAccessToSP();
}

void CodeGenerator::AssemblePopArgumentsAdaptorFrame(Register args_reg,
                                                     Register scratch1,
                                                     Register scratch2,
                                                     Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Label done;

  // Check if current frame is an arguments adaptor frame.
  __ Cmp(Operand(rbp, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &done, Label::kNear);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ SmiToInteger32(
      caller_args_count_reg,
      Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3, ReturnAddressState::kOnStack);
  __ bind(&done);
}

namespace {

void AdjustStackPointerForTailCall(MacroAssembler* masm,
                                   FrameAccessState* state,
                                   int new_slot_above_sp,
                                   bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  if (stack_slot_delta > 0) {
    masm->subq(rsp, Immediate(stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    masm->addq(rsp, Immediate(-stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_stack_slot) {
  CodeGenerator::PushTypeFlags flags(kImmediatePush | kScalarPush);
  ZoneVector<MoveOperands*> pushes(zone());
  GetPushCompatibleMoves(instr, flags, &pushes);

  if (!pushes.empty() &&
      (LocationOperand::cast(pushes.back()->destination()).index() + 1 ==
       first_unused_stack_slot)) {
    X64OperandConverter g(this, instr);
    for (auto move : pushes) {
      LocationOperand destination_location(
          LocationOperand::cast(move->destination()));
      InstructionOperand source(move->source());
      AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                    destination_location.index());
      if (source.IsStackSlot()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ Push(g.SlotToOperand(source_location.index()));
      } else if (source.IsRegister()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ Push(source_location.GetRegister());
      } else if (source.IsImmediate()) {
        __ Push(Immediate(ImmediateOperand::cast(source).inline_value()));
      } else {
        // Pushes of non-scalar data types is not supported.
        UNIMPLEMENTED();
      }
      frame_access_state()->IncreaseSPDelta(1);
      move->Eliminate();
    }
  }
  AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                first_unused_stack_slot, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_stack_slot) {
  AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                first_unused_stack_slot);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  X64OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ Call(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        __ addp(reg, Immediate(Code::kHeaderSize - kHeapObjectTag));
        __ call(reg);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallCodeObjectFromJSFunction:
    case kArchTailCallCodeObject: {
      if (arch_opcode == kArchTailCallCodeObjectFromJSFunction) {
        AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                         i.TempRegister(0), i.TempRegister(1),
                                         i.TempRegister(2));
      }
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ jmp(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        __ addp(reg, Immediate(Code::kHeaderSize - kHeapObjectTag));
        __ jmp(reg);
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!HasImmediateInput(instr, 0));
      Register reg = i.InputRegister(0);
      __ jmp(reg);
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmpp(rsi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, kWrongFunctionContext);
      }
      __ Call(FieldOperand(func, JSFunction::kCodeEntryOffset));
      frame_access_state()->ClearSPDelta();
      RecordCallPosition(instr);
      break;
    }
    case kArchTailCallJSFunctionFromJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmpp(rsi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, kWrongFunctionContext);
      }
      AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                       i.TempRegister(0), i.TempRegister(1),
                                       i.TempRegister(2));
      __ jmp(FieldOperand(func, JSFunction::kCodeEntryOffset));
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchPrepareCallCFunction: {
      // Frame alignment requires using FP-relative frame addressing.
      frame_access_state()->SetFrameAccessToFP();
      int const num_parameters = MiscField::decode(instr->opcode());
      __ PrepareCallCFunction(num_parameters);
      break;
    }
    case kArchPrepareTailCall:
      AssemblePrepareTailCall();
      break;
    case kArchCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_parameters);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters);
      }
      frame_access_state()->SetFrameAccessToDefault();
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchComment: {
      Address comment_string = i.InputExternalReference(0).address();
      __ RecordComment(reinterpret_cast<const char*>(comment_string));
      break;
    }
    case kArchDebugBreak:
      __ int3();
      break;
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      Deoptimizer::BailoutType bailout_type =
          Deoptimizer::BailoutType(MiscField::decode(instr->opcode()));
      CodeGenResult result = AssembleDeoptimizerCall(
          deopt_state_id, bailout_type, current_source_position_);
      if (result != kSuccess) return result;
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchStackPointer:
      __ movq(i.OutputRegister(), rsp);
      break;
    case kArchFramePointer:
      __ movq(i.OutputRegister(), rbp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ movq(i.OutputRegister(), Operand(rbp, 0));
      } else {
        __ movq(i.OutputRegister(), rbp);
      }
      break;
    case kArchTruncateDoubleToI: {
      auto result = i.OutputRegister();
      auto input = i.InputDoubleRegister(0);
      auto ool = new (zone()) OutOfLineTruncateDoubleToI(
          this, result, input, &unwinding_info_writer_);
      // We use Cvttsd2siq instead of Cvttsd2si due to performance reasons. The
      // use of Cvttsd2siq requires the movl below to avoid sign extension.
      __ Cvttsd2siq(result, input);
      __ cmpq(result, Immediate(1));
      __ j(overflow, ool->entry());
      __ bind(ool->exit());
      __ movl(result, result);
      break;
    }
    case kArchStoreWithWriteBarrier: {
      RecordWriteMode mode =
          static_cast<RecordWriteMode>(MiscField::decode(instr->opcode()));
      Register object = i.InputRegister(0);
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      Register value = i.InputRegister(index);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = new (zone()) OutOfLineRecordWrite(this, object, operand, value,
                                                   scratch0, scratch1, mode);
      __ movp(operand, value);
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask,
                       not_zero, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base;
      if (offset.from_stack_pointer()) {
        base = rsp;
      } else {
        base = rbp;
      }
      __ leaq(i.OutputRegister(), Operand(base, offset.offset()));
      break;
    }
    case kIeee754Float64Acos:
      ASSEMBLE_IEEE754_UNOP(acos);
      break;
    case kIeee754Float64Acosh:
      ASSEMBLE_IEEE754_UNOP(acosh);
      break;
    case kIeee754Float64Asin:
      ASSEMBLE_IEEE754_UNOP(asin);
      break;
    case kIeee754Float64Asinh:
      ASSEMBLE_IEEE754_UNOP(asinh);
      break;
    case kIeee754Float64Atan:
      ASSEMBLE_IEEE754_UNOP(atan);
      break;
    case kIeee754Float64Atanh:
      ASSEMBLE_IEEE754_UNOP(atanh);
      break;
    case kIeee754Float64Atan2:
      ASSEMBLE_IEEE754_BINOP(atan2);
      break;
    case kIeee754Float64Cbrt:
      ASSEMBLE_IEEE754_UNOP(cbrt);
      break;
    case kIeee754Float64Cos:
      ASSEMBLE_IEEE754_UNOP(cos);
      break;
    case kIeee754Float64Cosh:
      ASSEMBLE_IEEE754_UNOP(cosh);
      break;
    case kIeee754Float64Exp:
      ASSEMBLE_IEEE754_UNOP(exp);
      break;
    case kIeee754Float64Expm1:
      ASSEMBLE_IEEE754_UNOP(expm1);
      break;
    case kIeee754Float64Log:
      ASSEMBLE_IEEE754_UNOP(log);
      break;
    case kIeee754Float64Log1p:
      ASSEMBLE_IEEE754_UNOP(log1p);
      break;
    case kIeee754Float64Log2:
      ASSEMBLE_IEEE754_UNOP(log2);
      break;
    case kIeee754Float64Log10:
      ASSEMBLE_IEEE754_UNOP(log10);
      break;
    case kIeee754Float64Pow: {
      // TODO(bmeurer): Improve integration of the stub.
      __ Movsd(xmm2, xmm0);
      MathPowStub stub(isolate(), MathPowStub::DOUBLE);
      __ CallStub(&stub);
      __ Movsd(xmm0, xmm3);
      break;
    }
    case kIeee754Float64Sin:
      ASSEMBLE_IEEE754_UNOP(sin);
      break;
    case kIeee754Float64Sinh:
      ASSEMBLE_IEEE754_UNOP(sinh);
      break;
    case kIeee754Float64Tan:
      ASSEMBLE_IEEE754_UNOP(tan);
      break;
    case kIeee754Float64Tanh:
      ASSEMBLE_IEEE754_UNOP(tanh);
      break;
    case kX64Add32:
      ASSEMBLE_BINOP(addl);
      break;
    case kX64Add:
      ASSEMBLE_BINOP(addq);
      break;
    case kX64Sub32:
      ASSEMBLE_BINOP(subl);
      break;
    case kX64Sub:
      ASSEMBLE_BINOP(subq);
      break;
    case kX64And32:
      ASSEMBLE_BINOP(andl);
      break;
    case kX64And:
      ASSEMBLE_BINOP(andq);
      break;
    case kX64Cmp8:
      ASSEMBLE_COMPARE(cmpb);
      break;
    case kX64Cmp16:
      ASSEMBLE_COMPARE(cmpw);
      break;
    case kX64Cmp32:
      ASSEMBLE_COMPARE(cmpl);
      break;
    case kX64Cmp:
      ASSEMBLE_COMPARE(cmpq);
      break;
    case kX64Test8:
      ASSEMBLE_COMPARE(testb);
      break;
    case kX64Test16:
      ASSEMBLE_COMPARE(testw);
      break;
    case kX64Test32:
      ASSEMBLE_COMPARE(testl);
      break;
    case kX64Test:
      ASSEMBLE_COMPARE(testq);
      break;
    case kX64Imul32:
      ASSEMBLE_MULT(imull);
      break;
    case kX64Imul:
      ASSEMBLE_MULT(imulq);
      break;
    case kX64ImulHigh32:
      if (instr->InputAt(1)->IsRegister()) {
        __ imull(i.InputRegister(1));
      } else {
        __ imull(i.InputOperand(1));
      }
      break;
    case kX64UmulHigh32:
      if (instr->InputAt(1)->IsRegister()) {
        __ mull(i.InputRegister(1));
      } else {
        __ mull(i.InputOperand(1));
      }
      break;
    case kX64Idiv32:
      __ cdq();
      __ idivl(i.InputRegister(1));
      break;
    case kX64Idiv:
      __ cqo();
      __ idivq(i.InputRegister(1));
      break;
    case kX64Udiv32:
      __ xorl(rdx, rdx);
      __ divl(i.InputRegister(1));
      break;
    case kX64Udiv:
      __ xorq(rdx, rdx);
      __ divq(i.InputRegister(1));
      break;
    case kX64Not:
      ASSEMBLE_UNOP(notq);
      break;
    case kX64Not32:
      ASSEMBLE_UNOP(notl);
      break;
    case kX64Neg:
      ASSEMBLE_UNOP(negq);
      break;
    case kX64Neg32:
      ASSEMBLE_UNOP(negl);
      break;
    case kX64Or32:
      ASSEMBLE_BINOP(orl);
      break;
    case kX64Or:
      ASSEMBLE_BINOP(orq);
      break;
    case kX64Xor32:
      ASSEMBLE_BINOP(xorl);
      break;
    case kX64Xor:
      ASSEMBLE_BINOP(xorq);
      break;
    case kX64Shl32:
      ASSEMBLE_SHIFT(shll, 5);
      break;
    case kX64Shl:
      ASSEMBLE_SHIFT(shlq, 6);
      break;
    case kX64Shr32:
      ASSEMBLE_SHIFT(shrl, 5);
      break;
    case kX64Shr:
      ASSEMBLE_SHIFT(shrq, 6);
      break;
    case kX64Sar32:
      ASSEMBLE_SHIFT(sarl, 5);
      break;
    case kX64Sar:
      ASSEMBLE_SHIFT(sarq, 6);
      break;
    case kX64Ror32:
      ASSEMBLE_SHIFT(rorl, 5);
      break;
    case kX64Ror:
      ASSEMBLE_SHIFT(rorq, 6);
      break;
    case kX64Lzcnt:
      if (instr->InputAt(0)->IsRegister()) {
        __ Lzcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Lzcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Lzcnt32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Lzcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Lzcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Tzcnt:
      if (instr->InputAt(0)->IsRegister()) {
        __ Tzcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Tzcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Tzcnt32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Tzcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Tzcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Popcnt:
      if (instr->InputAt(0)->IsRegister()) {
        __ Popcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Popcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Popcnt32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Popcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Popcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kSSEFloat32Cmp:
      ASSEMBLE_SSE_BINOP(Ucomiss);
      break;
    case kSSEFloat32Add:
      ASSEMBLE_SSE_BINOP(addss);
      break;
    case kSSEFloat32Sub:
      ASSEMBLE_SSE_BINOP(subss);
      break;
    case kSSEFloat32Mul:
      ASSEMBLE_SSE_BINOP(mulss);
      break;
    case kSSEFloat32Div:
      ASSEMBLE_SSE_BINOP(divss);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulss depending on the result.
      __ movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kSSEFloat32Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 33);
      __ andps(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 31);
      __ xorps(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat32Sqrt:
      ASSEMBLE_SSE_UNOP(sqrtss);
      break;
    case kSSEFloat32ToFloat64:
      ASSEMBLE_SSE_UNOP(Cvtss2sd);
      break;
    case kSSEFloat32Round: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ Roundss(i.OutputDoubleRegister(), i.InputDoubleRegister(0), mode);
      break;
    }
    case kSSEFloat32ToInt32:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2si(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2si(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kSSEFloat32ToUint32: {
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2siq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    }
    case kSSEFloat64Cmp:
      ASSEMBLE_SSE_BINOP(Ucomisd);
      break;
    case kSSEFloat64Add:
      ASSEMBLE_SSE_BINOP(addsd);
      break;
    case kSSEFloat64Sub:
      ASSEMBLE_SSE_BINOP(subsd);
      break;
    case kSSEFloat64Mul:
      ASSEMBLE_SSE_BINOP(mulsd);
      break;
    case kSSEFloat64Div:
      ASSEMBLE_SSE_BINOP(divsd);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulsd depending on the result.
      __ Movapd(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kSSEFloat64Mod: {
      __ subq(rsp, Immediate(kDoubleSize));
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       kDoubleSize);
      // Move values to st(0) and st(1).
      __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(1));
      __ fld_d(Operand(rsp, 0));
      __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(0));
      __ fld_d(Operand(rsp, 0));
      // Loop while fprem isn't done.
      Label mod_loop;
      __ bind(&mod_loop);
      // This instructions traps on all kinds inputs, but we are assuming the
      // floating point control word is set to ignore them all.
      __ fprem();
      // The following 2 instruction implicitly use rax.
      __ fnstsw_ax();
      if (CpuFeatures::IsSupported(SAHF)) {
        CpuFeatureScope sahf_scope(masm(), SAHF);
        __ sahf();
      } else {
        __ shrl(rax, Immediate(8));
        __ andl(rax, Immediate(0xFF));
        __ pushq(rax);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
        __ popfq();
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kPointerSize);
      }
      __ j(parity_even, &mod_loop);
      // Move output to stack and clean up.
      __ fstp(1);
      __ fstp_d(Operand(rsp, 0));
      __ Movsd(i.OutputDoubleRegister(), Operand(rsp, 0));
      __ addq(rsp, Immediate(kDoubleSize));
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       -kDoubleSize);
      break;
    }
    case kSSEFloat32Max: {
      Label compare_nan, compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat32NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(above, &done_compare, Label::kNear);
      __ j(below, &compare_swap, Label::kNear);
      __ Movmskps(kScratchRegister, i.InputDoubleRegister(0));
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat32Min: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat32NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(below, &done_compare, Label::kNear);
      __ j(above, &compare_swap, Label::kNear);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movmskps(kScratchRegister, i.InputDoubleRegister(1));
      } else {
        __ Movss(kScratchDoubleReg, i.InputOperand(1));
        __ Movmskps(kScratchRegister, kScratchDoubleReg);
      }
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Max: {
      Label compare_nan, compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat64NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(above, &done_compare, Label::kNear);
      __ j(below, &compare_swap, Label::kNear);
      __ Movmskpd(kScratchRegister, i.InputDoubleRegister(0));
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movsd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Min: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat64NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(below, &done_compare, Label::kNear);
      __ j(above, &compare_swap, Label::kNear);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movmskpd(kScratchRegister, i.InputDoubleRegister(1));
      } else {
        __ Movsd(kScratchDoubleReg, i.InputOperand(1));
        __ Movmskpd(kScratchRegister, kScratchDoubleReg);
      }
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movsd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 1);
      __ andpd(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat64Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 63);
      __ xorpd(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat64Sqrt:
      ASSEMBLE_SSE_UNOP(Sqrtsd);
      break;
    case kSSEFloat64Round: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ Roundsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0), mode);
      break;
    }
    case kSSEFloat64ToFloat32:
      ASSEMBLE_SSE_UNOP(Cvtsd2ss);
      break;
    case kSSEFloat64ToInt32:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2si(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2si(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kSSEFloat64ToUint32: {
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2siq(i.OutputRegister(), i.InputOperand(0));
      }
      if (MiscField::decode(instr->opcode())) {
        __ AssertZeroExtended(i.OutputRegister());
      }
      break;
    }
    case kSSEFloat32ToInt64:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2siq(i.OutputRegister(), i.InputOperand(0));
      }
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 1);
        Label done;
        Label fail;
        __ Move(kScratchDoubleReg, static_cast<float>(INT64_MIN));
        if (instr->InputAt(0)->IsFPRegister()) {
          __ Ucomiss(kScratchDoubleReg, i.InputDoubleRegister(0));
        } else {
          __ Ucomiss(kScratchDoubleReg, i.InputOperand(0));
        }
        // If the input is NaN, then the conversion fails.
        __ j(parity_even, &fail);
        // If the input is INT64_MIN, then the conversion succeeds.
        __ j(equal, &done);
        __ cmpq(i.OutputRegister(0), Immediate(1));
        // If the conversion results in INT64_MIN, but the input was not
        // INT64_MIN, then the conversion fails.
        __ j(no_overflow, &done);
        __ bind(&fail);
        __ Set(i.OutputRegister(1), 0);
        __ bind(&done);
      }
      break;
    case kSSEFloat64ToInt64:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2siq(i.OutputRegister(0), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2siq(i.OutputRegister(0), i.InputOperand(0));
      }
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 1);
        Label done;
        Label fail;
        __ Move(kScratchDoubleReg, static_cast<double>(INT64_MIN));
        if (instr->InputAt(0)->IsFPRegister()) {
          __ Ucomisd(kScratchDoubleReg, i.InputDoubleRegister(0));
        } else {
          __ Ucomisd(kScratchDoubleReg, i.InputOperand(0));
        }
        // If the input is NaN, then the conversion fails.
        __ j(parity_even, &fail);
        // If the input is INT64_MIN, then the conversion succeeds.
        __ j(equal, &done);
        __ cmpq(i.OutputRegister(0), Immediate(1));
        // If the conversion results in INT64_MIN, but the input was not
        // INT64_MIN, then the conversion fails.
        __ j(no_overflow, &done);
        __ bind(&fail);
        __ Set(i.OutputRegister(1), 0);
        __ bind(&done);
      }
      break;
    case kSSEFloat32ToUint64: {
      Label done;
      Label success;
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 0);
      }
      // There does not exist a Float32ToUint64 instruction, so we have to use
      // the Float32ToInt64 instruction.
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2siq(i.OutputRegister(), i.InputOperand(0));
      }
      // Check if the result of the Float32ToInt64 conversion is positive, we
      // are already done.
      __ testq(i.OutputRegister(), i.OutputRegister());
      __ j(positive, &success);
      // The result of the first conversion was negative, which means that the
      // input value was not within the positive int64 range. We subtract 2^64
      // and convert it again to see if it is within the uint64 range.
      __ Move(kScratchDoubleReg, -9223372036854775808.0f);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ addss(kScratchDoubleReg, i.InputDoubleRegister(0));
      } else {
        __ addss(kScratchDoubleReg, i.InputOperand(0));
      }
      __ Cvttss2siq(i.OutputRegister(), kScratchDoubleReg);
      __ testq(i.OutputRegister(), i.OutputRegister());
      // The only possible negative value here is 0x80000000000000000, which is
      // used on x64 to indicate an integer overflow.
      __ j(negative, &done);
      // The input value is within uint64 range and the second conversion worked
      // successfully, but we still have to undo the subtraction we did
      // earlier.
      __ Set(kScratchRegister, 0x8000000000000000);
      __ orq(i.OutputRegister(), kScratchRegister);
      __ bind(&success);
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 1);
      }
      __ bind(&done);
      break;
    }
    case kSSEFloat64ToUint64: {
      Label done;
      Label success;
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 0);
      }
      // There does not exist a Float64ToUint64 instruction, so we have to use
      // the Float64ToInt64 instruction.
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2siq(i.OutputRegister(), i.InputOperand(0));
      }
      // Check if the result of the Float64ToInt64 conversion is positive, we
      // are already done.
      __ testq(i.OutputRegister(), i.OutputRegister());
      __ j(positive, &success);
      // The result of the first conversion was negative, which means that the
      // input value was not within the positive int64 range. We subtract 2^64
      // and convert it again to see if it is within the uint64 range.
      __ Move(kScratchDoubleReg, -9223372036854775808.0);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ addsd(kScratchDoubleReg, i.InputDoubleRegister(0));
      } else {
        __ addsd(kScratchDoubleReg, i.InputOperand(0));
      }
      __ Cvttsd2siq(i.OutputRegister(), kScratchDoubleReg);
      __ testq(i.OutputRegister(), i.OutputRegister());
      // The only possible negative value here is 0x80000000000000000, which is
      // used on x64 to indicate an integer overflow.
      __ j(negative, &done);
      // The input value is within uint64 range and the second conversion worked
      // successfully, but we still have to undo the subtraction we did
      // earlier.
      __ Set(kScratchRegister, 0x8000000000000000);
      __ orq(i.OutputRegister(), kScratchRegister);
      __ bind(&success);
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 1);
      }
      __ bind(&done);
      break;
    }
    case kSSEInt32ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtlsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt32ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtlsi2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt64ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtqsi2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt64ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtqsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint64ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ movq(kScratchRegister, i.InputRegister(0));
      } else {
        __ movq(kScratchRegister, i.InputOperand(0));
      }
      __ Cvtqui2ss(i.OutputDoubleRegister(), kScratchRegister,
                   i.TempRegister(0));
      break;
    case kSSEUint64ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ movq(kScratchRegister, i.InputRegister(0));
      } else {
        __ movq(kScratchRegister, i.InputOperand(0));
      }
      __ Cvtqui2sd(i.OutputDoubleRegister(), kScratchRegister,
                   i.TempRegister(0));
      break;
    case kSSEUint32ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ movl(kScratchRegister, i.InputRegister(0));
      } else {
        __ movl(kScratchRegister, i.InputOperand(0));
      }
      __ Cvtqsi2sd(i.OutputDoubleRegister(), kScratchRegister);
      break;
    case kSSEUint32ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ movl(kScratchRegister, i.InputRegister(0));
      } else {
        __ movl(kScratchRegister, i.InputOperand(0));
      }
      __ Cvtqsi2ss(i.OutputDoubleRegister(), kScratchRegister);
      break;
    case kSSEFloat64ExtractLowWord32:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movl(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ Movd(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kSSEFloat64ExtractHighWord32:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movl(i.OutputRegister(), i.InputOperand(0, kDoubleSize / 2));
      } else {
        __ Pextrd(i.OutputRegister(), i.InputDoubleRegister(0), 1);
      }
      break;
    case kSSEFloat64InsertLowWord32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputRegister(1), 0);
      } else {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 0);
      }
      break;
    case kSSEFloat64InsertHighWord32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputRegister(1), 1);
      } else {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 1);
      }
      break;
    case kSSEFloat64LoadLowWord32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Movd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Movd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kAVXFloat32Cmp: {
      CpuFeatureScope avx_scope(masm(), AVX);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ vucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ vucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      break;
    }
    case kAVXFloat32Add:
      ASSEMBLE_AVX_BINOP(vaddss);
      break;
    case kAVXFloat32Sub:
      ASSEMBLE_AVX_BINOP(vsubss);
      break;
    case kAVXFloat32Mul:
      ASSEMBLE_AVX_BINOP(vmulss);
      break;
    case kAVXFloat32Div:
      ASSEMBLE_AVX_BINOP(vdivss);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulss depending on the result.
      __ Movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kAVXFloat64Cmp: {
      CpuFeatureScope avx_scope(masm(), AVX);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ vucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ vucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      break;
    }
    case kAVXFloat64Add:
      ASSEMBLE_AVX_BINOP(vaddsd);
      break;
    case kAVXFloat64Sub:
      ASSEMBLE_AVX_BINOP(vsubsd);
      break;
    case kAVXFloat64Mul:
      ASSEMBLE_AVX_BINOP(vmulsd);
      break;
    case kAVXFloat64Div:
      ASSEMBLE_AVX_BINOP(vdivsd);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulsd depending on the result.
      __ Movapd(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kAVXFloat32Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsrlq(kScratchDoubleReg, kScratchDoubleReg, 33);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vandps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vandps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsllq(kScratchDoubleReg, kScratchDoubleReg, 31);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vxorps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vxorps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat64Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsrlq(kScratchDoubleReg, kScratchDoubleReg, 1);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vandpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vandpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat64Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsllq(kScratchDoubleReg, kScratchDoubleReg, 63);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vxorpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vxorpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kSSEFloat64SilenceNaN:
      __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
      __ Subsd(i.InputDoubleRegister(0), kScratchDoubleReg);
      break;
    case kX64Movsxbl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movsxbl);
      __ AssertZeroExtended(i.OutputRegister());
      break;
    case kX64Movzxbl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movzxbl);
      __ AssertZeroExtended(i.OutputRegister());
      break;
    case kX64Movsxbq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movsxbq);
      break;
    case kX64Movzxbq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movzxbq);
      __ AssertZeroExtended(i.OutputRegister());
      break;
    case kX64Movb: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ movb(operand, Immediate(i.InputInt8(index)));
      } else {
        __ movb(operand, i.InputRegister(index));
      }
      break;
    }
    case kX64Movsxwl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movsxwl);
      __ AssertZeroExtended(i.OutputRegister());
      break;
    case kX64Movzxwl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movzxwl);
      __ AssertZeroExtended(i.OutputRegister());
      break;
    case kX64Movsxwq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movsxwq);
      break;
    case kX64Movzxwq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movzxwq);
      __ AssertZeroExtended(i.OutputRegister());
      break;
    case kX64Movw: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ movw(operand, Immediate(i.InputInt16(index)));
      } else {
        __ movw(operand, i.InputRegister(index));
      }
      break;
    }
    case kX64Movl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      if (instr->HasOutput()) {
        if (instr->addressing_mode() == kMode_None) {
          if (instr->InputAt(0)->IsRegister()) {
            __ movl(i.OutputRegister(), i.InputRegister(0));
          } else {
            __ movl(i.OutputRegister(), i.InputOperand(0));
          }
        } else {
          __ movl(i.OutputRegister(), i.MemoryOperand());
        }
        __ AssertZeroExtended(i.OutputRegister());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ movl(operand, i.InputImmediate(index));
        } else {
          __ movl(operand, i.InputRegister(index));
        }
      }
      break;
    case kX64Movsxlq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      ASSEMBLE_MOVX(movsxlq);
      break;
    case kX64Movq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      if (instr->HasOutput()) {
        __ movq(i.OutputRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ movq(operand, i.InputImmediate(index));
        } else {
          __ movq(operand, i.InputRegister(index));
        }
      }
      break;
    case kX64Movss:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      if (instr->HasOutput()) {
        __ movss(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movss(operand, i.InputDoubleRegister(index));
      }
      break;
    case kX64Movsd:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr->InputCount(), i,
                          __ pc_offset(), instr);
      if (instr->HasOutput()) {
        __ Movsd(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ Movsd(operand, i.InputDoubleRegister(index));
      }
      break;
    case kX64BitcastFI:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movl(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ Movd(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kX64BitcastDL:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movq(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ Movq(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kX64BitcastIF:
      if (instr->InputAt(0)->IsRegister()) {
        __ Movd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ movss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kX64BitcastLD:
      if (instr->InputAt(0)->IsRegister()) {
        __ Movq(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Movsd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kX64Lea32: {
      AddressingMode mode = AddressingModeField::decode(instr->opcode());
      // Shorten "leal" to "addl", "subl" or "shll" if the register allocation
      // and addressing mode just happens to work out. The "addl"/"subl" forms
      // in these cases are faster based on measurements.
      if (i.InputRegister(0).is(i.OutputRegister())) {
        if (mode == kMode_MRI) {
          int32_t constant_summand = i.InputInt32(1);
          if (constant_summand > 0) {
            __ addl(i.OutputRegister(), Immediate(constant_summand));
          } else if (constant_summand < 0) {
            __ subl(i.OutputRegister(), Immediate(-constant_summand));
          }
        } else if (mode == kMode_MR1) {
          if (i.InputRegister(1).is(i.OutputRegister())) {
            __ shll(i.OutputRegister(), Immediate(1));
          } else {
            __ addl(i.OutputRegister(), i.InputRegister(1));
          }
        } else if (mode == kMode_M2) {
          __ shll(i.OutputRegister(), Immediate(1));
        } else if (mode == kMode_M4) {
          __ shll(i.OutputRegister(), Immediate(2));
        } else if (mode == kMode_M8) {
          __ shll(i.OutputRegister(), Immediate(3));
        } else {
          __ leal(i.OutputRegister(), i.MemoryOperand());
        }
      } else if (mode == kMode_MR1 &&
                 i.InputRegister(1).is(i.OutputRegister())) {
        __ addl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ leal(i.OutputRegister(), i.MemoryOperand());
      }
      __ AssertZeroExtended(i.OutputRegister());
      break;
    }
    case kX64Lea: {
      AddressingMode mode = AddressingModeField::decode(instr->opcode());
      // Shorten "leaq" to "addq", "subq" or "shlq" if the register allocation
      // and addressing mode just happens to work out. The "addq"/"subq" forms
      // in these cases are faster based on measurements.
      if (i.InputRegister(0).is(i.OutputRegister())) {
        if (mode == kMode_MRI) {
          int32_t constant_summand = i.InputInt32(1);
          if (constant_summand > 0) {
            __ addq(i.OutputRegister(), Immediate(constant_summand));
          } else if (constant_summand < 0) {
            __ subq(i.OutputRegister(), Immediate(-constant_summand));
          }
        } else if (mode == kMode_MR1) {
          if (i.InputRegister(1).is(i.OutputRegister())) {
            __ shlq(i.OutputRegister(), Immediate(1));
          } else {
            __ addq(i.OutputRegister(), i.InputRegister(1));
          }
        } else if (mode == kMode_M2) {
          __ shlq(i.OutputRegister(), Immediate(1));
        } else if (mode == kMode_M4) {
          __ shlq(i.OutputRegister(), Immediate(2));
        } else if (mode == kMode_M8) {
          __ shlq(i.OutputRegister(), Immediate(3));
        } else {
          __ leaq(i.OutputRegister(), i.MemoryOperand());
        }
      } else if (mode == kMode_MR1 &&
                 i.InputRegister(1).is(i.OutputRegister())) {
        __ addq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ leaq(i.OutputRegister(), i.MemoryOperand());
      }
      break;
    }
    case kX64Dec32:
      __ decl(i.OutputRegister());
      break;
    case kX64Inc32:
      __ incl(i.OutputRegister());
      break;
    case kX64Push:
      if (HasImmediateInput(instr, 0)) {
        __ pushq(i.InputImmediate(0));
        frame_access_state()->IncreaseSPDelta(1);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
      } else {
        if (instr->InputAt(0)->IsRegister()) {
          __ pushq(i.InputRegister(0));
          frame_access_state()->IncreaseSPDelta(1);
          unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                           kPointerSize);
        } else if (instr->InputAt(0)->IsFPRegister()) {
          // TODO(titzer): use another machine instruction?
          __ subq(rsp, Immediate(kDoubleSize));
          frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
          unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                           kDoubleSize);
          __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(0));
        } else {
          __ pushq(i.InputOperand(0));
          frame_access_state()->IncreaseSPDelta(1);
          unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                           kPointerSize);
        }
      }
      break;
    case kX64Poke: {
      int const slot = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        __ movq(Operand(rsp, slot * kPointerSize), i.InputImmediate(0));
      } else {
        __ movq(Operand(rsp, slot * kPointerSize), i.InputRegister(0));
      }
      break;
    }
    case kX64Xchgb: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      __ xchgb(i.InputRegister(index), operand);
      break;
    }
    case kX64Xchgw: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      __ xchgw(i.InputRegister(index), operand);
      break;
    }
    case kX64Xchgl: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      __ xchgl(i.InputRegister(index), operand);
      break;
    }
    case kX64Int32x4Create: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      __ Movd(dst, i.InputRegister(0));
      __ shufps(dst, dst, 0x0);
      break;
    }
    case kX64Int32x4ExtractLane: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      __ Pextrd(i.OutputRegister(), i.InputSimd128Register(0), i.InputInt8(1));
      break;
    }
    case kX64Int32x4ReplaceLane: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      if (instr->InputAt(2)->IsRegister()) {
        __ Pinsrd(i.OutputSimd128Register(), i.InputRegister(2),
                  i.InputInt8(1));
      } else {
        __ Pinsrd(i.OutputSimd128Register(), i.InputOperand(2), i.InputInt8(1));
      }
      break;
    }
    case kX64Int32x4Add: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      __ paddd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64Int32x4Sub: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      __ psubd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movsxbl);
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movzxbl);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movsxwl);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movzxwl);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movl);
      break;
    case kCheckedLoadWord64:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movq);
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(Movss, OutOfLineLoadFloat32NaN);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(Movsd, OutOfLineLoadFloat64NaN);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(movb);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(movw);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(movl);
      break;
    case kCheckedStoreWord64:
      ASSEMBLE_CHECKED_STORE_INTEGER(movq);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(Movss);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(Movsd);
      break;
    case kX64StackCheck:
      __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
      break;
    case kAtomicLoadInt8:
    case kAtomicLoadUint8:
    case kAtomicLoadInt16:
    case kAtomicLoadUint16:
    case kAtomicLoadWord32:
    case kAtomicStoreWord8:
    case kAtomicStoreWord16:
    case kAtomicStoreWord32:
      UNREACHABLE();  // Won't be generated by instruction selector.
      break;
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)

namespace {

Condition FlagsConditionToCondition(FlagsCondition condition) {
  switch (condition) {
    case kUnorderedEqual:
    case kEqual:
      return equal;
    case kUnorderedNotEqual:
    case kNotEqual:
      return not_equal;
    case kSignedLessThan:
      return less;
    case kSignedGreaterThanOrEqual:
      return greater_equal;
    case kSignedLessThanOrEqual:
      return less_equal;
    case kSignedGreaterThan:
      return greater;
    case kUnsignedLessThan:
      return below;
    case kUnsignedGreaterThanOrEqual:
      return above_equal;
    case kUnsignedLessThanOrEqual:
      return below_equal;
    case kUnsignedGreaterThan:
      return above;
    case kOverflow:
      return overflow;
    case kNotOverflow:
      return no_overflow;
    default:
      break;
  }
  UNREACHABLE();
  return no_condition;
}

}  // namespace

// Assembles branches after this instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Label::Distance flabel_distance =
      branch->fallthru ? Label::kNear : Label::kFar;
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  if (branch->condition == kUnorderedEqual) {
    __ j(parity_even, flabel, flabel_distance);
  } else if (branch->condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(branch->condition), tlabel);

  if (!branch->fallthru) __ jmp(flabel, flabel_distance);
}


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ jmp(GetLabel(target));
}

void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  class OutOfLineTrap final : public OutOfLineCode {
   public:
    OutOfLineTrap(CodeGenerator* gen, bool frame_elided, Instruction* instr)
        : OutOfLineCode(gen),
          frame_elided_(frame_elided),
          instr_(instr),
          gen_(gen) {}

    void Generate() final {
      X64OperandConverter i(gen_, instr_);

      Runtime::FunctionId trap_id = static_cast<Runtime::FunctionId>(
          i.InputInt32(instr_->InputCount() - 1));
      bool old_has_frame = __ has_frame();
      if (frame_elided_) {
        __ set_has_frame(true);
        __ EnterFrame(StackFrame::WASM_COMPILED);
      }
      GenerateCallToTrap(trap_id);
      if (frame_elided_) {
        __ set_has_frame(old_has_frame);
      }
      if (FLAG_debug_code) {
        __ ud2();
      }
    }

   private:
    void GenerateCallToTrap(Runtime::FunctionId trap_id) {
      if (trap_id == Runtime::kNumFunctions) {
        // We cannot test calls to the runtime in cctest/test-run-wasm.
        // Therefore we emit a call to C here instead of a call to the runtime.
        __ PrepareCallCFunction(0);
        __ CallCFunction(
            ExternalReference::wasm_call_trap_callback_for_testing(isolate()),
            0);
      } else {
        __ Move(rsi, isolate()->native_context());
        gen_->AssembleSourcePosition(instr_);
        __ CallRuntime(trap_id);
      }
      ReferenceMap* reference_map =
          new (gen_->zone()) ReferenceMap(gen_->zone());
      gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                            Safepoint::kNoLazyDeopt);
    }

    bool frame_elided_;
    Instruction* instr_;
    CodeGenerator* gen_;
  };
  bool frame_elided = !frame_access_state()->has_frame();
  auto ool = new (zone()) OutOfLineTrap(this, frame_elided, instr);
  Label* tlabel = ool->entry();
  Label end;
  if (condition == kUnorderedEqual) {
    __ j(parity_even, &end);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(condition), tlabel);
  __ bind(&end);
}

// Assembles boolean materializations after this instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  X64OperandConverter i(this, instr);
  Label done;

  // Materialize a full 64-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label check;
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  if (condition == kUnorderedEqual) {
    __ j(parity_odd, &check, Label::kNear);
    __ movl(reg, Immediate(0));
    __ jmp(&done, Label::kNear);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_odd, &check, Label::kNear);
    __ movl(reg, Immediate(1));
    __ jmp(&done, Label::kNear);
  }
  __ bind(&check);
  __ setcc(FlagsConditionToCondition(condition), reg);
  __ movzxbl(reg, reg);
  __ bind(&done);
}


void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  X64OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ cmpl(input, Immediate(i.InputInt32(index + 0)));
    __ j(equal, GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}


void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  X64OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  int32_t const case_count = static_cast<int32_t>(instr->InputCount() - 2);
  Label** cases = zone()->NewArray<Label*>(case_count);
  for (int32_t index = 0; index < case_count; ++index) {
    cases[index] = GetLabel(i.InputRpo(index + 2));
  }
  Label* const table = AddJumpTable(cases, case_count);
  __ cmpl(input, Immediate(case_count));
  __ j(above_equal, GetLabel(i.InputRpo(1)));
  __ leaq(kScratchRegister, Operand(table));
  __ jmp(Operand(kScratchRegister, input, times_8, 0));
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type,
    SourcePosition pos) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  if (deopt_entry == nullptr) return kTooManyDeoptimizationBailouts;
  DeoptimizeReason deoptimization_reason =
      GetDeoptimizationReason(deoptimization_id);
  __ RecordDeoptReason(deoptimization_reason, pos, deoptimization_id);
  __ call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
  return kSuccess;
}


namespace {

static const int kQuadWordSize = 16;

}  // namespace

void CodeGenerator::FinishFrame(Frame* frame) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves_fp = descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    frame->AlignSavedCalleeRegisterSlots();
    if (saves_fp != 0) {  // Save callee-saved XMM registers.
      const uint32_t saves_fp_count = base::bits::CountPopulation32(saves_fp);
      frame->AllocateSavedCalleeRegisterSlots(saves_fp_count *
                                              (kQuadWordSize / kPointerSize));
    }
  }
  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {  // Save callee-saved registers.
    int count = 0;
    for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
      if (((1 << i) & saves)) {
        ++count;
      }
    }
    frame->AllocateSavedCalleeRegisterSlots(count);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    int pc_base = __ pc_offset();

    if (descriptor->IsCFunctionCall()) {
      __ pushq(rbp);
      __ movq(rbp, rsp);
    } else if (descriptor->IsJSFunctionCall()) {
      __ Prologue(this->info()->GeneratePreagedPrologue());
      if (descriptor->PushArgumentCount()) {
        __ pushq(kJavaScriptCallArgCountRegister);
      }
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
    }

    if (!descriptor->IsJSFunctionCall() || !info()->GeneratePreagedPrologue()) {
      unwinding_info_writer_.MarkFrameConstructed(pc_base);
    }
  }
  int shrink_slots =
      frame()->GetTotalFrameSlotCount() - descriptor->CalculateFixedFrameSize();

  if (info()->is_osr()) {
    // TurboFan OSR-compiled functions cannot be entered directly.
    __ Abort(kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    shrink_slots -= static_cast<int>(OsrHelper(info()).UnoptimizedFrameSlots());
  }

  const RegList saves_fp = descriptor->CalleeSavedFPRegisters();
  if (shrink_slots > 0) {
    __ subq(rsp, Immediate(shrink_slots * kPointerSize));
  }

  if (saves_fp != 0) {  // Save callee-saved XMM registers.
    const uint32_t saves_fp_count = base::bits::CountPopulation32(saves_fp);
    const int stack_size = saves_fp_count * kQuadWordSize;
    // Adjust the stack pointer.
    __ subp(rsp, Immediate(stack_size));
    // Store the registers on the stack.
    int slot_idx = 0;
    for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
      if (!((1 << i) & saves_fp)) continue;
      __ movdqu(Operand(rsp, kQuadWordSize * slot_idx),
                XMMRegister::from_code(i));
      slot_idx++;
    }
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {  // Save callee-saved registers.
    for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
      if (!((1 << i) & saves)) continue;
      __ pushq(Register::from_code(i));
    }
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  // Restore registers.
  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    for (int i = 0; i < Register::kNumRegisters; i++) {
      if (!((1 << i) & saves)) continue;
      __ popq(Register::from_code(i));
    }
  }
  const RegList saves_fp = descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    const uint32_t saves_fp_count = base::bits::CountPopulation32(saves_fp);
    const int stack_size = saves_fp_count * kQuadWordSize;
    // Load the registers from the stack.
    int slot_idx = 0;
    for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
      if (!((1 << i) & saves_fp)) continue;
      __ movdqu(XMMRegister::from_code(i),
                Operand(rsp, kQuadWordSize * slot_idx));
      slot_idx++;
    }
    // Adjust the stack pointer.
    __ addp(rsp, Immediate(stack_size));
  }

  unwinding_info_writer_.MarkBlockWillExit();

  // Might need rcx for scratch if pop_size is too big or if there is a variable
  // pop count.
  DCHECK_EQ(0u, descriptor->CalleeSavedRegisters() & rcx.bit());
  DCHECK_EQ(0u, descriptor->CalleeSavedRegisters() & rdx.bit());
  size_t pop_size = descriptor->StackParameterCount() * kPointerSize;
  X64OperandConverter g(this, nullptr);
  if (descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    if (pop->IsImmediate() && g.ToConstant(pop).ToInt32() == 0) {
      // Canonicalize JSFunction return sites for now.
      if (return_label_.is_bound()) {
        __ jmp(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
        AssembleDeconstructFrame();
      }
    } else {
      AssembleDeconstructFrame();
    }
  }

  if (pop->IsImmediate()) {
    DCHECK_EQ(Constant::kInt32, g.ToConstant(pop).type());
    pop_size += g.ToConstant(pop).ToInt32() * kPointerSize;
    CHECK_LT(pop_size, static_cast<size_t>(std::numeric_limits<int>::max()));
    __ Ret(static_cast<int>(pop_size), rcx);
  } else {
    Register pop_reg = g.ToRegister(pop);
    Register scratch_reg = pop_reg.is(rcx) ? rdx : rcx;
    __ popq(scratch_reg);
    __ leaq(rsp, Operand(rsp, pop_reg, times_8, static_cast<int>(pop_size)));
    __ jmp(scratch_reg);
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X64OperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ movq(g.ToRegister(destination), src);
    } else {
      __ movq(g.ToOperand(destination), src);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Operand src = g.ToOperand(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ movq(dst, src);
    } else {
      // Spill on demand to use a temporary register for memory-to-memory
      // moves.
      Register tmp = kScratchRegister;
      Operand dst = g.ToOperand(destination);
      __ movq(tmp, src);
      __ movq(dst, tmp);
    }
  } else if (source->IsConstant()) {
    ConstantOperand* constant_source = ConstantOperand::cast(source);
    Constant src = g.ToConstant(constant_source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst = destination->IsRegister() ? g.ToRegister(destination)
                                               : kScratchRegister;
      switch (src.type()) {
        case Constant::kInt32: {
          if (RelocInfo::IsWasmPtrReference(src.rmode())) {
            __ movq(dst, src.ToInt64(), src.rmode());
          } else {
            // TODO(dcarney): don't need scratch in this case.
            int32_t value = src.ToInt32();
            if (value == 0) {
              __ xorl(dst, dst);
            } else {
              if (RelocInfo::IsWasmSizeReference(src.rmode())) {
                __ movl(dst, Immediate(value, src.rmode()));
              } else {
                __ movl(dst, Immediate(value));
              }
            }
          }
          break;
        }
        case Constant::kInt64:
          if (RelocInfo::IsWasmPtrReference(src.rmode())) {
            __ movq(dst, src.ToInt64(), src.rmode());
          } else {
            DCHECK(!RelocInfo::IsWasmSizeReference(src.rmode()));
            __ Set(dst, src.ToInt64());
          }
          break;
        case Constant::kFloat32:
          __ Move(dst,
                  isolate()->factory()->NewNumber(src.ToFloat32(), TENURED));
          break;
        case Constant::kFloat64:
          __ Move(dst,
                  isolate()->factory()->NewNumber(src.ToFloat64(), TENURED));
          break;
        case Constant::kExternalReference:
          __ Move(dst, src.ToExternalReference());
          break;
        case Constant::kHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          Heap::RootListIndex index;
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ Move(dst, src_object);
          }
          break;
        }
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(dcarney): load of labels on x64.
          break;
      }
      if (destination->IsStackSlot()) {
        __ movq(g.ToOperand(destination), kScratchRegister);
      }
    } else if (src.type() == Constant::kFloat32) {
      // TODO(turbofan): Can we do better here?
      uint32_t src_const = bit_cast<uint32_t>(src.ToFloat32());
      if (destination->IsFPRegister()) {
        __ Move(g.ToDoubleRegister(destination), src_const);
      } else {
        DCHECK(destination->IsFPStackSlot());
        Operand dst = g.ToOperand(destination);
        __ movl(dst, Immediate(src_const));
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      uint64_t src_const = bit_cast<uint64_t>(src.ToFloat64());
      if (destination->IsFPRegister()) {
        __ Move(g.ToDoubleRegister(destination), src_const);
      } else {
        DCHECK(destination->IsFPStackSlot());
        __ movq(kScratchRegister, src_const);
        __ movq(g.ToOperand(destination), kScratchRegister);
      }
    }
  } else if (source->IsFPRegister()) {
    XMMRegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      XMMRegister dst = g.ToDoubleRegister(destination);
      __ Movapd(dst, src);
    } else {
      DCHECK(destination->IsFPStackSlot());
      Operand dst = g.ToOperand(destination);
      MachineRepresentation rep =
          LocationOperand::cast(source)->representation();
      if (rep != MachineRepresentation::kSimd128) {
        __ Movsd(dst, src);
      } else {
        __ Movups(dst, src);
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    Operand src = g.ToOperand(source);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (destination->IsFPRegister()) {
      XMMRegister dst = g.ToDoubleRegister(destination);
      if (rep != MachineRepresentation::kSimd128) {
        __ Movsd(dst, src);
      } else {
        __ Movups(dst, src);
      }
    } else {
      Operand dst = g.ToOperand(destination);
      if (rep != MachineRepresentation::kSimd128) {
        __ Movsd(kScratchDoubleReg, src);
        __ Movsd(dst, kScratchDoubleReg);
      } else {
        __ Movups(kScratchDoubleReg, src);
        __ Movups(dst, kScratchDoubleReg);
      }
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X64OperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister() && destination->IsRegister()) {
    // Register-register.
    Register src = g.ToRegister(source);
    Register dst = g.ToRegister(destination);
    __ movq(kScratchRegister, src);
    __ movq(src, dst);
    __ movq(dst, kScratchRegister);
  } else if (source->IsRegister() && destination->IsStackSlot()) {
    Register src = g.ToRegister(source);
    __ pushq(src);
    frame_access_state()->IncreaseSPDelta(1);
    unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                     kPointerSize);
    Operand dst = g.ToOperand(destination);
    __ movq(src, dst);
    frame_access_state()->IncreaseSPDelta(-1);
    dst = g.ToOperand(destination);
    __ popq(dst);
    unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                     -kPointerSize);
  } else if ((source->IsStackSlot() && destination->IsStackSlot()) ||
             (source->IsFPStackSlot() && destination->IsFPStackSlot())) {
    // Memory-memory.
    Operand src = g.ToOperand(source);
    Operand dst = g.ToOperand(destination);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep != MachineRepresentation::kSimd128) {
      Register tmp = kScratchRegister;
      __ movq(tmp, dst);
      __ pushq(src);
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       kPointerSize);
      frame_access_state()->IncreaseSPDelta(1);
      src = g.ToOperand(source);
      __ movq(src, tmp);
      frame_access_state()->IncreaseSPDelta(-1);
      dst = g.ToOperand(destination);
      __ popq(dst);
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       -kPointerSize);
    } else {
      // Use the XOR trick to swap without a temporary.
      __ Movups(kScratchDoubleReg, src);
      __ Xorps(kScratchDoubleReg, dst);  // scratch contains src ^ dst.
      __ Movups(src, kScratchDoubleReg);
      __ Xorps(kScratchDoubleReg, dst);  // scratch contains src.
      __ Movups(dst, kScratchDoubleReg);
      __ Xorps(kScratchDoubleReg, src);  // scratch contains dst.
      __ Movups(src, kScratchDoubleReg);
    }
  } else if (source->IsFPRegister() && destination->IsFPRegister()) {
    // XMM register-register swap.
    XMMRegister src = g.ToDoubleRegister(source);
    XMMRegister dst = g.ToDoubleRegister(destination);
    __ Movapd(kScratchDoubleReg, src);
    __ Movapd(src, dst);
    __ Movapd(dst, kScratchDoubleReg);
  } else if (source->IsFPRegister() && destination->IsFPStackSlot()) {
    // XMM register-memory swap.
    XMMRegister src = g.ToDoubleRegister(source);
    Operand dst = g.ToOperand(destination);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep != MachineRepresentation::kSimd128) {
      __ Movsd(kScratchDoubleReg, src);
      __ Movsd(src, dst);
      __ Movsd(dst, kScratchDoubleReg);
    } else {
      __ Movups(kScratchDoubleReg, src);
      __ Movups(src, dst);
      __ Movups(dst, kScratchDoubleReg);
    }
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  for (size_t index = 0; index < target_count; ++index) {
    __ dq(targets[index]);
  }
}


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  if (!info()->ShouldEnsureSpaceForLazyDeopt()) {
    return;
  }

  int space_needed = Deoptimizer::patch_size();
  // Ensure that we have enough space after the previous lazy-bailout
  // instruction for patching the code here.
  int current_pc = __ pc_offset();
  if (current_pc < last_lazy_deopt_pc_ + space_needed) {
    int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
    __ Nop(padding_size);
  }
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
