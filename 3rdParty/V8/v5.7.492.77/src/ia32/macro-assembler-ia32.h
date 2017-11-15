// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_MACRO_ASSEMBLER_IA32_H_
#define V8_IA32_MACRO_ASSEMBLER_IA32_H_

#include "src/assembler.h"
#include "src/bailout-reason.h"
#include "src/frames.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Give alias names to registers for calling conventions.
const Register kReturnRegister0 = {Register::kCode_eax};
const Register kReturnRegister1 = {Register::kCode_edx};
const Register kReturnRegister2 = {Register::kCode_edi};
const Register kJSFunctionRegister = {Register::kCode_edi};
const Register kContextRegister = {Register::kCode_esi};
const Register kAllocateSizeRegister = {Register::kCode_edx};
const Register kInterpreterAccumulatorRegister = {Register::kCode_eax};
const Register kInterpreterBytecodeOffsetRegister = {Register::kCode_ecx};
const Register kInterpreterBytecodeArrayRegister = {Register::kCode_edi};
const Register kInterpreterDispatchTableRegister = {Register::kCode_esi};
const Register kJavaScriptCallArgCountRegister = {Register::kCode_eax};
const Register kJavaScriptCallNewTargetRegister = {Register::kCode_edx};
const Register kRuntimeCallFunctionRegister = {Register::kCode_ebx};
const Register kRuntimeCallArgCountRegister = {Register::kCode_eax};

// Convenience for platform-independent signatures.  We do not normally
// distinguish memory operands from other operands on ia32.
typedef Operand MemOperand;

enum RememberedSetAction { EMIT_REMEMBERED_SET, OMIT_REMEMBERED_SET };
enum SmiCheck { INLINE_SMI_CHECK, OMIT_SMI_CHECK };
enum PointersToHereCheck {
  kPointersToHereMaybeInteresting,
  kPointersToHereAreAlwaysInteresting
};

enum RegisterValueType { REGISTER_VALUE_IS_SMI, REGISTER_VALUE_IS_INT32 };

enum class ReturnAddressState { kOnStack, kNotOnStack };

#ifdef DEBUG
bool AreAliased(Register reg1, Register reg2, Register reg3 = no_reg,
                Register reg4 = no_reg, Register reg5 = no_reg,
                Register reg6 = no_reg, Register reg7 = no_reg,
                Register reg8 = no_reg);
#endif

// MacroAssembler implements a collection of frequently used macros.
class MacroAssembler: public Assembler {
 public:
  MacroAssembler(Isolate* isolate, void* buffer, int size,
                 CodeObjectRequired create_code_object);

  void Load(Register dst, const Operand& src, Representation r);
  void Store(Register src, const Operand& dst, Representation r);

  // Load a register with a long value as efficiently as possible.
  void Set(Register dst, int32_t x) {
    if (x == 0) {
      xor_(dst, dst);
    } else {
      mov(dst, Immediate(x));
    }
  }
  void Set(const Operand& dst, int32_t x) { mov(dst, Immediate(x)); }

