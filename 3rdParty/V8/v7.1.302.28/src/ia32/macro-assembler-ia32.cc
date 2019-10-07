// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/debug/debug.h"
#include "src/external-reference-table.h"
#include "src/frame-constants.h"
#include "src/frames-inl.h"
#include "src/instruction-stream.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/snapshot.h"

#include "src/ia32/assembler-ia32-inl.h"
#include "src/ia32/macro-assembler-ia32.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// MacroAssembler implementation.

MacroAssembler::MacroAssembler(Isolate* isolate,
                               const AssemblerOptions& options, void* buffer,
                               int size, CodeObjectRequired create_code_object)
    : TurboAssembler(isolate, options, buffer, size, create_code_object) {
  if (create_code_object == CodeObjectRequired::kYes) {
    // Unlike TurboAssembler, which can be used off the main thread and may not
    // allocate, macro assembler creates its own copy of the self-reference
    // marker in order to disambiguate between self-references during nested
    // code generation (e.g.: codegen of the current object triggers stub
    // compilation through CodeStub::GetCode()).
    code_object_ = Handle<HeapObject>::New(
        *isolate->factory()->NewSelfReferenceMarker(), isolate);
  }

#ifdef V8_EMBEDDED_BUILTINS
  // Fake it as long as we use indirections through an embedded external
  // reference. This will let us implement indirections without a real
  // root register.
  // TODO(jgruber, v8:6666): Remove once a real root register exists.
  if (FLAG_embedded_builtins) set_root_array_available(true);
#endif  // V8_EMBEDDED_BUILTINS
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  // TODO(jgruber, v8:6666): Support loads through the root register once it
  // exists.
  if (isolate()->heap()->RootCanBeTreatedAsConstant(index)) {
    Handle<Object> object = isolate()->heap()->root_handle(index);
    if (object->IsSmi()) {
      mov(destination, Immediate(Smi::cast(*object)));
      return;
    } else if (!options().isolate_independent_code) {
      DCHECK(object->IsHeapObject());
      mov(destination, Handle<HeapObject>::cast(object));
      return;
    }
  }
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  mov(destination, Immediate(static_cast<int>(index)));
  mov(destination,
      StaticArray(destination, times_pointer_size, roots_array_start));
}

void MacroAssembler::CompareRoot(Register with, Register scratch,
                                 RootIndex index) {
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  mov(scratch, Immediate(static_cast<int>(index)));
  cmp(with, StaticArray(scratch, times_pointer_size, roots_array_start));
}

void MacroAssembler::CompareRoot(Register with, RootIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  Handle<Object> object = isolate()->heap()->root_handle(index);
  if (object->IsHeapObject()) {
    cmp(with, Handle<HeapObject>::cast(object));
  } else {
    cmp(with, Immediate(Smi::cast(*object)));
  }
}

void MacroAssembler::CompareRoot(Operand with, RootIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  Handle<Object> object = isolate()->heap()->root_handle(index);
  if (object->IsHeapObject()) {
    cmp(with, Handle<HeapObject>::cast(object));
  } else {
    cmp(with, Immediate(Smi::cast(*object)));
  }
}

void MacroAssembler::PushRoot(RootIndex index) {
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(index));
  Handle<Object> object = isolate()->heap()->root_handle(index);
  if (object->IsHeapObject()) {
    Push(Handle<HeapObject>::cast(object));
  } else {
    Push(Smi::cast(*object));
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  DCHECK(!is_ebx_addressable_);
  DCHECK(isolate()->heap()->RootCanBeTreatedAsConstant(
      RootIndex::kBuiltinsConstantsTable));
  // TODO(jgruber): LoadRoot should be a register-relative load once we have
  // the kRootRegister.
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  mov(destination,
      FieldOperand(destination,
                   FixedArray::kHeaderSize + constant_index * kPointerSize));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  DCHECK(!is_ebx_addressable_);
  DCHECK(is_int32(offset));
  // TODO(jgruber): Register-relative load once kRootRegister exists.
  mov(destination, Immediate(ExternalReference::roots_array_start(isolate())));
  if (offset != 0) {
    add(destination, Immediate(offset));
  }
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  DCHECK(!is_ebx_addressable_);
  // TODO(jgruber): Register-relative load once kRootRegister exists.
  LoadRootRegisterOffset(destination, offset);
  mov(destination, Operand(destination, 0));
}

void TurboAssembler::LoadAddress(Register destination,
                                 ExternalReference source) {
  if (FLAG_embedded_builtins) {
    if (root_array_available_ && options().isolate_independent_code) {
      IndirectLoadExternalReference(destination, source);
      return;
    }
  }
  mov(destination, Immediate(source));
}

Operand TurboAssembler::StaticVariable(const ExternalReference& ext) {
  // TODO(jgruber,v8:6666): Root-relative operand once kRootRegister exists.
  return Operand(ext.address(), RelocInfo::EXTERNAL_REFERENCE);
}

Operand TurboAssembler::StaticArray(Register index, ScaleFactor scale,
                                    const ExternalReference& ext) {
  // TODO(jgruber,v8:6666): Root-relative operand once kRootRegister exists.
  return Operand(index, scale, ext.address(), RelocInfo::EXTERNAL_REFERENCE);
}

static constexpr Register saved_regs[] = {eax, ecx, edx};

