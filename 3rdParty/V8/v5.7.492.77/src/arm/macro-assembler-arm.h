// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM_MACRO_ASSEMBLER_ARM_H_
#define V8_ARM_MACRO_ASSEMBLER_ARM_H_

#include "src/assembler.h"
#include "src/bailout-reason.h"
#include "src/frames.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Give alias names to registers for calling conventions.
const Register kReturnRegister0 = {Register::kCode_r0};
const Register kReturnRegister1 = {Register::kCode_r1};
const Register kReturnRegister2 = {Register::kCode_r2};
const Register kJSFunctionRegister = {Register::kCode_r1};
const Register kContextRegister = {Register::kCode_r7};
const Register kAllocateSizeRegister = {Register::kCode_r1};
const Register kInterpreterAccumulatorRegister = {Register::kCode_r0};
const Register kInterpreterBytecodeOffsetRegister = {Register::kCode_r5};
const Register kInterpreterBytecodeArrayRegister = {Register::kCode_r6};
const Register kInterpreterDispatchTableRegister = {Register::kCode_r8};
const Register kJavaScriptCallArgCountRegister = {Register::kCode_r0};
const Register kJavaScriptCallNewTargetRegister = {Register::kCode_r3};
const Register kRuntimeCallFunctionRegister = {Register::kCode_r1};
const Register kRuntimeCallArgCountRegister = {Register::kCode_r0};

// ----------------------------------------------------------------------------
// Static helper functions

// Generate a MemOperand for loading a field from an object.
inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}


// Give alias names to registers
const Register cp = {Register::kCode_r7};  // JavaScript context pointer.
const Register pp = {Register::kCode_r8};  // Constant pool pointer.
const Register kRootRegister = {Register::kCode_r10};  // Roots array pointer.

// Flags used for AllocateHeapNumber
enum TaggingMode {
  // Tag the result.
  TAG_RESULT,
  // Don't tag
  DONT_TAG_RESULT
};


enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum PointersToHereCheck {
  kPointersToHereMaybeInteresting,
  kPointersToHereAreAlwaysInteresting
};
enum LinkRegisterStatus { kLRHasNotBeenSaved, kLRHasBeenSaved };


Register GetRegisterThatIsNotOneOf(Register reg1,
                                   Register reg2 = no_reg,
                                   Register reg3 = no_reg,
                                   Register reg4 = no_reg,
                                   Register reg5 = no_reg,
                                   Register reg6 = no_reg);


#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3 = no_reg,
                Register reg4 = no_reg,
                Register reg5 = no_reg,
                Register reg6 = no_reg,
                Register reg7 = no_reg,
                Register reg8 = no_reg);
#endif