  // Operations on roots in the root-array.
  void LoadRoot(Register destination, Heap::RootListIndex index);
  void StoreRoot(Register source, Register scratch, Heap::RootListIndex index);
  void CompareRoot(Register with, Register scratch, Heap::RootListIndex index);
  // These methods can only be used with constant roots (i.e. non-writable
  // and not in new space).
  void CompareRoot(Register with, Heap::RootListIndex index);
  void CompareRoot(const Operand& with, Heap::RootListIndex index);
  void PushRoot(Heap::RootListIndex index);

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(Register with, Heap::RootListIndex index, Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }
  void JumpIfRoot(const Operand& with, Heap::RootListIndex index,
                  Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(Register with, Heap::RootListIndex index,
                     Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }
  void JumpIfNotRoot(const Operand& with, Heap::RootListIndex index,
                     Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }

  // These functions do not arrange the registers in any particular order so
  // they are not useful for calls that can cause a GC.  The caller can
  // exclude up to 3 registers that do not need to be saved and restored.
  void PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                       Register exclusion2 = no_reg,
                       Register exclusion3 = no_reg);
  void PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1 = no_reg,
                      Register exclusion2 = no_reg,
                      Register exclusion3 = no_reg);

  // ---------------------------------------------------------------------------
  // GC Support
  enum RememberedSetFinalAction { kReturnAtEnd, kFallThroughAtEnd };

  // Record in the remembered set the fact that we have a pointer to new space
  // at the address pointed to by the addr register.  Only works if addr is not
  // in new space.
  void RememberedSetHelper(Register object,  // Used for debug code.
                           Register addr, Register scratch,
                           SaveFPRegsMode save_fp,
                           RememberedSetFinalAction and_then);

  void CheckPageFlag(Register object, Register scratch, int mask, Condition cc,
                     Label* condition_met,
                     Label::Distance condition_met_distance = Label::kFar);

  void CheckPageFlagForMap(
      Handle<Map> map, int mask, Condition cc, Label* condition_met,
      Label::Distance condition_met_distance = Label::kFar);

  // Check if object is in new space.  Jumps if the object is not in new space.
  // The register scratch can be object itself, but scratch will be clobbered.
  void JumpIfNotInNewSpace(Register object, Register scratch, Label* branch,
                           Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, zero, branch, distance);
  }

  // Check if object is in new space.  Jumps if the object is in new space.
  // The register scratch can be object itself, but it will be clobbered.
  void JumpIfInNewSpace(Register object, Register scratch, Label* branch,
                        Label::Distance distance = Label::kFar) {
    InNewSpace(object, scratch, not_zero, branch, distance);
  }

  // Check if an object has a given incremental marking color.  Also uses ecx!
  void HasColor(Register object, Register scratch0, Register scratch1,
                Label* has_color, Label::Distance has_color_distance,
                int first_bit, int second_bit);

  void JumpIfBlack(Register object, Register scratch0, Register scratch1,
                   Label* on_black,
                   Label::Distance on_black_distance = Label::kFar);

  // Checks the color of an object.  If the object is white we jump to the
  // incremental marker.
  void JumpIfWhite(Register value, Register scratch1, Register scratch2,
                   Label* value_is_white, Label::Distance distance);

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldOperand(reg, off).
  void RecordWriteField(
      Register object, int offset, Register value, Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // As above, but the offset has the tag presubtracted.  For use with
  // Operand(reg, off).
  void RecordWriteContextSlot(
      Register context, int offset, Register value, Register scratch,
      SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting) {
    RecordWriteField(context, offset + kHeapObjectTag, value, scratch, save_fp,
                     remembered_set_action, smi_check,
                     pointers_to_here_check_for_value);
  }

  // Notify the garbage collector that we wrote a pointer into a fixed array.
  // |array| is the array being stored into, |value| is the
  // object being stored.  |index| is the array index represented as a
  // Smi. All registers are clobbered by the operation RecordWriteArray
  // filters out smis so it does not update the write barrier if the
  // value is a smi.
  void RecordWriteArray(
      Register array, Register value, Register index, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // For page containing |object| mark region covering |address|
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. The address and value registers are clobbered by the
  // operation. RecordWrite filters out smis so it does not update the
  // write barrier if the value is a smi.
  void RecordWrite(
      Register object, Register address, Register value, SaveFPRegsMode save_fp,
      RememberedSetAction remembered_set_action = EMIT_REMEMBERED_SET,
      SmiCheck smi_check = INLINE_SMI_CHECK,
      PointersToHereCheck pointers_to_here_check_for_value =
          kPointersToHereMaybeInteresting);

  // Notify the garbage collector that we wrote a code entry into a
  // JSFunction. Only scratch is clobbered by the operation.
  void RecordWriteCodeEntryField(Register js_function, Register code_entry,
                                 Register scratch);

  // For page containing |object| mark the region covering the object's map
  // dirty. |object| is the object being stored into, |map| is the Map object
  // that was stored.
  void RecordWriteForMap(Register object, Handle<Map> map, Register scratch1,
                         Register scratch2, SaveFPRegsMode save_fp);

  // ---------------------------------------------------------------------------
  // Debugger Support

  void DebugBreak();

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue(bool code_pre_aging);

  // Enter specific kind of exit frame. Expects the number of
  // arguments in register eax and sets up the number of arguments in
  // register edi and the pointer to the first argument in register
  // esi.
  void EnterExitFrame(int argc, bool save_doubles, StackFrame::Type frame_type);

  void EnterApiExitFrame(int argc);

  // Leave the current exit frame. Expects the return value in
  // register eax:edx (untouched) and the pointer to the first
  // argument in register esi (if pop_arguments == true).
  void LeaveExitFrame(bool save_doubles, bool pop_arguments = true);

  // Leave the current exit frame. Expects the return value in
  // register eax (untouched).
  void LeaveApiExitFrame(bool restore_context);

  // Find the function context up the context chain.
  void LoadContext(Register dst, int context_chain_length);

  // Load the global proxy from the current context.
  void LoadGlobalProxy(Register dst);

  // Load the global function with the given index.
  void LoadGlobalFunction(int index, Register function);

  // Load the initial map from the global function. The registers
  // function and map can be the same.
  void LoadGlobalFunctionInitialMap(Register function, Register map);

  // Push and pop the registers that can hold pointers.
  void PushSafepointRegisters() { pushad(); }
  void PopSafepointRegisters() { popad(); }
  // Store the value in register/immediate src in the safepoint
  // register stack slot for register dst.
  void StoreToSafepointRegisterSlot(Register dst, Register src);
  void StoreToSafepointRegisterSlot(Register dst, Immediate src);
  void LoadFromSafepointRegisterSlot(Register dst, Register src);

  // Nop, because ia32 does not have a root register.
  void InitializeRootRegister() {}

  void LoadHeapObject(Register result, Handle<HeapObject> object);
  void CmpHeapObject(Register reg, Handle<HeapObject> object);
  void PushHeapObject(Handle<HeapObject> object);

  void LoadObject(Register result, Handle<Object> object) {
    AllowDeferredHandleDereference heap_object_check;
    if (object->IsHeapObject()) {
      LoadHeapObject(result, Handle<HeapObject>::cast(object));
    } else {
      Move(result, Immediate(object));
    }
  }

  void CmpObject(Register reg, Handle<Object> object) {
    AllowDeferredHandleDereference heap_object_check;
    if (object->IsHeapObject()) {
      CmpHeapObject(reg, Handle<HeapObject>::cast(object));
    } else {
      cmp(reg, Immediate(object));
    }
  }

  // Compare the given value and the value of weak cell.
  void CmpWeakValue(Register value, Handle<WeakCell> cell, Register scratch);

  void GetWeakValue(Register value, Handle<WeakCell> cell);

  // Load the value of the weak cell in the value register. Branch to the given
  // miss label if the weak cell was cleared.
  void LoadWeakValue(Register value, Handle<WeakCell> cell, Label* miss);

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // Removes current frame and its arguments from the stack preserving
  // the arguments and a return address pushed to the stack for the next call.
  // |ra_state| defines whether return address is already pushed to stack or
  // not. Both |callee_args_count| and |caller_args_count_reg| do not include
  // receiver. |callee_args_count| is not modified, |caller_args_count_reg|
  // is trashed. |number_of_temp_values_after_return_address| specifies
  // the number of words pushed to the stack after the return address. This is
  // to allow "allocation" of scratch registers that this function requires
  // by saving their values on the stack.
  void PrepareForTailCall(const ParameterCount& callee_args_count,
                          Register caller_args_count_reg, Register scratch0,
                          Register scratch1, ReturnAddressState ra_state,
                          int number_of_temp_values_after_return_address);

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
  void InvokeFunction(Register function, Register new_target,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Register function, const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  void InvokeFunction(Handle<JSFunction> function,
                      const ParameterCount& expected,
                      const ParameterCount& actual, InvokeFlag flag,
                      const CallWrapper& call_wrapper);

  // Expression support
  // cvtsi2sd instruction only writes to the low 64-bit of dst register, which
  // hinders register renaming and makes dependence chains longer. So we use
  // xorps to clear the dst register before cvtsi2sd to solve this issue.
  void Cvtsi2sd(XMMRegister dst, Register src) { Cvtsi2sd(dst, Operand(src)); }
  void Cvtsi2sd(XMMRegister dst, const Operand& src);

  void Cvtui2ss(XMMRegister dst, Register src, Register tmp);

  void ShlPair(Register high, Register low, uint8_t imm8);
  void ShlPair_cl(Register high, Register low);
  void ShrPair(Register high, Register low, uint8_t imm8);
  void ShrPair_cl(Register high, Register src);
  void SarPair(Register high, Register low, uint8_t imm8);
  void SarPair_cl(Register high, Register low);

  // Support for constant splitting.
  bool IsUnsafeImmediate(const Immediate& x);
  void SafeMove(Register dst, const Immediate& x);
  void SafePush(const Immediate& x);

  // Compare object type for heap object.
  // Incoming register is heap_object and outgoing register is map.
  void CmpObjectType(Register heap_object, InstanceType type, Register map);

  // Compare instance type for map.
  void CmpInstanceType(Register map, InstanceType type);

  // Compare an object's map with the specified map.
  void CompareMap(Register obj, Handle<Map> map);

  // Check if the map of an object is equal to a specified map and branch to
  // label if not. Skip the smi check if not required (object is known to be a
  // heap object). If mode is ALLOW_ELEMENT_TRANSITION_MAPS, then also match
  // against maps that are ElementsKind transition maps of the specified map.
  void CheckMap(Register obj, Handle<Map> map, Label* fail,
                SmiCheckType smi_check_type);

  // Check if the map of an object is equal to a specified weak map and branch
  // to a specified target if equal. Skip the smi check if not required
  // (object is known to be a heap object)
  void DispatchWeakMap(Register obj, Register scratch1, Register scratch2,
                       Handle<WeakCell> cell, Handle<Code> success,
                       SmiCheckType smi_check_type);

  // Check if the object in register heap_object is a string. Afterwards the
  // register map contains the object map and the register instance_type
  // contains the instance_type. The registers map and instance_type can be the
  // same in which case it contains the instance type afterwards. Either of the
  // registers map and instance_type can be the same as heap_object.
  Condition IsObjectStringType(Register heap_object, Register map,
                               Register instance_type);

  // Check if the object in register heap_object is a name. Afterwards the
  // register map contains the object map and the register instance_type
  // contains the instance_type. The registers map and instance_type can be the
  // same in which case it contains the instance type afterwards. Either of the
  // registers map and instance_type can be the same as heap_object.
  Condition IsObjectNameType(Register heap_object, Register map,
                             Register instance_type);

  // FCmp is similar to integer cmp, but requires unsigned
  // jcc instructions (je, ja, jae, jb, jbe, je, and jz).
  void FCmp();

  void ClampUint8(Register reg);

  void ClampDoubleToUint8(XMMRegister input_reg, XMMRegister scratch_reg,
                          Register result_reg);

  void SlowTruncateToI(Register result_reg, Register input_reg,
      int offset = HeapNumber::kValueOffset - kHeapObjectTag);

  void TruncateHeapNumberToI(Register result_reg, Register input_reg);
  void TruncateDoubleToI(Register result_reg, XMMRegister input_reg);

  void DoubleToI(Register result_reg, XMMRegister input_reg,
                 XMMRegister scratch, MinusZeroMode minus_zero_mode,
                 Label* lost_precision, Label* is_nan, Label* minus_zero,
                 Label::Distance dst = Label::kFar);

  // Smi tagging support.
  void SmiTag(Register reg) {
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    add(reg, reg);
  }
  void SmiUntag(Register reg) {
    sar(reg, kSmiTagSize);
  }

  // Modifies the register even if it does not contain a Smi!
  void SmiUntag(Register reg, Label* is_smi) {
    STATIC_ASSERT(kSmiTagSize == 1);
    sar(reg, kSmiTagSize);
    STATIC_ASSERT(kSmiTag == 0);
    j(not_carry, is_smi);
  }

  void LoadUint32(XMMRegister dst, Register src) {
    LoadUint32(dst, Operand(src));
  }
  void LoadUint32(XMMRegister dst, const Operand& src);

  // Jump the register contains a smi.
  inline void JumpIfSmi(Register value, Label* smi_label,
                        Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(zero, smi_label, distance);
  }
  // Jump if the operand is a smi.
  inline void JumpIfSmi(Operand value, Label* smi_label,
                        Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(zero, smi_label, distance);
  }
  // Jump if register contain a non-smi.
  inline void JumpIfNotSmi(Register value, Label* not_smi_label,
                           Label::Distance distance = Label::kFar) {
    test(value, Immediate(kSmiTagMask));
    j(not_zero, not_smi_label, distance);
  }

  // Jump if the value cannot be represented by a smi.
  inline void JumpIfNotValidSmiValue(Register value, Register scratch,
                                     Label* on_invalid,
                                     Label::Distance distance = Label::kFar) {
    mov(scratch, value);
    add(scratch, Immediate(0x40000000U));
    j(sign, on_invalid, distance);
  }

  // Jump if the unsigned integer value cannot be represented by a smi.
  inline void JumpIfUIntNotValidSmiValue(
      Register value, Label* on_invalid,
      Label::Distance distance = Label::kFar) {
    cmp(value, Immediate(0x40000000U));
    j(above_equal, on_invalid, distance);
  }

  void LoadInstanceDescriptors(Register map, Register descriptors);
  void EnumLength(Register dst, Register map);
  void NumberOfOwnDescriptors(Register dst, Register map);
  void LoadAccessor(Register dst, Register holder, int accessor_index,
                    AccessorComponent accessor);

  template<typename Field>
  void DecodeField(Register reg) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> Field::kShift;
    if (shift != 0) {
      sar(reg, shift);
    }
    and_(reg, Immediate(mask));
  }

  template<typename Field>
  void DecodeFieldToSmi(Register reg) {
    static const int shift = Field::kShift;
    static const int mask = (Field::kMask >> Field::kShift) << kSmiTagSize;
    STATIC_ASSERT((mask & (0x80000000u >> (kSmiTagSize - 1))) == 0);
    STATIC_ASSERT(kSmiTag == 0);
    if (shift < kSmiTagSize) {
      shl(reg, kSmiTagSize - shift);
    } else if (shift > kSmiTagSize) {
      sar(reg, shift - kSmiTagSize);
    }
    and_(reg, Immediate(mask));
  }

  void LoadPowerOf2(XMMRegister dst, Register scratch, int power);

  // Abort execution if argument is not a number, enabled via --debug-code.
  void AssertNumber(Register object);
  void AssertNotNumber(Register object);

  // Abort execution if argument is not a smi, enabled via --debug-code.
  void AssertSmi(Register object);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(Register object);

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
  void AssertUndefinedOrAllocationSite(Register object);

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new stack handler and link it into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  void PopStackHandler();

  // ---------------------------------------------------------------------------
  // Inline caching support

  void GetNumberHash(Register r0, Register scratch);

  // ---------------------------------------------------------------------------
  // Allocation support

  // Allocate an object in new space or old space. If the given space
  // is exhausted control continues at the gc_required label. The allocated
  // object is returned in result and end of the new object is returned in
  // result_end. The register scratch can be passed as no_reg in which case
  // an additional object reference will be added to the reloc info. The
  // returned pointers in result and result_end have not yet been tagged as
  // heap objects. If result_contains_top_on_entry is true the content of
  // result is known to be the allocation top on entry (could be result_end
  // from a previous call). If result_contains_top_on_entry is true scratch
  // should be no_reg as it is never used.
  void Allocate(int object_size, Register result, Register result_end,
                Register scratch, Label* gc_required, AllocationFlags flags);

  void Allocate(int header_size, ScaleFactor element_size,
                Register element_count, RegisterValueType element_count_type,
                Register result, Register result_end, Register scratch,
                Label* gc_required, AllocationFlags flags);

  void Allocate(Register object_size, Register result, Register result_end,
                Register scratch, Label* gc_required, AllocationFlags flags);

  // FastAllocate is right now only used for folded allocations. It just
  // increments the top pointer without checking against limit. This can only
  // be done if it was proved earlier that the allocation will succeed.
  void FastAllocate(int object_size, Register result, Register result_end,
                    AllocationFlags flags);
  void FastAllocate(Register object_size, Register result, Register result_end,
                    AllocationFlags flags);

  // Allocate a heap number in new space with undefined value. The
  // register scratch2 can be passed as no_reg; the others must be
  // valid registers. Returns tagged pointer in result register, or
  // jumps to gc_required if new space is full.
  void AllocateHeapNumber(Register result, Register scratch1, Register scratch2,
                          Label* gc_required, MutableMode mode = IMMUTABLE);

  // Allocate and initialize a JSValue wrapper with the specified {constructor}
  // and {value}.
  void AllocateJSValue(Register result, Register constructor, Register value,
                       Register scratch, Label* gc_required);

  // Initialize fields with filler values.  Fields starting at |current_address|
  // not including |end_address| are overwritten with the value in |filler|.  At
  // the end the loop, |current_address| takes the value of |end_address|.
  void InitializeFieldsWithFiller(Register current_address,
                                  Register end_address, Register filler);

  // ---------------------------------------------------------------------------
  // Support functions.

  // Check a boolean-bit of a Smi field.
  void BooleanBitTest(Register object, int field_offset, int bit_index);

  // Check if result is zero and op is negative.
  void NegativeZeroTest(Register result, Register op, Label* then_label);

  // Check if result is zero and any of op1 and op2 are negative.
  // Register scratch is destroyed, and it must be different from op2.
  void NegativeZeroTest(Register result, Register op1, Register op2,
                        Register scratch, Label* then_label);

  // Machine code version of Map::GetConstructor().
  // |temp| holds |result|'s map when done.
  void GetMapConstructor(Register result, Register map, Register temp);

  // Try to get function prototype of a function and puts the value in
  // the result register. Checks that the function really is a
  // function and jumps to the miss label if the fast checks fail. The
  // function register will be untouched; the other registers may be
  // clobbered.
  void TryGetFunctionPrototype(Register function, Register result,
                               Register scratch, Label* miss);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a code stub.  Generate the code if necessary.
  void CallStub(CodeStub* stub, TypeFeedbackId ast_id = TypeFeedbackId::None());

  // Tail call a code stub (jump).  Generate the code if necessary.
  void TailCallStub(CodeStub* stub);

  // Return from a code stub after popping its arguments.
  void StubReturn(int argc);

  // Call a runtime routine.
  void CallRuntime(const Runtime::Function* f, int num_arguments,
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
  void CallExternalReference(ExternalReference ref, int num_arguments);

  // Convenience function: tail call a runtime routine (jump).
  void TailCallRuntime(Runtime::FunctionId fid);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in esp[0], esp[4],
  // etc., not pushed. The argument count assumes all arguments are word sized.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  void PrepareCallCFunction(int num_arguments, Register scratch);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  void CallCFunction(ExternalReference function, int num_arguments);
  void CallCFunction(Register function, int num_arguments);

  // Jump to a runtime routine.
  void JumpToExternalReference(const ExternalReference& ext,
                               bool builtin_exit_frame = false);

  // ---------------------------------------------------------------------------
  // Utilities

  void Ret();

  // Return and drop arguments from stack, where the number of arguments
  // may be bigger than 2^16 - 1.  Requires a scratch register.
  void Ret(int bytes_dropped, Register scratch);

  // Emit code that loads |parameter_index|'th parameter from the stack to
  // the register according to the CallInterfaceDescriptor definition.
  // |sp_to_caller_sp_offset_in_words| specifies the number of words pushed
  // below the caller's sp (on ia32 it's at least return address).
  template <class Descriptor>
  void LoadParameterFromStack(
      Register reg, typename Descriptor::ParameterIndices parameter_index,
      int sp_to_ra_offset_in_words = 1) {
    DCHECK(Descriptor::kPassLastArgsOnStack);
    DCHECK_LT(parameter_index, Descriptor::kParameterCount);
    DCHECK_LE(Descriptor::kParameterCount - Descriptor::kStackArgumentsCount,
              parameter_index);
    int offset = (Descriptor::kParameterCount - parameter_index - 1 +
                  sp_to_ra_offset_in_words) *
                 kPointerSize;
    mov(reg, Operand(esp, offset));
  }

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the esp register.
  void Drop(int element_count);

  void Call(Label* target) { call(target); }
  void Call(Handle<Code> target, RelocInfo::Mode rmode,
            TypeFeedbackId id = TypeFeedbackId::None()) {
    call(target, rmode, id);
  }
  void Jump(Handle<Code> target, RelocInfo::Mode rmode) { jmp(target, rmode); }
  void Push(Register src) { push(src); }
  void Push(const Operand& src) { push(src); }
  void Push(Immediate value) { push(value); }
  void Pop(Register dst) { pop(dst); }
  void Pop(const Operand& dst) { pop(dst); }
  void PushReturnAddressFrom(Register src) { push(src); }
  void PopReturnAddressTo(Register dst) { pop(dst); }

  // Non-SSE2 instructions.
  void Pextrd(Register dst, XMMRegister src, int8_t imm8);
  void Pinsrd(XMMRegister dst, Register src, int8_t imm8) {
    Pinsrd(dst, Operand(src), imm8);
  }
  void Pinsrd(XMMRegister dst, const Operand& src, int8_t imm8);

  void Lzcnt(Register dst, Register src) { Lzcnt(dst, Operand(src)); }
  void Lzcnt(Register dst, const Operand& src);

  void Tzcnt(Register dst, Register src) { Tzcnt(dst, Operand(src)); }
  void Tzcnt(Register dst, const Operand& src);

  void Popcnt(Register dst, Register src) { Popcnt(dst, Operand(src)); }
  void Popcnt(Register dst, const Operand& src);

  // Move if the registers are not identical.
  void Move(Register target, Register source);

  // Move a constant into a destination using the most efficient encoding.
  void Move(Register dst, const Immediate& x);
  void Move(const Operand& dst, const Immediate& x);

  // Move an immediate into an XMM register.
  void Move(XMMRegister dst, uint32_t src);
  void Move(XMMRegister dst, uint64_t src);
  void Move(XMMRegister dst, float src) { Move(dst, bit_cast<uint32_t>(src)); }
  void Move(XMMRegister dst, double src) { Move(dst, bit_cast<uint64_t>(src)); }

  void Move(Register dst, Handle<Object> handle) { LoadObject(dst, handle); }
  void Move(Register dst, Smi* source) { Move(dst, Immediate(source)); }

  // Push a handle value.
  void Push(Handle<Object> handle) { push(Immediate(handle)); }
  void Push(Smi* smi) { Push(Immediate(smi)); }

  Handle<Object> CodeObject() {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }

  // Emit code for a truncating division by a constant. The dividend register is
  // unchanged, the result is in edx, and eax gets clobbered.
  void TruncatingDiv(Register dividend, int32_t divisor);

  // ---------------------------------------------------------------------------
  // StatsCounter support

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int value);
  void DecrementCounter(StatsCounter* counter, int value);
  void IncrementCounter(Condition cc, StatsCounter* counter, int value);
  void DecrementCounter(Condition cc, StatsCounter* counter, int value);

  // ---------------------------------------------------------------------------
  // Debugging

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, BailoutReason reason);

  void AssertFastElements(Register elements);

  // Like Assert(), but always enabled.
  void Check(Condition cc, BailoutReason reason);

  // Print a message to stdout and abort execution.
  void Abort(BailoutReason reason);

  // Check that the stack is aligned.
  void CheckStackAlignment();

  // Verify restrictions about code generated in stubs.
  void set_generating_stub(bool value) { generating_stub_ = value; }
  bool generating_stub() { return generating_stub_; }
  void set_has_frame(bool value) { has_frame_ = value; }
  bool has_frame() { return has_frame_; }
  inline bool AllowThisStubCall(CodeStub* stub);

  // ---------------------------------------------------------------------------
  // String utilities.

  // Checks if both objects are sequential one-byte strings, and jumps to label
  // if either is not.
  void JumpIfNotBothSequentialOneByteStrings(
      Register object1, Register object2, Register scratch1, Register scratch2,
      Label* on_not_flat_one_byte_strings);

  // Checks if the given register or operand is a unique name
  void JumpIfNotUniqueNameInstanceType(Register reg, Label* not_unique_name,
                                       Label::Distance distance = Label::kFar) {
    JumpIfNotUniqueNameInstanceType(Operand(reg), not_unique_name, distance);
  }

  void JumpIfNotUniqueNameInstanceType(Operand operand, Label* not_unique_name,
                                       Label::Distance distance = Label::kFar);

  void EmitSeqStringSetCharCheck(Register string, Register index,
                                 Register value, uint32_t encoding_mask);

  static int SafepointRegisterStackIndex(Register reg) {
    return SafepointRegisterStackIndex(reg.code());
  }

  // Load the type feedback vector from a JavaScript frame.
  void EmitLoadTypeFeedbackVector(Register vector);

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void EnterFrame(StackFrame::Type type, bool load_constant_pool_pointer_reg);
  void LeaveFrame(StackFrame::Type type);

  void EnterBuiltinFrame(Register context, Register target, Register argc);
  void LeaveBuiltinFrame(Register context, Register target, Register argc);

  // Expects object in eax and returns map with validated enum cache
  // in eax.  Assumes that any other register can be used as a scratch.
  void CheckEnumCache(Label* call_runtime);

  // AllocationMemento support. Arrays may have an associated
  // AllocationMemento object that can be checked for in order to pretransition
  // to another type.
  // On entry, receiver_reg should point to the array object.
  // scratch_reg gets clobbered.
  // If allocation info is present, conditional code is set to equal.
  void TestJSArrayForAllocationMemento(Register receiver_reg,
                                       Register scratch_reg,
                                       Label* no_memento_found);

 private:
  bool generating_stub_;
  bool has_frame_;
  // This handle will be patched with the code object on installation.
  Handle<Object> code_object_;

  // Helper functions for generating invokes.
  void InvokePrologue(const ParameterCount& expected,
                      const ParameterCount& actual, Label* done,
                      bool* definitely_mismatches, InvokeFlag flag,
                      Label::Distance done_distance,
                      const CallWrapper& call_wrapper);

  void EnterExitFramePrologue(StackFrame::Type frame_type);
  void EnterExitFrameEpilogue(int argc, bool save_doubles);

  void LeaveExitFrameEpilogue(bool restore_context);

  // Allocation support helpers.
  void LoadAllocationTopHelper(Register result, Register scratch,
                               AllocationFlags flags);

  void UpdateAllocationTopHelper(Register result_end, Register scratch,
                                 AllocationFlags flags);

  // Helper for implementing JumpIfNotInNewSpace and JumpIfInNewSpace.
  void InNewSpace(Register object, Register scratch, Condition cc,
                  Label* condition_met,
                  Label::Distance condition_met_distance = Label::kFar);

  // Helper for finding the mark bits for an address.  Afterwards, the
  // bitmap register points at the word with the mark bits and the mask
  // the position of the first bit.  Uses ecx as scratch and leaves addr_reg
  // unchanged.
  inline void GetMarkBits(Register addr_reg, Register bitmap_reg,
                          Register mask_reg);

  // Compute memory operands for safepoint stack slots.
  Operand SafepointRegisterSlot(Register reg);
  static int SafepointRegisterStackIndex(int reg_code);

  // Needs access to SafepointRegisterStackIndex for compiled frame
  // traversal.
  friend class StandardFrame;
};