static constexpr int kNumberOfSavedRegs = sizeof(saved_regs) / sizeof(Register);

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      bytes += kPointerSize;
    }
  }

  if (fp_mode == kSaveFPRegs) {
    // Count all XMM registers except XMM0.
    bytes += kDoubleSize * (XMMRegister::kNumRegisters - 1);
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      push(reg);
      bytes += kPointerSize;
    }
  }

  if (fp_mode == kSaveFPRegs) {
    // Save all XMM registers except XMM0.
    int delta = kDoubleSize * (XMMRegister::kNumRegisters - 1);
    sub(esp, Immediate(delta));
    for (int i = XMMRegister::kNumRegisters - 1; i > 0; i--) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(Operand(esp, (i - 1) * kDoubleSize), reg);
    }
    bytes += delta;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    // Restore all XMM registers except XMM0.
    int delta = kDoubleSize * (XMMRegister::kNumRegisters - 1);
    for (int i = XMMRegister::kNumRegisters - 1; i > 0; i--) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(reg, Operand(esp, (i - 1) * kDoubleSize));
    }
    add(esp, Immediate(delta));
    bytes += delta;
  }

  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      pop(reg);
      bytes += kPointerSize;
    }
  }

  return bytes;
}

void MacroAssembler::DoubleToI(Register result_reg, XMMRegister input_reg,
                               XMMRegister scratch, Label* lost_precision,
                               Label* is_nan, Label::Distance dst) {
  DCHECK(input_reg != scratch);
  cvttsd2si(result_reg, Operand(input_reg));
  Cvtsi2sd(scratch, Operand(result_reg));
  ucomisd(scratch, input_reg);
  j(not_equal, lost_precision, dst);
  j(parity_even, is_nan, dst);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register dst,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  lea(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    test_b(dst, Immediate(kPointerSize - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(dst, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      push(Register::from_code(i));
    }
  }
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  for (int i = Register::kNumRegisters - 1; i >= 0; --i) {
    if ((registers >> i) & 1u) {
      pop(Register::from_code(i));
    }
  }
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
  // TODO(albertnetymk): For now we ignore remembered_set_action and fp_mode,
  // i.e. always emit remember set and save FP registers in RecordWriteStub. If
  // large performance regression is observed, we should use these values to
  // avoid unnecessary work.

  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kRecordWrite);
  RegList registers = callable.descriptor().allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kObject));
  Register slot_parameter(
      callable.descriptor().GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register remembered_set_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kFPMode));

  push(object);
  push(address);

  pop(slot_parameter);
  pop(object_parameter);

  Move(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Move(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(callable.code(), RelocInfo::CODE_TARGET);

  RestoreRegisters(registers);
}

void MacroAssembler::RecordWrite(Register object, Register address,
                                 Register value, SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(object != value);
  DCHECK(object != address);
  DCHECK(value != address);
  AssertNotSmi(object);

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    Label ok;
    cmp(value, Operand(address, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    // Skip barrier if writing a smi.
    JumpIfSmi(value, &done, Label::kNear);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, zero, &done,
                Label::kNear);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Immediate(bit_cast<int32_t>(kZapValue)));
    mov(value, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  mov(eax, StaticVariable(restart_fp));
  test(eax, eax);
  j(not_zero, BUILTIN_CODE(isolate(), FrameDropperTrampoline),
    RelocInfo::CODE_TARGET);
}

void TurboAssembler::Cvtsi2ss(XMMRegister dst, Operand src) {
  xorps(dst, dst);
  cvtsi2ss(dst, src);
}

void TurboAssembler::Cvtsi2sd(XMMRegister dst, Operand src) {
  xorpd(dst, dst);
  cvtsi2sd(dst, src);
}

void TurboAssembler::Cvtui2ss(XMMRegister dst, Operand src, Register tmp) {
  Label done;
  Register src_reg = src.is_reg_only() ? src.reg() : tmp;
  if (src_reg == tmp) mov(tmp, src);
  cvtsi2ss(dst, src_reg);
  test(src_reg, src_reg);
  j(positive, &done, Label::kNear);

  // Compute {src/2 | (src&1)} (retain the LSB to avoid rounding errors).
  if (src_reg != tmp) mov(tmp, src_reg);
  shr(tmp, 1);
  // The LSB is shifted into CF. If it is set, set the LSB in {tmp}.
  Label msb_not_set;
  j(not_carry, &msb_not_set, Label::kNear);
  or_(tmp, Immediate(1));
  bind(&msb_not_set);
  cvtsi2ss(dst, tmp);
  addss(dst, dst);
  bind(&done);
}

void TurboAssembler::Cvttss2ui(Register dst, Operand src, XMMRegister tmp) {
  Label done;
  cvttss2si(dst, src);
  test(dst, dst);
  j(positive, &done);
  Move(tmp, static_cast<float>(INT32_MIN));
  addss(tmp, src);
  cvttss2si(dst, tmp);
  or_(dst, Immediate(0x80000000));
  bind(&done);
}

void TurboAssembler::Cvtui2sd(XMMRegister dst, Operand src) {
  Label done;
  cmp(src, Immediate(0));
  ExternalReference uint32_bias = ExternalReference::address_of_uint32_bias();
  Cvtsi2sd(dst, src);
  j(not_sign, &done, Label::kNear);
  addsd(dst, StaticVariable(uint32_bias));
  bind(&done);
}

void TurboAssembler::Cvttsd2ui(Register dst, Operand src, XMMRegister tmp) {
  Move(tmp, -2147483648.0);
  addsd(tmp, src);
  cvttsd2si(dst, tmp);
  add(dst, Immediate(0x80000000));
}

void TurboAssembler::ShlPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(high, low);
    shl(high, shift - 32);
    xor_(low, low);
  } else {
    shld(high, low, shift);
    shl(low, shift);
  }
}

