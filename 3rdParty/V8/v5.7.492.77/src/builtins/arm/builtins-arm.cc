// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

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
  //  -- r0                 : number of arguments excluding receiver
  //  -- r1                 : target
  //  -- r3                 : new.target
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // JumpToExternalReference expects r0 to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ add(r0, r0, Operand(num_extra_args + 1));

  // Insert extra arguments.
  __ SmiTag(r0);
  __ Push(r0, r1, r3);
  __ SmiUntag(r0);

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
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(r2);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
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
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(r2);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  __ mov(r3, r1);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

// static
void Builtins::Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  Condition const cc_done = (kind == MathMaxMinKind::kMin) ? mi : gt;
  Condition const cc_swap = (kind == MathMaxMinKind::kMin) ? gt : mi;
  Heap::RootListIndex const root_index =
      (kind == MathMaxMinKind::kMin) ? Heap::kInfinityValueRootIndex
                                     : Heap::kMinusInfinityValueRootIndex;
  DoubleRegister const reg = (kind == MathMaxMinKind::kMin) ? d2 : d1;

  // Load the accumulator with the default return value (either -Infinity or
  // +Infinity), with the tagged value in r5 and the double value in d1.
  __ LoadRoot(r5, root_index);
  __ vldr(d1, FieldMemOperand(r5, HeapNumber::kValueOffset));

  Label done_loop, loop;
  __ mov(r4, r0);
  __ bind(&loop);
  {
    // Check if all parameters done.
    __ sub(r4, r4, Operand(1), SetCC);
    __ b(lt, &done_loop);

    // Load the next parameter tagged value into r2.
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2));

    // Load the double value of the parameter into d2, maybe converting the
    // parameter to a number first using the ToNumber builtin if necessary.
    Label convert, convert_smi, convert_number, done_convert;
    __ bind(&convert);
    __ JumpIfSmi(r2, &convert_smi);
    __ ldr(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ JumpIfRoot(r3, Heap::kHeapNumberMapRootIndex, &convert_number);
    {
      // Parameter is not a Number, use the ToNumber builtin to convert it.
      DCHECK(!FLAG_enable_embedded_constant_pool);
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r0);
      __ SmiTag(r4);
      __ EnterBuiltinFrame(cp, r1, r0);
      __ Push(r4, r5);
      __ mov(r0, r2);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ mov(r2, r0);
      __ Pop(r4, r5);
      __ LeaveBuiltinFrame(cp, r1, r0);
      __ SmiUntag(r4);
      __ SmiUntag(r0);
      {
        // Restore the double accumulator value (d1).
        Label done_restore;
        __ SmiToDouble(d1, r5);
        __ JumpIfSmi(r5, &done_restore);
        __ vldr(d1, FieldMemOperand(r5, HeapNumber::kValueOffset));
        __ bind(&done_restore);
      }
    }
    __ b(&convert);
    __ bind(&convert_number);
    __ vldr(d2, FieldMemOperand(r2, HeapNumber::kValueOffset));
    __ b(&done_convert);
    __ bind(&convert_smi);
    __ SmiToDouble(d2, r2);
    __ bind(&done_convert);

    // Perform the actual comparison with the accumulator value on the left hand
    // side (d1) and the next parameter value on the right hand side (d2).
    Label compare_nan, compare_swap;
    __ VFPCompareAndSetFlags(d1, d2);
    __ b(cc_done, &loop);
    __ b(cc_swap, &compare_swap);
    __ b(vs, &compare_nan);

    // Left and right hand side are equal, check for -0 vs. +0.
    __ VmovHigh(ip, reg);
    __ cmp(ip, Operand(0x80000000));
    __ b(ne, &loop);

    // Result is on the right hand side.
    __ bind(&compare_swap);
    __ vmov(d1, d2);
    __ mov(r5, r2);
    __ b(&loop);

    // At least one side is NaN, which means that the result will be NaN too.
    __ bind(&compare_nan);
    __ LoadRoot(r5, Heap::kNanValueRootIndex);
    __ vldr(d1, FieldMemOperand(r5, HeapNumber::kValueOffset));
    __ b(&loop);
  }

  __ bind(&done_loop);
  // Drop all slots, including the receiver.
  __ add(r0, r0, Operand(1));
  __ Drop(r0);
  __ mov(r0, r5);
  __ Ret();
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r0.
  Label no_arguments;
  {
    __ mov(r2, r0);  // Store argc in r2.
    __ sub(r0, r0, Operand(1), SetCC);
    __ b(lo, &no_arguments);
    __ ldr(r0, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  }

  // 2a. Convert the first argument to a number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r2);
    __ EnterBuiltinFrame(cp, r1, r2);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, r1, r2);
    __ SmiUntag(r2);
  }

  {
    // Drop all arguments including the receiver.
    __ Drop(r2);
    __ Ret(1);
  }

  // 2b. No arguments, return +0.
  __ bind(&no_arguments);
  __ Move(r0, Smi::kZero);
  __ Ret(1);
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- r3                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // 2. Load the first argument into r2.
  {
    Label no_arguments, done;
    __ mov(r6, r0);  // Store argc in r6.
    __ sub(r0, r0, Operand(1), SetCC);
    __ b(lo, &no_arguments);
    __ ldr(r2, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    __ b(&done);
    __ bind(&no_arguments);
    __ Move(r2, Smi::kZero);
    __ bind(&done);
  }

  // 3. Make sure r2 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(r2, &done_convert);
    __ CompareObjectType(r2, r4, r4, HEAP_NUMBER_TYPE);
    __ b(eq, &done_convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r6);
      __ EnterBuiltinFrame(cp, r1, r6);
      __ Push(r3);
      __ Move(r0, r2);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ Move(r2, r0);
      __ Pop(r3);
      __ LeaveBuiltinFrame(cp, r1, r6);
      __ SmiUntag(r6);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ cmp(r1, r3);
  __ b(ne, &new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(r0, r1, r2, r4, r5, &new_object);
  __ b(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r6);
    __ EnterBuiltinFrame(cp, r1, r6);
    __ Push(r2);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(r2);
    __ LeaveBuiltinFrame(cp, r1, r6);
    __ SmiUntag(r6);
  }
  __ str(r2, FieldMemOperand(r0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r6);
    __ Ret(1);
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r0.
  Label no_arguments;
  {
    __ mov(r2, r0);  // Store argc in r2.
    __ sub(r0, r0, Operand(1), SetCC);
    __ b(lo, &no_arguments);
    __ ldr(r0, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  }

  // 2a. At least one argument, return r0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(r0, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(r0, r3, r3, FIRST_NONSTRING_TYPE);
    __ b(hi, &to_string);
    __ b(eq, &symbol_descriptive_string);
    __ b(&drop_frame_and_ret);
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(r0, Heap::kempty_stringRootIndex);
    __ Ret(1);
  }

  // 3a. Convert r0 to a string.
  __ bind(&to_string);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r2);
    __ EnterBuiltinFrame(cp, r1, r2);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, r1, r2);
    __ SmiUntag(r2);
  }
  __ b(&drop_frame_and_ret);

  // 3b. Convert symbol in r0 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Drop(r2);
    __ Drop(1);
    __ Push(r0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r2);
    __ Ret(1);
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- r3                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // 2. Load the first argument into r2.
  {
    Label no_arguments, done;
    __ mov(r6, r0);  // Store argc in r6.
    __ sub(r0, r0, Operand(1), SetCC);
    __ b(lo, &no_arguments);
    __ ldr(r2, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadRoot(r2, Heap::kempty_stringRootIndex);
    __ bind(&done);
  }

  // 3. Make sure r2 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(r2, &convert);
    __ CompareObjectType(r2, r4, r4, FIRST_NONSTRING_TYPE);
    __ b(lo, &done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r6);
      __ EnterBuiltinFrame(cp, r1, r6);
      __ Push(r3);
      __ Move(r0, r2);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ Move(r2, r0);
      __ Pop(r3);
      __ LeaveBuiltinFrame(cp, r1, r6);
      __ SmiUntag(r6);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ cmp(r1, r3);
  __ b(ne, &new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(r0, r1, r2, r4, r5, &new_object);
  __ b(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r6);
    __ EnterBuiltinFrame(cp, r1, r6);
    __ Push(r2);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(r2);
    __ LeaveBuiltinFrame(cp, r1, r6);
    __ SmiUntag(r6);
  }
  __ str(r2, FieldMemOperand(r0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r6);
    __ Ret(1);
  }
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCodeOffset));
  __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r2);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r1 : target function (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    __ SmiTag(r0);
    __ push(r0);
    // Push a copy of the target function and the new target.
    __ push(r1);
    __ push(r3);
    // Push function as parameter to the runtime call.
    __ Push(r1);

    __ CallRuntime(function_id, 1);
    __ mov(r2, r0);

    // Restore target function and new target.
    __ pop(r3);
    __ pop(r1);
    __ pop(r0);
    __ SmiUntag(r0, r0);
  }
  __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r2);
}

