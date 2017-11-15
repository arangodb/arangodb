// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- r3                 : number of arguments excluding receiver
  //  -- r4                 : target
  //  -- r6                 : new.target
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r4);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // JumpToExternalReference expects r3 to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ addi(r3, r3, Operand(num_extra_args + 1));

  // Insert extra arguments.
  __ SmiTag(r3);
  __ Push(r3, r4, r6);
  __ SmiUntag(r3);

  __ JumpToExternalReference(ExternalReference(address, masm->isolate()),
                             exit_frame_type == BUILTIN_EXIT);
}

// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the current native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}

// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the Array function from the current native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}

void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r4);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r5, r0);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, cr0);
    __ CompareObjectType(r5, r6, r7, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r4);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r5, r0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r6, r7, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  __ mr(r6, r4);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

// static
void Builtins::Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  Condition const cond_done = (kind == MathMaxMinKind::kMin) ? lt : gt;
  Heap::RootListIndex const root_index =
      (kind == MathMaxMinKind::kMin) ? Heap::kInfinityValueRootIndex
                                     : Heap::kMinusInfinityValueRootIndex;
  DoubleRegister const reg = (kind == MathMaxMinKind::kMin) ? d2 : d1;

  // Load the accumulator with the default return value (either -Infinity or
  // +Infinity), with the tagged value in r8 and the double value in d1.
  __ LoadRoot(r8, root_index);
  __ lfd(d1, FieldMemOperand(r8, HeapNumber::kValueOffset));

  // Setup state for loop
  // r5: address of arg[0] + kPointerSize
  // r6: number of slots to drop at exit (arguments + receiver)
  __ addi(r7, r3, Operand(1));

  Label done_loop, loop;
  __ mr(r7, r3);
  __ bind(&loop);
  {
    // Check if all parameters done.
    __ subi(r7, r7, Operand(1));
    __ cmpi(r7, Operand::Zero());
    __ blt(&done_loop);

    // Load the next parameter tagged value into r5.
    __ ShiftLeftImm(r5, r7, Operand(kPointerSizeLog2));
    __ LoadPX(r5, MemOperand(sp, r5));

    // Load the double value of the parameter into d2, maybe converting the
    // parameter to a number first using the ToNumber builtin if necessary.
    Label convert, convert_smi, convert_number, done_convert;
    __ bind(&convert);
    __ JumpIfSmi(r5, &convert_smi);
    __ LoadP(r6, FieldMemOperand(r5, HeapObject::kMapOffset));
    __ JumpIfRoot(r6, Heap::kHeapNumberMapRootIndex, &convert_number);
    {
      // Parameter is not a Number, use the ToNumber builtin to convert it.
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r3);
      __ SmiTag(r7);
      __ EnterBuiltinFrame(cp, r4, r3);
      __ Push(r7, r8);
      __ mr(r3, r5);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ mr(r5, r3);
      __ Pop(r7, r8);
      __ LeaveBuiltinFrame(cp, r4, r3);
      __ SmiUntag(r7);
      __ SmiUntag(r3);
      {
        // Restore the double accumulator value (d1).
        Label done_restore;
        __ SmiToDouble(d1, r8);
        __ JumpIfSmi(r8, &done_restore);
        __ lfd(d1, FieldMemOperand(r8, HeapNumber::kValueOffset));
        __ bind(&done_restore);
      }
    }
    __ b(&convert);
    __ bind(&convert_number);
    __ lfd(d2, FieldMemOperand(r5, HeapNumber::kValueOffset));
    __ b(&done_convert);
    __ bind(&convert_smi);
    __ SmiToDouble(d2, r5);
    __ bind(&done_convert);

    // Perform the actual comparison with the accumulator value on the left hand
    // side (d1) and the next parameter value on the right hand side (d2).
    Label compare_nan, compare_swap;
    __ fcmpu(d1, d2);
    __ bunordered(&compare_nan);
    __ b(cond_done, &loop);
    __ b(CommuteCondition(cond_done), &compare_swap);

    // Left and right hand side are equal, check for -0 vs. +0.
    __ TestDoubleIsMinusZero(reg, r9, r0);
    __ bne(&loop);

    // Update accumulator. Result is on the right hand side.
    __ bind(&compare_swap);
    __ fmr(d1, d2);
    __ mr(r8, r5);
    __ b(&loop);

    // At least one side is NaN, which means that the result will be NaN too.
    // We still need to visit the rest of the arguments.
    __ bind(&compare_nan);
    __ LoadRoot(r8, Heap::kNanValueRootIndex);
    __ lfd(d1, FieldMemOperand(r8, HeapNumber::kValueOffset));
    __ b(&loop);
  }

  __ bind(&done_loop);
  // Drop all slots, including the receiver.
  __ addi(r3, r3, Operand(1));
  __ Drop(r3);
  __ mr(r3, r8);
  __ Ret();
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r3.
  Label no_arguments;
  {
    __ mr(r5, r3);  // Store argc in r5.
    __ cmpi(r3, Operand::Zero());
    __ beq(&no_arguments);
    __ subi(r3, r3, Operand(1));
    __ ShiftLeftImm(r3, r3, Operand(kPointerSizeLog2));
    __ LoadPX(r3, MemOperand(sp, r3));
  }

  // 2a. Convert the first argument to a number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r5);
    __ EnterBuiltinFrame(cp, r4, r5);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, r4, r5);
    __ SmiUntag(r5);
  }

  {
    // Drop all arguments including the receiver.
    __ Drop(r5);
    __ Ret(1);
  }

  // 2b. No arguments, return +0.
  __ bind(&no_arguments);
  __ LoadSmiLiteral(r3, Smi::kZero);
  __ Ret(1);
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : constructor function
  //  -- r6                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // 2. Load the first argument into r5.
  {
    Label no_arguments, done;
    __ mr(r9, r3);  // Store argc in r9.
    __ cmpi(r3, Operand::Zero());
    __ beq(&no_arguments);
    __ subi(r3, r3, Operand(1));
    __ ShiftLeftImm(r5, r3, Operand(kPointerSizeLog2));
    __ LoadPX(r5, MemOperand(sp, r5));
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadSmiLiteral(r5, Smi::kZero);
    __ bind(&done);
  }

  // 3. Make sure r5 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(r5, &done_convert);
    __ CompareObjectType(r5, r7, r7, HEAP_NUMBER_TYPE);
    __ beq(&done_convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r9);
      __ EnterBuiltinFrame(cp, r4, r9);
      __ Push(r6);
      __ mr(r3, r5);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ mr(r5, r3);
      __ Pop(r6);
      __ LeaveBuiltinFrame(cp, r4, r9);
      __ SmiUntag(r9);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ cmp(r4, r6);
  __ bne(&new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(r3, r4, r5, r7, r8, &new_object);
  __ b(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r9);
    __ EnterBuiltinFrame(cp, r4, r9);
    __ Push(r5);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(r5);
    __ LeaveBuiltinFrame(cp, r4, r9);
    __ SmiUntag(r9);
  }
  __ StoreP(r5, FieldMemOperand(r3, JSValue::kValueOffset), r0);

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r9);
    __ Ret(1);
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r3.
  Label no_arguments;
  {
    __ mr(r5, r3);  // Store argc in r5.
    __ cmpi(r3, Operand::Zero());
    __ beq(&no_arguments);
    __ subi(r3, r3, Operand(1));
    __ ShiftLeftImm(r3, r3, Operand(kPointerSizeLog2));
    __ LoadPX(r3, MemOperand(sp, r3));
  }

  // 2a. At least one argument, return r3 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(r3, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(r3, r6, r6, FIRST_NONSTRING_TYPE);
    __ bgt(&to_string);
    __ beq(&symbol_descriptive_string);
    __ b(&drop_frame_and_ret);
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(r3, Heap::kempty_stringRootIndex);
    __ Ret(1);
  }

  // 3a. Convert r3 to a string.
  __ bind(&to_string);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r5);
    __ EnterBuiltinFrame(cp, r4, r5);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, r4, r5);
    __ SmiUntag(r5);
  }
  __ b(&drop_frame_and_ret);

  // 3b. Convert symbol in r3 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Drop(r5);
    __ Drop(1);
    __ Push(r3);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r5);
    __ Ret(1);
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : constructor function
  //  -- r6                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // 2. Load the first argument into r5.
  {
    Label no_arguments, done;
    __ mr(r9, r3);  // Store argc in r9.
    __ cmpi(r3, Operand::Zero());
    __ beq(&no_arguments);
    __ subi(r3, r3, Operand(1));
    __ ShiftLeftImm(r5, r3, Operand(kPointerSizeLog2));
    __ LoadPX(r5, MemOperand(sp, r5));
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadRoot(r5, Heap::kempty_stringRootIndex);
    __ bind(&done);
  }

  // 3. Make sure r5 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(r5, &convert);
    __ CompareObjectType(r5, r7, r7, FIRST_NONSTRING_TYPE);
    __ blt(&done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r9);
      __ EnterBuiltinFrame(cp, r4, r9);
      __ Push(r6);
      __ mr(r3, r5);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ mr(r5, r3);
      __ Pop(r6);
      __ LeaveBuiltinFrame(cp, r4, r9);
      __ SmiUntag(r9);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ cmp(r4, r6);
  __ bne(&new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(r3, r4, r5, r7, r8, &new_object);
  __ b(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r9);
    __ EnterBuiltinFrame(cp, r4, r9);
    __ Push(r5);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(r5);
    __ LeaveBuiltinFrame(cp, r4, r9);
    __ SmiUntag(r9);
  }
  __ StoreP(r5, FieldMemOperand(r3, JSValue::kValueOffset), r0);

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r9);
    __ Ret(1);
  }
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r4 : target function (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(r3);
    __ Push(r3, r4, r6, r4);

    __ CallRuntime(function_id, 1);
    __ mr(r5, r3);

    // Restore target function and new target.
    __ Pop(r3, r4, r6);
    __ SmiUntag(r3);
  }
  __ addi(ip, r5, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmpl(sp, ip);
  __ bge(&ok);

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSConstructStubHelper(MacroAssembler* masm, bool is_api_function,
                                    bool create_implicit_receiver,
                                    bool check_derived_construct) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- r4     : constructor function
  //  -- r6     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.

    if (!create_implicit_receiver) {
      __ SmiTag(r7, r3, SetRC);
      __ Push(cp, r7);
      __ PushRoot(Heap::kTheHoleValueRootIndex);
    } else {
      __ SmiTag(r3);
      __ Push(cp, r3);

      // Allocate the new receiver object.
      __ Push(r4, r6);
      __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
              RelocInfo::CODE_TARGET);
      __ mr(r7, r3);
      __ Pop(r4, r6);

      // ----------- S t a t e -------------
      //  -- r4: constructor function
      //  -- r6: new target
      //  -- r7: newly allocated object
      // -----------------------------------

      // Retrieve smi-tagged arguments count from the stack.
      __ LoadP(r3, MemOperand(sp));
      __ SmiUntag(r3, SetRC);

      // Push the allocated receiver to the stack. We need two copies
      // because we may have to return the original one and the calling
      // conventions dictate that the called function pops the receiver.
      __ Push(r7, r7);
    }

    // Set up pointer to last argument.
    __ addi(r5, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r3: number of arguments
    // r4: constructor function
    // r5: address of last argument (caller sp)
    // r6: new target
    // cr0: condition indicating whether r3 is zero
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: number of arguments (smi-tagged)
    Label loop, no_args;
    __ beq(&no_args, cr0);
    __ ShiftLeftImm(ip, r3, Operand(kPointerSizeLog2));
    __ sub(sp, sp, ip);
    __ mtctr(r3);
    __ bind(&loop);
    __ subi(ip, ip, Operand(kPointerSize));
    __ LoadPX(r0, MemOperand(r5, ip));
    __ StorePX(r0, MemOperand(sp, ip));
    __ bdnz(&loop);
    __ bind(&no_args);

    // Call the function.
    // r3: number of arguments
    // r4: constructor function
    // r6: new target

    ParameterCount actual(r3);
    __ InvokeFunction(r4, r6, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Store offset of return address for deoptimizer.
    if (create_implicit_receiver && !is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    // r3: result
    // sp[0]: receiver
    // sp[1]: number of arguments (smi-tagged)
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    if (create_implicit_receiver) {
      // If the result is an object (in the ECMA sense), we should get rid
      // of the receiver and use the result; see ECMA-262 section 13.2.2-7
      // on page 74.
      Label use_receiver, exit;

      // If the result is a smi, it is *not* an object in the ECMA sense.
      // r3: result
      // sp[0]: receiver
      // sp[1]: number of arguments (smi-tagged)
      __ JumpIfSmi(r3, &use_receiver);

      // If the type of the result (stored in its map) is less than
      // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
      __ CompareObjectType(r3, r4, r6, FIRST_JS_RECEIVER_TYPE);
      __ bge(&exit);

      // Throw away the result of the constructor invocation and use the
      // on-stack receiver as the result.
      __ bind(&use_receiver);
      __ LoadP(r3, MemOperand(sp));

      // Remove receiver from the stack, remove caller arguments, and
      // return.
      __ bind(&exit);
      // r3: result
      // sp[0]: receiver (newly allocated object)
      // sp[1]: number of arguments (smi-tagged)
      __ LoadP(r4, MemOperand(sp, 1 * kPointerSize));
    } else {
      __ LoadP(r4, MemOperand(sp));
    }

    // Leave construct frame.
  }

  // ES6 9.2.2. Step 13+
  // Check that the result is not a Smi, indicating that the constructor result
  // from a derived class is neither undefined nor an Object.
  if (check_derived_construct) {
    Label dont_throw;
    __ JumpIfNotSmi(r3, &dont_throw);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowDerivedConstructorReturnedNonObject);
    }
    __ bind(&dont_throw);
  }

  __ SmiToPtrArrayOffset(r4, r4);
  __ add(sp, sp, r4);
  __ addi(sp, sp, Operand(kPointerSize));
  if (create_implicit_receiver) {
    __ IncrementCounter(isolate->counters()->constructed_objects(), 1, r4, r5);
  }
  __ blr();
}

}  // namespace