void TurboAssembler::ShlPair_cl(Register high, Register low) {
  shld_cl(high, low);
  shl_cl(low);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(high, low);
  xor_(low, low);
  bind(&done);
}

void TurboAssembler::ShrPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(low, high);
    shr(low, shift - 32);
    xor_(high, high);
  } else {
    shrd(high, low, shift);
    shr(high, shift);
  }
}

void TurboAssembler::ShrPair_cl(Register high, Register low) {
  shrd_cl(low, high);
  shr_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  xor_(high, high);
  bind(&done);
}

void TurboAssembler::SarPair(Register high, Register low, uint8_t shift) {
  if (shift >= 32) {
    mov(low, high);
    sar(low, shift - 32);
    sar(high, 31);
  } else {
    shrd(high, low, shift);
    sar(high, shift);
  }
}

void TurboAssembler::SarPair_cl(Register high, Register low) {
  shrd_cl(low, high);
  sar_cl(high);
  Label done;
  test(ecx, Immediate(0x20));
  j(equal, &done, Label::kNear);
  mov(low, high);
  sar(high, 31);
  bind(&done);
}

void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  mov(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpw(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(equal, AbortReason::kOperandIsNotASmi);
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAConstructor);
    Push(object);
    mov(object, FieldOperand(object, HeapObject::kMapOffset));
    test_b(FieldOperand(object, Map::kBitFieldOffset),
           Immediate(Map::IsConstructorBit::kMask));
    Pop(object);
    Check(not_zero, AbortReason::kOperandIsNotAConstructor);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
    Push(object);
    CmpObjectType(object, JS_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotAFunction);
  }
}


void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotABoundFunction);
    Push(object);
    CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;

  test(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  {
    Push(object);
    Register map = object;

    // Load map
    mov(map, FieldOperand(object, HeapObject::kMapOffset));

    Label do_check;
    // Check if JSGeneratorObject
    CmpInstanceType(map, JS_GENERATOR_OBJECT_TYPE);
    j(equal, &do_check, Label::kNear);

    // Check if JSAsyncGeneratorObject
    CmpInstanceType(map, JS_ASYNC_GENERATOR_OBJECT_TYPE);

    bind(&do_check);
    Pop(object);
  }

  Check(equal, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    cmp(object, isolate()->factory()->undefined_value());
    j(equal, &done_checking);
    cmp(FieldOperand(object, 0),
        Immediate(isolate()->factory()->allocation_site_map()));
    Assert(equal, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    test(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmi);
  }
}

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(Immediate(StackFrame::TypeToMarker(type)));
}

void TurboAssembler::Prologue() {
  push(ebp);  // Caller's frame pointer.
  mov(ebp, esp);
  push(esi);  // Callee's context.
  push(edi);  // Callee's JS function.
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  push(ebp);
  mov(ebp, esp);
  push(Immediate(StackFrame::TypeToMarker(type)));
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    cmp(Operand(ebp, CommonFrameConstants::kContextOrFrameTypeOffset),
        Immediate(StackFrame::TypeToMarker(type)));
    Check(equal, AbortReason::kStackFrameTypesMustMatch);
  }
  leave();
}

#ifdef V8_OS_WIN
void TurboAssembler::AllocateStackFrame(Register bytes_scratch) {
  // In windows, we cannot increment the stack size by more than one page
  // (minimum page size is 4KB) without accessing at least one byte on the
  // page. Check this:
  // https://msdn.microsoft.com/en-us/library/aa227153(v=vs.60).aspx.
  constexpr int kPageSize = 4 * 1024;
  Label check_offset;
  Label touch_next_page;
  jmp(&check_offset);
  bind(&touch_next_page);
  sub(esp, Immediate(kPageSize));
  // Just to touch the page, before we increment further.
  mov(Operand(esp, 0), Immediate(0));
  sub(bytes_scratch, Immediate(kPageSize));

  bind(&check_offset);
  cmp(bytes_scratch, kPageSize);
  j(greater, &touch_next_page);

  sub(esp, bytes_scratch);
}
#endif

void MacroAssembler::EnterBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Push(ebp);
  Move(ebp, esp);
  Push(context);
  Push(target);
  Push(argc);
}

void MacroAssembler::LeaveBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Pop(argc);
  Pop(target);
  Pop(context);
  leave();
}