// The code patcher is used to patch (typically) small parts of code e.g. for
// debugging and other types of instrumentation. When using the code patcher
// the exact number of bytes specified must be emitted. Is not legal to emit
// relocation information. If any of these constraints are violated it causes
// an assertion.
class CodePatcher {
 public:
  CodePatcher(Isolate* isolate, byte* address, int size);
  ~CodePatcher();

  // Macro assembler to emit code.
  MacroAssembler* masm() { return &masm_; }

 private:
  byte* address_;        // The address of the code being patched.
  int size_;             // Number of bytes of the expected patch size.
  MacroAssembler masm_;  // Macro assembler used to generate the code.
};

// -----------------------------------------------------------------------------
// Static helper functions.

// Generate an Operand for loading a field from an object.
inline Operand FieldOperand(Register object, int offset) {
  return Operand(object, offset - kHeapObjectTag);
}

// Generate an Operand for loading an indexed field from an object.
inline Operand FieldOperand(Register object, Register index, ScaleFactor scale,
                            int offset) {
  return Operand(object, index, scale, offset - kHeapObjectTag);
}

inline Operand FixedArrayElementOperand(Register array, Register index_as_smi,
                                        int additional_offset = 0) {
  int offset = FixedArray::kHeaderSize + additional_offset * kPointerSize;
  return FieldOperand(array, index_as_smi, times_half_pointer_size, offset);
}

inline Operand ContextOperand(Register context, int index) {
  return Operand(context, Context::SlotOffset(index));
}

inline Operand ContextOperand(Register context, Register index) {
  return Operand(context, index, times_pointer_size, Context::SlotOffset(0));
}

inline Operand NativeContextOperand() {
  return ContextOperand(esi, Context::NATIVE_CONTEXT_INDEX);
}

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_MACRO_ASSEMBLER_IA32_H_