void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, true, false);
}

void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false, false);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, false);
}

void Builtins::Generate_JSBuiltinsConstructStubForDerived(
    MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, true);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the value to pass to the generator
  //  -- r4 : the JSGeneratorObject to resume
  //  -- r5 : the resume mode (tagged)
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(r4);

  // Store input value into generator object.
  __ StoreP(r3, FieldMemOperand(r4, JSGeneratorObject::kInputOrDebugPosOffset),
            r0);
  __ RecordWriteField(r4, JSGeneratorObject::kInputOrDebugPosOffset, r3, r6,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Store resume mode into generator object.
  __ StoreP(r5, FieldMemOperand(r4, JSGeneratorObject::kResumeModeOffset), r0);

  // Load suspended function and context.
  __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  __ LoadP(cp, FieldMemOperand(r7, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ mov(ip, Operand(debug_hook));
  __ LoadByte(ip, MemOperand(ip), r0);
  __ extsb(ip, ip);
  __ CmpSmiLiteral(ip, Smi::kZero, r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ mov(ip, Operand(debug_suspended_generator));
  __ LoadP(ip, MemOperand(ip));
  __ cmp(ip, r4);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Push receiver.
  __ LoadP(ip, FieldMemOperand(r4, JSGeneratorObject::kReceiverOffset));
  __ Push(ip);

  // ----------- S t a t e -------------
  //  -- r4    : the JSGeneratorObject to resume
  //  -- r5    : the resume mode (tagged)
  //  -- r7    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ LoadP(r6, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
  __ LoadWordArith(
      r3, FieldMemOperand(r6, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label loop, done_loop;
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
#if V8_TARGET_ARCH_PPC64
    __ cmpi(r3, Operand::Zero());
    __ beq(&done_loop);
#else
    __ SmiUntag(r3, SetRC);
    __ beq(&done_loop, cr0);
#endif
    __ mtctr(r3);
    __ bind(&loop);
    __ push(ip);
    __ bdnz(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(r6, r6, r6, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ mr(r6, r4);
    __ mr(r4, r7);
    __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
    __ JumpToJSEntry(ip);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r5, r7);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r4, r5);
    __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r5);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r4, r5);
    __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  __ push(r4);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };

// Clobbers r5; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
  // Make r5 the space we have left. The stack might already be overflowed
  // here which will cause r5 to become negative.
  __ sub(r5, sp, r5);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ SmiToPtrArrayOffset(r0, argc);
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ ShiftLeftImm(r0, argc, Operand(kPointerSizeLog2));
  }
  __ cmp(r5, r0);
  __ bgt(&okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r3: new.target
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  // r0,r8-r9, cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ LoadP(cp, MemOperand(cp));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(r4, r5);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r5.
    Generate_CheckStackOverflow(masm, r6, kArgcIsUntaggedInt);

    // Copy arguments to the stack in a loop.
    // r4: function
    // r6: argc
    // r7: argv, i.e. points to first arg
    Label loop, entry;
    __ ShiftLeftImm(r0, r6, Operand(kPointerSizeLog2));
    __ add(r5, r7, r0);
    // r5 points past last arg.
    __ b(&entry);
    __ bind(&loop);
    __ LoadP(r8, MemOperand(r7));  // read next parameter
    __ addi(r7, r7, Operand(kPointerSize));
    __ LoadP(r0, MemOperand(r8));  // dereference handle
    __ push(r0);                   // push parameter
    __ bind(&entry);
    __ cmp(r7, r5);
    __ bne(&loop);

    // Setup new.target and argc.
    __ mr(r7, r3);
    __ mr(r3, r6);
    __ mr(r6, r7);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);
    __ mr(r14, r7);
    __ mr(r15, r7);
    __ mr(r16, r7);
    __ mr(r17, r7);

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
  }
  __ blr();

  // r3: result
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ LoadP(args_count,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lwz(args_count,
         FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  __ add(sp, sp, args_count);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r4: the JS function object being called.
//   o r6: the new target
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(r4);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ LoadP(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  Label array_done;
  Register debug_info = r5;
  DCHECK(!debug_info.is(r3));
  __ LoadP(debug_info,
           FieldMemOperand(r3, SharedFunctionInfo::kDebugInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r3, SharedFunctionInfo::kFunctionDataOffset));
  __ CmpSmiLiteral(debug_info, DebugInfo::uninitialized(), r0);
  __ beq(&array_done);
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ bind(&array_done);

  // Check whether we should continue to use the interpreter.
  Label switch_to_different_code_kind;
  __ LoadP(r3, FieldMemOperand(r3, SharedFunctionInfo::kCodeOffset));
  __ mov(ip, Operand(masm->CodeObject()));  // Self-reference to this code.
  __ cmp(r3, ip);
  __ bne(&switch_to_different_code_kind);

  // Increment invocation count for the function.
  __ LoadP(r7, FieldMemOperand(r4, JSFunction::kLiteralsOffset));
  __ LoadP(r7, FieldMemOperand(r7, LiteralsArray::kFeedbackVectorOffset));
  __ LoadP(r8, FieldMemOperand(r7, TypeFeedbackVector::kInvocationCountIndex *
                                           kPointerSize +
                                       TypeFeedbackVector::kHeaderSize));
  __ AddSmiLiteral(r8, r8, Smi::FromInt(1), r0);
  __ StoreP(r8, FieldMemOperand(r7, TypeFeedbackVector::kInvocationCountIndex *
                                            kPointerSize +
                                        TypeFeedbackVector::kHeaderSize),
            r0);

  // Check function data field is actually a BytecodeArray object.

  if (FLAG_debug_code) {
    __ TestIfSmi(kInterpreterBytecodeArrayRegister, r0);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, cr0);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r3, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov(r8, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ StoreByte(r8, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                   BytecodeArray::kBytecodeAgeOffset),
               r0);

  // Load initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push new.target, bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r3, kInterpreterBytecodeOffsetRegister);
  __ Push(r6, kInterpreterBytecodeArrayRegister, r3);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ lwz(r5, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r6, sp, r5);
    __ LoadRoot(r0, Heap::kRealStackLimitRootIndex);
    __ cmpl(r6, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    __ ShiftRightImm(r5, r5, Operand(kPointerSizeLog2), SetRC);
    __ beq(&no_args, cr0);
    __ mtctr(r5);
    __ bind(&loop);
    __ push(r6);
    __ bdnz(&loop);
    __ bind(&no_args);
  }

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(ip, r4, Operand(kPointerSizeLog2));
  __ LoadPX(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Call(ip);

  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in r3.
  LeaveInterpreterFrame(masm, r5);
  __ blr();

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kCodeOffset));
  __ addi(r7, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(r7, FieldMemOperand(r4, JSFunction::kCodeEntryOffset), r0);
  __ RecordWriteCodeEntryField(r4, r7, r8);
  __ JumpToJSEntry(r7);
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftImm(r0, num_args, Operand(kPointerSizeLog2));
  __ cmp(scratch, r0);
  __ ble(stack_overflow);  // Signed comparison.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register count, Register scratch,
                                         Label* stack_overflow) {
  // A stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, num_args, scratch, stack_overflow);

  Label loop;
  __ addi(index, index, Operand(kPointerSize));  // Bias up for LoadPU
  __ mtctr(count);
  __ bind(&loop);
  __ LoadPU(scratch, MemOperand(index, -kPointerSize));
  __ push(scratch);
  __ bdnz(&loop);
}

// static
void Builtins::Generate_InterpreterPushArgsAndCallImpl(
    MacroAssembler* masm, TailCallMode tail_call_mode,
    CallableType function_type) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r5 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Calculate number of arguments (add one for receiver).
  __ addi(r6, r3, Operand(1));

  // Push the arguments. r5, r6, r7 will be modified.
  Generate_InterpreterPushArgs(masm, r6, r5, r6, r7, &stack_overflow);

  // Call the target.
  if (function_type == CallableType::kJSFunction) {
    __ Jump(masm->isolate()->builtins()->CallFunction(ConvertReceiverMode::kAny,
                                                      tail_call_mode),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(function_type, CallableType::kAny);
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                              tail_call_mode),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable Code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsAndConstructImpl(
    MacroAssembler* masm, CallableType construct_type) {
  // ----------- S t a t e -------------
  // -- r3 : argument count (not including receiver)
  // -- r6 : new target
  // -- r4 : constructor to call
  // -- r5 : allocation site feedback if available, undefined otherwise.
  // -- r7 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ li(r0, Operand::Zero());
  __ push(r0);

  // Push the arguments (skip if none).
  Label skip;
  __ cmpi(r3, Operand::Zero());
  __ beq(&skip);
  // Push the arguments. r8, r7, r9 will be modified.
  Generate_InterpreterPushArgs(masm, r3, r7, r3, r8, &stack_overflow);
  __ bind(&skip);

  __ AssertUndefinedOrAllocationSite(r5, r8);
  if (construct_type == CallableType::kJSFunction) {
    __ AssertFunction(r4);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kConstructStubOffset));
    // Jump to the construct function.
    __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(ip);

  } else {
    DCHECK_EQ(construct_type, CallableType::kAny);
    // Call the constructor with r3, r4, and r6 unmodified.
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable Code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsAndConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- r3 : argument count (not including receiver)
  // -- r4 : target to call verified to be Array function
  // -- r5 : allocation site feedback if available, undefined otherwise.
  // -- r6 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  __ addi(r7, r3, Operand(1));  // Add one for receiver.

  // Push the arguments. r6, r8, r3 will be modified.
  Generate_InterpreterPushArgs(masm, r7, r6, r7, r8, &stack_overflow);

  // Array constructor expects constructor in r6. It is same as r4 here.
  __ mr(r6, r4);

  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ Move(r5, masm->isolate()->builtins()->InterpreterEntryTrampoline());
  __ addi(r0, r5, Operand(interpreter_entry_return_pc_offset->value() +
                          Code::kHeaderSize - kHeapObjectTag));
  __ mtlr(r0);

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ TestIfSmi(kInterpreterBytecodeArrayRegister, r0);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, cr0);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r4, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(ip, r4, Operand(kPointerSizeLog2));
  __ LoadPX(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Jump(ip);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ LoadP(r4, MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadP(r5,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister, r4, r5);
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ Move(r5, r3);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ StoreP(r5,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r6 : new target (preserved for callee)
  //  -- r4 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;
  Label try_shared;
  Label loop_top, loop_bottom;

  Register closure = r4;
  Register map = r9;
  Register index = r5;
  __ LoadP(map,
           FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(map,
           FieldMemOperand(map, SharedFunctionInfo::kOptimizedCodeMapOffset));
  __ LoadP(index, FieldMemOperand(map, FixedArray::kLengthOffset));
  __ CmpSmiLiteral(index, Smi::FromInt(2), r0);
  __ blt(&gotta_call_runtime);

  // Find literals.
  // r10 : native context
  // r5  : length / index
  // r9  : optimized code map
  // r6  : new target
  // r4  : closure
  Register native_context = r10;
  __ LoadP(native_context, NativeContextMemOperand());

  __ bind(&loop_top);
  Register temp = r11;
  Register array_pointer = r8;

  // Does the native context match?
  __ SmiToPtrArrayOffset(array_pointer, index);
  __ add(array_pointer, map, array_pointer);
  __ LoadP(temp, FieldMemOperand(array_pointer,
                                 SharedFunctionInfo::kOffsetToPreviousContext));
  __ LoadP(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ cmp(temp, native_context);
  __ bne(&loop_bottom);
  // Literals available?
  __ LoadP(temp,
           FieldMemOperand(array_pointer,
                           SharedFunctionInfo::kOffsetToPreviousLiterals));
  __ LoadP(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ JumpIfSmi(temp, &gotta_call_runtime);

  // Save the literals in the closure.
  __ StoreP(temp, FieldMemOperand(closure, JSFunction::kLiteralsOffset), r0);
  __ RecordWriteField(closure, JSFunction::kLiteralsOffset, temp, r7,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Code available?
  Register entry = r7;
  __ LoadP(entry,
           FieldMemOperand(array_pointer,
                           SharedFunctionInfo::kOffsetToPreviousCachedCode));
  __ LoadP(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found literals and code. Get them into the closure and return.
  // Store code entry in the closure.
  __ addi(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset), r0);
  __ RecordWriteCodeEntryField(closure, entry, r8);

  // Link the closure into the optimized function list.
  // r7 : code entry
  // r10: native context
  // r4 : closure
  __ LoadP(
      r8, ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ StoreP(r8, FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset),
            r0);
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, r8, temp,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ StoreP(
      closure,
      ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST), r0);
  // Save closure before the write barrier.
  __ mr(r8, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, r8, temp,
                            kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ JumpToJSEntry(entry);

  __ bind(&loop_bottom);
  __ SubSmiLiteral(index, index, Smi::FromInt(SharedFunctionInfo::kEntryLength),
                   r0);
  __ CmpSmiLiteral(index, Smi::FromInt(1), r0);
  __ bgt(&loop_top);

  // We found neither literals nor code.
  __ b(&gotta_call_runtime);

  __ bind(&try_shared);
  __ LoadP(entry,
           FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ lbz(r8, FieldMemOperand(entry,
                             SharedFunctionInfo::kMarkedForTierUpByteOffset));
  __ TestBit(r8, SharedFunctionInfo::kMarkedForTierUpBitWithinByte, r0);
  __ bne(&gotta_call_runtime, cr0);

  // If SFI points to anything other than CompileLazy, install that.
  __ LoadP(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ mov(r8, Operand(masm->CodeObject()));
  __ cmp(entry, r8);
  __ beq(&gotta_call_runtime);

  // Install the SFI's code entry.
  __ addi(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset), r0);
  __ RecordWriteCodeEntryField(closure, entry, r8);
  __ JumpToJSEntry(entry);

  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

void Builtins::Generate_CompileBaseline(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileBaseline);
}

void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm,
                                 Runtime::kCompileOptimized_NotConcurrent);
}

void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileOptimized_Concurrent);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argument count (preserved for callee)
  //  -- r4 : new target (preserved for callee)
  //  -- r6 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(r7, r3);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(r3);
    __ Push(r3, r4, r6, r4);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmpi(r7, Operand(j));
        __ bne(&over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ LoadP(r7, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                        i * kPointerSize));
        __ push(r7);
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(Heap::kUndefinedValueRootIndex);
      }
      if (j < 3) {
        __ jmp(&args_done);
        __ bind(&over);
      }
    }
    __ bind(&args_done);

    // Call runtime, on success unwind frame, and parent frame.
    __ CallRuntime(Runtime::kInstantiateAsmJs, 4);
    // A smi 0 is returned on failure, an object on success.
    __ JumpIfSmi(r3, &failed);

    __ Drop(2);
    __ pop(r7);
    __ SmiUntag(r7);
    scope.GenerateLeaveFrame();

    __ addi(r7, r7, Operand(1));
    __ Drop(r7);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(r3, r4, r6);
    __ SmiUntag(r3);
  }
  // On failure, tail call back to regular js.
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Point r3 at the start of the PlatformCodeAge sequence.
  __ mr(r3, ip);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r3 - contains return address (beginning of patch sequence)
  //   r4 - isolate
  //   r6 - new target
  //   lr - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ mflr(r0);
  __ MultiPush(r0.bit() | r3.bit() | r4.bit() | r6.bit() | fp.bit());
  __ PrepareCallCFunction(2, 0, r5);
  __ mov(r4, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ MultiPop(r0.bit() | r3.bit() | r4.bit() | r6.bit() | fp.bit());
  __ mtlr(r0);
  __ mr(ip, r3);
  __ Jump(ip);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                              \
  void Builtins::Generate_Make##C##CodeYoungAgain(MacroAssembler* masm) { \
    GenerateMakeCodeYoungAgainCommon(masm);                               \
  }
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR

void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Point r3 at the start of the PlatformCodeAge sequence.
  __ mr(r3, ip);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r3 - contains return address (beginning of patch sequence)
  //   r4 - isolate
  //   r6 - new target
  //   lr - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ mflr(r0);
  __ MultiPush(r0.bit() | r3.bit() | r4.bit() | r6.bit() | fp.bit());
  __ PrepareCallCFunction(2, 0, r5);
  __ mov(r4, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ MultiPop(r0.bit() | r3.bit() | r4.bit() | r6.bit() | fp.bit());
  __ mtlr(r0);
  __ mr(ip, r3);

  // Perform prologue operations usually performed by the young code stub.
  __ PushStandardFrame(r4);

  // Jump to point after the code-age stub.
  __ addi(r3, ip, Operand(kNoCodeAgeSequenceLength));
  __ Jump(r3);
}

void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}

void Builtins::Generate_MarkCodeAsToBeExecutedOnce(MacroAssembler* masm) {
  Generate_MarkCodeAsExecutedOnce(masm);
}

static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ MultiPush(kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ addi(sp, sp, Operand(kPointerSize));  // Ignore state
  __ blr();                                // Jump to miss handler
}

void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kDontSaveFPRegs);
}

void Builtins::Generate_NotifyStubFailureSaveDoubles(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kSaveFPRegs);
}