void MacroAssembler::EnterExitFramePrologue(StackFrame::Type frame_type) {
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  DCHECK_EQ(+2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(+1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  push(ebp);
  mov(ebp, esp);

  // Reserve room for entry stack pointer and push the code object.
  push(Immediate(StackFrame::TypeToMarker(frame_type)));
  DCHECK_EQ(-2 * kPointerSize, ExitFrameConstants::kSPOffset);
  push(Immediate(0));  // Saved entry sp, patched before call.
  DCHECK_EQ(-3 * kPointerSize, ExitFrameConstants::kCodeOffset);
  push(Immediate(CodeObject()));  // Accessed from ExitFrame::code_slot.

  STATIC_ASSERT(edx == kRuntimeCallFunctionRegister);
  STATIC_ASSERT(esi == kContextRegister);

  // Save the frame pointer and the context in top.
  ExternalReference c_entry_fp_address =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  ExternalReference context_address =
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate());
  ExternalReference c_function_address =
      ExternalReference::Create(IsolateAddressId::kCFunctionAddress, isolate());
  mov(StaticVariable(c_entry_fp_address), ebp);
  mov(StaticVariable(context_address), esi);
  mov(StaticVariable(c_function_address), edx);
}


void MacroAssembler::EnterExitFrameEpilogue(int argc, bool save_doubles) {
  // Optionally save all XMM registers.
  if (save_doubles) {
    int space = XMMRegister::kNumRegisters * kDoubleSize + argc * kPointerSize;
    sub(esp, Immediate(space));
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(Operand(ebp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else {
    sub(esp, Immediate(argc * kPointerSize));
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = base::OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo(kFrameAlignment));
    and_(esp, -kFrameAlignment);
  }

  // Patch the saved entry sp.
  mov(Operand(ebp, ExitFrameConstants::kSPOffset), esp);
}

void MacroAssembler::EnterExitFrame(int argc, bool save_doubles,
                                    StackFrame::Type frame_type) {
  EnterExitFramePrologue(frame_type);

  // Set up argc and argv in callee-saved registers.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  mov(edi, eax);
  lea(esi, Operand(ebp, eax, times_4, offset));

  // Reserve space for argc, argv and isolate.
  EnterExitFrameEpilogue(argc, save_doubles);
}


void MacroAssembler::EnterApiExitFrame(int argc) {
  EnterExitFramePrologue(StackFrame::EXIT);
  EnterExitFrameEpilogue(argc, false);
}


void MacroAssembler::LeaveExitFrame(bool save_doubles, bool pop_arguments) {
  // Optionally restore all XMM registers.
  if (save_doubles) {
    const int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(reg, Operand(ebp, offset - ((i + 1) * kDoubleSize)));
    }
  }

  if (pop_arguments) {
    // Get the return address from the stack and restore the frame pointer.
    mov(ecx, Operand(ebp, 1 * kPointerSize));
    mov(ebp, Operand(ebp, 0 * kPointerSize));

    // Pop the arguments and the receiver from the caller stack.
    lea(esp, Operand(esi, 1 * kPointerSize));

    // Push the return address to get ready to return.
    push(ecx);
  } else {
    // Otherwise just leave the exit frame.
    leave();
  }

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveExitFrameEpilogue() {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address =
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate());
  mov(esi, StaticVariable(context_address));
#ifdef DEBUG
  mov(StaticVariable(context_address), Immediate(Context::kInvalidContext));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address =
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate());
  mov(StaticVariable(c_entry_fp_address), Immediate(0));
}

void MacroAssembler::LeaveApiExitFrame() {
  mov(esp, ebp);
  pop(ebp);

  LeaveExitFrameEpilogue();
}


void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  push(Immediate(0));  // Padding.

  // Link the current handler as the next handler.
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  push(StaticVariable(handler_address));

  // Set this new handler as the current one.
  mov(StaticVariable(handler_address), esp);
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address =
      ExternalReference::Create(IsolateAddressId::kHandlerAddress, isolate());
  pop(StaticVariable(handler_address));
  add(esp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


void MacroAssembler::CallStub(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET);
}

void TurboAssembler::CallStubDelayed(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs.
  call(stub);
}

void MacroAssembler::TailCallStub(CodeStub* stub) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}

bool TurboAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame() || !stub->SometimesSetsUpAFrame();
}

void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Move(kRuntimeCallArgCountRegister, Immediate(num_arguments));
  Move(kRuntimeCallFunctionRegister, Immediate(ExternalReference::Create(f)));
  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
}

