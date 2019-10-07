// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/objects-inl.h"
#include "src/objects/js-generator.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  __ Mov(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  if (exit_frame_type == BUILTIN_EXIT) {
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK(exit_frame_type == EXIT);
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithExitFrame),
            RelocInfo::CODE_TARGET);
  }
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_InternalArrayConstructor");
  Label generic_array_code;

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ Ldr(x10, FieldMemOperand(x1, JSFunction::kPrototypeOrInitialMapOffset));
    __ Tst(x10, kSmiTagMask);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
    __ CompareObjectType(x10, x11, x12, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  __ Jump(BUILTIN_CODE(masm->isolate(), InternalArrayConstructorImpl),
          RelocInfo::CODE_TARGET);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x1 : target function (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    // Push another copy as a parameter to the runtime call.
    __ SmiTag(x0);
    __ Push(x0, x1, x3, padreg);
    __ PushArgument(x1);

    __ CallRuntime(function_id, 1);
    __ Mov(x2, x0);

    // Restore target function and new target.
    __ Pop(padreg, x3, x1, x0);
    __ SmiUntag(x0);
  }

  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  __ Add(x2, x2, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x2);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  Label post_instantiation_deopt_entry;

  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x3     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubHelper");

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label already_aligned;
    Register argc = x0;

    if (__ emit_debug_code()) {
      // Check that FrameScope pushed the context on to the stack already.
      __ Peek(x2, 0);
      __ Cmp(x2, cp);
      __ Check(eq, AbortReason::kUnexpectedValue);
    }

    // Push number of arguments.
    __ SmiTag(x11, argc);
    __ Push(x11, padreg);

    // Add a slot for the receiver, and round up to maintain alignment.
    Register slot_count = x2;
    Register slot_count_without_rounding = x12;
    __ Add(slot_count_without_rounding, argc, 2);
    __ Bic(slot_count, slot_count_without_rounding, 1);
    __ Claim(slot_count);

    // Preserve the incoming parameters on the stack.
    __ LoadRoot(x10, RootIndex::kTheHoleValue);

    // Compute a pointer to the slot immediately above the location on the
    // stack to which arguments will be later copied.
    __ SlotAddress(x2, argc);

    // Poke the hole (receiver) in the highest slot.
    __ Str(x10, MemOperand(x2));
    __ Tbnz(slot_count_without_rounding, 0, &already_aligned);

    // Store padding, if needed.
    __ Str(padreg, MemOperand(x2, 1 * kPointerSize));
    __ Bind(&already_aligned);

    // Copy arguments to the expression stack.
    {
      Register count = x2;
      Register dst = x10;
      Register src = x11;
      __ Mov(count, argc);
      __ SlotAddress(dst, 0);
      __ Add(src, fp, StandardFrameConstants::kCallerSPOffset);
      __ CopyDoubleWords(dst, src, count);
    }

    // ----------- S t a t e -------------
    //  --                     x0: number of arguments (untagged)
    //  --                     x1: constructor function
    //  --                     x3: new target
    // If argc is odd:
    //  --     sp[0*kPointerSize]: argument n - 1
    //  --             ...
    //  -- sp[(n-1)*kPointerSize]: argument 0
    //  -- sp[(n+0)*kPointerSize]: the hole (receiver)
    //  -- sp[(n+1)*kPointerSize]: padding
    //  -- sp[(n+2)*kPointerSize]: padding
    //  -- sp[(n+3)*kPointerSize]: number of arguments (tagged)
    //  -- sp[(n+4)*kPointerSize]: context (pushed by FrameScope)
    // If argc is even:
    //  --     sp[0*kPointerSize]: argument n - 1
    //  --             ...
    //  -- sp[(n-1)*kPointerSize]: argument 0
    //  -- sp[(n+0)*kPointerSize]: the hole (receiver)
    //  -- sp[(n+1)*kPointerSize]: padding
    //  -- sp[(n+2)*kPointerSize]: number of arguments (tagged)
    //  -- sp[(n+3)*kPointerSize]: context (pushed by FrameScope)
    // -----------------------------------

    // Call the function.
    ParameterCount actual(argc);
    __ InvokeFunction(x1, x3, actual, CALL_FUNCTION);

    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame. Use fp relative
    // addressing to avoid the circular dependency between padding existence and
    // argc parity.
    __ SmiUntag(x1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ DropArguments(x1, TurboAssembler::kCountExcludesReceiver);
  __ Ret();
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Label* stack_overflow) {
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();

  // Check the stack for overflow.
  // We are not trying to catch interruptions (e.g. debug break and
  // preemption) here, so the "real stack limit" is checked.
  Label enough_stack_space;
  __ LoadRoot(scratch, RootIndex::kRealStackLimit);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ Sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ Cmp(scratch, Operand(num_args, LSL, kPointerSizeLog2));
  __ B(le, stack_overflow);
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x3     : new target
  //  -- lr     : return address
  //  -- cp     : context pointer
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubGeneric");

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    if (__ emit_debug_code()) {
      // Check that FrameScope pushed the context on to the stack already.
      __ Peek(x2, 0);
      __ Cmp(x2, cp);
      __ Check(eq, AbortReason::kUnexpectedValue);
    }

    // Preserve the incoming parameters on the stack.
    __ SmiTag(x0);
    __ Push(x0, x1, padreg, x3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  --        sp[1*kPointerSize]: padding
    //  -- x1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context (pushed by FrameScope)
    // -----------------------------------

    __ Ldr(x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kFlagsOffset));
    __ TestAndBranchIfAnySet(w4,
                             SharedFunctionInfo::IsDerivedConstructorBit::kMask,
                             &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        x4, x5);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ B(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ Bind(&not_create_implicit_receiver);
    __ LoadRoot(x0, RootIndex::kTheHoleValue);

    // ----------- S t a t e -------------
    //  --                          x0: receiver
    //  -- Slot 4 / sp[0*kPointerSize]: new target
    //  -- Slot 3 / sp[1*kPointerSize]: padding
    //  -- Slot 2 / sp[2*kPointerSize]: constructor function
    //  -- Slot 1 / sp[3*kPointerSize]: number of arguments (tagged)
    //  -- Slot 0 / sp[4*kPointerSize]: context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());

    __ Bind(&post_instantiation_deopt_entry);

    // Restore new target from the top of the stack.
    __ Peek(x3, 0 * kPointerSize);

    // Restore constructor function and argument count.
    __ Ldr(x1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ SmiUntag(x12, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Copy arguments to the expression stack. The called function pops the
    // receiver along with its arguments, so we need an extra receiver on the
    // stack, in case we have to return it later.

    // Overwrite the new target with a receiver.
    __ Poke(x0, 0);

    // Push two further copies of the receiver. One will be popped by the called
    // function. The second acts as padding if the number of arguments plus
    // receiver is odd - pushing receiver twice avoids branching. It also means
    // that we don't have to handle the even and odd cases specially on
    // InvokeFunction's return, as top of stack will be the receiver in either
    // case.
    __ Push(x0, x0);

    // ----------- S t a t e -------------
    //  --                        x3: new target
    //  --                       x12: number of arguments (untagged)
    //  --        sp[0*kPointerSize]: implicit receiver (overwrite if argc odd)
    //  --        sp[1*kPointerSize]: implicit receiver
    //  --        sp[2*kPointerSize]: implicit receiver
    //  --        sp[3*kPointerSize]: padding
    //  -- x1 and sp[4*kPointerSize]: constructor function
    //  --        sp[5*kPointerSize]: number of arguments (tagged)
    //  --        sp[6*kPointerSize]: context
    // -----------------------------------

    // Round the number of arguments down to the next even number, and claim
    // slots for the arguments. If the number of arguments was odd, the last
    // argument will overwrite one of the receivers pushed above.
    __ Bic(x10, x12, 1);

    // Check if we have enough stack space to push all arguments.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, x10, &stack_overflow);
    __ B(&enough_stack_space);

    __ Bind(&stack_overflow);
    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();

    __ Bind(&enough_stack_space);
    __ Claim(x10);

    // Copy the arguments.
    {
      Register count = x2;
      Register dst = x10;
      Register src = x11;
      __ Mov(count, x12);
      __ SlotAddress(dst, 0);
      __ Add(src, fp, StandardFrameConstants::kCallerSPOffset);
      __ CopyDoubleWords(dst, src, count);
    }

    // Call the function.
    __ Mov(x0, x12);
    ParameterCount actual(x0);
    __ InvokeFunction(x1, x3, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: padding
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ CompareRoot(x0, RootIndex::kUndefinedValue);
    __ B(eq, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(x0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ JumpIfObjectType(x0, x4, x5, FIRST_JS_RECEIVER_TYPE, &leave_frame, ge);
    __ B(&use_receiver);

    __ Bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ Bind(&use_receiver);
    __ Peek(x0, 0 * kPointerSize);
    __ CompareRoot(x0, RootIndex::kTheHoleValue);
    __ B(eq, &do_throw);

    __ Bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ SmiUntag(x1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ DropArguments(x1, TurboAssembler::kCountExcludesReceiver);
  __ Ret();
}
void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ PushArgument(x1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the value to pass to the generator
  //  -- x1 : the JSGeneratorObject to resume
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(x1);

  // Store input value into generator object.
  __ Str(x0, FieldMemOperand(x1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(x1, JSGeneratorObject::kInputOrDebugPosOffset, x0, x3,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ Ldr(x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  __ Ldr(cp, FieldMemOperand(x4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ Mov(x10, debug_hook);
  __ Ldrsb(x10, MemOperand(x10));
  __ CompareAndBranch(x10, Operand(0), ne, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ Mov(x10, debug_suspended_generator);
  __ Ldr(x10, MemOperand(x10));
  __ CompareAndBranch(x10, Operand(x1), eq,
                      &prepare_step_in_suspended_generator);
  __ Bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ CompareRoot(sp, RootIndex::kRealStackLimit);
  __ B(lo, &stack_overflow);

  // Get number of arguments for generator function.
  __ Ldr(x10, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
  __ Ldrh(w10, FieldMemOperand(
                   x10, SharedFunctionInfo::kFormalParameterCountOffset));

  // Claim slots for arguments and receiver (rounded up to a multiple of two).
  __ Add(x11, x10, 2);
  __ Bic(x11, x11, 1);
  __ Claim(x11);

  // Store padding (which might be replaced by the receiver).
  __ Sub(x11, x11, 1);
  __ Poke(padreg, Operand(x11, LSL, kPointerSizeLog2));

  // Poke receiver into highest claimed slot.
  __ Ldr(x5, FieldMemOperand(x1, JSGeneratorObject::kReceiverOffset));
  __ Poke(x5, Operand(x10, LSL, kPointerSizeLog2));

  // ----------- S t a t e -------------
  //  -- x1                       : the JSGeneratorObject to resume
  //  -- x4                       : generator function
  //  -- x10                      : argument count
  //  -- cp                       : generator context
  //  -- lr                       : return address
  //  -- sp[arg count]            : generator receiver
  //  -- sp[0 .. arg count - 1]   : claimed for args
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.

  __ Ldr(x5,
         FieldMemOperand(x1, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label loop, done;
    __ Cbz(x10, &done);
    __ Mov(x12, 0);

    __ Bind(&loop);
    __ Sub(x10, x10, 1);
    __ Add(x11, x5, Operand(x12, LSL, kPointerSizeLog2));
    __ Ldr(x11, FieldMemOperand(x11, FixedArray::kHeaderSize));
    __ Poke(x11, Operand(x10, LSL, kPointerSizeLog2));
    __ Add(x12, x12, 1);
    __ Cbnz(x10, &loop);
    __ Bind(&done);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    Label check_has_bytecode_array;
    __ Ldr(x3, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(x3, FieldMemOperand(x3, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(x3, x0, x0, INTERPRETER_DATA_TYPE);
    __ B(ne, &check_has_bytecode_array);
    __ Ldr(x3, FieldMemOperand(x3, InterpreterData::kBytecodeArrayOffset));
    __ Bind(&check_has_bytecode_array);
    __ CompareObjectType(x3, x3, x3, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ Ldr(x0, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ Ldrh(w0, FieldMemOperand(
                    x0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Mov(x3, x1);
    __ Mov(x1, x4);
    static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
    __ Ldr(x2, FieldMemOperand(x1, JSFunction::kCodeOffset));
    __ Add(x2, x2, Code::kHeaderSize - kHeapObjectTag);
    __ Jump(x2);
  }

  __ Bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push hole as receiver since we do not use it for stepping.
    __ LoadRoot(x5, RootIndex::kTheHoleValue);
    __ Push(x1, padreg, x4, x5);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(padreg, x1);
    __ Ldr(x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  }
  __ B(&stepping_prepared);

  __ Bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1, padreg);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(padreg, x1);
    __ Ldr(x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  }
  __ B(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();  // This should be unreachable.
  }
}

// Input:
//   x0: new.target.
//   x1: function.
//   x2: receiver.
//   x3: argc.
//   x4: argv.
// Output:
//   x0: result.
static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  Register new_target = x0;
  Register function = x1;
  Register receiver = x2;
  Register argc = x3;
  Register argv = x4;
  Register scratch = x10;
  Register slots_to_claim = x11;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  {
    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    __ Mov(scratch, ExternalReference::Create(IsolateAddressId::kContextAddress,
                                              masm->isolate()));
    __ Ldr(cp, MemOperand(scratch));

    // Claim enough space for the arguments, the receiver and the function,
    // including an optional slot of padding.
    __ Add(slots_to_claim, argc, 3);
    __ Bic(slots_to_claim, slots_to_claim, 1);

    // Check if we have enough stack space to push all arguments.
    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, slots_to_claim, &stack_overflow);
    __ B(&enough_stack_space);

    __ Bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();

    __ Bind(&enough_stack_space);
    __ Claim(slots_to_claim);

    // Store padding (which might be overwritten).
    __ SlotAddress(scratch, slots_to_claim);
    __ Str(padreg, MemOperand(scratch, -kPointerSize));

    // Store receiver and function on the stack.
    __ SlotAddress(scratch, argc);
    __ Stp(receiver, function, MemOperand(scratch));

    // Copy arguments to the stack in a loop, in reverse order.
    // x3: argc.
    // x4: argv.
    Label loop, done;

    // Skip the argument set up if we have no arguments.
    __ Cbz(argc, &done);

    // scratch has been set to point to the location of the receiver, which
    // marks the end of the argument copy.

    __ Bind(&loop);
    // Load the handle.
    __ Ldr(x11, MemOperand(argv, kPointerSize, PostIndex));
    // Dereference the handle.
    __ Ldr(x11, MemOperand(x11));
    // Poke the result into the stack.
    __ Str(x11, MemOperand(scratch, -kPointerSize, PreIndex));
    // Loop if we've not reached the end of copy marker.
    __ Cmp(sp, scratch);
    __ B(lt, &loop);

    __ Bind(&done);

    __ Mov(scratch, argc);
    __ Mov(argc, new_target);
    __ Mov(new_target, scratch);
    // x0: argc.
    // x3: new.target.

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    // The original values have been saved in JSEntryStub::GenerateBody().
    __ LoadRoot(x19, RootIndex::kUndefinedValue);
    __ Mov(x20, x19);
    __ Mov(x21, x19);
    __ Mov(x22, x19);
    __ Mov(x23, x19);
    __ Mov(x24, x19);
    __ Mov(x25, x19);
    __ Mov(x28, x19);
    // Don't initialize the reserved registers.
    // x26 : root register (kRootRegister).
    // x27 : context pointer (cp).
    // x29 : frame pointer (fp).

    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS internal frame and remove the parameters (except function),
    // and return.
  }

  // Result is in x0. Return.
  __ Ret();
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  // Store code entry in the closure.
  __ Str(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ Mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_size = scratch;

  // Get the arguments + receiver count.
  __ Ldr(args_size,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ldr(args_size.W(),
         FieldMemOperand(args_size, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  if (__ emit_debug_code()) {
    __ Tst(args_size, kPointerSize - 1);
    __ Check(eq, AbortReason::kUnexpectedValue);
  }
  __ Lsr(args_size, args_size, kPointerSizeLog2);
  __ DropArguments(args_size);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ CompareAndBranch(smi_entry, Operand(Smi::FromEnum(marker)), ne, &no_match);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee if needed, and caller)
  //  -- x3 : new target (preserved for callee if needed, and caller)
  //  -- x1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(
      !AreAliased(feedback_vector, x0, x1, x3, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_weak_ref, fallthrough;

  Register closure = x1;
  Register optimized_code_entry = scratch1;

  __ Ldr(
      optimized_code_entry,
      FieldMemOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret is at a weak reference to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_weak_ref);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ CompareAndBranch(optimized_code_entry,
                        Operand(Smi::FromEnum(OptimizationMarker::kNone)), eq,
                        &fallthrough);

    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kLogFirstExecution,
                                  Runtime::kFunctionFirstExecution);
    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kCompileOptimized,
                                  Runtime::kCompileOptimized_NotConcurrent);
    TailCallRuntimeIfMarkerEquals(
        masm, optimized_code_entry,
        OptimizationMarker::kCompileOptimizedConcurrent,
        Runtime::kCompileOptimized_Concurrent);

    {
      // Otherwise, the marker is InOptimizationQueue, so fall through hoping
      // that an interrupt will eventually update the slot with optimized code.
      if (FLAG_debug_code) {
        __ Cmp(
            optimized_code_entry,
            Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
        __ Assert(eq, AbortReason::kExpectedOptimizationSentinel);
      }
      __ B(&fallthrough);
    }
  }

  {
    // Optimized code slot is a weak reference.
    __ bind(&optimized_code_slot_is_weak_ref);

    __ LoadWeakValue(optimized_code_entry, optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ Ldr(scratch2, FieldMemOperand(optimized_code_entry,
                                     Code::kCodeDataContainerOffset));
    __ Ldr(
        scratch2,
        FieldMemOperand(scratch2, CodeDataContainer::kKindSpecificFlagsOffset));
    __ TestAndBranchIfAnySet(scratch2, 1 << Code::kMarkedForDeoptimizationBit,
                             &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
    __ Add(x2, optimized_code_entry,
           Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(x2);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    __ bind(&found_deoptimized_code);
    GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
  }

  // Fall-through if the optimized code cell is clear and there is no
  // optimization marker.
  __ bind(&fallthrough);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Label* if_return) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ Mov(bytecode_size_table, ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ Cmp(bytecode, Operand(0x3));
  __ B(hi, &process_bytecode);
  __ Tst(bytecode, Operand(0x1));
  __ B(ne, &extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ Add(bytecode_offset, bytecode_offset, Operand(1));
  __ Ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ Add(bytecode_size_table, bytecode_size_table,
         Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ B(&process_bytecode);

  __ Bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ Add(bytecode_offset, bytecode_offset, Operand(1));
  __ Ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ Add(bytecode_size_table, bytecode_size_table,
         Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  __ Bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                              \
  __ Cmp(x1, Operand(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ B(if_return, eq);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
  __ Ldr(scratch1.W(), MemOperand(bytecode_size_table, bytecode, LSL, 2));
  __ Add(bytecode_offset, bytecode_offset, scratch1);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   - x1: the JS function object being called.
//   - x3: the incoming new target or generator object
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - lr: return address.
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = x1;
  Register feedback_vector = x2;

  // Load the feedback vector from the closure.
  __ Ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ Ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, x7, x4, x5);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ Push(lr, fp, cp, closure);
  __ Add(fp, sp, StandardFrameConstants::kFixedFrameSizeFromFp);

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  Label has_bytecode_array;
  __ Ldr(x0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(x0, SharedFunctionInfo::kFunctionDataOffset));
  __ CompareObjectType(kInterpreterBytecodeArrayRegister, x11, x11,
                       INTERPRETER_DATA_TYPE);
  __ B(ne, &has_bytecode_array);
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(kInterpreterBytecodeArrayRegister,
                         InterpreterData::kBytecodeArrayOffset));
  __ Bind(&has_bytecode_array);

  // Increment invocation count for the function.
  __ Ldr(x11, FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ Ldr(x11, FieldMemOperand(x11, Cell::kValueOffset));
  __ Ldr(w10, FieldMemOperand(x11, FeedbackVector::kInvocationCountOffset));
  __ Add(w10, w10, Operand(1));
  __ Str(w10, FieldMemOperand(x11, FeedbackVector::kInvocationCountOffset));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ AssertNotSmi(
        kInterpreterBytecodeArrayRegister,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x0, x0,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ Mov(x10, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ Strb(x10, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kBytecodeAgeOffset));

  // Load the initial bytecode offset.
  __ Mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(x0, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, x0);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ Ldr(w11, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ Sub(x10, sp, Operand(x11));
    __ CompareRoot(x10, RootIndex::kRealStackLimit);
    __ B(hs, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // Note: there should always be at least one stack slot for the return
    // register in the register file.
    Label loop_header;
    __ LoadRoot(x10, RootIndex::kUndefinedValue);
    __ Lsr(x11, x11, kPointerSizeLog2);
    // Round up the number of registers to a multiple of 2, to align the stack
    // to 16 bytes.
    __ Add(x11, x11, 1);
    __ Bic(x11, x11, 1);
    __ PushMultipleTimes(x10, x11);
    __ Bind(&loop_header);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in x3.
  Label no_incoming_new_target_or_generator_register;
  __ Ldrsw(x10,
           FieldMemOperand(
               kInterpreterBytecodeArrayRegister,
               BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Cbz(x10, &no_incoming_new_target_or_generator_register);
  __ Str(x3, MemOperand(fp, x10, LSL, kPointerSizeLog2));
  __ Bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Mov(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ Ldrb(x18, MemOperand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x18, LSL, kPointerSizeLog2));
  __ Ldr(kJavaScriptCallCodeStartRegister,
         MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ Ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, x1, x2,
                                &do_return);
  __ B(&do_dispatch);

  __ bind(&do_return);
  // The return value is in x0.
  LeaveInterpreterFrame(masm, x2);
  __ Ret();
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args,
                                         Register first_arg_index,
                                         Register spread_arg_out,
                                         ConvertReceiverMode receiver_mode,
                                         InterpreterPushArgsMode mode) {
  Register last_arg_addr = x10;
  Register stack_addr = x11;
  Register slots_to_claim = x12;
  Register slots_to_copy = x13;  // May include receiver, unlike num_args.

  DCHECK(!AreAliased(num_args, first_arg_index, last_arg_addr, stack_addr,
                     slots_to_claim, slots_to_copy));
  // spread_arg_out may alias with the first_arg_index input.
  DCHECK(!AreAliased(spread_arg_out, last_arg_addr, stack_addr, slots_to_claim,
                     slots_to_copy));

  // Add one slot for the receiver.
  __ Add(slots_to_claim, num_args, 1);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Exclude final spread from slots to claim and the number of arguments.
    __ Sub(slots_to_claim, slots_to_claim, 1);
    __ Sub(num_args, num_args, 1);
  }

  // Add a stack check before pushing arguments.
  Label stack_overflow, done;
  Generate_StackOverflowCheck(masm, slots_to_claim, &stack_overflow);
  __ B(&done);
  __ Bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ Unreachable();
  __ Bind(&done);

  // Round up to an even number of slots and claim them.
  __ Add(slots_to_claim, slots_to_claim, 1);
  __ Bic(slots_to_claim, slots_to_claim, 1);
  __ Claim(slots_to_claim);

  {
    // Store padding, which may be overwritten.
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireX();
    __ Sub(scratch, slots_to_claim, 1);
    __ Poke(padreg, Operand(scratch, LSL, kPointerSizeLog2));
  }

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // Store "undefined" as the receiver arg if we need to.
    Register receiver = x14;
    __ LoadRoot(receiver, RootIndex::kUndefinedValue);
    __ SlotAddress(stack_addr, num_args);
    __ Str(receiver, MemOperand(stack_addr));
    __ Mov(slots_to_copy, num_args);
  } else {
    // If we're not given an explicit receiver to store, we'll need to copy it
    // together with the rest of the arguments.
    __ Add(slots_to_copy, num_args, 1);
  }

  __ Sub(last_arg_addr, first_arg_index,
         Operand(slots_to_copy, LSL, kPointerSizeLog2));
  __ Add(last_arg_addr, last_arg_addr, kPointerSize);

  // Load the final spread argument into spread_arg_out, if necessary.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Ldr(spread_arg_out, MemOperand(last_arg_addr, -kPointerSize));
  }

  // Copy the rest of the arguments.
  __ SlotAddress(stack_addr, 0);
  __ CopyDoubleWords(stack_addr, last_arg_addr, slots_to_copy);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  // Push the arguments. num_args may be updated according to mode.
  // spread_arg_out will be updated to contain the last spread argument, when
  // mode == InterpreterPushArgsMode::kWithFinalSpread.
  Register num_args = x0;
  Register first_arg_index = x2;
  Register spread_arg_out =
      (mode == InterpreterPushArgsMode::kWithFinalSpread) ? x2 : no_reg;
  Generate_InterpreterPushArgs(masm, num_args, first_arg_index, spread_arg_out,
                               receiver_mode, mode);

  // Call the target.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- x0 : argument count (not including receiver)
  // -- x3 : new target
  // -- x1 : constructor to call
  // -- x2 : allocation site feedback if available, undefined otherwise
  // -- x4 : address of the first argument
  // -----------------------------------
  __ AssertUndefinedOrAllocationSite(x2);

  // Push the arguments. num_args may be updated according to mode.
  // spread_arg_out will be updated to contain the last spread argument, when
  // mode == InterpreterPushArgsMode::kWithFinalSpread.
  Register num_args = x0;
  Register first_arg_index = x4;
  Register spread_arg_out =
      (mode == InterpreterPushArgsMode::kWithFinalSpread) ? x2 : no_reg;
  Generate_InterpreterPushArgs(masm, num_args, first_arg_index, spread_arg_out,
                               ConvertReceiverMode::kNullOrUndefined, mode);

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(x1);

    // Tail call to the array construct stub (still in the caller
    // context at this point).
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Label builtin_trampoline, trampoline_loaded;
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);

  // If the SFI function_data is an InterpreterData, get the trampoline stored
  // in it, otherwise get the trampoline from the builtins list.
  __ Ldr(x1, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ Ldr(x1, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x1, FieldMemOperand(x1, SharedFunctionInfo::kFunctionDataOffset));
  __ CompareObjectType(x1, kInterpreterDispatchTableRegister,
                       kInterpreterDispatchTableRegister,
                       INTERPRETER_DATA_TYPE);
  __ B(ne, &builtin_trampoline);

  __ Ldr(x1,
         FieldMemOperand(x1, InterpreterData::kInterpreterTrampolineOffset));
  __ B(&trampoline_loaded);

  __ Bind(&builtin_trampoline);
  __ LoadObject(x1, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));

  __ Bind(&trampoline_loaded);
  __ Add(lr, x1, Operand(interpreter_entry_return_pc_offset->value() +
                         Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ Mov(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ Ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(
        kInterpreterBytecodeArrayRegister,
        AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x1, x1,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ Ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Ldrb(x18, MemOperand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x18, LSL, kPointerSizeLog2));
  __ Ldr(kJavaScriptCallCodeStartRegister,
         MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, x1, x2,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(x2, kInterpreterBytecodeOffsetRegister);
  __ Str(x2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x1 : new target (preserved for callee)
  //  -- x3 : target function (preserved for callee)
  // -----------------------------------
  Register argc = x0;
  Register new_target = x1;
  Register target = x3;

  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push argument count, a copy of the target function and the new target,
    // together with some padding to maintain 16-byte alignment.
    __ SmiTag(argc);
    __ Push(argc, new_target, target, padreg);

    // Push another copy of new target as a parameter to the runtime call and
    // copy the rest of the arguments from caller (stdlib, foreign, heap).
    Label args_done;
    Register undef = x10;
    Register scratch1 = x12;
    Register scratch2 = x13;
    Register scratch3 = x14;
    __ LoadRoot(undef, RootIndex::kUndefinedValue);

    Label at_least_one_arg;
    Label three_args;
    DCHECK_NULL(Smi::kZero);
    __ Cbnz(argc, &at_least_one_arg);

    // No arguments.
    __ Push(new_target, undef, undef, undef);
    __ B(&args_done);

    __ Bind(&at_least_one_arg);
    // Load two arguments, though we may only use one (for the one arg case).
    __ Ldp(scratch2, scratch1,
           MemOperand(fp, StandardFrameConstants::kCallerSPOffset));

    // Set flags for determining the value of smi-tagged argc.
    //  lt => 1, eq => 2, gt => 3.
    __ Cmp(argc, Smi::FromInt(2));
    __ B(gt, &three_args);

    // One or two arguments.
    // If there is one argument (flags are lt), scratch2 contains that argument,
    // and scratch1 must be undefined.
    __ CmovX(scratch1, scratch2, lt);
    __ CmovX(scratch2, undef, lt);
    __ Push(new_target, scratch1, scratch2, undef);
    __ B(&args_done);

    // Three arguments.
    __ Bind(&three_args);
    __ Ldr(scratch3, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                        2 * kPointerSize));
    __ Push(new_target, scratch3, scratch1, scratch2);

    __ Bind(&args_done);

    // Call runtime, on success unwind frame, and parent frame.
    __ CallRuntime(Runtime::kInstantiateAsmJs, 4);

    // A smi 0 is returned on failure, an object on success.
    __ JumpIfSmi(x0, &failed);

    // Peek the argument count from the stack, untagging at the same time.
    __ SmiUntag(x4, MemOperand(sp, 3 * kPointerSize));
    __ Drop(4);
    scope.GenerateLeaveFrame();

    // Drop arguments and receiver.
    __ DropArguments(x4, TurboAssembler::kCountExcludesReceiver);
    __ Ret();

    __ Bind(&failed);
    // Restore target function and new target.
    __ Pop(padreg, target, new_target, argc);
    __ SmiUntag(argc);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ Ldr(x4, FieldMemOperand(new_target, JSFunction::kCodeOffset));
  __ Add(x4, x4, Code::kHeaderSize - kHeapObjectTag);
  __ Jump(x4);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  int frame_size = BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp +
                   (allocatable_register_count +
                    BuiltinContinuationFrameConstants::PaddingSlotCount(
                        allocatable_register_count)) *
                       kPointerSize;

  // Set up frame pointer.
  __ Add(fp, sp, frame_size);

  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ Str(x0,
           MemOperand(fp, BuiltinContinuationFrameConstants::kCallerSPOffset));
  }

  // Restore registers in pairs.
  int offset = -BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp -
               allocatable_register_count * kPointerSize;
  for (int i = allocatable_register_count - 1; i > 0; i -= 2) {
    int code1 = config->GetAllocatableGeneralCode(i);
    int code2 = config->GetAllocatableGeneralCode(i - 1);
    Register reg1 = Register::from_code(code1);
    Register reg2 = Register::from_code(code2);
    __ Ldp(reg1, reg2, MemOperand(fp, offset));
    offset += 2 * kPointerSize;
  }

  // Restore first register separately, if number of registers is odd.
  if (allocatable_register_count % 2 != 0) {
    int code = config->GetAllocatableGeneralCode(0);
    __ Ldr(Register::from_code(code), MemOperand(fp, offset));
  }

  if (java_script_builtin) __ SmiUntag(kJavaScriptCallArgCountRegister);

  // Load builtin object.
  UseScratchRegisterScope temps(masm);
  Register builtin = temps.AcquireX();
  __ Ldr(builtin,
         MemOperand(fp, BuiltinContinuationFrameConstants::kBuiltinOffset));

  // Restore fp, lr.
  __ Mov(sp, fp);
  __ Pop(fp, lr);

  // Call builtin.
  __ Add(builtin, builtin, Code::kHeaderSize - kHeapObjectTag);
  __ Br(builtin);
}
}  // namespace

void Builtins::Generate_ContinueToCodeStubBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, false);
}

void Builtins::Generate_ContinueToCodeStubBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, true);
}

void Builtins::Generate_ContinueToJavaScriptBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, false);
}

void Builtins::Generate_ContinueToJavaScriptBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, true);
}

void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Pop TOS register and padding.
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), x0.code());
  __ Pop(x0, padreg);
  __ Ret();
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ Ldr(x0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(x0, MemOperand(x0, JavaScriptFrameConstants::kFunctionOffset));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ PushArgument(x0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CompareAndBranch(x0, Smi::kZero, ne, &skip);
  __ Ret();

  __ Bind(&skip);

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ Ldr(x1, MemOperand(x0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ SmiUntag(x1,
              FieldMemOperand(x1, FixedArray::OffsetOfElementAt(
                                      DeoptimizationData::kOsrPcOffsetIndex)));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Add(x0, x0, x1);
  __ Add(lr, x0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- sp[0]    : argArray (if argc == 2)
  //  -- sp[8]    : thisArg  (if argc >= 1)
  //  -- sp[16]   : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_FunctionPrototypeApply");

  Register argc = x0;
  Register arg_array = x2;
  Register receiver = x1;
  Register this_arg = x0;
  Register undefined_value = x3;
  Register null_value = x4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);
  __ LoadRoot(null_value, RootIndex::kNullValue);

  // 1. Load receiver into x1, argArray into x2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Register saved_argc = x10;
    Register scratch = x11;

    // Push two undefined values on the stack, to put it in a consistent state
    // so that we can always read three arguments from it.
    __ Push(undefined_value, undefined_value);

    // The state of the stack (with arrows pointing to the slots we will read)
    // is as follows:
    //
    //       argc = 0               argc = 1                argc = 2
    // -> sp[16]: receiver    -> sp[24]: receiver     -> sp[32]: receiver
    // -> sp[8]:  undefined   -> sp[16]: this_arg     -> sp[24]: this_arg
    // -> sp[0]:  undefined   -> sp[8]:  undefined    -> sp[16]: arg_array
    //                           sp[0]:  undefined       sp[8]:  undefined
    //                                                   sp[0]:  undefined
    //
    // There are now always three arguments to read, in the slots starting from
    // slot argc.
    __ SlotAddress(scratch, argc);

    __ Mov(saved_argc, argc);
    __ Ldp(arg_array, this_arg, MemOperand(scratch));  // Overwrites argc.
    __ Ldr(receiver, MemOperand(scratch, 2 * kPointerSize));

    __ Drop(2);  // Drop the undefined values we pushed above.
    __ DropArguments(saved_argc, TurboAssembler::kCountExcludesReceiver);

    __ PushArgument(this_arg);
  }

  // ----------- S t a t e -------------
  //  -- x2      : argArray
  //  -- x1      : receiver
  //  -- sp[0]   : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ Cmp(arg_array, null_value);
  __ Ccmp(arg_array, undefined_value, ZFlag, ne);
  __ B(eq, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ Bind(&no_arguments);
  {
    __ Mov(x0, 0);
    DCHECK(receiver.Is(x1));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  Register argc = x0;
  Register function = x1;

  ASM_LOCATION("Builtins::Generate_FunctionPrototypeCall");

  // 1. Get the callable to call (passed as receiver) from the stack.
  __ Peek(function, Operand(argc, LSL, kXRegSizeLog2));

  // 2. Handle case with no arguments.
  {
    Label non_zero;
    Register scratch = x10;
    __ Cbnz(argc, &non_zero);
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
    // Overwrite receiver with undefined, which will be the new receiver.
    // We do not need to overwrite the padding slot above it with anything.
    __ Poke(scratch, 0);
    // Call function. The argument count is already zero.
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
    __ Bind(&non_zero);
  }

  // 3. Overwrite the receiver with padding. If argc is odd, this is all we
  //    need to do.
  Label arguments_ready;
  __ Poke(padreg, Operand(argc, LSL, kXRegSizeLog2));
  __ Tbnz(argc, 0, &arguments_ready);

  // 4. If argc is even:
  //    Copy arguments two slots higher in memory, overwriting the original
  //    receiver and padding.
  {
    Label loop;
    Register copy_from = x10;
    Register copy_to = x11;
    Register count = x12;
    Register last_arg_slot = x13;
    __ Mov(count, argc);
    __ Sub(last_arg_slot, argc, 1);
    __ SlotAddress(copy_from, last_arg_slot);
    __ Add(copy_to, copy_from, 2 * kPointerSize);
    __ CopyDoubleWords(copy_to, copy_from, count,
                       TurboAssembler::kSrcLessThanDst);
    // Drop two slots. These are copies of the last two arguments.
    __ Drop(2);
  }

  // 5. Adjust argument count to make the original first argument the new
  //    receiver and call the callable.
  __ Bind(&arguments_ready);
  __ Sub(argc, argc, 1);
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- sp[0]    : argumentsList (if argc == 3)
  //  -- sp[8]    : thisArgument  (if argc >= 2)
  //  -- sp[16]   : target        (if argc >= 1)
  //  -- sp[24]   : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ReflectApply");

  Register argc = x0;
  Register arguments_list = x2;
  Register target = x1;
  Register this_argument = x4;
  Register undefined_value = x3;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into x1 (if present), argumentsList into x2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Push four undefined values on the stack, to put it in a consistent state
    // so that we can always read the three arguments we need from it. The
    // fourth value is used for stack alignment.
    __ Push(undefined_value, undefined_value, undefined_value, undefined_value);

    // The state of the stack (with arrows pointing to the slots we will read)
    // is as follows:
    //
    //       argc = 0               argc = 1                argc = 2
    //    sp[32]: receiver       sp[40]: receiver        sp[48]: receiver
    // -> sp[24]: undefined   -> sp[32]: target       -> sp[40]: target
    // -> sp[16]: undefined   -> sp[24]: undefined    -> sp[32]: this_argument
    // -> sp[8]:  undefined   -> sp[16]: undefined    -> sp[24]: undefined
    //    sp[0]:  undefined      sp[8]:  undefined       sp[16]: undefined
    //                           sp[0]:  undefined       sp[8]:  undefined
    //                                                   sp[0]:  undefined
    //       argc = 3
    //    sp[56]: receiver
    // -> sp[48]: target
    // -> sp[40]: this_argument
    // -> sp[32]: arguments_list
    //    sp[24]: undefined
    //    sp[16]: undefined
    //    sp[8]:  undefined
    //    sp[0]:  undefined
    //
    // There are now always three arguments to read, in the slots starting from
    // slot (argc + 1).
    Register scratch = x10;
    __ SlotAddress(scratch, argc);
    __ Ldp(arguments_list, this_argument,
           MemOperand(scratch, 1 * kPointerSize));
    __ Ldr(target, MemOperand(scratch, 3 * kPointerSize));

    __ Drop(4);  // Drop the undefined values we pushed above.
    __ DropArguments(argc, TurboAssembler::kCountExcludesReceiver);

    __ PushArgument(this_argument);
  }

  // ----------- S t a t e -------------
  //  -- x2      : argumentsList
  //  -- x1      : target
  //  -- sp[0]   : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- sp[0]    : new.target (optional)
  //  -- sp[8]    : argumentsList
  //  -- sp[16]   : target
  //  -- sp[24]   : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ReflectConstruct");

  Register argc = x0;
  Register arguments_list = x2;
  Register target = x1;
  Register new_target = x3;
  Register undefined_value = x4;

  __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);

  // 1. Load target into x1 (if present), argumentsList into x2 (if present),
  // new.target into x3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Push four undefined values on the stack, to put it in a consistent state
    // so that we can always read the three arguments we need from it. The
    // fourth value is used for stack alignment.
    __ Push(undefined_value, undefined_value, undefined_value, undefined_value);

    // The state of the stack (with arrows pointing to the slots we will read)
    // is as follows:
    //
    //       argc = 0               argc = 1                argc = 2
    //    sp[32]: receiver       sp[40]: receiver        sp[48]: receiver
    // -> sp[24]: undefined   -> sp[32]: target       -> sp[40]: target
    // -> sp[16]: undefined   -> sp[24]: undefined    -> sp[32]: arguments_list
    // -> sp[8]:  undefined   -> sp[16]: undefined    -> sp[24]: undefined
    //    sp[0]:  undefined      sp[8]:  undefined       sp[16]: undefined
    //                           sp[0]:  undefined       sp[8]:  undefined
    //                                                   sp[0]:  undefined
    //       argc = 3
    //    sp[56]: receiver
    // -> sp[48]: target
    // -> sp[40]: arguments_list
    // -> sp[32]: new_target
    //    sp[24]: undefined
    //    sp[16]: undefined
    //    sp[8]:  undefined
    //    sp[0]:  undefined
    //
    // There are now always three arguments to read, in the slots starting from
    // slot (argc + 1).
    Register scratch = x10;
    __ SlotAddress(scratch, argc);
    __ Ldp(new_target, arguments_list, MemOperand(scratch, 1 * kPointerSize));
    __ Ldr(target, MemOperand(scratch, 3 * kPointerSize));

    __ Cmp(argc, 2);
    __ CmovX(new_target, target, ls);  // target if argc <= 2.

    __ Drop(4);  // Drop the undefined values we pushed above.
    __ DropArguments(argc, TurboAssembler::kCountExcludesReceiver);

    // Push receiver (undefined).
    __ PushArgument(undefined_value);
  }

  // ----------- S t a t e -------------
  //  -- x2      : argumentsList
  //  -- x1      : target
  //  -- x3      : new.target
  //  -- sp[0]   : receiver (undefined)
  // -----------------------------------

  // 2. We don't need to check explicitly for constructor target here,
  // since that's the first thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 3. We don't need to check explicitly for constructor new.target here,
  // since that's the second thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 4. Construct the target with the given new.target and argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithArrayLike),
          RelocInfo::CODE_TARGET);
}

namespace {

void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ Push(lr, fp);
  __ Mov(x11, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
  __ Push(x11, x1);  // x1: function
  __ SmiTag(x11, x0);  // x0: number of arguments.
  __ Push(x11, padreg);
  __ Add(fp, sp, ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp);
}

void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then drop the parameters and the receiver.
  __ Ldr(x10, MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Mov(sp, fp);
  __ Pop(fp, lr);

  // Drop actual parameters and receiver.
  __ SmiUntag(x10);
  __ DropArguments(x10, TurboAssembler::kCountExcludesReceiver);
}

// Prepares the stack for copying the varargs. First we claim the necessary
// slots, taking care of potential padding. Then we copy the existing arguments
// one slot up or one slot down, as needed.
void Generate_PrepareForCopyingVarargs(MacroAssembler* masm, Register argc,
                                       Register len) {
  Label len_odd, exit;
  Register slots_to_copy = x10;  // If needed.
  __ Add(slots_to_copy, argc, 1);
  __ Add(argc, argc, len);
  __ Tbnz(len, 0, &len_odd);
  __ Claim(len);
  __ B(&exit);

  __ Bind(&len_odd);
  // Claim space we need. If argc is even, slots_to_claim = len + 1, as we need
  // one extra padding slot. If argc is odd, we know that the original arguments
  // will have a padding slot we can reuse (since len is odd), so
  // slots_to_claim = len - 1.
  {
    Register scratch = x11;
    Register slots_to_claim = x12;
    __ Add(slots_to_claim, len, 1);
    __ And(scratch, argc, 1);
    __ Sub(slots_to_claim, slots_to_claim, Operand(scratch, LSL, 1));
    __ Claim(slots_to_claim);
  }

  Label copy_down;
  __ Tbz(slots_to_copy, 0, &copy_down);

  // Copy existing arguments one slot up.
  {
    Register src = x11;
    Register dst = x12;
    Register scratch = x13;
    __ Sub(scratch, argc, 1);
    __ SlotAddress(src, scratch);
    __ SlotAddress(dst, argc);
    __ CopyDoubleWords(dst, src, slots_to_copy,
                       TurboAssembler::kSrcLessThanDst);
  }
  __ B(&exit);

  // Copy existing arguments one slot down and add padding.
  __ Bind(&copy_down);
  {
    Register src = x11;
    Register dst = x12;
    Register scratch = x13;
    __ Add(src, len, 1);
    __ Mov(dst, len);  // CopySlots will corrupt dst.
    __ CopySlots(dst, src, slots_to_copy);
    __ Add(scratch, argc, 1);
    __ Poke(padreg, Operand(scratch, LSL, kPointerSizeLog2));  // Store padding.
  }

  __ Bind(&exit);
}

}  // namespace

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- x1 : target
  //  -- x0 : number of parameters on the stack (not including the receiver)
  //  -- x2 : arguments list (a FixedArray)
  //  -- x4 : len (number of elements to push from args)
  //  -- x3 : new.target (for [[Construct]])
  // -----------------------------------
  if (masm->emit_debug_code()) {
    // Allow x2 to be a FixedArray, or a FixedDoubleArray if x4 == 0.
    Label ok, fail;
    __ AssertNotSmi(x2, AbortReason::kOperandIsNotAFixedArray);
    __ Ldr(x10, FieldMemOperand(x2, HeapObject::kMapOffset));
    __ Ldrh(x13, FieldMemOperand(x10, Map::kInstanceTypeOffset));
    __ Cmp(x13, FIXED_ARRAY_TYPE);
    __ B(eq, &ok);
    __ Cmp(x13, FIXED_DOUBLE_ARRAY_TYPE);
    __ B(ne, &fail);
    __ Cmp(x4, 0);
    __ B(eq, &ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  Register arguments_list = x2;
  Register argc = x0;
  Register len = x4;

  Label stack_overflow;
  Generate_StackOverflowCheck(masm, len, &stack_overflow);

  // Skip argument setup if we don't need to push any varargs.
  Label done;
  __ Cbz(len, &done);

  Generate_PrepareForCopyingVarargs(masm, argc, len);

  // Push varargs.
  {
    Label loop;
    Register src = x10;
    Register the_hole_value = x11;
    Register undefined_value = x12;
    Register scratch = x13;
    __ Add(src, arguments_list, FixedArray::kHeaderSize - kHeapObjectTag);
    __ LoadRoot(the_hole_value, RootIndex::kTheHoleValue);
    __ LoadRoot(undefined_value, RootIndex::kUndefinedValue);
    // We do not use the CompareRoot macro as it would do a LoadRoot behind the
    // scenes and we want to avoid that in a loop.
    // TODO(all): Consider using Ldp and Stp.
    __ Bind(&loop);
    __ Sub(len, len, 1);
    __ Ldr(scratch, MemOperand(src, kPointerSize, PostIndex));
    __ Cmp(scratch, the_hole_value);
    __ Csel(scratch, scratch, undefined_value, ne);
    __ Poke(scratch, Operand(len, LSL, kPointerSizeLog2));
    __ Cbnz(len, &loop);
  }
  __ Bind(&done);

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x3 : the new.target (for [[Construct]] calls)
  //  -- x1 : the target to call (can be any Object)
  //  -- x2 : start index (to support rest parameters)
  // -----------------------------------

  Register argc = x0;
  Register start_index = x2;

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(x3, &new_target_not_constructor);
    __ Ldr(x5, FieldMemOperand(x3, HeapObject::kMapOffset));
    __ Ldrb(x5, FieldMemOperand(x5, Map::kBitFieldOffset));
    __ TestAndBranchIfAnySet(x5, Map::IsConstructorBit::kMask,
                             &new_target_constructor);
    __ Bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ PushArgument(x3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ Bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  // args_fp will point to the frame that contains the actual arguments, which
  // will be the current frame unless we have an arguments adaptor frame, in
  // which case args_fp points to the arguments adaptor frame.
  Register args_fp = x5;
  Register len = x6;
  {
    Label arguments_adaptor, arguments_done;
    Register scratch = x10;
    __ Ldr(args_fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ Ldr(x4, MemOperand(args_fp,
                          CommonFrameConstants::kContextOrFrameTypeOffset));
    __ Cmp(x4, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
    __ B(eq, &arguments_adaptor);
    {
      __ Ldr(scratch,
             MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
      __ Ldr(scratch,
             FieldMemOperand(scratch, JSFunction::kSharedFunctionInfoOffset));
      __ Ldrh(len,
              FieldMemOperand(scratch,
                              SharedFunctionInfo::kFormalParameterCountOffset));
      __ Mov(args_fp, fp);
    }
    __ B(&arguments_done);
    __ Bind(&arguments_adaptor);
    {
      // Just load the length from ArgumentsAdaptorFrame.
      __ SmiUntag(
          len,
          MemOperand(args_fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
    }
    __ Bind(&arguments_done);
  }

  Label stack_done, stack_overflow;
  __ Subs(len, len, start_index);
  __ B(le, &stack_done);
  // Check for stack overflow.
  Generate_StackOverflowCheck(masm, x6, &stack_overflow);

  Generate_PrepareForCopyingVarargs(masm, argc, len);

  // Push varargs.
  {
    Register dst = x13;
    __ Add(args_fp, args_fp, 2 * kPointerSize);
    __ SlotAddress(dst, 0);
    __ CopyDoubleWords(dst, args_fp, len);
  }
  __ B(&stack_done);

  __ Bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ Bind(&stack_done);

  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  ASM_LOCATION("Builtins::Generate_CallFunction");
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(x1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that function is not a "classConstructor".
  Label class_constructor;
  __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w3, FieldMemOperand(x2, SharedFunctionInfo::kFlagsOffset));
  __ TestAndBranchIfAnySet(w3, SharedFunctionInfo::IsClassConstructorBit::kMask,
                           &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ TestAndBranchIfAnySet(w3,
                           SharedFunctionInfo::IsNativeBit::kMask |
                               SharedFunctionInfo::IsStrictBit::kMask,
                           &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : the function to call (checked to be a JSFunction)
    //  -- x2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(x3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Peek(x3, Operand(x0, LSL, kXRegSizeLog2));
      __ JumpIfSmi(x3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(x3, x4, x4, FIRST_JS_RECEIVER_TYPE);
      __ B(hs, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(x3, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(x3, RootIndex::kNullValue, &convert_to_object);
        __ Bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(x3);
        }
        __ B(&convert_receiver);
      }
      __ Bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(x0);
        __ Push(padreg, x0, x1, cp);
        __ Mov(x0, x3);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Mov(x3, x0);
        __ Pop(cp, x1, x0, padreg);
        __ SmiUntag(x0);
      }
      __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
      __ Bind(&convert_receiver);
    }
    __ Poke(x3, Operand(x0, LSL, kXRegSizeLog2));
  }
  __ Bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  //  -- x2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ Ldrh(x2,
          FieldMemOperand(x2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(x0);
  ParameterCount expected(x2);
  __ InvokeFunctionCode(x1, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ Bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ PushArgument(x1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : target (checked to be a JSBoundFunction)
  //  -- x3 : new.target (only in case of [[Construct]])
  // -----------------------------------

  Register bound_argc = x4;
  Register bound_argv = x2;

  // Load [[BoundArguments]] into x2 and length of that into x4.
  Label no_bound_arguments;
  __ Ldr(bound_argv,
         FieldMemOperand(x1, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntag(bound_argc,
              FieldMemOperand(bound_argv, FixedArray::kLengthOffset));
  __ Cbz(bound_argc, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : target (checked to be a JSBoundFunction)
    //  -- x2 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- x3 : new.target (only in case of [[Construct]])
    //  -- x4 : the number of [[BoundArguments]]
    // -----------------------------------

    Register argc = x0;

    // Check for stack overflow.
    {
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      Label done;
      __ LoadRoot(x10, RootIndex::kRealStackLimit);
      // Make x10 the space we have left. The stack might already be overflowed
      // here which will cause x10 to become negative.
      __ Sub(x10, sp, x10);
      // Check if the arguments will overflow the stack.
      __ Cmp(x10, Operand(bound_argc, LSL, kPointerSizeLog2));
      __ B(hs, &done);
      __ TailCallRuntime(Runtime::kThrowStackOverflow);
      __ Bind(&done);
    }

    // Check if we need padding.
    Label copy_args, copy_bound_args;
    Register total_argc = x15;
    Register slots_to_claim = x12;
    __ Add(total_argc, argc, bound_argc);
    __ Mov(slots_to_claim, bound_argc);
    __ Tbz(bound_argc, 0, &copy_args);

    // Load receiver before we start moving the arguments. We will only
    // need this in this path because the bound arguments are odd.
    Register receiver = x14;
    __ Peek(receiver, Operand(argc, LSL, kPointerSizeLog2));

    // Claim space we need. If argc is even, slots_to_claim = bound_argc + 1,
    // as we need one extra padding slot. If argc is odd, we know that the
    // original arguments will have a padding slot we can reuse (since
    // bound_argc is odd), so slots_to_claim = bound_argc - 1.
    {
      Register scratch = x11;
      __ Add(slots_to_claim, bound_argc, 1);
      __ And(scratch, total_argc, 1);
      __ Sub(slots_to_claim, slots_to_claim, Operand(scratch, LSL, 1));
    }

    // Copy bound arguments.
    __ Bind(&copy_args);
    // Skip claim and copy of existing arguments in the special case where we
    // do not need to claim any slots (this will be the case when
    // bound_argc == 1 and the existing arguments have padding we can reuse).
    __ Cbz(slots_to_claim, &copy_bound_args);
    __ Claim(slots_to_claim);
    {
      Register count = x10;
      // Relocate arguments to a lower address.
      __ Mov(count, argc);
      __ CopySlots(0, slots_to_claim, count);

      __ Bind(&copy_bound_args);
      // Copy [[BoundArguments]] to the stack (below the arguments). The first
      // element of the array is copied to the highest address.
      {
        Label loop;
        Register counter = x10;
        Register scratch = x11;
        Register copy_to = x12;
        __ Add(bound_argv, bound_argv,
               FixedArray::kHeaderSize - kHeapObjectTag);
        __ SlotAddress(copy_to, argc);
        __ Add(argc, argc,
               bound_argc);  // Update argc to include bound arguments.
        __ Lsl(counter, bound_argc, kPointerSizeLog2);
        __ Bind(&loop);
        __ Sub(counter, counter, kPointerSize);
        __ Ldr(scratch, MemOperand(bound_argv, counter));
        // Poke into claimed area of stack.
        __ Str(scratch, MemOperand(copy_to, kPointerSize, PostIndex));
        __ Cbnz(counter, &loop);
      }

      {
        Label done;
        Register scratch = x10;
        __ Tbz(bound_argc, 0, &done);
        // Store receiver.
        __ Add(scratch, sp, Operand(total_argc, LSL, kPointerSizeLog2));
        __ Str(receiver, MemOperand(scratch, kPointerSize, PostIndex));
        __ Tbnz(total_argc, 0, &done);
        // Store padding.
        __ Str(padreg, MemOperand(scratch));
        __ Bind(&done);
      }
    }
  }
  __ Bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(x1);

  // Patch the receiver to [[BoundThis]].
  __ Ldr(x10, FieldMemOperand(x1, JSBoundFunction::kBoundThisOffset));
  __ Poke(x10, Operand(x0, LSL, kPointerSizeLog2));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ Ldr(x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(x1, &non_callable);
  __ Bind(&non_smi);
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq);
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ Ldrb(x4, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x4, Map::IsCallableBit::kMask, &non_callable);

  // Check if target is a proxy and call CallProxy external builtin
  __ Cmp(x5, JS_PROXY_TYPE);
  __ B(ne, &non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ Bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, x1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ PushArgument(x1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (checked to be a JSFunction)
  //  -- x3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(x1);
  __ AssertFunction(x1);

  // Calling convention for function specific ConstructStubs require
  // x2 to contain either an AllocationSite or undefined.
  __ LoadRoot(x2, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ Ldr(x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kFlagsOffset));
  __ TestAndBranchIfAllClear(
      w4, SharedFunctionInfo::ConstructAsBuiltinBit::kMask, &call_generic_stub);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  //  -- x3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(x1);
  __ AssertBoundFunction(x1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ Cmp(x1, x3);
    __ B(ne, &done);
    __ Ldr(x3,
           FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ Bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ Ldr(x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (can be any Object)
  //  -- x3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(x1, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ Ldr(x4, FieldMemOperand(x1, HeapObject::kMapOffset));
  __ Ldrb(x2, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x2, Map::IsConstructorBit::kMask,
                             &non_constructor);

  // Dispatch based on instance type.
  __ CompareInstanceType(x4, x5, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ Cmp(x5, JS_PROXY_TYPE);
  __ B(ne, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, x1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ArgumentsAdaptorTrampoline");
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  //  -- x3 : new target (passed through to callee)
  // -----------------------------------

  // The frame we are about to construct will look like:
  //
  //  slot      Adaptor frame
  //       +-----------------+--------------------------------
  //  -n-1 |    receiver     |                            ^
  //       |  (parameter 0)  |                            |
  //       |- - - - - - - - -|                            |
  //  -n   |                 |                          Caller
  //  ...  |       ...       |                       frame slots --> actual args
  //  -2   |  parameter n-1  |                            |
  //       |- - - - - - - - -|                            |
  //  -1   |   parameter n   |                            v
  //  -----+-----------------+--------------------------------
  //   0   |   return addr   |                            ^
  //       |- - - - - - - - -|                            |
  //   1   | saved frame ptr | <-- frame ptr              |
  //       |- - - - - - - - -|                            |
  //   2   |Frame Type Marker|                            |
  //       |- - - - - - - - -|                            |
  //   3   |    function     |                          Callee
  //       |- - - - - - - - -|                        frame slots
  //   4   |     num of      |                            |
  //       |   actual args   |                            |
  //       |- - - - - - - - -|                            |
  //   5   |     padding     |                            |
  //       |-----------------+----                        |
  //  [6]  |    [padding]    |   ^                        |
  //       |- - - - - - - - -|   |                        |
  // 6+pad |    receiver     |   |                        |
  //       |  (parameter 0)  |   |                        |
  //       |- - - - - - - - -|   |                        |
  // 7+pad |   parameter 1   |   |                        |
  //       |- - - - - - - - -| Frame slots ----> expected args
  // 8+pad |   parameter 2   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |                 |   |                        |
  //  ...  |       ...       |   |                        |
  //       |   parameter m   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |   [undefined]   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |                 |   |                        |
  //       |       ...       |   |                        |
  //       |   [undefined]   |   v   <-- stack ptr        v
  //  -----+-----------------+---------------------------------
  //
  // There is an optional slot of padding above the receiver to ensure stack
  // alignment of the arguments.
  // If the number of expected arguments is larger than the number of actual
  // arguments, the remaining expected slots will be filled with undefined.

  Register argc_actual = x0;    // Excluding the receiver.
  Register argc_expected = x2;  // Excluding the receiver.
  Register function = x1;

  Label dont_adapt_arguments, stack_overflow;

  Label enough_arguments;
  __ Cmp(argc_expected, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ B(eq, &dont_adapt_arguments);

  EnterArgumentsAdaptorFrame(masm);

  Register copy_from = x10;
  Register copy_end = x11;
  Register copy_to = x12;
  Register argc_to_copy = x13;
  Register argc_unused_actual = x14;
  Register scratch1 = x15, scratch2 = x16;

  // We need slots for the expected arguments, with one extra slot for the
  // receiver.
  __ RecordComment("-- Stack check --");
  __ Add(scratch1, argc_expected, 1);
  Generate_StackOverflowCheck(masm, scratch1, &stack_overflow);

  // Round up number of slots to be even, to maintain stack alignment.
  __ RecordComment("-- Allocate callee frame slots --");
  __ Add(scratch1, scratch1, 1);
  __ Bic(scratch1, scratch1, 1);
  __ Claim(scratch1, kPointerSize);

  __ Mov(copy_to, sp);

  // Preparing the expected arguments is done in four steps, the order of
  // which is chosen so we can use LDP/STP and avoid conditional branches as
  // much as possible.

  // (1) If we don't have enough arguments, fill the remaining expected
  // arguments with undefined, otherwise skip this step.
  __ Subs(scratch1, argc_actual, argc_expected);
  __ Csel(argc_unused_actual, xzr, scratch1, lt);
  __ Csel(argc_to_copy, argc_expected, argc_actual, ge);
  __ B(ge, &enough_arguments);

  // Fill the remaining expected arguments with undefined.
  __ RecordComment("-- Fill slots with undefined --");
  __ Sub(copy_end, copy_to, Operand(scratch1, LSL, kPointerSizeLog2));
  __ LoadRoot(scratch1, RootIndex::kUndefinedValue);

  Label fill;
  __ Bind(&fill);
  __ Stp(scratch1, scratch1, MemOperand(copy_to, 2 * kPointerSize, PostIndex));
  // We might write one slot extra, but that is ok because we'll overwrite it
  // below.
  __ Cmp(copy_end, copy_to);
  __ B(hi, &fill);

  // Correct copy_to, for the case where we wrote one additional slot.
  __ Mov(copy_to, copy_end);

  __ Bind(&enough_arguments);
  // (2) Copy all of the actual arguments, or as many as we need.
  Label skip_copy;
  __ RecordComment("-- Copy actual arguments --");
  __ Cbz(argc_to_copy, &skip_copy);
  __ Add(copy_end, copy_to, Operand(argc_to_copy, LSL, kPointerSizeLog2));
  __ Add(copy_from, fp, 2 * kPointerSize);
  // Adjust for difference between actual and expected arguments.
  __ Add(copy_from, copy_from,
         Operand(argc_unused_actual, LSL, kPointerSizeLog2));

  // Copy arguments. We use load/store pair instructions, so we might overshoot
  // by one slot, but since we copy the arguments starting from the last one, if
  // we do overshoot, the extra slot will be overwritten later by the receiver.
  Label copy_2_by_2;
  __ Bind(&copy_2_by_2);
  __ Ldp(scratch1, scratch2,
         MemOperand(copy_from, 2 * kPointerSize, PostIndex));
  __ Stp(scratch1, scratch2, MemOperand(copy_to, 2 * kPointerSize, PostIndex));
  __ Cmp(copy_end, copy_to);
  __ B(hi, &copy_2_by_2);
  __ Bind(&skip_copy);

  // (3) Store padding, which might be overwritten by the receiver, if it is not
  // necessary.
  __ RecordComment("-- Store padding --");
  __ Str(padreg, MemOperand(fp, -5 * kPointerSize));

  // (4) Store receiver. Calculate target address from the sp to avoid checking
  // for padding. Storing the receiver will overwrite either the extra slot
  // we copied with the actual arguments, if we did copy one, or the padding we
  // stored above.
  __ RecordComment("-- Store receiver --");
  __ Add(copy_from, fp, 2 * kPointerSize);
  __ Ldr(scratch1, MemOperand(copy_from, argc_actual, LSL, kPointerSizeLog2));
  __ Str(scratch1, MemOperand(sp, argc_expected, LSL, kPointerSizeLog2));

  // Arguments have been adapted. Now call the entry point.
  __ RecordComment("-- Call entry point --");
  __ Mov(argc_actual, argc_expected);
  // x0 : expected number of arguments
  // x1 : function (passed through to callee)
  // x3 : new target (passed through to callee)
  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  __ Ldr(x2, FieldMemOperand(function, JSFunction::kCodeOffset));
  __ Add(x2, x2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(x2);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // Call the entry point without adapting the arguments.
  __ RecordComment("-- Call without adapting args --");
  __ Bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  __ Ldr(x2, FieldMemOperand(function, JSFunction::kCodeOffset));
  __ Add(x2, x2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(x2);

  __ Bind(&stack_overflow);
  __ RecordComment("-- Stack overflow --");
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in w8 by the jump table trampoline.
  // Sign extend and convert to Smi for the runtime call.
  __ sxtw(x8, w8);
  __ SmiTag(x8, x8);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr RegList gp_regs =
        Register::ListOf<x0, x1, x2, x3, x4, x5, x6, x7>();
    constexpr RegList fp_regs =
        Register::ListOf<d0, d1, d2, d3, d4, d5, d6, d7>();
    __ PushXRegList(gp_regs);
    __ PushDRegList(fp_regs);

    // Pass instance and function index as explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, x8);
    // Load the correct CEntry builtin from the instance object.
    __ Ldr(x2, FieldMemOperand(kWasmInstanceRegister,
                               WasmInstanceObject::kCEntryStubOffset));
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Mov(cp, Smi::kZero);
    __ CallRuntimeWithCEntry(Runtime::kWasmCompileLazy, x2);
    // The entrypoint address is the return value.
    __ mov(x8, kReturnRegister0);

    // Restore registers.
    __ PopDRegList(fp_regs);
    __ PopXRegList(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(x8);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // The Abort mechanism relies on CallRuntime, which in turn relies on
  // CEntry, so until this stub has been generated, we have to use a
  // fall-back Abort mechanism.
  //
  // Note that this stub must be generated before any use of Abort.
  HardAbortScope hard_aborts(masm);

  ASM_LOCATION("CEntry::Generate entry");
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Register parameters:
  //    x0: argc (including receiver, untagged)
  //    x1: target
  // If argv_mode == kArgvInRegister:
  //    x11: argv (pointer to first argument)
  //
  // The stack on entry holds the arguments and the receiver, with the receiver
  // at the highest address:
  //
  //    sp]argc-1]: receiver
  //    sp[argc-2]: arg[argc-2]
  //    ...           ...
  //    sp[1]:      arg[1]
  //    sp[0]:      arg[0]
  //
  // The arguments are in reverse order, so that arg[argc-2] is actually the
  // first argument to the target function and arg[0] is the last.
  const Register& argc_input = x0;
  const Register& target_input = x1;

  // Calculate argv, argc and the target address, and store them in
  // callee-saved registers so we can retry the call without having to reload
  // these arguments.
  // TODO(jbramley): If the first call attempt succeeds in the common case (as
  // it should), then we might be better off putting these parameters directly
  // into their argument registers, rather than using callee-saved registers and
  // preserving them on the stack.
  const Register& argv = x21;
  const Register& argc = x22;
  const Register& target = x23;

  // Derive argv from the stack pointer so that it points to the first argument
  // (arg[argc-2]), or just below the receiver in case there are no arguments.
  //  - Adjust for the arg[] array.
  Register temp_argv = x11;
  if (argv_mode == kArgvOnStack) {
    __ SlotAddress(temp_argv, x0);
    //  - Adjust for the receiver.
    __ Sub(temp_argv, temp_argv, 1 * kPointerSize);
  }

  // Reserve three slots to preserve x21-x23 callee-saved registers.
  int extra_stack_space = 3;
  // Enter the exit frame.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      save_doubles == kSaveFPRegs, x10, extra_stack_space,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // Poke callee-saved registers into reserved space.
  __ Poke(argv, 1 * kPointerSize);
  __ Poke(argc, 2 * kPointerSize);
  __ Poke(target, 3 * kPointerSize);

  // We normally only keep tagged values in callee-saved registers, as they
  // could be pushed onto the stack by called stubs and functions, and on the
  // stack they can confuse the GC. However, we're only calling C functions
  // which can push arbitrary data onto the stack anyway, and so the GC won't
  // examine that part of the stack.
  __ Mov(argc, argc_input);
  __ Mov(target, target_input);
  __ Mov(argv, temp_argv);

  // x21 : argv
  // x22 : argc
  // x23 : call target
  //
  // The stack (on entry) holds the arguments and the receiver, with the
  // receiver at the highest address:
  //
  //         argv[8]:     receiver
  // argv -> argv[0]:     arg[argc-2]
  //         ...          ...
  //         argv[...]:   arg[1]
  //         argv[...]:   arg[0]
  //
  // Immediately below (after) this is the exit frame, as constructed by
  // EnterExitFrame:
  //         fp[8]:    CallerPC (lr)
  //   fp -> fp[0]:    CallerFP (old fp)
  //         fp[-8]:   Space reserved for SPOffset.
  //         fp[-16]:  CodeObject()
  //         sp[...]:  Saved doubles, if saved_doubles is true.
  //         sp[32]:   Alignment padding, if necessary.
  //         sp[24]:   Preserved x23 (used for target).
  //         sp[16]:   Preserved x22 (used for argc).
  //         sp[8]:    Preserved x21 (used for argv).
  //   sp -> sp[0]:    Space reserved for the return address.
  //
  // After a successful call, the exit frame, preserved registers (x21-x23) and
  // the arguments (including the receiver) are dropped or popped as
  // appropriate. The stub then returns.
  //
  // After an unsuccessful call, the exit frame and suchlike are left
  // untouched, and the stub either throws an exception by jumping to one of
  // the exception_returned label.

  // Prepare AAPCS64 arguments to pass to the builtin.
  __ Mov(x0, argc);
  __ Mov(x1, argv);
  __ Mov(x2, ExternalReference::isolate_address(masm->isolate()));

  Label return_location;
  __ Adr(x12, &return_location);
  __ Poke(x12, 0);

  if (__ emit_debug_code()) {
    // Verify that the slot below fp[kSPOffset]-8 points to the return location
    // (currently in x12).
    UseScratchRegisterScope temps(masm);
    Register temp = temps.AcquireX();
    __ Ldr(temp, MemOperand(fp, ExitFrameConstants::kSPOffset));
    __ Ldr(temp, MemOperand(temp, -static_cast<int64_t>(kXRegSize)));
    __ Cmp(temp, x12);
    __ Check(eq, AbortReason::kReturnAddressNotFoundInFrame);
  }

  // Call the builtin.
  __ Blr(target);
  __ Bind(&return_location);

  // Result returned in x0 or x1:x0 - do not destroy these registers!

  //  x0    result0      The return code from the call.
  //  x1    result1      For calls which return ObjectPair.
  //  x21   argv
  //  x22   argc
  //  x23   target
  const Register& result = x0;

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(result, RootIndex::kException);
  __ B(eq, &exception_returned);

  // The call succeeded, so unwind the stack and return.

  // Restore callee-saved registers x21-x23.
  __ Mov(x11, argc);

  __ Peek(argv, 1 * kPointerSize);
  __ Peek(argc, 2 * kPointerSize);
  __ Peek(target, 3 * kPointerSize);

  __ LeaveExitFrame(save_doubles == kSaveFPRegs, x10, x9);
  if (argv_mode == kArgvOnStack) {
    // Drop the remaining stack slots and return from the stub.
    __ DropArguments(x11);
  }
  __ AssertFPCRState();
  __ Ret();

  // Handling of exception.
  __ Bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set x0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Mov(x0, 0);  // argc.
    __ Mov(x1, 0);  // argv.
    __ Mov(x2, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ Mov(cp, pending_handler_context_address);
  __ Ldr(cp, MemOperand(cp));
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireX();
    __ Mov(scratch, pending_handler_sp_address);
    __ Ldr(scratch, MemOperand(scratch));
    __ Mov(sp, scratch);
  }
  __ Mov(fp, pending_handler_fp_address);
  __ Ldr(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label not_js_frame;
  __ Cbz(cp, &not_js_frame);
  __ Str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ Bind(&not_js_frame);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  __ Mov(x10, pending_handler_entrypoint_address);
  __ Ldr(x10, MemOperand(x10));
  __ Br(x10);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label done;
  Register result = x7;

  DCHECK(result.Is64Bits());

  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  UseScratchRegisterScope temps(masm);
  Register scratch1 = temps.AcquireX();
  Register scratch2 = temps.AcquireX();
  DoubleRegister double_scratch = temps.AcquireD();

  // Account for saved regs.
  const int kArgumentOffset = 2 * kPointerSize;

  __ Push(result, scratch1);  // scratch1 is also pushed to preserve alignment.
  __ Peek(double_scratch, kArgumentOffset);

  // Try to convert with a FPU convert instruction.  This handles all
  // non-saturating cases.
  __ TryConvertDoubleToInt64(result, double_scratch, &done);
  __ Fmov(result, double_scratch);

  // If we reach here we need to manually convert the input to an int32.

  // Extract the exponent.
  Register exponent = scratch1;
  __ Ubfx(exponent, result, HeapNumber::kMantissaBits,
          HeapNumber::kExponentBits);

  // It the exponent is >= 84 (kMantissaBits + 32), the result is always 0 since
  // the mantissa gets shifted completely out of the int32_t result.
  __ Cmp(exponent, HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 32);
  __ CzeroX(result, ge);
  __ B(ge, &done);

  // The Fcvtzs sequence handles all cases except where the conversion causes
  // signed overflow in the int64_t target. Since we've already handled
  // exponents >= 84, we can guarantee that 63 <= exponent < 84.

  if (masm->emit_debug_code()) {
    __ Cmp(exponent, HeapNumber::kExponentBias + 63);
    // Exponents less than this should have been handled by the Fcvt case.
    __ Check(ge, AbortReason::kUnexpectedValue);
  }

  // Isolate the mantissa bits, and set the implicit '1'.
  Register mantissa = scratch2;
  __ Ubfx(mantissa, result, 0, HeapNumber::kMantissaBits);
  __ Orr(mantissa, mantissa, 1UL << HeapNumber::kMantissaBits);

  // Negate the mantissa if necessary.
  __ Tst(result, kXSignMask);
  __ Cneg(mantissa, mantissa, ne);

  // Shift the mantissa bits in the correct place. We know that we have to shift
  // it left here, because exponent >= 63 >= kMantissaBits.
  __ Sub(exponent, exponent,
         HeapNumber::kExponentBias + HeapNumber::kMantissaBits);
  __ Lsl(result, mantissa, exponent);

  __ Bind(&done);
  __ Poke(result, kArgumentOffset);
  __ Pop(scratch1, result);
  __ Ret();
}

void Builtins::Generate_MathPowInternal(MacroAssembler* masm) {
  Register exponent_integer = x12;
  Register saved_lr = x19;
  VRegister result_double = d0;
  VRegister base_double = d0;
  VRegister exponent_double = d1;
  VRegister base_double_copy = d2;
  VRegister scratch1_double = d6;
  VRegister scratch0_double = d7;

  // A fast-path for integer exponents.
  Label exponent_is_integer;
  // Allocate a heap number for the result, and return it.
  Label done;

  // Unpack the inputs.

  // Handle double (heap number) exponents.
  // Detect integer exponents stored as doubles and handle those in the
  // integer fast-path.
  __ TryRepresentDoubleAsInt64(exponent_integer, exponent_double,
                               scratch0_double, &exponent_is_integer);

  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ Mov(saved_lr, lr);
    __ CallCFunction(ExternalReference::power_double_double_function(), 0, 2);
    __ Mov(lr, saved_lr);
    __ B(&done);
  }

  __ Bind(&exponent_is_integer);

  // Find abs(exponent). For negative exponents, we can find the inverse later.
  Register exponent_abs = x13;
  __ Cmp(exponent_integer, 0);
  __ Cneg(exponent_abs, exponent_integer, mi);

  // Repeatedly multiply to calculate the power.
  //  result = 1.0;
  //  For each bit n (exponent_integer{n}) {
  //    if (exponent_integer{n}) {
  //      result *= base;
  //    }
  //    base *= base;
  //    if (remaining bits in exponent_integer are all zero) {
  //      break;
  //    }
  //  }
  Label power_loop, power_loop_entry, power_loop_exit;
  __ Fmov(scratch1_double, base_double);
  __ Fmov(base_double_copy, base_double);
  __ Fmov(result_double, 1.0);
  __ B(&power_loop_entry);

  __ Bind(&power_loop);
  __ Fmul(scratch1_double, scratch1_double, scratch1_double);
  __ Lsr(exponent_abs, exponent_abs, 1);
  __ Cbz(exponent_abs, &power_loop_exit);

  __ Bind(&power_loop_entry);
  __ Tbz(exponent_abs, 0, &power_loop);
  __ Fmul(result_double, result_double, scratch1_double);
  __ B(&power_loop);

  __ Bind(&power_loop_exit);

  // If the exponent was positive, result_double holds the result.
  __ Tbz(exponent_integer, kXSignBit, &done);

  // The exponent was negative, so find the inverse.
  __ Fmov(scratch0_double, 1.0);
  __ Fdiv(result_double, scratch0_double, result_double);
  // ECMA-262 only requires Math.pow to return an 'implementation-dependent
  // approximation' of base^exponent. However, mjsunit/math-pow uses Math.pow
  // to calculate the subnormal value 2^-1074. This method of calculating
  // negative powers doesn't work because 2^1074 overflows to infinity. To
  // catch this corner-case, we bail out if the result was 0. (This can only
  // occur if the divisor is infinity or the base is zero.)
  __ Fcmp(result_double, 0.0);
  __ B(&done, ne);

  AllowExternalCallThatCantCauseGC scope(masm);
  __ Mov(saved_lr, lr);
  __ Fmov(base_double, base_double_copy);
  __ Scvtf(exponent_double, exponent_integer);
  __ CallCFunction(ExternalReference::power_double_double_function(), 0, 2);
  __ Mov(lr, saved_lr);
  __ Bind(&done);
  __ Ret();
}

namespace {

void GenerateInternalArrayConstructorCase(MacroAssembler* masm,
                                          ElementsKind kind) {
  Label zero_case, n_case;
  Register argc = x0;

  __ Cbz(argc, &zero_case);
  __ CompareAndBranch(argc, 1, ne, &n_case);

  // One argument.
  if (IsFastPackedElementsKind(kind)) {
    Label packed_case;

    // We might need to create a holey array; look at the first argument.
    __ Peek(x10, 0);
    __ Cbz(x10, &packed_case);

    __ Jump(CodeFactory::InternalArraySingleArgumentConstructor(
                masm->isolate(), GetHoleyElementsKind(kind))
                .code(),
            RelocInfo::CODE_TARGET);

    __ Bind(&packed_case);
  }

  __ Jump(
      CodeFactory::InternalArraySingleArgumentConstructor(masm->isolate(), kind)
          .code(),
      RelocInfo::CODE_TARGET);

  __ Bind(&zero_case);
  // No arguments.
  __ Jump(CodeFactory::InternalArrayNoArgumentConstructor(masm->isolate(), kind)
              .code(),
          RelocInfo::CODE_TARGET);

  __ Bind(&n_case);
  // N arguments.
  // Load undefined into the allocation site parameter as required by
  // ArrayNArgumentsConstructor.
  __ LoadRoot(kJavaScriptCallExtraArg1Register, RootIndex::kUndefinedValue);
  Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayNArgumentsConstructor);
  __ Jump(code, RelocInfo::CODE_TARGET);
}

}  // namespace

void Builtins::Generate_InternalArrayConstructorImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argc
  //  -- x1 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  Register constructor = x1;

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    Label unexpected_map, map_ok;
    // Initial map for the builtin Array function should be a map.
    __ Ldr(x10, FieldMemOperand(constructor,
                                JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ JumpIfSmi(x10, &unexpected_map);
    __ JumpIfObjectType(x10, x10, x11, MAP_TYPE, &map_ok);
    __ Bind(&unexpected_map);
    __ Abort(AbortReason::kUnexpectedInitialMapForArrayFunction);
    __ Bind(&map_ok);
  }

  Register kind = w3;
  // Figure out the right elements kind
  __ Ldr(x10, FieldMemOperand(constructor,
                              JSFunction::kPrototypeOrInitialMapOffset));

  // Retrieve elements_kind from map.
  __ LoadElementsKindFromMap(kind, x10);

  if (FLAG_debug_code) {
    Label done;
    __ Cmp(x3, PACKED_ELEMENTS);
    __ Ccmp(x3, HOLEY_ELEMENTS, ZFlag, ne);
    __ Assert(
        eq,
        AbortReason::kInvalidElementsKindForInternalArrayOrInternalPackedArray);
  }

  Label fast_elements_case;
  __ CompareAndBranch(kind, PACKED_ELEMENTS, eq, &fast_elements_case);
  GenerateInternalArrayConstructorCase(masm, HOLEY_ELEMENTS);

  __ Bind(&fast_elements_case);
  GenerateInternalArrayConstructorCase(masm, PACKED_ELEMENTS);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