enum TargetAddressStorageMode {
  CAN_INLINE_TARGET_ADDRESS,
  NEVER_INLINE_TARGET_ADDRESS
};

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(Isolate* isolate, void* buffer, int size,
                 CodeObjectRequired create_code_object);


  // Returns the size of a call in instructions. Note, the value returned is
  // only valid as long as no entries are added to the constant pool between
  // checking the call size and emitting the actual call.
  static int CallSize(Register target, Condition cond = al);
  int CallSize(Address target, RelocInfo::Mode rmode, Condition cond = al);
  int CallStubSize(CodeStub* stub,
                   TypeFeedbackId ast_id = TypeFeedbackId::None(),
                   Condition cond = al);

  // Jump, Call, and Ret pseudo instructions implementing inter-working.
  void Jump(Register target, Condition cond = al);
  void Jump(Address target, RelocInfo::Mode rmode, Condition cond = al);
  void Jump(Handle<Code> code, RelocInfo::Mode rmode, Condition cond = al);
  void Call(Register target, Condition cond = al);
  void Call(Address target, RelocInfo::Mode rmode,
            Condition cond = al,
            TargetAddressStorageMode mode = CAN_INLINE_TARGET_ADDRESS);
  void Call(Handle<Code> code, RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
            TypeFeedbackId ast_id = TypeFeedbackId::None(), Condition cond = al,
            TargetAddressStorageMode mode = CAN_INLINE_TARGET_ADDRESS);
  int CallSize(Handle<Code> code,
               RelocInfo::Mode rmode = RelocInfo::CODE_TARGET,
               TypeFeedbackId ast_id = TypeFeedbackId::None(),
               Condition cond = al);
  void Ret(Condition cond = al);

  // Used for patching in calls to the deoptimizer.
  void CallDeoptimizer(Address target);
  static int CallDeoptimizerSize();

  // Emit code that loads |parameter_index|'th parameter from the stack to
  // the register according to the CallInterfaceDescriptor definition.
  // |sp_to_caller_sp_offset_in_words| specifies the number of words pushed
  // below the caller's sp.
  template <class Descriptor>
  void LoadParameterFromStack(
      Register reg, typename Descriptor::ParameterIndices parameter_index,
      int sp_to_ra_offset_in_words = 0) {
    DCHECK(Descriptor::kPassLastArgsOnStack);
    UNIMPLEMENTED();
  }

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the sp register.
  void Drop(int count, Condition cond = al);
  void Drop(Register count, Condition cond = al);

  void Ret(int drop, Condition cond = al);

  // Swap two registers.  If the scratch register is omitted then a slightly
  // less efficient form using xor instead of mov is emitted.
  void Swap(Register reg1,
            Register reg2,
            Register scratch = no_reg,
            Condition cond = al);

  void Mls(Register dst, Register src1, Register src2, Register srcA,
           Condition cond = al);
  void And(Register dst, Register src1, const Operand& src2,
           Condition cond = al);
  void Ubfx(Register dst, Register src, int lsb, int width,
            Condition cond = al);
  void Sbfx(Register dst, Register src, int lsb, int width,
            Condition cond = al);
  // The scratch register is not used for ARMv7.
  // scratch can be the same register as src (in which case it is trashed), but
  // not the same as dst.
  void Bfi(Register dst,
           Register src,
           Register scratch,
           int lsb,
           int width,
           Condition cond = al);
  void Bfc(Register dst, Register src, int lsb, int width, Condition cond = al);

  void Call(Label* target);
  void Push(Register src) { push(src); }
  void Pop(Register dst) { pop(dst); }

  // Register move. May do nothing if the registers are identical.
  void Move(Register dst, Smi* smi) { mov(dst, Operand(smi)); }
  void Move(Register dst, Handle<Object> value);
  void Move(Register dst, Register src, Condition cond = al);
  void Move(Register dst, const Operand& src, SBit sbit = LeaveCC,
            Condition cond = al) {
    if (!src.is_reg() || !src.rm().is(dst) || sbit != LeaveCC) {
      mov(dst, src, sbit, cond);
    }
  }
  void Move(SwVfpRegister dst, SwVfpRegister src, Condition cond = al);
  void Move(DwVfpRegister dst, DwVfpRegister src, Condition cond = al);
  void Move(QwNeonRegister dst, QwNeonRegister src);
  // Register swap.
  void Swap(DwVfpRegister srcdst0, DwVfpRegister srcdst1);
  void Swap(QwNeonRegister srcdst0, QwNeonRegister srcdst1);

  void Load(Register dst, const MemOperand& src, Representation r);
  void Store(Register src, const MemOperand& dst, Representation r);

  // Load an object from the root table.
  void LoadRoot(Register destination,
                Heap::RootListIndex index,
                Condition cond = al);
  // Store an object to the root table.
  void StoreRoot(Register source,
                 Heap::RootListIndex index,
                 Condition cond = al);

  // ---------------------------------------------------------------------------
  // GC Support

  void IncrementalMarkingRecordWriteHelper(Register object,
                                           Register value,
                                           Register address);

  enum RememberedSetFinalAction {
    kReturnAtEnd,
    kFallThroughAtEnd
  };

  // Record in the remembered set the fact that we have a pointer to new space
  // at the address pointed to by the addr register.  Only works if addr is not
  // in new space.
  void RememberedSetHelper(Register object,  // Used for debug code.
                           Register addr,
                           Register scratch,
                           SaveFPRegsMode save_fp,
                           RememberedSetFinalAction and_then);

  void CheckPageFlag(Register object,
                     Register scratch,
                     int mask,
                     Condition cc,
                     Label* condition_met);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object,
                           Register scratch,
                           Label* branch) {
    InNewSpace(object, scratch, eq, branch);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object,
                        Register scratch,
                        Label* branch) {
    InNewSpace(object, scratch, ne, branch);
  }

  // Check if an object has a given incremental marking color.
  void HasColor(Register object,
                Register scratch0,
                Register scratch1,
                Label* has_color,
                int first_bit,
                int second_bit);

  void JumpIfBlack(Register object,
                   Register scratch0,
                   Register scratch1,
                   Label* on_black);

  // Checks the color of an object.  If the object is white we jump to the
  // incremental marker.
  void JumpIfWhite(Register value, Register scratch1, Register scratch2,
                   Register scratch3, Label* value_is_white);

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldMemOperand(reg, off).
  void RecordWriteField(
      Register object,
      int offset,
      Register value,
      Register scratch,
      LinkRegisterStatus lr_status,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // As above, but the offset has the tag presubtracted.  For use with
  // MemOperand(reg, off).
  inline void RecordWriteContextSlot(
      Register context,
      int offset,
      Register value,
      Register scratch,
      LinkRegisterStatus lr_status,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting) {
    RecordWriteField(context,
                     offset + kHeapObjectTag,
                     value,
                     scratch,
                     lr_status,
                     save_fp,
                     remembered_set_action,
                     smi_check,
                     pointers_to_here_check_for_value);
  }

  // Notify the garbage collector that we wrote a code entry into a
  // JSFunction. Only scratch is clobbered by the operation.
  void RecordWriteCodeEntryField(Register js_function, Register code_entry,
                                 Register scratch);

  void RecordWriteForMap(
      Register object,
      Register map,
      Register dst,
      LinkRegisterStatus lr_status,
      SaveFPRegsMode save_fp);

  // For a given |object| notify the garbage collector that the slot |address|
  // has been written.  |value| is the object being stored. The value and
  // address registers are clobbered by the operation.
  void RecordWrite(
      Register object,
      Register address,
      Register value,
      LinkRegisterStatus lr_status,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // Push a handle.
  void Push(Handle<Object> handle);
  void Push(Smi* smi) { Push(Handle<Smi>(smi, isolate())); }

  // Push two registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Condition cond = al) {
    if (src1.code() > src2.code()) {
      stm(db_w, sp, src1.bit() | src2.bit(), cond);
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      str(src2, MemOperand(sp, 4, NegPreIndex), cond);
    }
  }

  // Push three registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Condition cond = al) {
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        stm(db_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
      } else {
        stm(db_w, sp, src1.bit() | src2.bit(), cond);
        str(src3, MemOperand(sp, 4, NegPreIndex), cond);
      }
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      Push(src2, src3, cond);
    }
  }

  // Push four registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1,
            Register src2,
            Register src3,
            Register src4,
            Condition cond = al) {
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        if (src3.code() > src4.code()) {
          stm(db_w,
              sp,
              src1.bit() | src2.bit() | src3.bit() | src4.bit(),
              cond);
        } else {
          stm(db_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
          str(src4, MemOperand(sp, 4, NegPreIndex), cond);
        }
      } else {
        stm(db_w, sp, src1.bit() | src2.bit(), cond);
        Push(src3, src4, cond);
      }
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      Push(src2, src3, src4, cond);
    }
  }

  // Push five registers.  Pushes leftmost register first (to highest address).
  void Push(Register src1, Register src2, Register src3, Register src4,
            Register src5, Condition cond = al) {
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        if (src3.code() > src4.code()) {
          if (src4.code() > src5.code()) {
            stm(db_w, sp,
                src1.bit() | src2.bit() | src3.bit() | src4.bit() | src5.bit(),
                cond);
          } else {
            stm(db_w, sp, src1.bit() | src2.bit() | src3.bit() | src4.bit(),
                cond);
            str(src5, MemOperand(sp, 4, NegPreIndex), cond);
          }
        } else {
          stm(db_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
          Push(src4, src5, cond);
        }
      } else {
        stm(db_w, sp, src1.bit() | src2.bit(), cond);
        Push(src3, src4, src5, cond);
      }
    } else {
      str(src1, MemOperand(sp, 4, NegPreIndex), cond);
      Push(src2, src3, src4, src5, cond);
    }
  }

  // Pop two registers. Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Condition cond = al) {
    DCHECK(!src1.is(src2));
    if (src1.code() > src2.code()) {
      ldm(ia_w, sp, src1.bit() | src2.bit(), cond);
    } else {
      ldr(src2, MemOperand(sp, 4, PostIndex), cond);
      ldr(src1, MemOperand(sp, 4, PostIndex), cond);
    }
  }

  // Pop three registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1, Register src2, Register src3, Condition cond = al) {
    DCHECK(!AreAliased(src1, src2, src3));
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        ldm(ia_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
      } else {
        ldr(src3, MemOperand(sp, 4, PostIndex), cond);
        ldm(ia_w, sp, src1.bit() | src2.bit(), cond);
      }
    } else {
      Pop(src2, src3, cond);
      ldr(src1, MemOperand(sp, 4, PostIndex), cond);
    }
  }

  // Pop four registers.  Pops rightmost register first (from lower address).
  void Pop(Register src1,
           Register src2,
           Register src3,
           Register src4,
           Condition cond = al) {
    DCHECK(!AreAliased(src1, src2, src3, src4));
    if (src1.code() > src2.code()) {
      if (src2.code() > src3.code()) {
        if (src3.code() > src4.code()) {
          ldm(ia_w,
              sp,
              src1.bit() | src2.bit() | src3.bit() | src4.bit(),
              cond);
        } else {
          ldr(src4, MemOperand(sp, 4, PostIndex), cond);
          ldm(ia_w, sp, src1.bit() | src2.bit() | src3.bit(), cond);
        }
      } else {
        Pop(src3, src4, cond);
        ldm(ia_w, sp, src1.bit() | src2.bit(), cond);
      }
    } else {
      Pop(src2, src3, src4, cond);
      ldr(src1, MemOperand(sp, 4, PostIndex), cond);
    }
  }

  // Push a fixed frame, consisting of lr, fp, constant pool (if
  // FLAG_enable_embedded_constant_pool)
  void PushCommonFrame(Register marker_reg = no_reg);

  // Push a standard frame, consisting of lr, fp, constant pool (if
  // FLAG_enable_embedded_constant_pool), context and JS function
  void PushStandardFrame(Register function_reg);

  void PopCommonFrame(Register marker_reg = no_reg);

  // Push and pop the registers that can hold pointers, as defined by the
  // RegList constant kSafepointSavedRegisters.
  void PushSafepointRegisters();
  void PopSafepointRegisters();
  // Store value in register src in the safepoint stack slot for
  // register dst.
  void StoreToSafepointRegisterSlot(Register src, Register dst);
  // Load the value of the src register from its safepoint stack slot
  // into register dst.
  void LoadFromSafepointRegisterSlot(Register dst, Register src);

  // Load two consecutive registers with two consecutive memory locations.
  void Ldrd(Register dst1,
            Register dst2,
            const MemOperand& src,
            Condition cond = al);

  // Store two consecutive registers to two consecutive memory locations.
  void Strd(Register src1,
            Register src2,
            const MemOperand& dst,
            Condition cond = al);

  // If the value is a NaN, canonicalize the value else, do nothing.
  void VFPCanonicalizeNaN(const DwVfpRegister dst,
                          const DwVfpRegister src,
                          const Condition cond = al);
  void VFPCanonicalizeNaN(const DwVfpRegister value,
                          const Condition cond = al) {
    VFPCanonicalizeNaN(value, value, cond);
  }

  // Compare single values and move the result to the normal condition flags.
  void VFPCompareAndSetFlags(const SwVfpRegister src1, const SwVfpRegister src2,
                             const Condition cond = al);
  void VFPCompareAndSetFlags(const SwVfpRegister src1, const float src2,
                             const Condition cond = al);

  // Compare double values and move the result to the normal condition flags.
  void VFPCompareAndSetFlags(const DwVfpRegister src1,
                             const DwVfpRegister src2,
                             const Condition cond = al);
  void VFPCompareAndSetFlags(const DwVfpRegister src1,
                             const double src2,
                             const Condition cond = al);

  // Compare single values and then load the fpscr flags to a register.
  void VFPCompareAndLoadFlags(const SwVfpRegister src1,
                              const SwVfpRegister src2,
                              const Register fpscr_flags,
                              const Condition cond = al);
  void VFPCompareAndLoadFlags(const SwVfpRegister src1, const float src2,
                              const Register fpscr_flags,
                              const Condition cond = al);

  // Compare double values and then load the fpscr flags to a register.
  void VFPCompareAndLoadFlags(const DwVfpRegister src1,
                              const DwVfpRegister src2,
                              const Register fpscr_flags,
                              const Condition cond = al);
  void VFPCompareAndLoadFlags(const DwVfpRegister src1,
                              const double src2,
                              const Register fpscr_flags,
                              const Condition cond = al);

  void Vmov(const DwVfpRegister dst,
            const double imm,
            const Register scratch = no_reg);

  void VmovHigh(Register dst, DwVfpRegister src);
  void VmovHigh(DwVfpRegister dst, Register src);
  void VmovLow(Register dst, DwVfpRegister src);
  void VmovLow(DwVfpRegister dst, Register src);

  // Simulate s-register moves for imaginary s32 - s63 registers.
  void VmovExtended(Register dst, int src_code);
  void VmovExtended(int dst_code, Register src);
  // Move between s-registers and imaginary s-registers.
  void VmovExtended(int dst_code, int src_code, Register scratch);
  void VmovExtended(int dst_code, const MemOperand& src, Register scratch);
  void VmovExtended(const MemOperand& dst, int src_code, Register scratch);

  void ExtractLane(Register dst, QwNeonRegister src, NeonDataType dt, int lane);
  void ExtractLane(SwVfpRegister dst, QwNeonRegister src, Register scratch,
                   int lane);
  void ReplaceLane(QwNeonRegister dst, QwNeonRegister src, Register src_lane,
                   NeonDataType dt, int lane);
  void ReplaceLane(QwNeonRegister dst, QwNeonRegister src,
                   SwVfpRegister src_lane, Register scratch, int lane);
  void Swizzle(QwNeonRegister dst, QwNeonRegister src, Register scratch,
               NeonSize size, uint32_t lanes);

  void LslPair(Register dst_low, Register dst_high, Register src_low,
               Register src_high, Register scratch, Register shift);
  void LslPair(Register dst_low, Register dst_high, Register src_low,
               Register src_high, uint32_t shift);
  void LsrPair(Register dst_low, Register dst_high, Register src_low,
               Register src_high, Register scratch, Register shift);
  void LsrPair(Register dst_low, Register dst_high, Register src_low,
               Register src_high, uint32_t shift);
  void AsrPair(Register dst_low, Register dst_high, Register src_low,
               Register src_high, Register scratch, Register shift);
  void AsrPair(Register dst_low, Register dst_high, Register src_low,
               Register src_high, uint32_t shift);

  // Loads the number from object into dst register.
  // If |object| is neither smi nor heap number, |not_number| is jumped to
  // with |object| still intact.
  void LoadNumber(Register object,
                  LowDwVfpRegister dst,
                  Register heap_number_map,
                  Register scratch,
                  Label* not_number);

  // Loads the number from object into double_dst in the double format.
  // Control will jump to not_int32 if the value cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be loaded.
  void LoadNumberAsInt32Double(Register object,
                               DwVfpRegister double_dst,
                               Register heap_number_map,
                               Register scratch,
                               LowDwVfpRegister double_scratch,
                               Label* not_int32);

  // Loads the number from object into dst as a 32-bit integer.
  // Control will jump to not_int32 if the object cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be converted.
  void LoadNumberAsInt32(Register object,
                         Register dst,
                         Register heap_number_map,
                         Register scratch,
                         DwVfpRegister double_scratch0,
                         LowDwVfpRegister double_scratch1,
                         Label* not_int32);

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue(bool code_pre_aging);

  // Enter exit frame.
  // stack_space - extra stack space, used for alignment before call to C.
  void EnterExitFrame(bool save_doubles, int stack_space = 0,
                      StackFrame::Type frame_type = StackFrame::EXIT);

  // Leave the current exit frame. Expects the return value in r0.
  // Expect the number of values, pushed prior to the exit frame, to
  // remove in a register (or no_reg, if there is nothing to remove).
  void LeaveExitFrame(bool save_doubles, Register argument_count,
                      bool restore_context,
                      bool argument_count_is_length = false);

  // Get the actual activation frame alignment for target environment.
  static int ActivationFrameAlignment();

  void LoadContext(Register dst, int context_chain_length);

  // Load the global object from the current context.
  void LoadGlobalObject(Register dst) {
    LoadNativeContextSlot(Context::EXTENSION_INDEX, dst);
  }

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst) {
    LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
  }

  void LoadNativeContextSlot(int index, Register dst);

  // Load the initial map from the global function. The registers
  // function and map can be the same, function is then overwritten.
  void LoadGlobalFunctionInitialMap(Register function,
                                    Register map,
                                    Register scratch);

  void InitializeRootRegister() {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(isolate());
    mov(kRootRegister, Operand(roots_array_start));
  }

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // Both |callee_args_count| and |caller_args_count_reg| do not include
  // receiver. |callee_args_count| is not modified, |caller_args_count_reg|
  // is trashed.
  void PrepareForTailCall(const ParameterCount& callee_args_count,
                          Register caller_args_count_reg, Register scratch0,
                          Register scratch1);

  // Invoke the JavaScript function code by either calling or jumping.
  void InvokeFunctionCode(Register function, Register new_target,
                          const ParameterCount& expected,
                          const ParameterCount& actual, InvokeFlag flag,
                          const CallWrapper& call_wrapper);

  // On function call, call into the debugger if necessary.
  void CheckDebugHook(Register fun, Register new_target,
                      const ParameterCount& expected,
                      const ParameterCount& actual);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  void InvokeFunction(Register function,
                      Register new_target,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Register function,
                      const ParameterCount& expected,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void IsObjectJSStringType(Register object,
                            Register scratch,
                            Label* fail);

  void IsObjectNameType(Register object,
                        Register scratch,
                        Label* fail);

  // ---------------------------------------------------------------------------
  // Debugger Support

  void DebugBreak();

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new stack handler and link into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  // Must preserve the result register.
  void PopStackHandler();

  // ---------------------------------------------------------------------------
  // Inline caching support

  void GetNumberHash(Register t0, Register scratch);

  inline void MarkCode(NopMarkerTypes type) {
    nop(type);
  }

  // Check if the given instruction is a 'type' marker.
  // i.e. check if is is a mov r<type>, r<type> (referenced as nop(type))
  // These instructions are generated to mark special location in the code,
  // like some special IC code.
  static inline bool IsMarkedCode(Instr instr, int type) {
    DCHECK((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER));
    return IsNop(instr, type);
  }


  static inline int GetCodeMarker(Instr instr) {
    int dst_reg_offset = 12;
    int dst_mask = 0xf << dst_reg_offset;
    int src_mask = 0xf;
    int dst_reg = (instr & dst_mask) >> dst_reg_offset;
    int src_reg = instr & src_mask;
    uint32_t non_register_mask = ~(dst_mask | src_mask);
    uint32_t mov_mask = al | 13 << 21;

    // Return <n> if we have a mov rn rn, else return -1.
    int type = ((instr & non_register_mask) == mov_mask) &&
               (dst_reg == src_reg) &&
               (FIRST_IC_MARKER <= dst_reg) && (dst_reg < LAST_CODE_MARKER)
                   ? src_reg
                   : -1;
    DCHECK((type == -1) ||
           ((FIRST_IC_MARKER <= type) && (type < LAST_CODE_MARKER)));
    return type;
  }


  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space or old space. The object_size is
  // specified either in bytes or in words if the allocation flag SIZE_IN_WORDS
  // is passed. If the space is exhausted control continues at the gc_required
  // label. The allocated object is returned in result. If the flag
  // tag_allocated_object is true the result is tagged as as a heap object.
  // All registers are clobbered also when control continues at the gc_required
  // label.
  void Allocate(int object_size,
                Register result,
                Register scratch1,
                Register scratch2,
                Label* gc_required,
                AllocationFlags flags);

  void Allocate(Register object_size, Register result, Register result_end,
                Register scratch, Label* gc_required, AllocationFlags flags);

  // FastAllocate is right now only used for folded allocations. It just
  // increments the top pointer without checking against limit. This can only
  // be done if it was proved earlier that the allocation will succeed.
  void FastAllocate(int object_size, Register result, Register scratch1,
                    Register scratch2, AllocationFlags flags);

  void FastAllocate(Register object_size, Register result, Register result_end,
                    Register scratch, AllocationFlags flags);

  // Allocates a heap number or jumps to the gc_required label if the young
  // space is full and a scavenge is needed. All registers are clobbered also
  // when control continues at the gc_required label.
  void AllocateHeapNumber(Register result,
                          Register scratch1,
                          Register scratch2,
                          Register heap_number_map,
                          Label* gc_required,
                          MutableMode mode = IMMUTABLE);
  void AllocateHeapNumberWithValue(Register result,
                                   DwVfpRegister value,
                                   Register scratch1,
                                   Register scratch2,
                                   Register heap_number_map,
                                   Label* gc_required);

  // Allocate and initialize a JSValue wrapper with the specified {constructor}
  // and {value}.
  void AllocateJSValue(Register result, Register constructor, Register value,
                       Register scratch1, Register scratch2,
                       Label* gc_required);

  // Initialize fields with filler values.  Fields starting at |current_address|
  // not including |end_address| are overwritten with the value in |filler|.  At
  // the end the loop, |current_address| takes the value of |end_address|.
  void InitializeFieldsWithFiller(Register current_address,
                                  Register end_address, Register filler);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Machine code version of Map::GetConstructor().
  // |temp| holds |result|'s map when done, and |temp2| its instance type.
  void GetMapConstructor(Register result, Register map, Register temp,
                         Register temp2);

  // Try to get function prototype of a function and puts the value in
  // the result register. Checks that the function really is a
  // function and jumps to the miss label if the fast checks fail. The
  // function register will be untouched; the other registers may be
  // clobbered.
  void TryGetFunctionPrototype(Register function, Register result,
                               Register scratch, Label* miss);

  // Compare object type for heap object.  heap_object contains a non-Smi
  // whose object type should be compared with the given type.  This both
  // sets the flags and leaves the object type in the type_reg register.
  // It leaves the map in the map register (unless the type_reg and map register
  // are the same register).  It leaves the heap object in the heap_object
  // register unless the heap_object register is the same register as one of the
  // other registers.
  // Type_reg can be no_reg. In that case ip is used.
  void CompareObjectType(Register heap_object,
                         Register map,
                         Register type_reg,
                         InstanceType type);

  // Compare instance type in a map.  map contains a valid map object whose
  // object type should be compared with the given type.  This both
  // sets the flags and leaves the object type in the type_reg register.
  void CompareInstanceType(Register map,
                           Register type_reg,
                           InstanceType type);

  // Compare an object's map with the specified map and its transitioned
  // elements maps if mode is ALLOW_ELEMENT_TRANSITION_MAPS. Condition flags are
  // set with result of map compare. If multiple map compares are required, the
  // compare sequences branches to early_success.
  void CompareMap(Register obj,
                  Register scratch,
                  Handle<Map> map,
                  Label* early_success);

  // As above, but the map of the object is already loaded into the register
  // which is preserved by the code generated.
  void CompareMap(Register obj_map,
                  Handle<Map> map,
                  Label* early_success);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specified map.
  void CheckMap(Register obj,
                Register scratch,
                Handle<Map> map,
                Label* fail,
                SmiCheckType smi_check_type);


  void CheckMap(Register obj,
                Register scratch,
                Heap::RootListIndex index,
                Label* fail,
                SmiCheckType smi_check_type);


  // Check if the map of an object is equal to a specified weak map and branch
  // to a specified target if equal. Skip the smi check if not required
  // (object is known to be a heap object)
  void DispatchWeakMap(Register obj, Register scratch1, Register scratch2,
                       Handle<WeakCell> cell, Handle<Code> success,
                       SmiCheckType smi_check_type);

  // Compare the given value and the value of weak cell.
  void CmpWeakValue(Register value, Handle<WeakCell> cell, Register scratch);

  void GetWeakValue(Register value, Handle<WeakCell> cell);

  // Load the value of the weak cell in the value register. Branch to the given
  // miss label if the weak cell was cleared.
  void LoadWeakValue(Register value, Handle<WeakCell> cell, Label* miss);

  // Compare the object in a register to a value from the root list.
  // Uses the ip register as scratch.
  void CompareRoot(Register obj, Heap::RootListIndex index);
  void PushRoot(Heap::RootListIndex index) {
    LoadRoot(ip, index);
    Push(ip);
  }

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(Register with, Heap::RootListIndex index, Label* if_equal) {
    CompareRoot(with, index);
    b(eq, if_equal);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(Register with, Heap::RootListIndex index,
                     Label* if_not_equal) {
    CompareRoot(with, index);
    b(ne, if_not_equal);
  }

  // Load and check the instance type of an object for being a string.
  // Loads the type into the second argument register.
  // Returns a condition that will be enabled if the object was a string
  // and the passed-in condition passed. If the passed-in condition failed
  // then flags remain unchanged.
  Condition IsObjectStringType(Register obj,
                               Register type,
                               Condition cond = al) {
    ldr(type, FieldMemOperand(obj, HeapObject::kMapOffset), cond);
    ldrb(type, FieldMemOperand(type, Map::kInstanceTypeOffset), cond);
    tst(type, Operand(kIsNotStringMask), cond);
    DCHECK_EQ(0u, kStringTag);
    return eq;
  }


  // Get the number of least significant bits from a register
  void GetLeastBitsFromSmi(Register dst, Register src, int num_least_bits);
  void GetLeastBitsFromInt32(Register dst, Register src, int mun_least_bits);

  // Load the value of a smi object into a double register.
  // The register value must be between d0 and d15.
  void SmiToDouble(LowDwVfpRegister value, Register smi);

  // Check if a double can be exactly represented as a signed 32-bit integer.
  // Z flag set to one if true.
  void TestDoubleIsInt32(DwVfpRegister double_input,
                         LowDwVfpRegister double_scratch);

  // Try to convert a double to a signed 32-bit integer.
  // Z flag set to one and result assigned if the conversion is exact.
  void TryDoubleToInt32Exact(Register result,
                             DwVfpRegister double_input,
                             LowDwVfpRegister double_scratch);

  // Floor a double and writes the value to the result register.
  // Go to exact if the conversion is exact (to be able to test -0),
  // fall through calling code if an overflow occurred, else go to done.
  // In return, input_high is loaded with high bits of input.
  void TryInt32Floor(Register result,
                     DwVfpRegister double_input,
                     Register input_high,
                     LowDwVfpRegister double_scratch,
                     Label* done,
                     Label* exact);

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. Goes to 'done' if it
  // succeeds, otherwise falls through if result is saturated. On return
  // 'result' either holds answer, or is clobbered on fall through.
  //
  // Only public for the test code in test-code-stubs-arm.cc.
  void TryInlineTruncateDoubleToI(Register result,
                                  DwVfpRegister input,
                                  Label* done);

  // Performs a truncating conversion of a floating point number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32.
  // Exits with 'result' holding the answer.
  void TruncateDoubleToI(Register result, DwVfpRegister double_input);

  // Performs a truncating conversion of a heap number as used by
  // the JS bitwise operations. See ECMA-262 9.5: ToInt32. 'result' and 'input'
  // must be different registers.  Exits with 'result' holding the answer.
  void TruncateHeapNumberToI(Register result, Register object);

  // Converts the smi or heap number in object to an int32 using the rules
  // for ToInt32 as described in ECMAScript 9.5.: the value is truncated
  // and brought into the range -2^31 .. +2^31 - 1. 'result' and 'input' must be
  // different registers.
  void TruncateNumberToI(Register object,
                         Register result,
                         Register heap_number_map,
                         Register scratch1,
                         Label* not_int32);

  // Check whether d16-d31 are available on the CPU. The result is given by the
  // Z condition flag: Z==0 if d16-d31 available, Z==1 otherwise.
  void CheckFor32DRegs(Register scratch);

  // Does a runtime check for 16/32 FP registers. Either way, pushes 32 double
  // values to location, saving [d0..(d15|d31)].
  void SaveFPRegs(Register location, Register scratch);

  // Does a runtime check for 16/32 FP registers. Either way, pops 32 double
  // values to location, restoring [d0..(d15|d31)].
  void RestoreFPRegs(Register location, Register scratch);

  // Perform a floating-point min or max operation with the
  // (IEEE-754-compatible) semantics of ARM64's fmin/fmax. Some cases, typically
  // NaNs or +/-0.0, are expected to be rare and are handled in out-of-line
  // code. The specific behaviour depends on supported instructions.
  //
  // These functions assume (and assert) that !left.is(right). It is permitted
  // for the result to alias either input register.
  void FloatMax(SwVfpRegister result, SwVfpRegister left, SwVfpRegister right,
                Label* out_of_line);
  void FloatMin(SwVfpRegister result, SwVfpRegister left, SwVfpRegister right,
                Label* out_of_line);
  void FloatMax(DwVfpRegister result, DwVfpRegister left, DwVfpRegister right,
                Label* out_of_line);
  void FloatMin(DwVfpRegister result, DwVfpRegister left, DwVfpRegister right,
                Label* out_of_line);

  // Generate out-of-line cases for the macros above.
  void FloatMaxOutOfLine(SwVfpRegister result, SwVfpRegister left,
                         SwVfpRegister right);
  void FloatMinOutOfLine(SwVfpRegister result, SwVfpRegister left,
                         SwVfpRegister right);
  void FloatMaxOutOfLine(DwVfpRegister result, DwVfpRegister left,
                         DwVfpRegister right);
  void FloatMinOutOfLine(DwVfpRegister result, DwVfpRegister left,
                         DwVfpRegister right);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.
  void CallStub(CodeStub* stub,
                TypeFeedbackId ast_id = TypeFeedbackId::None(),
                Condition cond = al);

  // Call a code stub.
  void TailCallStub(CodeStub* stub, Condition cond = al);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f,
                   int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs);
  void CallRuntimeSaveDoubles(Runtime::FunctionId fid) {
    const Runtime::Function* function = Runtime::FunctionForId(fid);
    CallRuntime(function, function->nargs, kSaveFPRegs);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    const Runtime::Function* function = Runtime::FunctionForId(fid);
    CallRuntime(function, function->nargs, save_doubles);
  }

  // Convenience function: Same as above, but takes the fid instead.
  void CallRuntime(Runtime::FunctionId fid, int num_arguments,
                   SaveFPRegsMode save_doubles = kDontSaveFPRegs) {
    CallRuntime(Runtime::FunctionForId(fid), num_arguments, save_doubles);
  }

  // Convenience function: call an external reference.
  void CallExternalReference(const ExternalReference& ext,
                             int num_arguments);

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid);

  int CalculateStackPassedWords(int num_reg_arguments,
                                int num_double_arguments);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, non-register arguments must be stored in
  // sp[0], sp[4], etc., not pushed. The argument count assumes all arguments
  // are word sized. If double arguments are used, this function assumes that
  // all double arguments are stored before core registers; otherwise the
  // correct alignment of the double values is not guaranteed.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  void PrepareCallCFunction(int num_reg_arguments,
                            int num_double_registers,
                            Register scratch);
  void PrepareCallCFunction(int num_reg_arguments,
                            Register scratch);

  // There are two ways of passing double arguments on ARM, depending on
  // whether soft or hard floating point ABI is used. These functions
  // abstract parameter passing for the three different ways we call
  // C functions from generated code.
  void MovToFloatParameter(DwVfpRegister src);
  void MovToFloatParameters(DwVfpRegister src1, DwVfpRegister src2);
  void MovToFloatResult(DwVfpRegister src);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);
  void CallCFunction(ExternalReference function,
                     int num_reg_arguments,
                     int num_double_arguments);
  void CallCFunction(Register function,
                     int num_reg_arguments,
                     int num_double_arguments);

  void MovFromFloatParameter(DwVfpRegister dst);
  void MovFromFloatResult(DwVfpRegister dst);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& builtin,
                               bool builtin_exit_frame = false);

  Handle<Object> CodeObject() {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }


  // Emit code for a truncating division by a constant. The dividend register is
  // unchanged and ip gets clobbered. Dividend and result must be different.
  void TruncatingDiv(Register result, Register dividend, int32_t divisor);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value,
                  Register scratch1, Register scratch2);
  void IncrementCounter(StatsCounter* counter, int value,
                        Register scratch1, Register scratch2);
  void DecrementCounter(StatsCounter* counter, int value,
                        Register scratch1, Register scratch2);


  // ---------------------------------------------------------------------------
  // Debugging

  // Calls Abort(msg) if the condition cond is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cond, BailoutReason reason);
  void AssertFastElements(Register elements);

  // Like Assert(), but always enabled.
  void Check(Condition cond, BailoutReason reason);

  // Print a message to stdout and abort execution.
  void Abort(BailoutReason msg);

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() { return has_frame_; }
  inline bool AllowThisStubCall(CodeStub* stub);

  // EABI variant for double arguments in use.
  bool use_eabi_hardfloat() {
#ifdef __arm__
    return base::OS::ArmUsingHardFloat();
#elif USE_EABI_HARDFLOAT
    return true;
#else
    return false;
#endif
  }

  // ---------------------------------------------------------------------------
  // Number utilities

  // Check whether the value of reg is a power of two and not zero. If not
  // control continues at the label not_power_of_two. If reg is a power of two
  // the register scratch contains the value of (reg - 1) when control falls
  // through.
  void JumpIfNotPowerOfTwoOrZero(Register reg,
                                 Register scratch,
                                 Label* not_power_of_two_or_zero);
  // Check whether the value of reg is a power of two and not zero.
  // Control falls through if it is, with scratch containing the mask
  // value (reg - 1).
  // Otherwise control jumps to the 'zero_and_neg' label if the value of reg is
  // zero or negative, or jumps to the 'not_power_of_two' label if the value is
  // strictly positive but not a power of two.
  void JumpIfNotPowerOfTwoOrZeroAndNeg(Register reg,
                                       Register scratch,
                                       Label* zero_and_neg,
                                       Label* not_power_of_two);

  // ---------------------------------------------------------------------------
  // Smi utilities

  void SmiTag(Register reg, SBit s = LeaveCC) {
    add(reg, reg, Operand(reg), s);
  }
  void SmiTag(Register dst, Register src, SBit s = LeaveCC) {
    add(dst, src, Operand(src), s);
  }

  // Try to convert int32 to smi. If the value is to large, preserve
  // the original value and jump to not_a_smi. Destroys scratch and
  // sets flags.
  void TrySmiTag(Register reg, Label* not_a_smi) {
    TrySmiTag(reg, reg, not_a_smi);
  }
  void TrySmiTag(Register reg, Register src, Label* not_a_smi) {
    SmiTag(ip, src, SetCC);
    b(vs, not_a_smi);
    mov(reg, ip);
  }


  void SmiUntag(Register reg, SBit s = LeaveCC) {
    mov(reg, Operand::SmiUntag(reg), s);
  }
  void SmiUntag(Register dst, Register src, SBit s = LeaveCC) {
    mov(dst, Operand::SmiUntag(src), s);
  }

  // Untag the source value into destination and jump if source is a smi.
  // Souce and destination can be the same register.
  void UntagAndJumpIfSmi(Register dst, Register src, Label* smi_case);

  // Test if the register contains a smi (Z == 0 (eq) if true).
  inline void SmiTst(Register value) {
    tst(value, Operand(kSmiTagMask));
  }
  inline void NonNegativeSmiTst(Register value) {
    tst(value, Operand(kSmiTagMask | kSmiSignMask));
  }
  // Jump if the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label) {
    tst(value, Operand(kSmiTagMask));
    b(eq, smi_label);
  }
  // Jump if either of the registers contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label) {
    tst(value, Operand(kSmiTagMask));
    b(ne, not_smi_label);
  }
  // Jump if either of the registers contain a non-smi.
  void JumpIfNotBothSmi(Register reg1, Register reg2, Label* on_not_both_smi);
  // Jump if either of the registers contain a smi.
  void JumpIfEitherSmi(Register reg1, Register reg2, Label* on_either_smi);

  // Abort execution if argument is a number, enabled via --debug-code.
  void AssertNotNumber(Register object);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);
  void AssertSmi(Register object);

  // Abort execution if argument is not a string, enabled via --debug-code.
  void AssertString(Register object);

  // Abort execution if argument is not a name, enabled via --debug-code.
  void AssertName(Register object);

  // Abort execution if argument is not a JSFunction, enabled via --debug-code.
  void AssertFunction(Register object);

  // Abort execution if argument is not a JSBoundFunction,
  // enabled via --debug-code.
  void AssertBoundFunction(Register object);

  // Abort execution if argument is not a JSGeneratorObject,
  // enabled via --debug-code.
  void AssertGeneratorObject(Register object);

  // Abort execution if argument is not a JSReceiver, enabled via --debug-code.
  void AssertReceiver(Register object);

  // Abort execution if argument is not undefined or an AllocationSite, enabled
  // via --debug-code.
  void AssertUndefinedOrAllocationSite(Register object, Register scratch);

  // Abort execution if reg is not the root value with the given index,
  // enabled via --debug-code.
  void AssertIsRoot(Register reg, Heap::RootListIndex index);

  // ---------------------------------------------------------------------------
  // HeapNumber utilities

  void JumpIfNotHeapNumber(Register object,
                           Register heap_number_map,
                           Register scratch,
                           Label* on_not_heap_number);

  // ---------------------------------------------------------------------------
  // String utilities

  // Checks if both objects are sequential one-byte strings and jumps to label
  // if either is not. Assumes that neither object is a smi.
  void JumpIfNonSmisNotBothSequentialOneByteStrings(Register object1,
                                                    Register object2,
                                                    Register scratch1,
                                                    Register scratch2,
                                                    Label* failure);

  // Checks if both objects are sequential one-byte strings and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialOneByteStrings(Register first, Register second,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* not_flat_one_byte_strings);

  // Checks if both instance types are sequential one-byte strings and jumps to
  // label if either is not.
  void JumpIfBothInstanceTypesAreNotSequentialOneByte(
      Register first_object_instance_type, Register second_object_instance_type,
      Register scratch1, Register scratch2, Label* failure);

  void JumpIfNotUniqueNameInstanceType(Register reg, Label* not_unique_name);

  void EmitSeqStringSetCharCheck(Register string,
                                 Register index,
                                 Register value,
                                 uint32_t encoding_mask);


  void ClampUint8(Register output_reg, Register input_reg);

  void ClampDoubleToUint8(Register result_reg,
                          DwVfpRegister input_reg,
                          LowDwVfpRegister double_scratch);


  void LoadInstanceDescriptors(Register map, Register descriptors);
  void EnumLength(Register dst, Register map);
  void NumberOfOwnDescriptors(Register dst, Register map);
  void LoadAccessor(Register dst, Register holder, int accessor_index,
                    AccessorComponent accessor);

  template<typename Field>
  void DecodeField(Register dst, Register src) {
    Ubfx(dst, src, Field::kShift, Field::kSize);
  }

  template<typename Field>
  void DecodeField(Register reg) {
    DecodeField<Field>(reg, reg);
  }

  template<typename Field>
  void DecodeFieldToSmi(Register dst, Register src) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> shift << kSmiTagSize;
    STATIC_ASSERT((mask & (0x80000000u >> (kSmiTagSize - 1))) == 0);
    STATIC_ASSERT(kSmiTag == 0);
    if (shift < kSmiTagSize) {
      mov(dst, Operand(src, LSL, kSmiTagSize - shift));
      and_(dst, dst, Operand(mask));
    } else if (shift > kSmiTagSize) {
      mov(dst, Operand(src, LSR, shift - kSmiTagSize));
      and_(dst, dst, Operand(mask));
    } else {
      and_(dst, src, Operand(mask));
    }
  }

  template<typename Field>
  void DecodeFieldToSmi(Register reg) {
    DecodeField<Field>(reg, reg);
  }

  // Load the type feedback vector from a JavaScript frame.
  void EmitLoadTypeFeedbackVector(Register vector);

  // Activation support.
  void EnterFrame(StackFrame::Type type,
                  bool load_constant_pool_pointer_reg = false);
  // Returns the pc offset at which the frame ends.
  int LeaveFrame(StackFrame::Type type);

  void EnterBuiltinFrame(Register context, Register target, Register argc);
  void LeaveBuiltinFrame(Register context, Register target, Register argc);

  // Expects object in r0 and returns map with validated enum cache
  // in r0.  Assumes that any other register can be used as a scratch.
  void CheckEnumCache(Label* call_runtime);

  // AllocationMemento support. Arrays may have an associated
  // AllocationMemento object that can be checked for in order to pretransition
  // to another type.
  // On entry, receiver_reg should point to the array object.
  // scratch_reg gets clobbered.
  // If allocation info is present, condition flags are set to eq.
  void TestJSArrayForAllocationMemento(Register receiver_reg,
                                       Register scratch_reg,
                                       Label* no_memento_found);

  // Loads the constant pool pointer (pp) register.
  void LoadConstantPoolPointerRegisterFromCodeTargetAddress(
      Register code_target_address);
  void LoadConstantPoolPointerRegister();

 private:
  void CallCFunctionHelper(Register function,
                           int num_reg_arguments,
                           int num_double_arguments);

  void Jump(intptr_t target, RelocInfo::Mode rmode, Condition cond = al);

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual,
                      Label* done,
                      bool* definitely_mismatches,
                      InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InitializeNewString(Register string,
                           Register length,
                           Heap::RootListIndex map_index,
                           Register scratch1,
                           Register scratch2);

  // Helper for implementing JumpIfNotInNewSpace and JumpIfInNewSpace.
  void InNewSpace(Register object,
                  Register scratch,
                  Condition cond,  // eq for new space, ne otherwise.
                  Label* branch);

  // Helper for finding the mark bits for an address.  Afterwards, the
  // bitmap register points at the word with the mark bits and the mask
  // the position of the first bit.  Leaves addr_reg unchanged.
  inline void GetMarkBits(Register addr_reg,
                          Register bitmap_reg,
                          Register mask_reg);

  // Compute memory operands for safepoint stack slots.
  static int SafepointRegisterStackIndex(int reg_code);
  MemOperand SafepointRegisterSlot(Register reg);
  MemOperand SafepointRegistersAndDoublesSlot(Register reg);

  // Implementation helpers for FloatMin and FloatMax.
  template <typename T>
  void FloatMaxHelper(T result, T left, T right, Label* out_of_line);
  template <typename T>
  void FloatMinHelper(T result, T left, T right, Label* out_of_line);
  template <typename T>
  void FloatMaxOutOfLineHelper(T result, T left, T right);
  template <typename T>
  void FloatMinOutOfLineHelper(T result, T left, T right);

  bool generating_stub_;
  bool has_frame_;
  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Needs access to SafepointRegisterStackIndex for compiled frame
  // traversal.
  friend class StandardFrame;
};