void TurboAssembler::CallRuntimeWithCEntry(Runtime::FunctionId fid,
                                           Register centry) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Move(kRuntimeCallArgCountRegister, Immediate(f->nargs));
  Move(kRuntimeCallFunctionRegister, Immediate(ExternalReference::Create(f)));
  DCHECK(!AreAliased(centry, kRuntimeCallArgCountRegister,
                     kRuntimeCallFunctionRegister));
  add(centry, Immediate(Code::kHeaderSize - kHeapObjectTag));
  Call(centry);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  // ----------- S t a t e -------------
  //  -- esp[0]                 : return address
  //  -- esp[8]                 : argument num_arguments - 1
  //  ...
  //  -- esp[8 * num_arguments] : argument 0 (receiver)
  //
  //  For runtime functions with variable arguments:
  //  -- eax                    : number of  arguments
  // -----------------------------------

  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    // TODO(1236192): Most runtime routines don't need the number of
    // arguments passed in because it is constant. At some point we
    // should remove this need and make the runtime routine entry code
    // smarter.
    Move(kRuntimeCallArgCountRegister, Immediate(function->nargs));
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             bool builtin_exit_frame) {
  // Set the entry point and jump to the C entry runtime stub.
  Move(kRuntimeCallFunctionRegister, Immediate(ext));
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs,
                                          kArgvOnStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToInstructionStream(Address entry) {
  jmp(entry, RelocInfo::OFF_HEAP_TARGET);
}

void TurboAssembler::PrepareForTailCall(
    const ParameterCount& callee_args_count, Register caller_args_count_reg,
    Register scratch0, Register scratch1,
    int number_of_temp_values_after_return_address) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
#endif

  // Calculate the destination address where we will put the return address
  // after we drop current frame.
  Register new_sp_reg = scratch0;
  if (callee_args_count.is_reg()) {
    sub(caller_args_count_reg, callee_args_count.reg());
    lea(new_sp_reg,
        Operand(ebp, caller_args_count_reg, times_pointer_size,
                StandardFrameConstants::kCallerPCOffset -
                    number_of_temp_values_after_return_address * kPointerSize));
  } else {
    lea(new_sp_reg, Operand(ebp, caller_args_count_reg, times_pointer_size,
                            StandardFrameConstants::kCallerPCOffset -
                                (callee_args_count.immediate() +
                                 number_of_temp_values_after_return_address) *
                                    kPointerSize));
  }

  if (FLAG_debug_code) {
    cmp(esp, new_sp_reg);
    Check(below, AbortReason::kStackAccessBelowStackPointer);
  }

  // Copy return address from caller's frame to current frame's return address
  // to avoid its trashing and let the following loop copy it to the right
  // place.
  Register tmp_reg = scratch1;
  mov(tmp_reg, Operand(ebp, StandardFrameConstants::kCallerPCOffset));
  mov(Operand(esp, number_of_temp_values_after_return_address * kPointerSize),
      tmp_reg);

  // Restore caller's frame pointer now as it could be overwritten by
  // the copying loop.
  mov(ebp, Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // +2 here is to copy both receiver and return address.
  Register count_reg = caller_args_count_reg;
  if (callee_args_count.is_reg()) {
    lea(count_reg, Operand(callee_args_count.reg(),
                           2 + number_of_temp_values_after_return_address));
  } else {
    mov(count_reg, Immediate(callee_args_count.immediate() + 2 +
                             number_of_temp_values_after_return_address));
    // TODO(ishell): Unroll copying loop for small immediate values.
  }

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).
  Label loop, entry;
  jmp(&entry, Label::kNear);
  bind(&loop);
  dec(count_reg);
  mov(tmp_reg, Operand(esp, count_reg, times_pointer_size, 0));
  mov(Operand(new_sp_reg, count_reg, times_pointer_size, 0), tmp_reg);
  bind(&entry);
  cmp(count_reg, Immediate(0));
  j(not_equal, &loop, Label::kNear);

  // Leave current frame.
  mov(esp, new_sp_reg);
}

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual, Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance done_near) {
  DCHECK_IMPLIES(expected.is_reg(), expected.reg() == ecx);
  DCHECK_IMPLIES(actual.is_reg(), actual.reg() == eax);

  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label invoke;
  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    mov(eax, actual.immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        mov(ecx, expected.immediate());
      }
    }
  } else {
    if (actual.is_immediate()) {
      // Expected is in register, actual is immediate. This is the
      // case when we invoke function values without going through the
      // IC mechanism.
      mov(eax, actual.immediate());
      cmp(expected.reg(), actual.immediate());
      j(equal, &invoke);
      DCHECK(expected.reg() == ecx);
    } else if (expected.reg() != actual.reg()) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmp(expected.reg(), actual.reg());
      j(equal, &invoke);
      DCHECK(actual.reg() == eax);
      DCHECK(expected.reg() == ecx);
    } else {
      definitely_matches = true;
      Move(eax, actual.reg());
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      Call(adaptor, RelocInfo::CODE_TARGET);
      if (!*definitely_mismatches) {
        jmp(done, done_near);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual) {
  Label skip_hook;

  ExternalReference debug_hook_active =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  cmpb(StaticVariable(debug_hook_active), Immediate(0));
  j(equal, &skip_hook);

  {
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
    if (expected.is_reg()) {
      SmiTag(expected.reg());
      Push(expected.reg());
    }
    if (actual.is_reg()) {
      SmiTag(actual.reg());
      Push(actual.reg());
      SmiUntag(actual.reg());
    }
    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun);
    Push(fun);
    Operand receiver_op =
        actual.is_reg()
            ? Operand(ebp, actual.reg(), times_pointer_size, kPointerSize * 2)
            : Operand(ebp, actual.immediate() * times_pointer_size +
                               kPointerSize * 2);
    Push(receiver_op);
    CallRuntime(Runtime::kDebugOnFunctionCall);
    Pop(fun);
    if (new_target.is_valid()) {
      Pop(new_target);
    }
    if (actual.is_reg()) {
      Pop(actual.reg());
      SmiUntag(actual.reg());
    }
    if (expected.is_reg()) {
      Pop(expected.reg());
      SmiUntag(expected.reg());
    }
  }
  bind(&skip_hook);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function == edi);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == edx);
  DCHECK_IMPLIES(expected.is_reg(), expected.reg() == ecx);
  DCHECK_IMPLIES(actual.is_reg(), actual.reg() == eax);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    mov(edx, isolate()->factory()->undefined_value());
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag,
                 Label::kNear);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    static_assert(kJavaScriptCallCodeStartRegister == ecx, "ABI mismatch");
    mov(ecx, FieldOperand(function, JSFunction::kCodeOffset));
    add(ecx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    if (flag == CALL_FUNCTION) {
      call(ecx);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      jmp(ecx);
    }
    bind(&done);
  }
}