void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmp(sp, Operand(ip));
  __ b(hs, &ok);

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSConstructStubHelper(MacroAssembler* masm, bool is_api_function,
                                    bool create_implicit_receiver,
                                    bool check_derived_construct) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- r3     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r0);
    __ Push(cp, r0);

    if (create_implicit_receiver) {
      // Allocate the new receiver object.
      __ Push(r1, r3);
      __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
              RelocInfo::CODE_TARGET);
      __ mov(r4, r0);
      __ Pop(r1, r3);

      // ----------- S t a t e -------------
      //  -- r1: constructor function
      //  -- r3: new target
      //  -- r4: newly allocated object
      // -----------------------------------

      // Retrieve smi-tagged arguments count from the stack.
      __ ldr(r0, MemOperand(sp));
    }

    __ SmiUntag(r0);

    if (create_implicit_receiver) {
      // Push the allocated receiver to the stack. We need two copies
      // because we may have to return the original one and the calling
      // conventions dictate that the called function pops the receiver.
      __ push(r4);
      __ push(r4);
    } else {
      __ PushRoot(Heap::kTheHoleValueRootIndex);
    }

    // Set up pointer to last argument.
    __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r0: number of arguments
    // r1: constructor function
    // r2: address of last argument (caller sp)
    // r3: new target
    // r4: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: number of arguments (smi-tagged)
    Label loop, entry;
    __ SmiTag(r4, r0);
    __ b(&entry);
    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, r4, LSL, kPointerSizeLog2 - 1));
    __ push(ip);
    __ bind(&entry);
    __ sub(r4, r4, Operand(2), SetCC);
    __ b(ge, &loop);

    // Call the function.
    // r0: number of arguments
    // r1: constructor function
    // r3: new target
    ParameterCount actual(r0);
    __ InvokeFunction(r1, r3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Store offset of return address for deoptimizer.
    if (create_implicit_receiver && !is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    // r0: result
    // sp[0]: receiver
    // sp[1]: number of arguments (smi-tagged)
    __ ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    if (create_implicit_receiver) {
      // If the result is an object (in the ECMA sense), we should get rid
      // of the receiver and use the result; see ECMA-262 section 13.2.2-7
      // on page 74.
      Label use_receiver, exit;

      // If the result is a smi, it is *not* an object in the ECMA sense.
      // r0: result
      // sp[0]: receiver
      // sp[1]: number of arguments (smi-tagged)
      __ JumpIfSmi(r0, &use_receiver);

      // If the type of the result (stored in its map) is less than
      // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
      __ CompareObjectType(r0, r1, r3, FIRST_JS_RECEIVER_TYPE);
      __ b(ge, &exit);

      // Throw away the result of the constructor invocation and use the
      // on-stack receiver as the result.
      __ bind(&use_receiver);
      __ ldr(r0, MemOperand(sp));

      // Remove receiver from the stack, remove caller arguments, and
      // return.
      __ bind(&exit);
      // r0: result
      // sp[0]: receiver (newly allocated object)
      // sp[1]: number of arguments (smi-tagged)
      __ ldr(r1, MemOperand(sp, 1 * kPointerSize));
    } else {
      __ ldr(r1, MemOperand(sp));
    }

    // Leave construct frame.
  }

  // ES6 9.2.2. Step 13+
  // Check that the result is not a Smi, indicating that the constructor result
  // from a derived class is neither undefined nor an Object.
  if (check_derived_construct) {
    Label dont_throw;
    __ JumpIfNotSmi(r0, &dont_throw);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowDerivedConstructorReturnedNonObject);
    }
    __ bind(&dont_throw);
  }

  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - 1));
  __ add(sp, sp, Operand(kPointerSize));
  if (create_implicit_receiver) {
    __ IncrementCounter(isolate->counters()->constructed_objects(), 1, r1, r2);
  }
  __ Jump(lr);
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
  //  -- r0 : the value to pass to the generator
  //  -- r1 : the JSGeneratorObject to resume
  //  -- r2 : the resume mode (tagged)
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(r1);

  // Store input value into generator object.
  __ str(r0, FieldMemOperand(r1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(r1, JSGeneratorObject::kInputOrDebugPosOffset, r0, r3,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Store resume mode into generator object.
  __ str(r2, FieldMemOperand(r1, JSGeneratorObject::kResumeModeOffset));

  // Load suspended function and context.
  __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));
  __ ldr(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ mov(ip, Operand(debug_hook));
  __ ldrsb(ip, MemOperand(ip));
  __ cmp(ip, Operand(0));
  __ b(ne, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ mov(ip, Operand(debug_suspended_generator));
  __ ldr(ip, MemOperand(ip));
  __ cmp(ip, Operand(r1));
  __ b(eq, &prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Push receiver.
  __ ldr(ip, FieldMemOperand(r1, JSGeneratorObject::kReceiverOffset));
  __ Push(ip);

  // ----------- S t a t e -------------
  //  -- r1    : the JSGeneratorObject to resume
  //  -- r2    : the resume mode (tagged)
  //  -- r4    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ ldr(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r3,
         FieldMemOperand(r3, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ sub(r3, r3, Operand(Smi::FromInt(1)), SetCC);
    __ b(mi, &done_loop);
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ b(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ ldr(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r3, FieldMemOperand(r3, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(r3, r3, r3, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ ldr(r0, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r0, FieldMemOperand(
                   r0, SharedFunctionInfo::kFormalParameterCountOffset));
    __ SmiUntag(r0);
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(r3, r1);
    __ Move(r1, r4);
    __ ldr(r5, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
    __ Jump(r5);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1, r2, r4);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r1, r2);
    __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1, r2);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r1, r2);
    __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ push(r1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };

// Clobbers r2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
  // Make r2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ sub(r2, sp, r2);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ cmp(r2, Operand::PointerOffsetFromSmiKey(argc));
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ cmp(r2, Operand(argc, LSL, kPointerSizeLog2));
  }
  __ b(gt, &okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r0: new.target
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  // r5-r6, r8 (if !FLAG_enable_embedded_constant_pool) and cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ ldr(cp, MemOperand(cp));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(r1, r2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r2.
    Generate_CheckStackOverflow(masm, r3, kArgcIsUntaggedInt);

    // Remember new.target.
    __ mov(r5, r0);

    // Copy arguments to the stack in a loop.
    // r1: function
    // r3: argc
    // r4: argv, i.e. points to first arg
    Label loop, entry;
    __ add(r2, r4, Operand(r3, LSL, kPointerSizeLog2));
    // r2 points past last arg.
    __ b(&entry);
    __ bind(&loop);
    __ ldr(r0, MemOperand(r4, kPointerSize, PostIndex));  // read next parameter
    __ ldr(r0, MemOperand(r0));                           // dereference handle
    __ push(r0);                                          // push parameter
    __ bind(&entry);
    __ cmp(r4, r2);
    __ b(ne, &loop);

    // Setup new.target and argc.
    __ mov(r0, Operand(r3));
    __ mov(r3, Operand(r5));

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mov(r5, Operand(r4));
    __ mov(r6, Operand(r4));
    if (!FLAG_enable_embedded_constant_pool) {
      __ mov(r8, Operand(r4));
    }
    if (kR9Available == 1) {
      __ mov(r9, Operand(r4));
    }

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
    // Respect ABI stack constraint.
  }
  __ Jump(lr);

  // r0: result
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
  __ ldr(args_count,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(args_count,
         FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments.
  __ add(sp, sp, args_count, LeaveCC);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r1: the JS function object being called.
//   o r3: the new target
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
  __ PushStandardFrame(r1);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ ldr(r0, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  Register debug_info = kInterpreterBytecodeArrayRegister;
  DCHECK(!debug_info.is(r0));
  __ ldr(debug_info, FieldMemOperand(r0, SharedFunctionInfo::kDebugInfoOffset));
  __ cmp(debug_info, Operand(DebugInfo::uninitialized()));
  // Load original bytecode array or the debug copy.
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(r0, SharedFunctionInfo::kFunctionDataOffset), eq);
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex), ne);

  // Check whether we should continue to use the interpreter.
  Label switch_to_different_code_kind;
  __ ldr(r0, FieldMemOperand(r0, SharedFunctionInfo::kCodeOffset));
  __ cmp(r0, Operand(masm->CodeObject()));  // Self-reference to this code.
  __ b(ne, &switch_to_different_code_kind);

  // Increment invocation count for the function.
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kLiteralsOffset));
  __ ldr(r2, FieldMemOperand(r2, LiteralsArray::kFeedbackVectorOffset));
  __ ldr(r9, FieldMemOperand(
                 r2, TypeFeedbackVector::kInvocationCountIndex * kPointerSize +
                         TypeFeedbackVector::kHeaderSize));
  __ add(r9, r9, Operand(Smi::FromInt(1)));
  __ str(r9, FieldMemOperand(
                 r2, TypeFeedbackVector::kInvocationCountIndex * kPointerSize +
                         TypeFeedbackVector::kHeaderSize));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ SmiTst(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r0, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov(r9, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ strb(r9, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kBytecodeAgeOffset));

  // Load the initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push new.target, bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r0, kInterpreterBytecodeOffsetRegister);
  __ Push(r3, kInterpreterBytecodeArrayRegister, r0);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ ldr(r4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r9, sp, Operand(r4));
    __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
    __ cmp(r9, Operand(r2));
    __ b(hs, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
    __ b(&loop_check, al);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(r9);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ sub(r4, r4, Operand(kPointerSize), SetCC);
    __ b(&loop_header, ge);
  }

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ldr(ip, MemOperand(kInterpreterDispatchTableRegister, r1, LSL,
                        kPointerSizeLog2));
  __ Call(ip);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in r0.
  LeaveInterpreterFrame(masm, r2);
  __ Jump(lr);

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kCodeOffset));
  __ add(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ str(r4, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(r1, r4, r5);
  __ Jump(r4);
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
  __ cmp(scratch, Operand(num_args, LSL, kPointerSizeLog2));
  __ b(le, stack_overflow);  // Signed comparison.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register limit, Register scratch,
                                         Label* stack_overflow) {
  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, num_args, scratch, stack_overflow);

  // Find the address of the last argument.
  __ mov(limit, num_args);
  __ mov(limit, Operand(limit, LSL, kPointerSizeLog2));
  __ sub(limit, index, limit);

  Label loop_header, loop_check;
  __ b(al, &loop_check);
  __ bind(&loop_header);
  __ ldr(scratch, MemOperand(index, -kPointerSize, PostIndex));
  __ push(scratch);
  __ bind(&loop_check);
  __ cmp(index, limit);
  __ b(gt, &loop_header);
}

// static
void Builtins::Generate_InterpreterPushArgsAndCallImpl(
    MacroAssembler* masm, TailCallMode tail_call_mode,
    CallableType function_type) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  __ add(r3, r0, Operand(1));  // Add one for receiver.

  // Push the arguments. r2, r4, r5 will be modified.
  Generate_InterpreterPushArgs(masm, r3, r2, r4, r5, &stack_overflow);

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
    // Unreachable code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsAndConstructImpl(
    MacroAssembler* masm, CallableType construct_type) {
  // ----------- S t a t e -------------
  // -- r0 : argument count (not including receiver)
  // -- r3 : new target
  // -- r1 : constructor to call
  // -- r2 : allocation site feedback if available, undefined otherwise.
  // -- r4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ mov(ip, Operand::Zero());
  __ push(ip);

  // Push the arguments. r5, r4, r6 will be modified.
  Generate_InterpreterPushArgs(masm, r0, r4, r5, r6, &stack_overflow);

  __ AssertUndefinedOrAllocationSite(r2, r5);
  if (construct_type == CallableType::kJSFunction) {
    __ AssertFunction(r1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kConstructStubOffset));
    // Jump to the construct function.
    __ add(pc, r4, Operand(Code::kHeaderSize - kHeapObjectTag));

  } else {
    DCHECK_EQ(construct_type, CallableType::kAny);
    // Call the constructor with r0, r1, and r3 unmodified.
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsAndConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- r0 : argument count (not including receiver)
  // -- r1 : target to call verified to be Array function
  // -- r2 : allocation site feedback if available, undefined otherwise.
  // -- r3 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  __ add(r4, r0, Operand(1));  // Add one for receiver.

  // TODO(mythria): Add a stack check before pushing arguments.
  // Push the arguments. r3, r5, r6 will be modified.
  Generate_InterpreterPushArgs(masm, r4, r3, r5, r6, &stack_overflow);

  // Array constructor expects constructor in r3. It is same as r1 here.
  __ mov(r3, r1);

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
  __ Move(r2, masm->isolate()->builtins()->InterpreterEntryTrampoline());
  __ add(lr, r2, Operand(interpreter_entry_return_pc_offset->value() +
                         Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r1, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ldr(ip, MemOperand(kInterpreterDispatchTableRegister, r1, LSL,
                        kPointerSizeLog2));
  __ mov(pc, ip);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ ldr(r1, MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(r2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister, r1, r2);
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ mov(r2, r0);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ str(r2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argument count (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  //  -- r1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime, gotta_call_runtime_no_stack;
  Label try_shared;
  Label loop_top, loop_bottom;

  Register argument_count = r0;
  Register closure = r1;
  Register new_target = r3;
  __ push(argument_count);
  __ push(new_target);
  __ push(closure);

  Register map = argument_count;
  Register index = r2;
  __ ldr(map, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(map,
         FieldMemOperand(map, SharedFunctionInfo::kOptimizedCodeMapOffset));
  __ ldr(index, FieldMemOperand(map, FixedArray::kLengthOffset));
  __ cmp(index, Operand(Smi::FromInt(2)));
  __ b(lt, &gotta_call_runtime);

  // Find literals.
  // r3  : native context
  // r2  : length / index
  // r0  : optimized code map
  // stack[0] : new target
  // stack[4] : closure
  Register native_context = r3;
  __ ldr(native_context, NativeContextMemOperand());

  __ bind(&loop_top);
  Register temp = r1;
  Register array_pointer = r5;

  // Does the native context match?
  __ add(array_pointer, map, Operand::PointerOffsetFromSmiKey(index));
  __ ldr(temp, FieldMemOperand(array_pointer,
                               SharedFunctionInfo::kOffsetToPreviousContext));
  __ ldr(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ cmp(temp, native_context);
  __ b(ne, &loop_bottom);
  // Literals available?
  __ ldr(temp, FieldMemOperand(array_pointer,
                               SharedFunctionInfo::kOffsetToPreviousLiterals));
  __ ldr(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ JumpIfSmi(temp, &gotta_call_runtime);

  // Save the literals in the closure.
  __ ldr(r4, MemOperand(sp, 0));
  __ str(temp, FieldMemOperand(r4, JSFunction::kLiteralsOffset));
  __ push(index);
  __ RecordWriteField(r4, JSFunction::kLiteralsOffset, temp, index,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(index);

  // Code available?
  Register entry = r4;
  __ ldr(entry,
         FieldMemOperand(array_pointer,
                         SharedFunctionInfo::kOffsetToPreviousCachedCode));
  __ ldr(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found literals and code. Get them into the closure and return.
  __ pop(closure);
  // Store code entry in the closure.
  __ add(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ str(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, r5);

  // Link the closure into the optimized function list.
  // r4 : code entry
  // r3 : native context
  // r1 : closure
  __ ldr(r5,
         ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ str(r5, FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset));
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, r5, r0,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ str(closure,
         ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  // Save closure before the write barrier.
  __ mov(r5, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure, r0,
                            kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ mov(closure, r5);
  __ pop(new_target);
  __ pop(argument_count);
  __ Jump(entry);

  __ bind(&loop_bottom);
  __ sub(index, index, Operand(Smi::FromInt(SharedFunctionInfo::kEntryLength)));
  __ cmp(index, Operand(Smi::FromInt(1)));
  __ b(gt, &loop_top);

  // We found neither literals nor code.
  __ jmp(&gotta_call_runtime);

  __ bind(&try_shared);
  __ pop(closure);
  __ pop(new_target);
  __ pop(argument_count);
  __ ldr(entry,
         FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ ldrb(r5, FieldMemOperand(entry,
                              SharedFunctionInfo::kMarkedForTierUpByteOffset));
  __ tst(r5, Operand(1 << SharedFunctionInfo::kMarkedForTierUpBitWithinByte));
  __ b(ne, &gotta_call_runtime_no_stack);

  // If SFI points to anything other than CompileLazy, install that.
  __ ldr(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(r5, masm->CodeObject());
  __ cmp(entry, r5);
  __ b(eq, &gotta_call_runtime_no_stack);

  // Install the SFI's code entry.
  __ add(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ str(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, r5);
  __ Jump(entry);

  __ bind(&gotta_call_runtime);
  __ pop(closure);
  __ pop(new_target);
  __ pop(argument_count);
  __ bind(&gotta_call_runtime_no_stack);
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
  //  -- r0 : argument count (preserved for callee)
  //  -- r1 : new target (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(r4, r0);
    // Push the number of arguments to the callee.
    __ SmiTag(r0);
    __ push(r0);
    // Push a copy of the target function and the new target.
    __ push(r1);
    __ push(r3);

    // The function.
    __ push(r1);
    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmp(r4, Operand(j));
        __ b(ne, &over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ ldr(r4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                      i * kPointerSize));
        __ push(r4);
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
    __ JumpIfSmi(r0, &failed);

    __ Drop(2);
    __ pop(r4);
    __ SmiUntag(r4);
    scope.GenerateLeaveFrame();

    __ add(r4, r4, Operand(1));
    __ Drop(r4);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ pop(r3);
    __ pop(r1);
    __ pop(r0);
    __ SmiUntag(r0);
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

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r0 - contains return address (beginning of patch sequence)
  //   r1 - isolate
  //   r3 - new target
  FrameScope scope(masm, StackFrame::MANUAL);
  __ stm(db_w, sp, r0.bit() | r1.bit() | r3.bit() | fp.bit() | lr.bit());
  __ PrepareCallCFunction(2, 0, r2);
  __ mov(r1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ ldm(ia_w, sp, r0.bit() | r1.bit() | r3.bit() | fp.bit() | lr.bit());
  __ mov(pc, r0);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                              \
  void Builtins::Generate_Make##C##CodeYoungAgain(MacroAssembler* masm) { \
    GenerateMakeCodeYoungAgainCommon(masm);                               \
  }
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR

void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, as in GenerateMakeCodeYoungAgainCommon, we are relying on the fact
  // that make_code_young doesn't do any garbage collection which allows us to
  // save/restore the registers without worrying about which of them contain
  // pointers.

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r0 - contains return address (beginning of patch sequence)
  //   r1 - isolate
  //   r3 - new target
  FrameScope scope(masm, StackFrame::MANUAL);
  __ stm(db_w, sp, r0.bit() | r1.bit() | r3.bit() | fp.bit() | lr.bit());
  __ PrepareCallCFunction(2, 0, r2);
  __ mov(r1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ ldm(ia_w, sp, r0.bit() | r1.bit() | r3.bit() | fp.bit() | lr.bit());

  // Perform prologue operations usually performed by the young code stub.
  __ PushStandardFrame(r1);

  // Jump to point after the code-age stub.
  __ add(r0, r0, Operand(kNoCodeAgeSequenceLength));
  __ mov(pc, r0);
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
    __ stm(db_w, sp, kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
    __ ldm(ia_w, sp, kJSCallerSaved | kCalleeSaved);
  }

  __ add(sp, sp, Operand(kPointerSize));  // Ignore state
  __ mov(pc, lr);                         // Jump to miss handler
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
    __ mov(r0, Operand(Smi::FromInt(static_cast<int>(type))));
    __ push(r0);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Get the full codegen state from the stack and untag it -> r6.
  __ ldr(r6, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(r6);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ cmp(r6,
         Operand(static_cast<int>(Deoptimizer::BailoutState::NO_REGISTERS)));
  __ b(ne, &with_tos_register);
  __ add(sp, sp, Operand(1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r0.code());
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));
  __ cmp(r6,
         Operand(static_cast<int>(Deoptimizer::BailoutState::TOS_REGISTER)));
  __ b(ne, &unknown_state);
  __ add(sp, sp, Operand(2 * kPointerSize));  // Remove state.
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

static void CompatibleReceiverCheck(MacroAssembler* masm, Register receiver,
                                    Register function_template_info,
                                    Register scratch0, Register scratch1,
                                    Register scratch2,
                                    Label* receiver_check_failed) {
  Register signature = scratch0;
  Register map = scratch1;
  Register constructor = scratch2;

  // If there is no signature, return the holder.
  __ ldr(signature, FieldMemOperand(function_template_info,
                                    FunctionTemplateInfo::kSignatureOffset));
  __ CompareRoot(signature, Heap::kUndefinedValueRootIndex);
  Label receiver_check_passed;
  __ b(eq, &receiver_check_passed);

  // Walk the prototype chain.
  __ ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  Label prototype_loop_start;
  __ bind(&prototype_loop_start);

  // Get the constructor, if any.
  __ GetMapConstructor(constructor, map, ip, ip);
  __ cmp(ip, Operand(JS_FUNCTION_TYPE));
  Label next_prototype;
  __ b(ne, &next_prototype);
  Register type = constructor;
  __ ldr(type,
         FieldMemOperand(constructor, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(type, FieldMemOperand(type, SharedFunctionInfo::kFunctionDataOffset));

  // Loop through the chain of inheriting function templates.
  Label function_template_loop;
  __ bind(&function_template_loop);

  // If the signatures match, we have a compatible receiver.
  __ cmp(signature, type);
  __ b(eq, &receiver_check_passed);

  // If the current type is not a FunctionTemplateInfo, load the next prototype
  // in the chain.
  __ JumpIfSmi(type, &next_prototype);
  __ CompareObjectType(type, ip, ip, FUNCTION_TEMPLATE_INFO_TYPE);

  // Otherwise load the parent function template and iterate.
  __ ldr(type,
         FieldMemOperand(type, FunctionTemplateInfo::kParentTemplateOffset),
         eq);
  __ b(&function_template_loop, eq);

  // Load the next prototype.
  __ bind(&next_prototype);
  __ ldr(ip, FieldMemOperand(map, Map::kBitField3Offset));
  __ tst(ip, Operand(Map::HasHiddenPrototype::kMask));
  __ b(eq, receiver_check_failed);
  __ ldr(receiver, FieldMemOperand(map, Map::kPrototypeOffset));
  __ ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Iterate.
  __ b(&prototype_loop_start);

  __ bind(&receiver_check_passed);
}

void Builtins::Generate_HandleFastApiCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                 : number of arguments excluding receiver
  //  -- r1                 : callee
  //  -- lr                 : return address
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------

  // Load the FunctionTemplateInfo.
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r3, FieldMemOperand(r3, SharedFunctionInfo::kFunctionDataOffset));

  // Do the compatible receiver check.
  Label receiver_check_failed;
  __ ldr(r2, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  CompatibleReceiverCheck(masm, r2, r3, r4, r5, r6, &receiver_check_failed);

  // Get the callback offset from the FunctionTemplateInfo, and jump to the
  // beginning of the code.
  __ ldr(r4, FieldMemOperand(r3, FunctionTemplateInfo::kCallCodeOffset));
  __ ldr(r4, FieldMemOperand(r4, CallHandlerInfo::kFastHandlerOffset));
  __ add(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r4);

  // Compatible receiver check failed: throw an Illegal Invocation exception.
  __ bind(&receiver_check_failed);
  // Drop the arguments (including the receiver)
  __ add(r0, r0, Operand(1));
  __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
  __ TailCallRuntime(Runtime::kThrowIllegalInvocation);
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ ldr(r0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ ldr(r0, MemOperand(r0, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ cmp(r0, Operand(Smi::kZero));
  __ b(ne, &skip);
  __ Ret();

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ ldr(r1, FieldMemOperand(r0, Code::kDeoptimizationDataOffset));

  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ add(r0, r0, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    if (FLAG_enable_embedded_constant_pool) {
      __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r0);
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ ldr(r1, FieldMemOperand(
                   r1, FixedArray::OffsetOfElementAt(
                           DeoptimizationInputData::kOsrPcOffsetIndex)));

    // Compute the target address = code start + osr_offset
    __ add(lr, r0, Operand::SmiUntag(r1));

    // And "return" to the OSR entry point of the function.
    __ Ret();
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
  //  -- r0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into r1, argArray into r0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ mov(r3, r2);
    __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));  // receiver
    __ sub(r4, r0, Operand(1), SetCC);
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // thisArg
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r3, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // argArray
    __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r2, MemOperand(sp, 0));
    __ mov(r0, r3);
  }

  // ----------- S t a t e -------------
  //  -- r0    : argArray
  //  -- r1    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(r1, &receiver_not_callable);
  __ ldr(r4, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsCallable));
  __ b(eq, &receiver_not_callable);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r0, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(r0, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(r0, Operand(0));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ str(r1, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of arguments
  {
    Label done;
    __ cmp(r0, Operand::Zero());
    __ b(ne, &done);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ add(r0, r0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  // r0: actual number of arguments
  __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r0: actual number of arguments
  // r1: callable
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));

    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, -kPointerSize));
    __ str(ip, MemOperand(r2));
    __ sub(r2, r2, Operand(kPointerSize));
    __ cmp(r2, sp);
    __ b(ne, &loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ sub(r0, r0, Operand(1));
    __ pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r1 (if present), argumentsList into r0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ mov(r2, r1);
    __ mov(r3, r1);
    __ sub(r4, r0, Operand(1), SetCC);
    __ ldr(r1, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // target
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // thisArgument
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r3, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // argumentsList
    __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r2, MemOperand(sp, 0));
    __ mov(r0, r3);
  }

  // ----------- S t a t e -------------
  //  -- r0    : argumentsList
  //  -- r1    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(r1, &target_not_callable);
  __ ldr(r4, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsCallable));
  __ b(eq, &target_not_callable);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ str(r1, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r1 (if present), argumentsList into r0 (if present),
  // new.target into r3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ mov(r2, r1);
    __ str(r2, MemOperand(sp, r0, LSL, kPointerSizeLog2));  // receiver
    __ sub(r4, r0, Operand(1), SetCC);
    __ ldr(r1, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // target
    __ mov(r3, r1);  // new.target defaults to target
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r2, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // argumentsList
    __ sub(r4, r4, Operand(1), SetCC, ge);
    __ ldr(r3, MemOperand(sp, r4, LSL, kPointerSizeLog2), ge);  // new.target
    __ add(sp, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ mov(r0, r2);
  }

  // ----------- S t a t e -------------
  //  -- r0    : argumentsList
  //  -- r3    : new.target
  //  -- r1    : target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(r1, &target_not_constructor);
  __ ldr(r4, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsConstructor));
  __ b(eq, &target_not_constructor);

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(r3, &new_target_not_constructor);
  __ ldr(r4, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsConstructor));
  __ b(eq, &new_target_not_constructor);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ str(r1, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ str(r3, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r0);
  __ mov(r4, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ stm(db_w, sp, r0.bit() | r1.bit() | r4.bit() |
                       (FLAG_enable_embedded_constant_pool ? pp.bit() : 0) |
                       fp.bit() | lr.bit());
  __ add(fp, sp,
         Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ldr(r1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                              kPointerSize)));

  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR);
  __ add(sp, sp, Operand::PointerOffsetFromSmiKey(r1));
  __ add(sp, sp, Operand(kPointerSize));  // adjust for receiver
}

// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : argumentsList
  //  -- r1    : target
  //  -- r3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(r0, &create_runtime);

    // Load the map of argumentsList into r2.
    __ ldr(r2, FieldMemOperand(r0, HeapObject::kMapOffset));

    // Load native context into r4.
    __ ldr(r4, NativeContextMemOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ ldr(ip, ContextMemOperand(r4, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ cmp(ip, r2);
    __ b(eq, &create_arguments);
    __ ldr(ip, ContextMemOperand(r4, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ cmp(ip, r2);
    __ b(eq, &create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CompareInstanceType(r2, ip, JS_ARRAY_TYPE);
    __ b(eq, &create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r1, r3, r0);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(r1, r3);
      __ ldr(r2, FieldMemOperand(r0, FixedArray::kLengthOffset));
      __ SmiUntag(r2);
    }
    __ jmp(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ ldr(r2, FieldMemOperand(r0, JSArgumentsObject::kLengthOffset));
    __ ldr(r4, FieldMemOperand(r0, JSObject::kElementsOffset));
    __ ldr(ip, FieldMemOperand(r4, FixedArray::kLengthOffset));
    __ cmp(r2, ip);
    __ b(ne, &create_runtime);
    __ SmiUntag(r2);
    __ mov(r0, r4);
    __ b(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ bind(&create_holey_array);
    __ ldr(r2, FieldMemOperand(r2, Map::kPrototypeOffset));
    __ ldr(r4, ContextMemOperand(r4, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ cmp(r2, r4);
    __ b(ne, &create_runtime);
    __ LoadRoot(r4, Heap::kArrayProtectorRootIndex);
    __ ldr(r2, FieldMemOperand(r4, PropertyCell::kValueOffset));
    __ cmp(r2, Operand(Smi::FromInt(Isolate::kProtectorValid)));
    __ b(ne, &create_runtime);
    __ ldr(r2, FieldMemOperand(r0, JSArray::kLengthOffset));
    __ ldr(r0, FieldMemOperand(r0, JSArray::kElementsOffset));
    __ SmiUntag(r2);
    __ b(&done_create);

    // Try to create the list from a JSArray object.
    //  -- r2 and r4 must be preserved till bne create_holey_array.
    __ bind(&create_array);
    __ ldr(r5, FieldMemOperand(r2, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(r5);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ cmp(r5, Operand(FAST_HOLEY_ELEMENTS));
    __ b(hi, &create_runtime);
    // Only FAST_XXX after this point, FAST_HOLEY_XXX are odd values.
    __ tst(r5, Operand(1));
    __ b(ne, &create_holey_array);
    // FAST_SMI_ELEMENTS or FAST_ELEMENTS after this point.
    __ ldr(r2, FieldMemOperand(r0, JSArray::kLengthOffset));
    __ ldr(r0, FieldMemOperand(r0, JSArray::kElementsOffset));
    __ SmiUntag(r2);

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
    __ cmp(ip, Operand(r2, LSL, kPointerSizeLog2));
    __ b(gt, &done);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- r1    : target
  //  -- r0    : args (a FixedArray built from argumentsList)
  //  -- r2    : len (number of elements to push from args)
  //  -- r3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ mov(r4, Operand(0));
    __ LoadRoot(r5, Heap::kTheHoleValueRootIndex);
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    Label done, loop;
    __ bind(&loop);
    __ cmp(r4, r2);
    __ b(eq, &done);
    __ add(ip, r0, Operand(r4, LSL, kPointerSizeLog2));
    __ ldr(ip, FieldMemOperand(ip, FixedArray::kHeaderSize));
    __ cmp(r5, ip);
    __ mov(ip, r6, LeaveCC, eq);
    __ Push(ip);
    __ add(r4, r4, Operand(1));
    __ b(&loop);
    __ bind(&done);
    __ Move(r0, r4);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
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
  __ ldrb(scratch1, MemOperand(scratch1));
  __ cmp(scratch1, Operand(0));
  __ b(eq, &done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ ldr(scratch3,
           MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ cmp(scratch3, Operand(Smi::FromInt(StackFrame::STUB)));
    __ b(ne, &no_interpreter_frame);
    __ ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ ldr(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(scratch3,
         MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmp(scratch3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &no_arguments_adaptor);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ ldr(caller_args_count_reg,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ b(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ ldr(scratch1,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  __ ldr(scratch1,
         FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(caller_args_count_reg,
         FieldMemOperand(scratch1,
                         SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(caller_args_count_reg);

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
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldrb(r3, FieldMemOperand(r2, SharedFunctionInfo::kFunctionKindByteOffset));
  __ tst(r3, Operand(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ b(ne, &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ ldrb(r3, FieldMemOperand(r2, SharedFunctionInfo::kNativeByteOffset));
  __ tst(r3, Operand((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                     (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ b(ne, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- r0 : the number of arguments (not including the receiver)
    //  -- r1 : the function to call (checked to be a JSFunction)
    //  -- r2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r3);
    } else {
      Label convert_to_object, convert_receiver;
      __ ldr(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));
      __ JumpIfSmi(r3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
      __ b(hs, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r3, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(r3, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(r3);
        }
        __ b(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(r0);
        __ Push(r0, r1);
        __ mov(r0, r3);
        __ Push(cp);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(r3, r0);
        __ Pop(r0, r1);
        __ SmiUntag(r0);
      }
      __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ str(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSFunction)
  //  -- r2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r0, r3, r4, r5);
  }

  __ ldr(r2,
         FieldMemOperand(r2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(r2);
  ParameterCount actual(r0);
  ParameterCount expected(r2);
  __ InvokeFunctionCode(r1, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ push(r1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : target (checked to be a JSBoundFunction)
  //  -- r3 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r2 and length of that into r4.
  Label no_bound_arguments;
  __ ldr(r2, FieldMemOperand(r1, JSBoundFunction::kBoundArgumentsOffset));
  __ ldr(r4, FieldMemOperand(r2, FixedArray::kLengthOffset));
  __ SmiUntag(r4);
  __ cmp(r4, Operand(0));
  __ b(eq, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- r0 : the number of arguments (not including the receiver)
    //  -- r1 : target (checked to be a JSBoundFunction)
    //  -- r2 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r3 : new.target (only in case of [[Construct]])
    //  -- r4 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ sub(sp, sp, Operand(r4, LSL, kPointerSizeLog2));
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
      __ b(gt, &done);  // Signed comparison.
      // Restore the stack pointer.
      __ add(sp, sp, Operand(r4, LSL, kPointerSizeLog2));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Relocate arguments down the stack.
    {
      Label loop, done_loop;
      __ mov(r5, Operand(0));
      __ bind(&loop);
      __ cmp(r5, r0);
      __ b(gt, &done_loop);
      __ ldr(ip, MemOperand(sp, r4, LSL, kPointerSizeLog2));
      __ str(ip, MemOperand(sp, r5, LSL, kPointerSizeLog2));
      __ add(r4, r4, Operand(1));
      __ add(r5, r5, Operand(1));
      __ b(&loop);
      __ bind(&done_loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ ldr(r4, FieldMemOperand(r2, FixedArray::kLengthOffset));
      __ SmiUntag(r4);
      __ add(r2, r2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ bind(&loop);
      __ sub(r4, r4, Operand(1), SetCC);
      __ ldr(ip, MemOperand(r2, r4, LSL, kPointerSizeLog2));
      __ str(ip, MemOperand(sp, r0, LSL, kPointerSizeLog2));
      __ add(r0, r0, Operand(1));
      __ b(gt, &loop);
    }
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r1);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r0, r3, r4, r5);
  }

  // Patch the receiver to [[BoundThis]].
  __ ldr(ip, FieldMemOperand(r1, JSBoundFunction::kBoundThisOffset));
  __ str(ip, MemOperand(sp, r0, LSL, kPointerSizeLog2));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ ldr(r1, FieldMemOperand(r1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ip, Operand(ExternalReference(Builtins::kCall_ReceiverIsAny,
                                       masm->isolate())));
  __ ldr(ip, MemOperand(ip));
  __ add(pc, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r1, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r1, r4, r5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
          RelocInfo::CODE_TARGET, eq);
  __ cmp(r5, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsCallable));
  __ b(eq, &non_callable);

  __ cmp(r5, Operand(JS_PROXY_TYPE));
  __ b(ne, &non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r0, r3, r4, r5);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ Push(r1);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ add(r0, r0, Operand(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver the (original) target.
  __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (checked to be a JSFunction)
  //  -- r3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(r1);

  // Calling convention for function specific ConstructStubs require
  // r2 to contain either an AllocationSite or undefined.
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kConstructStubOffset));
  __ add(pc, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSBoundFunction)
  //  -- r3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(r1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  __ cmp(r1, r3);
  __ ldr(r3, FieldMemOperand(r1, JSBoundFunction::kBoundTargetFunctionOffset),
         eq);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ ldr(r1, FieldMemOperand(r1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ip, Operand(ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ ldr(ip, MemOperand(ip));
  __ add(pc, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (checked to be a JSProxy)
  //  -- r3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ Push(r1);
  __ Push(r3);
  // Include the pushed new_target, constructor and the receiver.
  __ add(r0, r0, Operand(3));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (can be any Object)
  //  -- r3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(r1, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(r1, r4, r5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ ldrb(r2, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r2, Operand(1 << Map::kIsConstructor));
  __ b(eq, &non_constructor);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ cmp(r5, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructBoundFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ cmp(r5, Operand(JS_PROXY_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r1);
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
  //  -- r1 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r1);
  __ Push(r1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r1);
  __ Move(r2, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(r1, r2);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  __ Push(r1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  //  -- r3 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ cmp(r0, r2);
  __ b(lt, &too_few);
  __ cmp(r2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ b(eq, &dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r2, r5, &stack_overflow);

    // Calculate copy start address into r0 and copy end address into r4.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));
    // adjust for return address and receiver
    __ add(r0, r0, Operand(2 * kPointerSize));
    __ sub(r4, r0, Operand(r2, LSL, kPointerSizeLog2));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    // r4: copy end address

    Label copy;
    __ bind(&copy);
    __ ldr(ip, MemOperand(r0, 0));
    __ push(ip);
    __ cmp(r0, r4);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r2, r5, &stack_overflow);

    // Calculate copy start address into r0 and copy end address is fp.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ ldr(ip, MemOperand(r0, 2 * kPointerSize));
    __ push(ip);
    __ cmp(r0, fp);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    // Fill the remaining expected arguments with undefined.
    // r1: function
    // r2: expected number of arguments
    // r3: new target (passed through to callee)
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ sub(r4, fp, Operand(r2, LSL, kPointerSizeLog2));
    // Adjust for frame.
    __ sub(r4, r4, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                           2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(ip);
    __ cmp(sp, r4);
    __ b(ne, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(r0, r2);
  // r0 : expected number of arguments
  // r1 : function (passed through to callee)
  // r3 : new target (passed through to callee)
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ Call(r4);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Jump(lr);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ Jump(r4);

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

#endif  // V8_TARGET_ARCH_ARM