// The code patcher is used to patch (typically) small parts of code e.g. for
// debugging and other types of instrumentation. When using the code patcher
// the exact number of bytes specified must be emitted. It is not legal to emit
// relocation information. If any of these constraints are violated it causes
// an assertion to fail.
class CodePatcher {
 public:
  enum FlushICache {
    FLUSH,
    DONT_FLUSH
  };

  CodePatcher(Isolate* isolate, byte* address, int instructions,
              FlushICache flush_cache = FLUSH);
  ~CodePatcher();

  // Macro assembler to emit code.
  MacroAssembler* masm() { return &masm_; }

  // Emit an instruction directly.
  void Emit(Instr instr);

  // Emit an address directly.
  void Emit(Address addr);

  // Emit the condition part of an instruction leaving the rest of the current
  // instruction unchanged.
  void EmitCondition(Condition cond);

 private:
  byte* address_;  // The address of the code being patched.
  int size_;  // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
  FlushICache flush_cache_;  // Whether to flush the I cache after patching.
};


// -----------------------------------------------------------------------------
// Static helper functions.

inline MemOperand ContextMemOperand(Register context, int index = 0) {
  return MemOperand(context, Context::SlotOffset(index));
}


inline MemOperand NativeContextMemOperand() {
  return ContextMemOperand(cp, Context::NATIVE_CONTEXT_INDEX);
}

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM_MACRO_ASSEMBLER_ARM_H_
