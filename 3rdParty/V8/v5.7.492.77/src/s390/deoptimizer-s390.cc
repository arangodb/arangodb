// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer.h"
#include "src/codegen.h"
#include "src/full-codegen/full-codegen.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {

// LAY + LGHI/LHI + BRCL
const int Deoptimizer::table_entry_size_ = 16;

int Deoptimizer::patch_size() {
#if V8_TARGET_ARCH_S390X
  const int kCallInstructionSize = 16;
#else
  const int kCallInstructionSize = 10;
#endif
  return kCallInstructionSize;
}

void Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(Handle<Code> code) {
  // Empty because there is no need for relocation information for the code
  // patching in Deoptimizer::PatchCodeForDeoptimization below.
}

void Deoptimizer::PatchCodeForDeoptimization(Isolate* isolate, Code* code) {
  Address code_start_address = code->instruction_start();

  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  if (FLAG_zap_code_space) {
    // Fail hard and early if we enter this code object again.
    byte* pointer = code->FindCodeAgeSequence();
    if (pointer != NULL) {
      pointer += kNoCodeAgeSequenceLength;
    } else {
      pointer = code->instruction_start();
    }
    CodePatcher patcher(isolate, pointer, 2);
    patcher.masm()->bkpt(0);

    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(code->deoptimization_data());
    int osr_offset = data->OsrPcOffset()->value();
    if (osr_offset > 0) {
      CodePatcher osr_patcher(isolate, code->instruction_start() + osr_offset,
                              2);
      osr_patcher.masm()->bkpt(0);
    }
  }

  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  // For each LLazyBailout instruction insert a call to the corresponding
  // deoptimization entry.
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    Address call_address = code_start_address + deopt_data->Pc(i)->value();
    Address deopt_entry = GetDeoptimizationEntry(isolate, i, LAZY);
    // We need calls to have a predictable size in the unoptimized code, but
    // this is optimized code, so we don't have to have a predictable size.
    int call_size_in_bytes = MacroAssembler::CallSizeNotPredictableCodeSize(
        deopt_entry, kRelocInfo_NONEPTR);
    DCHECK(call_size_in_bytes <= patch_size());
    CodePatcher patcher(isolate, call_address, call_size_in_bytes);
    patcher.masm()->Call(deopt_entry, kRelocInfo_NONEPTR);
    DCHECK(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    DCHECK(call_address + patch_size() <= code->instruction_end());
#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }
}

void Deoptimizer::SetPlatformCompiledStubRegisters(
    FrameDescription* output_frame, CodeStubDescriptor* descriptor) {
  ApiFunction function(descriptor->deoptimization_handler());
  ExternalReference xref(&function, ExternalReference::BUILTIN_CALL, isolate_);
  intptr_t handler = reinterpret_cast<intptr_t>(xref.address());
  int params = descriptor->GetHandlerParameterCount();
  output_frame->SetRegister(r2.code(), params);
  output_frame->SetRegister(r3.code(), handler);
}

void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < DoubleRegister::kNumRegisters; ++i) {
    double double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
}