void MacroAssembler::InvokeFunction(Register fun, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(fun == edi);
  mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  movzx_w(ecx,
          FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(ecx);
  InvokeFunctionCode(edi, new_target, expected, actual, flag);
}

void MacroAssembler::LoadGlobalProxy(Register dst) {
  mov(dst, NativeContextOperand());
  mov(dst, ContextOperand(dst, Context::GLOBAL_PROXY_INDEX));
}

void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the native context from the current context.
  mov(function, NativeContextOperand());
  // Load the function from the native context.
  mov(function, ContextOperand(function, index));
}

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the lowest encoding,
  // which means that lowest encodings are furthest away from
  // the stack pointer.
  DCHECK(reg_code >= 0 && reg_code < kNumSafepointRegisters);
  return kNumSafepointRegisters - reg_code - 1;
}

void TurboAssembler::Ret() { ret(0); }

void TurboAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    pop(scratch);
    add(esp, Immediate(bytes_dropped));
    push(scratch);
    ret(0);
  }
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    add(esp, Immediate(stack_elements * kPointerSize));
  }
}

void TurboAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    mov(dst, src);
  }
}

void TurboAssembler::Move(Register dst, const Immediate& src) {
  if (!src.is_heap_object_request() && src.is_zero()) {
    xor_(dst, dst);  // Shorter than mov of 32-bit immediate 0.
  } else if (src.is_external_reference()) {
    LoadAddress(dst, src.external_reference());
  } else {
    mov(dst, src);
  }
}

void TurboAssembler::Move(Operand dst, const Immediate& src) { mov(dst, src); }

void TurboAssembler::Move(Register dst, Handle<HeapObject> src) {
  if (root_array_available_ && options().isolate_independent_code) {
    IndirectLoadConstant(dst, src);
    return;
  }
  mov(dst, src);
}

void TurboAssembler::Move(XMMRegister dst, uint32_t src) {
  if (src == 0) {
    pxor(dst, dst);
  } else {
    unsigned cnt = base::bits::CountPopulation(src);
    unsigned nlz = base::bits::CountLeadingZeros32(src);
    unsigned ntz = base::bits::CountTrailingZeros32(src);
    if (nlz + cnt + ntz == 32) {
      pcmpeqd(dst, dst);
      if (ntz == 0) {
        psrld(dst, 32 - cnt);
      } else {
        pslld(dst, 32 - cnt);
        if (nlz != 0) psrld(dst, nlz);
      }
    } else {
      push(eax);
      mov(eax, Immediate(src));
      movd(dst, Operand(eax));
      pop(eax);
    }
  }
}

void TurboAssembler::Move(XMMRegister dst, uint64_t src) {
  if (src == 0) {
    pxor(dst, dst);
  } else {
    uint32_t lower = static_cast<uint32_t>(src);
    uint32_t upper = static_cast<uint32_t>(src >> 32);
    unsigned cnt = base::bits::CountPopulation(src);
    unsigned nlz = base::bits::CountLeadingZeros64(src);
    unsigned ntz = base::bits::CountTrailingZeros64(src);
    if (nlz + cnt + ntz == 64) {
      pcmpeqd(dst, dst);
      if (ntz == 0) {
        psrlq(dst, 64 - cnt);
      } else {
        psllq(dst, 64 - cnt);
        if (nlz != 0) psrlq(dst, nlz);
      }
    } else if (lower == 0) {
      Move(dst, upper);
      psllq(dst, 32);
    } else if (CpuFeatures::IsSupported(SSE4_1)) {
      CpuFeatureScope scope(this, SSE4_1);
      push(eax);
      Move(eax, Immediate(lower));
      movd(dst, Operand(eax));
      if (upper != lower) {
        Move(eax, Immediate(upper));
      }
      pinsrd(dst, Operand(eax), 1);
      pop(eax);
    } else {
      push(Immediate(upper));
      push(Immediate(lower));
      movsd(dst, Operand(esp, 0));
      add(esp, Immediate(kDoubleSize));
    }
  }
}

void TurboAssembler::Pshufhw(XMMRegister dst, Operand src, uint8_t shuffle) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshufhw(dst, src, shuffle);
  } else {
    pshufhw(dst, src, shuffle);
  }
}

void TurboAssembler::Pshuflw(XMMRegister dst, Operand src, uint8_t shuffle) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshuflw(dst, src, shuffle);
  } else {
    pshuflw(dst, src, shuffle);
  }
}

void TurboAssembler::Pshufd(XMMRegister dst, Operand src, uint8_t shuffle) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshufd(dst, src, shuffle);
  } else {
    pshufd(dst, src, shuffle);
  }
}

void TurboAssembler::Psraw(XMMRegister dst, uint8_t shift) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsraw(dst, dst, shift);
  } else {
    psraw(dst, shift);
  }
}

void TurboAssembler::Psrlw(XMMRegister dst, uint8_t shift) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsrlw(dst, dst, shift);
  } else {
    psrlw(dst, shift);
  }
}

void TurboAssembler::Psignb(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsignb(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    psignb(dst, src);
    return;
  }
  FATAL("no AVX or SSE3 support");
}

void TurboAssembler::Psignw(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsignw(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    psignw(dst, src);
    return;
  }
  FATAL("no AVX or SSE3 support");
}