static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass the function and deoptimization type to the runtime system.
    __ LoadSmiLiteral(r3, Smi::FromInt(static_cast<int>(type)));
    __ push(r3);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Get the full codegen state from the stack and untag it -> r9.
  __ LoadP(r9, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(r9);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ cmpi(
      r9,
      Operand(static_cast<intptr_t>(Deoptimizer::BailoutState::NO_REGISTERS)));
  __ bne(&with_tos_register);
  __ addi(sp, sp, Operand(1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r3.code());
  __ LoadP(r3, MemOperand(sp, 1 * kPointerSize));
  __ cmpi(
      r9,
      Operand(static_cast<intptr_t>(Deoptimizer::BailoutState::TOS_REGISTER)));
  __ bne(&unknown_state);
  __ addi(sp, sp, Operand(2 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&unknown_state);
  __ stop("no cases left");
}

void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}

void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}

void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}

// Clobbers registers {r7, r8, r9, r10}.
void CompatibleReceiverCheck(MacroAssembler* masm, Register receiver,
                             Register function_template_info,
                             Label* receiver_check_failed) {
  Register signature = r7;
  Register map = r8;
  Register constructor = r9;
  Register scratch = r10;

  // If there is no signature, return the holder.
  __ LoadP(signature, FieldMemOperand(function_template_info,
                                      FunctionTemplateInfo::kSignatureOffset));
  Label receiver_check_passed;
  __ JumpIfRoot(signature, Heap::kUndefinedValueRootIndex,
                &receiver_check_passed);

  // Walk the prototype chain.
  __ LoadP(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  Label prototype_loop_start;
  __ bind(&prototype_loop_start);

  // Get the constructor, if any.
  __ GetMapConstructor(constructor, map, scratch, scratch);
  __ cmpi(scratch, Operand(JS_FUNCTION_TYPE));
  Label next_prototype;
  __ bne(&next_prototype);
  Register type = constructor;
  __ LoadP(type,
           FieldMemOperand(constructor, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(type,
           FieldMemOperand(type, SharedFunctionInfo::kFunctionDataOffset));

  // Loop through the chain of inheriting function templates.
  Label function_template_loop;
  __ bind(&function_template_loop);

  // If the signatures match, we have a compatible receiver.
  __ cmp(signature, type);
  __ beq(&receiver_check_passed);

  // If the current type is not a FunctionTemplateInfo, load the next prototype
  // in the chain.
  __ JumpIfSmi(type, &next_prototype);
  __ CompareObjectType(type, scratch, scratch, FUNCTION_TEMPLATE_INFO_TYPE);
  __ bne(&next_prototype);

  // Otherwise load the parent function template and iterate.
  __ LoadP(type,
           FieldMemOperand(type, FunctionTemplateInfo::kParentTemplateOffset));
  __ b(&function_template_loop);

  // Load the next prototype.
  __ bind(&next_prototype);
  __ lwz(scratch, FieldMemOperand(map, Map::kBitField3Offset));
  __ DecodeField<Map::HasHiddenPrototype>(scratch, SetRC);
  __ beq(receiver_check_failed, cr0);

  __ LoadP(receiver, FieldMemOperand(map, Map::kPrototypeOffset));
  __ LoadP(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Iterate.
  __ b(&prototype_loop_start);

  __ bind(&receiver_check_passed);
}

void Builtins::Generate_HandleFastApiCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                 : number of arguments excluding receiver
  //  -- r4                 : callee
  //  -- lr                 : return address
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------

  // Load the FunctionTemplateInfo.
  __ LoadP(r6, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kFunctionDataOffset));

  // Do the compatible receiver check.
  Label receiver_check_failed;
  __ ShiftLeftImm(r11, r3, Operand(kPointerSizeLog2));
  __ LoadPX(r5, MemOperand(sp, r11));
  CompatibleReceiverCheck(masm, r5, r6, &receiver_check_failed);

  // Get the callback offset from the FunctionTemplateInfo, and jump to the
  // beginning of the code.
  __ LoadP(r7, FieldMemOperand(r6, FunctionTemplateInfo::kCallCodeOffset));
  __ LoadP(r7, FieldMemOperand(r7, CallHandlerInfo::kFastHandlerOffset));
  __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);

  // Compatible receiver check failed: throw an Illegal Invocation exception.
  __ bind(&receiver_check_failed);
  // Drop the arguments (including the receiver);
  __ addi(r11, r11, Operand(kPointerSize));
  __ add(sp, sp, r11);
  __ TailCallRuntime(Runtime::kThrowIllegalInvocation);
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ LoadP(r3, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ LoadP(r3, MemOperand(r3, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r3);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CmpSmiLiteral(r3, Smi::kZero, r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadP(r4, FieldMemOperand(r3, Code::kDeoptimizationDataOffset));

  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ addi(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    if (FLAG_enable_embedded_constant_pool) {
      __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r3);
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ LoadP(r4, FieldMemOperand(
                     r4, FixedArray::OffsetOfElementAt(
                             DeoptimizationInputData::kOsrPcOffsetIndex)));
    __ SmiUntag(r4);

    // Compute the target address = code start + osr_offset
    __ add(r0, r3, r4);

    // And "return" to the OSR entry point of the function.
    __ mtlr(r0);
    __ blr();
  }
}

void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, false);
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, true);
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into r4, argArray into r3 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label skip;
    Register arg_size = r5;
    Register new_sp = r6;
    Register scratch = r7;
    __ ShiftLeftImm(arg_size, r3, Operand(kPointerSizeLog2));
    __ add(new_sp, sp, arg_size);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ mr(scratch, r3);
    __ LoadP(r4, MemOperand(new_sp, 0));  // receiver
    __ cmpi(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 1 * -kPointerSize));  // thisArg
    __ beq(&skip);
    __ LoadP(r3, MemOperand(new_sp, 2 * -kPointerSize));  // argArray
    __ bind(&skip);
    __ mr(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r3    : argArray
  //  -- r4    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(r4, &receiver_not_callable);
  __ LoadP(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::kIsCallable, r0);
  __ beq(&receiver_not_callable, cr0);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r3, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(r3, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ li(r3, Operand::Zero());
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ StoreP(r4, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r3: actual number of arguments
  {
    Label done;
    __ cmpi(r3, Operand::Zero());
    __ bne(&done);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ addi(r3, r3, Operand(1));
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  // r3: actual number of arguments
  __ ShiftLeftImm(r5, r3, Operand(kPointerSizeLog2));
  __ LoadPX(r4, MemOperand(sp, r5));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r3: actual number of arguments
  // r4: callable
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r5, sp, r5);

    __ mtctr(r3);
    __ bind(&loop);
    __ LoadP(ip, MemOperand(r5, -kPointerSize));
    __ StoreP(ip, MemOperand(r5));
    __ subi(r5, r5, Operand(kPointerSize));
    __ bdnz(&loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ subi(r3, r3, Operand(1));
    __ pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r4 (if present), argumentsList into r3 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label skip;
    Register arg_size = r5;
    Register new_sp = r6;
    Register scratch = r7;
    __ ShiftLeftImm(arg_size, r3, Operand(kPointerSizeLog2));
    __ add(new_sp, sp, arg_size);
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mr(scratch, r4);
    __ mr(r3, r4);
    __ cmpi(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r4, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ beq(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 2 * -kPointerSize));  // thisArgument
    __ cmpi(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r3, MemOperand(new_sp, 3 * -kPointerSize));  // argumentsList
    __ bind(&skip);
    __ mr(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r3    : argumentsList
  //  -- r4    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(r4, &target_not_callable);
  __ LoadP(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::kIsCallable, r0);
  __ beq(&target_not_callable, cr0);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ StoreP(r4, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r4 (if present), argumentsList into r3 (if present),
  // new.target into r6 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label skip;
    Register arg_size = r5;
    Register new_sp = r7;
    __ ShiftLeftImm(arg_size, r3, Operand(kPointerSizeLog2));
    __ add(new_sp, sp, arg_size);
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mr(r3, r4);
    __ mr(r6, r4);
    __ StoreP(r4, MemOperand(new_sp, 0));  // receiver (undefined)
    __ cmpi(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r4, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ mr(r6, r4);  // new.target defaults to target
    __ beq(&skip);
    __ LoadP(r3, MemOperand(new_sp, 2 * -kPointerSize));  // argumentsList
    __ cmpi(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r6, MemOperand(new_sp, 3 * -kPointerSize));  // new.target
    __ bind(&skip);
    __ mr(sp, new_sp);
  }

  // ----------- S t a t e -------------
  //  -- r3    : argumentsList
  //  -- r6    : new.target
  //  -- r4    : target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(r4, &target_not_constructor);
  __ LoadP(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::kIsConstructor, r0);
  __ beq(&target_not_constructor, cr0);

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(r6, &new_target_not_constructor);
  __ LoadP(r7, FieldMemOperand(r6, HeapObject::kMapOffset));
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::kIsConstructor, r0);
  __ beq(&new_target_not_constructor, cr0);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ StoreP(r4, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ StoreP(r6, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r3);
  __ LoadSmiLiteral(r7, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ mflr(r0);
  __ push(r0);
  if (FLAG_enable_embedded_constant_pool) {
    __ Push(fp, kConstantPoolRegister, r7, r4, r3);
  } else {
    __ Push(fp, r7, r4, r3);
  }
  __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                          kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ LoadP(r4, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                                kPointerSize)));
  int stack_adjustment = kPointerSize;  // adjust for receiver
  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR, stack_adjustment);
  __ SmiToPtrArrayOffset(r0, r4);
  __ add(sp, sp, r0);
}

// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3    : argumentsList
  //  -- r4    : target
  //  -- r6    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(r3, &create_runtime);

    // Load the map of argumentsList into r5.
    __ LoadP(r5, FieldMemOperand(r3, HeapObject::kMapOffset));

    // Load native context into r7.
    __ LoadP(r7, NativeContextMemOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ LoadP(ip, ContextMemOperand(r7, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ cmp(ip, r5);
    __ beq(&create_arguments);
    __ LoadP(ip, ContextMemOperand(r7, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ cmp(ip, r5);
    __ beq(&create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CompareInstanceType(r5, ip, JS_ARRAY_TYPE);
    __ beq(&create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r4, r6, r3);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(r4, r6);
      __ LoadP(r5, FieldMemOperand(r3, FixedArray::kLengthOffset));
      __ SmiUntag(r5);
    }
    __ b(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ LoadP(r5, FieldMemOperand(r3, JSArgumentsObject::kLengthOffset));
    __ LoadP(r7, FieldMemOperand(r3, JSObject::kElementsOffset));
    __ LoadP(ip, FieldMemOperand(r7, FixedArray::kLengthOffset));
    __ cmp(r5, ip);
    __ bne(&create_runtime);
    __ SmiUntag(r5);
    __ mr(r3, r7);
    __ b(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ bind(&create_holey_array);
    __ LoadP(r5, FieldMemOperand(r5, Map::kPrototypeOffset));
    __ LoadP(r7, ContextMemOperand(r7, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ cmp(r5, r7);
    __ bne(&create_runtime);
    __ LoadRoot(r7, Heap::kArrayProtectorRootIndex);
    __ LoadP(r5, FieldMemOperand(r7, PropertyCell::kValueOffset));
    __ CmpSmiLiteral(r5, Smi::FromInt(Isolate::kProtectorValid), r0);
    __ bne(&create_runtime);
    __ LoadP(r5, FieldMemOperand(r3, JSArray::kLengthOffset));
    __ LoadP(r3, FieldMemOperand(r3, JSArray::kElementsOffset));
    __ SmiUntag(r5);
    __ b(&done_create);

    // Try to create the list from a JSArray object.
    // -- r5 and r7 must be preserved till bne create_holey_array.
    __ bind(&create_array);
    __ lbz(r8, FieldMemOperand(r5, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(r8);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ cmpi(r8, Operand(FAST_HOLEY_ELEMENTS));
    __ bgt(&create_runtime);
    // Only FAST_XXX after this point, FAST_HOLEY_XXX are odd values.
    __ TestBit(r8, Map::kHasNonInstancePrototype, r0);
    __ bne(&create_holey_array, cr0);
    // FAST_SMI_ELEMENTS or FAST_ELEMENTS after this point.
    __ LoadP(r5, FieldMemOperand(r3, JSArray::kLengthOffset));
    __ LoadP(r3, FieldMemOperand(r3, JSArray::kElementsOffset));
    __ SmiUntag(r5);

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(ip, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ sub(ip, sp, ip);
    // Check if the arguments will overflow the stack.
    __ ShiftLeftImm(r0, r5, Operand(kPointerSizeLog2));
    __ cmp(ip, r0);  // Signed comparison.
    __ bgt(&done);
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- r4    : target
  //  -- r3    : args (a FixedArray built from argumentsList)
  //  -- r5    : len (number of elements to push from args)
  //  -- r6    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
    Label loop, no_args, skip;
    __ cmpi(r5, Operand::Zero());
    __ beq(&no_args);
    __ addi(r3, r3,
            Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
    __ mtctr(r5);
    __ bind(&loop);
    __ LoadPU(ip, MemOperand(r3, kPointerSize));
    __ CompareRoot(ip, Heap::kTheHoleValueRootIndex);
    __ bne(&skip);
    __ mr(ip, r9);
    __ bind(&skip);
    __ push(ip);
    __ bdnz(&loop);
    __ bind(&no_args);
    __ mr(r3, r5);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(r6, Heap::kUndefinedValueRootIndex);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET, eq);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

namespace {

// Drops top JavaScript frame and an arguments adaptor frame below it (if
// present) preserving all the arguments prepared for current call.
// Does nothing if debugger is currently active.
// ES6 14.6.3. PrepareForTailCall
//
// Stack structure for the function g() tail calling f():
//
// ------- Caller frame: -------
// |  ...
// |  g()'s arg M
// |  ...
// |  g()'s arg 1
// |  g()'s receiver arg
// |  g()'s caller pc
// ------- g()'s frame: -------
// |  g()'s caller fp      <- fp
// |  g()'s context
// |  function pointer: g
// |  -------------------------
// |  ...
// |  ...
// |  f()'s arg N
// |  ...
// |  f()'s arg 1
// |  f()'s receiver arg   <- sp (f()'s caller pc is not on the stack yet!)
// ----------------------
//
void PrepareForTailCall(MacroAssembler* masm, Register args_reg,
                        Register scratch1, Register scratch2,
                        Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Comment cmnt(masm, "[ PrepareForTailCall");

  // Prepare for tail call only if ES2015 tail call elimination is enabled.
  Label done;
  ExternalReference is_tail_call_elimination_enabled =
      ExternalReference::is_tail_call_elimination_enabled_address(
          masm->isolate());
  __ mov(scratch1, Operand(is_tail_call_elimination_enabled));
  __ lbz(scratch1, MemOperand(scratch1));
  __ cmpi(scratch1, Operand::Zero());
  __ beq(&done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ LoadP(scratch3,
             MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ CmpSmiLiteral(scratch3, Smi::FromInt(StackFrame::STUB), r0);
    __ bne(&no_interpreter_frame);
    __ LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ LoadP(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(
      scratch3,
      MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpSmiLiteral(scratch3, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&no_arguments_adaptor);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mr(fp, scratch2);
  __ LoadP(caller_args_count_reg,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ b(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ LoadP(scratch1,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  __ LoadP(scratch1,
           FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ LoadWordArith(
      caller_args_count_reg,
      FieldMemOperand(scratch1,
                      SharedFunctionInfo::kFormalParameterCountOffset));
#if !V8_TARGET_ARCH_PPC64
  __ SmiUntag(caller_args_count_reg);
#endif

  __ bind(&formal_parameter_count_loaded);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}
}  // namespace

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode,
                                     TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r4);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ lwz(r6, FieldMemOperand(r5, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestBitMask(r6, FunctionKind::kClassConstructor
                         << SharedFunctionInfo::kFunctionKindShift,
                 r0);
  __ bne(&class_constructor, cr0);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ andi(r0, r6, Operand((1 << SharedFunctionInfo::kStrictModeBit) |
                          (1 << SharedFunctionInfo::kNativeBit)));
  __ bne(&done_convert, cr0);
  {
    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r4 : the function to call (checked to be a JSFunction)
    //  -- r5 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r6);
    } else {
      Label convert_to_object, convert_receiver;
      __ ShiftLeftImm(r6, r3, Operand(kPointerSizeLog2));
      __ LoadPX(r6, MemOperand(sp, r6));
      __ JumpIfSmi(r6, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r6, r7, r7, FIRST_JS_RECEIVER_TYPE);
      __ bge(&done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r6, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(r6, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(r6);
        }
        __ b(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(r3);
        __ Push(r3, r4);
        __ mr(r3, r6);
        __ Push(cp);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mr(r6, r3);
        __ Pop(r3, r4);
        __ SmiUntag(r3);
      }
      __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ ShiftLeftImm(r7, r3, Operand(kPointerSizeLog2));
    __ StorePX(r6, MemOperand(sp, r7));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  //  -- r5 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r3, r6, r7, r8);
  }

  __ LoadWordArith(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
#if !V8_TARGET_ARCH_PPC64
  __ SmiUntag(r5);
#endif
  ParameterCount actual(r3);
  ParameterCount expected(r5);
  __ InvokeFunctionCode(r4, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameAndConstantPoolScope frame(masm, StackFrame::INTERNAL);
    __ push(r4);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : target (checked to be a JSBoundFunction)
  //  -- r6 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r5 and length of that into r7.
  Label no_bound_arguments;
  __ LoadP(r5, FieldMemOperand(r4, JSBoundFunction::kBoundArgumentsOffset));
  __ LoadP(r7, FieldMemOperand(r5, FixedArray::kLengthOffset));
  __ SmiUntag(r7, SetRC);
  __ beq(&no_bound_arguments, cr0);
  {
    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r4 : target (checked to be a JSBoundFunction)
    //  -- r5 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r6 : new.target (only in case of [[Construct]])
    //  -- r7 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ mr(r9, sp);  // preserve previous stack pointer
      __ ShiftLeftImm(r10, r7, Operand(kPointerSizeLog2));
      __ sub(sp, sp, r10);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
      __ bgt(&done);  // Signed comparison.
      // Restore the stack pointer.
      __ mr(sp, r9);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Relocate arguments down the stack.
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r9 : the previous stack pointer
    //  -- r10: the size of the [[BoundArguments]]
    {
      Label skip, loop;
      __ li(r8, Operand::Zero());
      __ cmpi(r3, Operand::Zero());
      __ beq(&skip);
      __ mtctr(r3);
      __ bind(&loop);
      __ LoadPX(r0, MemOperand(r9, r8));
      __ StorePX(r0, MemOperand(sp, r8));
      __ addi(r8, r8, Operand(kPointerSize));
      __ bdnz(&loop);
      __ bind(&skip);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ addi(r5, r5, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ add(r5, r5, r10);
      __ mtctr(r7);
      __ bind(&loop);
      __ LoadPU(r0, MemOperand(r5, -kPointerSize));
      __ StorePX(r0, MemOperand(sp, r8));
      __ addi(r8, r8, Operand(kPointerSize));
      __ bdnz(&loop);
      __ add(r3, r3, r7);
    }
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r4);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r3, r6, r7, r8);
  }

  // Patch the receiver to [[BoundThis]].
  __ LoadP(ip, FieldMemOperand(r4, JSBoundFunction::kBoundThisOffset));
  __ ShiftLeftImm(r0, r3, Operand(kPointerSizeLog2));
  __ StorePX(ip, MemOperand(sp, r0));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadP(r4,
           FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ip, Operand(ExternalReference(Builtins::kCall_ReceiverIsAny,
                                       masm->isolate())));
  __ LoadP(ip, MemOperand(ip));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r4, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r4, r7, r8, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
          RelocInfo::CODE_TARGET, eq);
  __ cmpi(r8, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::kIsCallable, r0);
  __ beq(&non_callable, cr0);

  __ cmpi(r8, Operand(JS_PROXY_TYPE));
  __ bne(&non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r3, r6, r7, r8);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ Push(r4);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ addi(r3, r3, Operand(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver the (original) target.
  __ ShiftLeftImm(r8, r3, Operand(kPointerSizeLog2));
  __ StorePX(r4, MemOperand(sp, r8));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r4);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (checked to be a JSFunction)
  //  -- r6 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(r4);

  // Calling convention for function specific ConstructStubs require
  // r5 to contain either an AllocationSite or undefined.
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kConstructStubOffset));
  __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSBoundFunction)
  //  -- r6 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(r4);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  __ cmp(r4, r6);
  __ bne(&skip);
  __ LoadP(r6,
           FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadP(r4,
           FieldMemOperand(r4, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ip, Operand(ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ LoadP(ip, MemOperand(ip));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (checked to be a JSProxy)
  //  -- r6 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ Push(r4, r6);
  // Include the pushed new_target, constructor and the receiver.
  __ addi(r3, r3, Operand(3));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (can be any Object)
  //  -- r6 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(r4, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(r4, r7, r8, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ lbz(r5, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r5, Map::kIsConstructor, r0);
  __ beq(&non_constructor, cr0);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ cmpi(r8, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructBoundFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ cmpi(r8, Operand(JS_PROXY_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ ShiftLeftImm(r8, r3, Operand(kPointerSizeLog2));
    __ StorePX(r4, MemOperand(sp, r8));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r4);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(masm->isolate()->builtins()->ConstructedNonConstructable(),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r4);
  __ Push(r4);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r4);
  __ LoadSmiLiteral(r5, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(r4, r5);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  __ push(r4);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : actual number of arguments
  //  -- r4 : function (passed through to callee)
  //  -- r5 : expected number of arguments
  //  -- r6 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
  __ cmp(r3, r5);
  __ blt(&too_few);
  __ cmpi(r5, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ beq(&dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r5, r8, &stack_overflow);

    // Calculate copy start address into r3 and copy end address into r7.
    // r3: actual number of arguments as a smi
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r3, r3);
    __ add(r3, r3, fp);
    // adjust for return address and receiver
    __ addi(r3, r3, Operand(2 * kPointerSize));
    __ ShiftLeftImm(r7, r5, Operand(kPointerSizeLog2));
    __ sub(r7, r3, r7);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r3: copy start address
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // r7: copy end address
    // ip: code entry to call

    Label copy;
    __ bind(&copy);
    __ LoadP(r0, MemOperand(r3, 0));
    __ push(r0);
    __ cmp(r3, r7);  // Compare before moving to next argument.
    __ subi(r3, r3, Operand(kPointerSize));
    __ bne(&copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);

    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r5, r8, &stack_overflow);

    // Calculate copy start address into r0 and copy end address is fp.
    // r3: actual number of arguments as a smi
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r3, r3);
    __ add(r3, r3, fp);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r3: copy start address
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ LoadP(r0, MemOperand(r3, 2 * kPointerSize));
    __ push(r0);
    __ cmp(r3, fp);  // Compare before moving to next argument.
    __ subi(r3, r3, Operand(kPointerSize));
    __ bne(&copy);

    // Fill the remaining expected arguments with undefined.
    // r4: function
    // r5: expected number of arguments
    // r6: new target (passed through to callee)
    // ip: code entry to call
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ ShiftLeftImm(r7, r5, Operand(kPointerSizeLog2));
    __ sub(r7, fp, r7);
    // Adjust for frame.
    __ subi(r7, r7, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(r0);
    __ cmp(sp, r7);
    __ bne(&fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mr(r3, r5);
  // r3 : expected number of arguments
  // r4 : function (passed through to callee)
  // r6 : new target (passed through to callee)
  __ CallJSEntry(ip);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ blr();

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ JumpToJSEntry(ip);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);
  }
}

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