#define __ masm()->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::TableEntryGenerator::Generate() {
  GeneratePrologue();

  // Save all the registers onto the stack
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;

  // Save all double registers before messing with them.
  __ lay(sp, MemOperand(sp, -kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ StoreDouble(dreg, MemOperand(sp, offset));
  }

  // Push all GPRs onto the stack
  __ lay(sp, MemOperand(sp, -kNumberOfRegisters * kPointerSize));
  __ StoreMultipleP(r0, sp, MemOperand(sp));  // Save all 16 registers

  __ mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ StoreP(fp, MemOperand(ip));

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize;

  // Get the bailout id from the stack.
  __ LoadP(r4, MemOperand(sp, kSavedRegistersAreaSize));

  // Cleanse the Return address for 31-bit
  __ CleanseP(r14);

  // Get the address of the location in the code object (r5)(return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r6.
  __ LoadRR(r5, r14);
  __ la(r6, MemOperand(sp, kSavedRegistersAreaSize + (1 * kPointerSize)));
  __ SubP(r6, fp, r6);

  // Allocate a new deoptimizer object.
  // Pass six arguments in r2 to r7.
  __ PrepareCallCFunction(6, r7);
  __ LoadImmP(r2, Operand::Zero());
  Label context_check;
  __ LoadP(r3, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r3, &context_check);
  __ LoadP(r2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ LoadImmP(r3, Operand(type()));  // bailout type,
  // r4: bailout id already loaded.
  // r5: code address or 0 already loaded.
  // r6: Fp-to-sp delta.
  // Parm6: isolate is passed on the stack.
  __ mov(r7, Operand(ExternalReference::isolate_address(isolate())));
  __ StoreP(r7, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));

  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }

  // Preserve "deoptimizer" object in register r2 and get the input
  // frame descriptor pointer to r3 (deoptimizer->input_);
  __ LoadP(r3, MemOperand(r2, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  // DCHECK(Register::kNumRegisters == kNumberOfRegisters);
  // __ mvc(MemOperand(r3, FrameDescription::registers_offset()),
  //        MemOperand(sp), kNumberOfRegisters * kPointerSize);
  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  // TODO(john.yan): optimize the following code by using mvc instruction
  DCHECK(Register::kNumRegisters == kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ LoadP(r4, MemOperand(sp, i * kPointerSize));
    __ StoreP(r4, MemOperand(r3, offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy double registers to
  // double_registers_[DoubleRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset = code * kDoubleSize + kNumberOfRegisters * kPointerSize;
    // TODO(joransiu): MVC opportunity
    __ LoadDouble(d0, MemOperand(sp, src_offset));
    __ StoreDouble(d0, MemOperand(r3, dst_offset));
  }

  // Remove the bailout id and the saved registers from the stack.
  __ la(sp, MemOperand(sp, kSavedRegistersAreaSize + (1 * kPointerSize)));

  // Compute a pointer to the unwinding limit in register r4; that is
  // the first stack slot not part of the input frame.
  __ LoadP(r4, MemOperand(r3, FrameDescription::frame_size_offset()));
  __ AddP(r4, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ la(r5, MemOperand(r3, FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ b(&pop_loop_header, Label::kNear);
  __ bind(&pop_loop);
  __ pop(r6);
  __ StoreP(r6, MemOperand(r5, 0));
  __ la(r5, MemOperand(r5, kPointerSize));
  __ bind(&pop_loop_header);
  __ CmpP(r4, sp);
  __ bne(&pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r2);  // Preserve deoptimizer object across call.
  // r2: deoptimizer object; r3: scratch.
  __ PrepareCallCFunction(1, r3);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 1);
  }
  __ pop(r2);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadP(sp, MemOperand(r2, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r6 = current "FrameDescription** output_",
  // r3 = one past the last FrameDescription**.
  __ LoadlW(r3, MemOperand(r2, Deoptimizer::output_count_offset()));
  __ LoadP(r6, MemOperand(r2, Deoptimizer::output_offset()));  // r6 is output_.
  __ ShiftLeftP(r3, r3, Operand(kPointerSizeLog2));
  __ AddP(r3, r6, r3);
  __ b(&outer_loop_header, Label::kNear);

  __ bind(&outer_push_loop);
  // Inner loop state: r4 = current FrameDescription*, r5 = loop index.
  __ LoadP(r4, MemOperand(r6, 0));  // output_[ix]
  __ LoadP(r5, MemOperand(r4, FrameDescription::frame_size_offset()));
  __ b(&inner_loop_header, Label::kNear);

  __ bind(&inner_push_loop);
  __ AddP(r5, Operand(-sizeof(intptr_t)));
  __ AddP(r8, r4, r5);
  __ LoadP(r8, MemOperand(r8, FrameDescription::frame_content_offset()));
  __ push(r8);

  __ bind(&inner_loop_header);
  __ CmpP(r5, Operand::Zero());
  __ bne(&inner_push_loop);  // test for gt?

  __ AddP(r6, r6, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ CmpP(r6, r3);
  __ blt(&outer_push_loop);

  __ LoadP(r3, MemOperand(r2, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ ld(dreg, MemOperand(r3, src_offset));
  }

  // Push state, pc, and continuation from the last output frame.
  __ LoadP(r8, MemOperand(r4, FrameDescription::state_offset()));
  __ push(r8);
  __ LoadP(r8, MemOperand(r4, FrameDescription::pc_offset()));
  __ push(r8);
  __ LoadP(r8, MemOperand(r4, FrameDescription::continuation_offset()));
  __ push(r8);

  // Restore the registers from the last output frame.
  __ LoadRR(r1, r4);
  for (int i = kNumberOfRegisters - 1; i > 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs & (1 << i)) != 0) {
      __ LoadP(ToRegister(i), MemOperand(r1, offset));
    }
  }

  __ InitializeRootRegister();

  __ pop(ip);  // get continuation, leave pc on stack
  __ pop(r14);
  __ Jump(ip);
  __ stop("Unreachable.");
}

void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  // Create a sequence of deoptimization entries. Note that any
  // registers may be still live.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    __ lay(sp, MemOperand(sp, -kPointerSize));
    __ LoadImmP(ip, Operand(i));
    __ b(&done);
    int end = masm()->pc_offset();
    USE(end);
    DCHECK(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
  __ StoreP(ip, MemOperand(sp));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No out-of-line constant pool support.
  UNREACHABLE();
}

#undef __

}  // namespace internal
}  // namespace v8