void TurboAssembler::Psignd(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpsignd(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    psignd(dst, src);
    return;
  }
  FATAL("no AVX or SSE3 support");
}

void TurboAssembler::Pshufb(XMMRegister dst, Operand src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpshufb(dst, dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    pshufb(dst, src);
    return;
  }
  FATAL("no AVX or SSE3 support");
}

void TurboAssembler::Pblendw(XMMRegister dst, Operand src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpblendw(dst, dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pblendw(dst, src, imm8);
    return;
  }
  FATAL("no AVX or SSE4.1 support");
}

void TurboAssembler::Palignr(XMMRegister dst, Operand src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpalignr(dst, dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSSE3)) {
    CpuFeatureScope sse_scope(this, SSSE3);
    palignr(dst, src, imm8);
    return;
  }
  FATAL("no AVX or SSE3 support");
}

void TurboAssembler::Pextrb(Register dst, XMMRegister src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpextrb(dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrb(dst, src, imm8);
    return;
  }
  FATAL("no AVX or SSE4.1 support");
}

void TurboAssembler::Pextrw(Register dst, XMMRegister src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpextrw(dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrw(dst, src, imm8);
    return;
  }
  FATAL("no AVX or SSE4.1 support");
}

void TurboAssembler::Pextrd(Register dst, XMMRegister src, uint8_t imm8) {
  if (imm8 == 0) {
    Movd(dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpextrd(dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrd(dst, src, imm8);
    return;
  }
  // Without AVX or SSE, we can only have 64-bit values in xmm registers.
  // We don't have an xmm scratch register, so move the data via the stack. This
  // path is rarely required, so it's acceptable to be slow.
  DCHECK_LT(imm8, 2);
  sub(esp, Immediate(kDoubleSize));
  movsd(Operand(esp, 0), src);
  mov(dst, Operand(esp, imm8 * kUInt32Size));
  add(esp, Immediate(kDoubleSize));
}

void TurboAssembler::Pinsrd(XMMRegister dst, Operand src, uint8_t imm8) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vpinsrd(dst, dst, src, imm8);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pinsrd(dst, src, imm8);
    return;
  }
  // Without AVX or SSE, we can only have 64-bit values in xmm registers.
  // We don't have an xmm scratch register, so move the data via the stack. This
  // path is rarely required, so it's acceptable to be slow.
  DCHECK_LT(imm8, 2);
  sub(esp, Immediate(kDoubleSize));
  // Write original content of {dst} to the stack.
  movsd(Operand(esp, 0), dst);
  // Overwrite the portion specified in {imm8}.
  if (src.is_reg_only()) {
    mov(Operand(esp, imm8 * kUInt32Size), src.reg());
  } else {
    movss(dst, src);
    movss(Operand(esp, imm8 * kUInt32Size), dst);
  }
  // Load back the full value into {dst}.
  movsd(dst, Operand(esp, 0));
  add(esp, Immediate(kDoubleSize));
}

void TurboAssembler::Lzcnt(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcnt(dst, src);
    return;
  }
  Label not_zero_src;
  bsr(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, Immediate(63));  // 63^31 == 32
  bind(&not_zero_src);
  xor_(dst, Immediate(31));  // for x in [0..31], 31^x == 31-x.
}

void TurboAssembler::Tzcnt(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcnt(dst, src);
    return;
  }
  Label not_zero_src;
  bsf(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Move(dst, Immediate(32));  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}

void TurboAssembler::Popcnt(Register dst, Operand src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcnt(dst, src);
    return;
  }
  FATAL("no POPCNT support");
}

void MacroAssembler::LoadWeakValue(Register in_out, Label* target_if_cleared) {
  cmp(in_out, Immediate(kClearedWeakHeapObject));
  j(equal, target_if_cleared);

  and_(in_out, Immediate(~kWeakHeapObjectMask));
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = StaticVariable(ExternalReference::Create(counter));
    if (value == 1) {
      inc(operand);
    } else {
      add(operand, Immediate(value));
    }
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand operand = StaticVariable(ExternalReference::Create(counter));
    if (value == 1) {
      dec(operand);
    } else {
      sub(operand, Immediate(value));
    }
  }
}

void TurboAssembler::Assert(Condition cc, AbortReason reason) {
  if (emit_debug_code()) Check(cc, reason);
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void TurboAssembler::Check(Condition cc, AbortReason reason) {
  Label L;
  j(cc, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}

void TurboAssembler::CheckStackAlignment() {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    Label alignment_as_expected;
    test(esp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}

void TurboAssembler::Abort(AbortReason reason) {
#ifdef DEBUG
  const char* msg = GetAbortReason(reason);
  RecordComment("Abort message: ");
  RecordComment(msg);
#endif

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    int3();
    return;
  }

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NONE);
    PrepareCallCFunction(1, eax);
    mov(Operand(esp, 0), Immediate(static_cast<int>(reason)));
    CallCFunction(ExternalReference::abort_with_reason(), 1);
    return;
  }

  Move(edx, Smi::FromInt(static_cast<int>(reason)));

  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame()) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // will not return here
  int3();
}


