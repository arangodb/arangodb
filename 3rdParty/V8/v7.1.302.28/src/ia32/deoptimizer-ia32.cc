// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/assembler-inl.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {

const int Deoptimizer::table_entry_size_ = 10;

#define __ masm()->

void Deoptimizer::TableEntryGenerator::Generate() {
  Assembler::SupportsRootRegisterScope supports_root_register(masm());

  GeneratePrologue();

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  const int kDoubleRegsSize = kDoubleSize * XMMRegister::kNumRegisters;
  __ sub(esp, Immediate(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ movsd(Operand(esp, offset), xmm_reg);
  }

  STATIC_ASSERT(kFloatSize == kPointerSize);
  const int kFloatRegsSize = kFloatSize * XMMRegister::kNumRegisters;
  __ sub(esp, Immediate(kFloatRegsSize));
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int offset = code * kFloatSize;
    __ movss(Operand(esp, offset), xmm_reg);
  }

  __ pushad();

  ExternalReference c_entry_fp_address =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  __ mov(masm()->StaticVariable(c_entry_fp_address), ebp);

  const int kSavedRegistersAreaSize =
      kNumberOfRegisters * kPointerSize + kDoubleRegsSize + kFloatRegsSize;

  // Get the bailout id from the stack.
  __ mov(esi, Operand(esp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object
  // and compute the fp-to-sp delta in register edx.
  __ mov(ecx, Operand(esp, kSavedRegistersAreaSize + 1 * kPointerSize));
  __ lea(edx, Operand(esp, kSavedRegistersAreaSize + 2 * kPointerSize));

  __ sub(edx, ebp);
  __ neg(edx);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(6, eax);
  __ mov(eax, Immediate(0));
  Label context_check;
  __ mov(edi, Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(edi, &context_check);
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ mov(Operand(esp, 0 * kPointerSize), eax);  // Function.
  __ mov(Operand(esp, 1 * kPointerSize),
         Immediate(static_cast<int>(deopt_kind())));
  __ mov(Operand(esp, 2 * kPointerSize), esi);  // Bailout id.
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Code address or 0.
  __ mov(Operand(esp, 4 * kPointerSize), edx);  // Fp-to-sp delta.
  __ mov(Operand(esp, 5 * kPointerSize),
         Immediate(ExternalReference::isolate_address(isolate())));
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve deoptimizer object in register eax and get the input
  // frame descriptor pointer.
  __ mov(esi, Operand(eax, Deoptimizer::input_offset()));

  // Fill in the input registers.
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ pop(Operand(esi, offset));
  }

  int float_regs_offset = FrameDescription::float_registers_offset();
  // Fill in the float input registers.
  for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
    int dst_offset = i * kFloatSize + float_regs_offset;
    __ pop(Operand(esi, dst_offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Fill in the double input registers.
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset = code * kDoubleSize;
    __ movsd(xmm0, Operand(esp, src_offset));
    __ movsd(Operand(esi, dst_offset), xmm0);
  }

  // Clear FPU all exceptions.
  // TODO(ulan): Find out why the TOP register is not zero here in some cases,
  // and check that the generated code never deoptimizes with unbalanced stack.
  __ fnclex();

  // Remove the bailout id, return address and the double registers.
  __ add(esp, Immediate(kDoubleRegsSize + 2 * kPointerSize));

  // Compute a pointer to the unwinding limit in register ecx; that is
  // the first stack slot not part of the input frame.
  __ mov(ecx, Operand(esi, FrameDescription::frame_size_offset()));
  __ add(ecx, esp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ lea(edx, Operand(esi, FrameDescription::frame_content_offset()));
  Label pop_loop_header;
  __ jmp(&pop_loop_header);
  Label pop_loop;
  __ bind(&pop_loop);
  __ pop(Operand(edx, 0));
  __ add(edx, Immediate(sizeof(uint32_t)));
  __ bind(&pop_loop_header);
  __ cmp(ecx, esp);
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(eax);
  __ PrepareCallCFunction(1, esi);
  __ mov(Operand(esp, 0 * kPointerSize), eax);
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(eax);

  __ mov(esp, Operand(eax, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
  // Outer loop state: eax = current FrameDescription**, edx = one
  // past the last FrameDescription**.
  __ mov(edx, Operand(eax, Deoptimizer::output_count_offset()));
  __ mov(eax, Operand(eax, Deoptimizer::output_offset()));
  __ lea(edx, Operand(eax, edx, times_4, 0));
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: esi = current FrameDescription*, ecx = loop
  // index.
  __ mov(esi, Operand(eax, 0));
  __ mov(ecx, Operand(esi, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ sub(ecx, Immediate(sizeof(uint32_t)));
  __ push(Operand(esi, ecx, times_1, FrameDescription::frame_content_offset()));
  __ bind(&inner_loop_header);
  __ test(ecx, ecx);
  __ j(not_zero, &inner_push_loop);
  __ add(eax, Immediate(kPointerSize));
  __ bind(&outer_loop_header);
  __ cmp(eax, edx);
  __ j(below, &outer_push_loop);

  // In case of a failed STUB, we have to restore the XMM registers.
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ movsd(xmm_reg, Operand(esi, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ push(Operand(esi, FrameDescription::pc_offset()));
  __ push(Operand(esi, FrameDescription::continuation_offset()));

  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ push(Operand(esi, offset));
  }

  // Restore the registers from the stack.
  Assembler::AllowExplicitEbxAccessScope restoring_spilled_value(masm());
  __ popad();

  // Return to the continuation point.
  __ ret(0);
}


void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  // Create a sequence of deoptimization entries.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    __ push_imm32(i);
    __ jmp(&done);
    DCHECK(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
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

#endif  // V8_TARGET_ARCH_IA32
