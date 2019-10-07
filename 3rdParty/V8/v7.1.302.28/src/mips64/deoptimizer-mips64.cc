// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/deoptimizer.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {

#define __ masm()->


// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::TableEntryGenerator::Generate() {
  GeneratePrologue();

  // Unlike on ARM we don't save all the registers, just the useful ones.
  // For the rest, there are gaps on the stack, so the offsets remain the same.
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;
  RegList saved_regs = restored_regs | sp.bit() | ra.bit();

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;
  const int kFloatRegsSize = kFloatSize * FloatRegister::kNumRegisters;

  // Save all double FPU registers before messing with them.
  __ Dsubu(sp, sp, Operand(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister fpu_reg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ Sdc1(fpu_reg, MemOperand(sp, offset));
  }

  // Save all float FPU registers before messing with them.
  __ Dsubu(sp, sp, Operand(kFloatRegsSize));
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    const FloatRegister fpu_reg = FloatRegister::from_code(code);
    int offset = code * kFloatSize;
    __ Swc1(fpu_reg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ Dsubu(sp, sp, kNumberOfRegisters * kPointerSize);
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs & (1 << i)) != 0) {
      __ Sd(ToRegister(i), MemOperand(sp, kPointerSize * i));
    }
  }

  __ li(a2, Operand(ExternalReference::Create(
                IsolateAddressId::kCEntryFPAddress, isolate())));
  __ Sd(fp, MemOperand(a2));

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize + kFloatRegsSize;

  // Get the bailout id from the stack.
  __ Ld(a2, MemOperand(sp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object (a3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register a4.
  __ mov(a3, ra);
  // Correct one word for bailout id.
  __ Daddu(a4, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));

  __ Dsubu(a4, fp, a4);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(6, a5);
  // Pass six arguments, according to n64 ABI.
  __ mov(a0, zero_reg);
  Label context_check;
  __ Ld(a1, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(a1, &context_check);
  __ Ld(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ li(a1, Operand(static_cast<int>(deopt_kind())));
  // a2: bailout id already loaded.
  // a3: code address or 0 already loaded.
  // a4: already has fp-to-sp delta.
  __ li(a5, Operand(ExternalReference::isolate_address(isolate())));

  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve "deoptimizer" object in register v0 and get the input
  // frame descriptor pointer to a1 (deoptimizer->input_);
  // Move deopt-obj to a0 for call to Deoptimizer::ComputeOutputFrames() below.
  __ mov(a0, v0);
  __ Ld(a1, MemOperand(v0, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((saved_regs & (1 << i)) != 0) {
      __ Ld(a2, MemOperand(sp, i * kPointerSize));
      __ Sd(a2, MemOperand(a1, offset));
    } else if (FLAG_debug_code) {
      __ li(a2, kDebugZapValue);
      __ Sd(a2, MemOperand(a1, offset));
    }
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy FPU registers to
  // double_registers_[DoubleRegister::kNumAllocatableRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kPointerSize + kFloatRegsSize;
    __ Ldc1(f0, MemOperand(sp, src_offset));
    __ Sdc1(f0, MemOperand(a1, dst_offset));
  }

  int float_regs_offset = FrameDescription::float_registers_offset();
  // Copy FPU registers to
  // float_registers_[FloatRegister::kNumAllocatableRegisters]
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    int dst_offset = code * kFloatSize + float_regs_offset;
    int src_offset = code * kFloatSize + kNumberOfRegisters * kPointerSize;
    __ Lwc1(f0, MemOperand(sp, src_offset));
    __ Swc1(f0, MemOperand(a1, dst_offset));
  }

  // Remove the bailout id and the saved registers from the stack.
  __ Daddu(sp, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));

  // Compute a pointer to the unwinding limit in register a2; that is
  // the first stack slot not part of the input frame.
  __ Ld(a2, MemOperand(a1, FrameDescription::frame_size_offset()));
  __ Daddu(a2, a2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ Daddu(a3, a1, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ BranchShort(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(a4);
  __ Sd(a4, MemOperand(a3, 0));
  __ daddiu(a3, a3, sizeof(uint64_t));
  __ bind(&pop_loop_header);
  __ BranchShort(&pop_loop, ne, a2, Operand(sp));
  // Compute the output frame in the deoptimizer.
  __ push(a0);  // Preserve deoptimizer object across call.
  // a0: deoptimizer object; a1: scratch.
  __ PrepareCallCFunction(1, a1);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(a0);  // Restore deoptimizer object (class Deoptimizer).

  __ Ld(sp, MemOperand(a0, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
  // Outer loop state: a4 = current "FrameDescription** output_",
  // a1 = one past the last FrameDescription**.
  __ Lw(a1, MemOperand(a0, Deoptimizer::output_count_offset()));
  __ Ld(a4, MemOperand(a0, Deoptimizer::output_offset()));  // a4 is output_.
  __ Dlsa(a1, a4, a1, kPointerSizeLog2);
  __ BranchShort(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: a2 = current FrameDescription*, a3 = loop index.
  __ Ld(a2, MemOperand(a4, 0));  // output_[ix]
  __ Ld(a3, MemOperand(a2, FrameDescription::frame_size_offset()));
  __ BranchShort(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ Dsubu(a3, a3, Operand(sizeof(uint64_t)));
  __ Daddu(a6, a2, Operand(a3));
  __ Ld(a7, MemOperand(a6, FrameDescription::frame_content_offset()));
  __ push(a7);
  __ bind(&inner_loop_header);
  __ BranchShort(&inner_push_loop, ne, a3, Operand(zero_reg));

  __ Daddu(a4, a4, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ BranchShort(&outer_push_loop, lt, a4, Operand(a1));

  __ Ld(a1, MemOperand(a0, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister fpu_reg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ Ldc1(fpu_reg, MemOperand(a1, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ Ld(a6, MemOperand(a2, FrameDescription::pc_offset()));
  __ push(a6);
  __ Ld(a6, MemOperand(a2, FrameDescription::continuation_offset()));
  __ push(a6);

  // Technically restoring 'at' should work unless zero_reg is also restored
  // but it's safer to check for this.
  DCHECK(!(at.bit() & restored_regs));
  // Restore the registers from the last output frame.
  __ mov(at, a2);
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs & (1 << i)) != 0) {
      __ Ld(ToRegister(i), MemOperand(at, offset));
    }
  }

  __ InitializeRootRegister();

  __ pop(at);  // Get continuation, leave pc on stack.
  __ pop(ra);
  __ Jump(at);
  __ stop("Unreachable.");
}


// Maximum size of a table entry generated below.
#ifdef _MIPS_ARCH_MIPS64R6
const int Deoptimizer::table_entry_size_ = 2 * kInstrSize;
#else
const int Deoptimizer::table_entry_size_ = 3 * kInstrSize;
#endif

void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm());

  // Create a sequence of deoptimization entries.
  // Note that registers are still live when jumping to an entry.
  Label table_start, done, trampoline_jump;
  __ bind(&table_start);
#ifdef _MIPS_ARCH_MIPS64R6
  int kMaxEntriesBranchReach =
      (1 << (kImm26Bits - 2)) / (table_entry_size_ / kInstrSize);
#else
  int kMaxEntriesBranchReach =
      (1 << (kImm16Bits - 2)) / (table_entry_size_ / kInstrSize);
#endif

  if (count() <= kMaxEntriesBranchReach) {
    // Common case.
    for (int i = 0; i < count(); i++) {
      Label start;
      __ bind(&start);
      DCHECK(is_int16(i));
      if (kArchVariant == kMips64r6) {
        __ li(kScratchReg, i);
        __ BranchShort(PROTECT, &done);
      } else {
        __ BranchShort(USE_DELAY_SLOT, &done);  // Expose delay slot.
        __ li(kScratchReg, i);                  // In the delay slot.
        __ nop();
      }

      DCHECK_EQ(table_entry_size_, masm()->SizeOfCodeGeneratedSince(&start));
    }

    DCHECK_EQ(masm()->SizeOfCodeGeneratedSince(&table_start),
              count() * table_entry_size_);
    __ bind(&done);
    __ Push(kScratchReg);
  } else {
    DCHECK_NE(kArchVariant, kMips64r6);
    // Uncommon case, the branch cannot reach.
    // Create mini trampoline to reach the end of the table
    for (int i = 0, j = 0; i < count(); i++, j++) {
      Label start;
      __ bind(&start);
      DCHECK(is_int16(i));
      if (j >= kMaxEntriesBranchReach) {
        j = 0;
        __ li(kScratchReg, i);
        __ bind(&trampoline_jump);
        trampoline_jump = Label();
        __ BranchShort(USE_DELAY_SLOT, &trampoline_jump);
        __ nop();
      } else {
        __ BranchShort(USE_DELAY_SLOT, &trampoline_jump);  // Expose delay slot.
        __ li(kScratchReg, i);                             // In the delay slot.
        __ nop();
      }
      DCHECK_EQ(table_entry_size_, masm()->SizeOfCodeGeneratedSince(&start));
    }

    DCHECK_EQ(masm()->SizeOfCodeGeneratedSince(&table_start),
              count() * table_entry_size_);
    __ bind(&trampoline_jump);
    __ Push(kScratchReg);
  }
}

bool Deoptimizer::PadTopOfStackRegister() { return false; }

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}


#undef __


}  // namespace internal
}  // namespace v8