void TurboAssembler::PrepareCallCFunction(int num_arguments, Register scratch) {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  if (frame_alignment != 0) {
    // Make stack end at alignment and make room for num_arguments words
    // and the original value of esp.
    mov(scratch, esp);
    sub(esp, Immediate((num_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    and_(esp, -frame_alignment);
    mov(Operand(esp, num_arguments * kPointerSize), scratch);
  } else {
    sub(esp, Immediate(num_arguments * kPointerSize));
  }
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  // Trashing eax is ok as it will be the return value.
  mov(eax, Immediate(function));
  CallCFunction(eax, num_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  DCHECK_LE(num_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
    CheckStackAlignment();
  }

  call(function);
  if (base::OS::ActivationFrameAlignment() != 0) {
    mov(esp, Operand(esp, num_arguments * kPointerSize));
  } else {
    add(esp, Immediate(num_arguments * kPointerSize));
  }
}

void TurboAssembler::Call(Handle<Code> code_object, RelocInfo::Mode rmode) {
  if (FLAG_embedded_builtins) {
    // TODO(jgruber): Pc-relative builtin-to-builtin calls.
    if (root_array_available_ && options().isolate_independent_code) {
      // TODO(jgruber): There's no scratch register on ia32. Any call that
      // requires loading a code object from the builtins constant table must:
      // 1) spill two scratch registers, 2) load the target into scratch1, 3)
      // store the target into a virtual register on the isolate using scratch2,
      // 4) restore both scratch registers, and finally 5) call through the
      // virtual register. All affected call sites should vanish once all
      // builtins are embedded on ia32.
      UNREACHABLE();
    } else if (options().inline_offheap_trampolines) {
      int builtin_index = Builtins::kNoBuiltinId;
      if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin_index) &&
          Builtins::IsIsolateIndependent(builtin_index)) {
        // Inline the trampoline.
        RecordCommentForOffHeapTrampoline(builtin_index);
        CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
        EmbeddedData d = EmbeddedData::FromBlob();
        Address entry = d.InstructionStartOfBuiltin(builtin_index);
        call(entry, RelocInfo::OFF_HEAP_TARGET);
        return;
      }
    }
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  call(code_object, rmode);
}

void TurboAssembler::Jump(Handle<Code> code_object, RelocInfo::Mode rmode) {
  if (FLAG_embedded_builtins) {
    // TODO(jgruber): Pc-relative builtin-to-builtin calls.
    if (root_array_available_ && options().isolate_independent_code) {
      // TODO(jgruber): There's no scratch register on ia32. Any call that
      // requires loading a code object from the builtins constant table must:
      // 1) spill two scratch registers, 2) load the target into scratch1, 3)
      // store the target into a virtual register on the isolate using scratch2,
      // 4) restore both scratch registers, and finally 5) call through the
      // virtual register. All affected call sites should vanish once all
      // builtins are embedded on ia32.
      UNREACHABLE();
    } else if (options().inline_offheap_trampolines) {
      int builtin_index = Builtins::kNoBuiltinId;
      if (isolate()->builtins()->IsBuiltinHandle(code_object, &builtin_index) &&
          Builtins::IsIsolateIndependent(builtin_index)) {
        // Inline the trampoline.
        RecordCommentForOffHeapTrampoline(builtin_index);
        CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
        EmbeddedData d = EmbeddedData::FromBlob();
        Address entry = d.InstructionStartOfBuiltin(builtin_index);
        jmp(entry, RelocInfo::OFF_HEAP_TARGET);
        return;
      }
    }
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  jmp(code_object, rmode);
}

void TurboAssembler::RetpolineCall(Register reg) {
  Label setup_return, setup_target, inner_indirect_branch, capture_spec;

  jmp(&setup_return);  // Jump past the entire retpoline below.

  bind(&inner_indirect_branch);
  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  mov(Operand(esp, 0), reg);
  ret(0);

  bind(&setup_return);
  call(&inner_indirect_branch);  // Callee will return after this instruction.
}

void TurboAssembler::RetpolineCall(Address destination, RelocInfo::Mode rmode) {
  Label setup_return, setup_target, inner_indirect_branch, capture_spec;

  jmp(&setup_return);  // Jump past the entire retpoline below.

  bind(&inner_indirect_branch);
  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  mov(Operand(esp, 0), destination, rmode);
  ret(0);

  bind(&setup_return);
  call(&inner_indirect_branch);  // Callee will return after this instruction.
}

void TurboAssembler::RetpolineJump(Register reg) {
  Label setup_target, capture_spec;

  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  mov(Operand(esp, 0), reg);
  ret(0);
}

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met,
                                   Label::Distance condition_met_distance) {
  DCHECK(cc == zero || cc == not_zero);
  if (scratch == object) {
    and_(scratch, Immediate(~kPageAlignmentMask));
  } else {
    mov(scratch, Immediate(~kPageAlignmentMask));
    and_(scratch, object);
  }
  if (mask < (1 << kBitsPerByte)) {
    test_b(Operand(scratch, MemoryChunk::kFlagsOffset), Immediate(mask));
  } else {
    test(Operand(scratch, MemoryChunk::kFlagsOffset), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}

void TurboAssembler::ComputeCodeStartAddress(Register dst) {
  // In order to get the address of the current instruction, we first need
  // to use a call and then use a pop, thus pushing the return address to
  // the stack and then popping it into the register.
  Label current;
  call(&current);
  int pc = pc_offset();
  bind(&current);
  pop(dst);
  if (pc != 0) {
    sub(dst, Immediate(pc));
  }
}

void TurboAssembler::ResetSpeculationPoisonRegister() {
  mov(kSpeculationPoisonRegister, Immediate(-1));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
