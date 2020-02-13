// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

#include "src/api/api-arguments.h"
#include "src/codegen/code-factory.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/logging/counters.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/mips/constants-mips.h"
#include "src/codegen/register-configuration.h"
#include "src/heap/heap-inl.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ li(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, t0);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForInternalArrayFunction,
              t0, Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForInternalArrayFunction,
              t0, Operand(MAP_TYPE));
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  __ Jump(BUILTIN_CODE(masm->isolate(), InternalArrayConstructorImpl),
          RelocInfo::CODE_TARGET);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- a1 : target function (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ Push(a1, a3, a1);

    __ CallRuntime(function_id, 1);

    // Restore target function and new target.
    __ Pop(a1, a3);
  }

  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ Addu(a2, v0, Code::kHeaderSize - kHeapObjectTag);
  __ Jump(a2);
}

namespace {

void LoadRealStackLimit(MacroAssembler* masm, Register destination) {
  DCHECK(masm->root_array_available());
  Isolate* isolate = masm->isolate();
  ExternalReference limit = ExternalReference::address_of_real_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  __ Lw(destination, MemOperand(kRootRegister, static_cast<int32_t>(offset)));
}

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a3     : new target
  //  -- cp     : context
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0);
    __ SmiUntag(a0);

    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Set up pointer to last argument.
    __ Addu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(t3, a0);
    // ----------- S t a t e -------------
    //  --                        a0: number of arguments (untagged)
    //  --                        a3: new target
    //  --                        t2: pointer to last argument
    //  --                        t3: counter
    //  --        sp[0*kPointerSize]: the hole (receiver)
    //  --        sp[1*kPointerSize]: number of arguments (tagged)
    //  --        sp[2*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Lsa(t0, t2, t3, kPointerSizeLog2);
    __ lw(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Addu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments (untagged)
    // a1: constructor function
    // a3: new target
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION);

    // Restore context from the frame.
    __ lw(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - 1);
  __ Addu(sp, sp, kPointerSize);
  __ Ret();
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch1, Register scratch2,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  LoadRealStackLimit(masm, scratch1);
  // Make scratch1 the space we have left. The stack might already be overflowed
  // here which will cause scratch1 to become negative.
  __ subu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ sll(scratch2, num_args, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, scratch1, Operand(scratch2));
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  --      a0: number of arguments (untagged)
  //  --      a1: constructor function
  //  --      a3: new target
  //  --      cp: context
  //  --      ra: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0, a1);
    __ PushRoot(RootIndex::kTheHoleValue);
    __ Push(a3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  --        sp[1*kPointerSize]: padding
    //  -- a1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------

    __ lw(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(t2, FieldMemOperand(t2, SharedFunctionInfo::kFlagsOffset));
    __ DecodeField<SharedFunctionInfo::FunctionKindBits>(t2);
    __ JumpIfIsInRange(t2, kDefaultDerivedConstructor, kDerivedConstructor,
                       &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        t2, t3);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
            RelocInfo::CODE_TARGET);
    __ Branch(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(v0, RootIndex::kTheHoleValue);

    // ----------- S t a t e -------------
    //  --                          v0: receiver
    //  -- Slot 4 / sp[0*kPointerSize]: new target
    //  -- Slot 3 / sp[1*kPointerSize]: padding
    //  -- Slot 2 / sp[2*kPointerSize]: constructor function
    //  -- Slot 1 / sp[3*kPointerSize]: number of arguments (tagged)
    //  -- Slot 0 / sp[4*kPointerSize]: context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());
    __ bind(&post_instantiation_deopt_entry);

    // Restore new target.
    __ Pop(a3);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(v0, v0);

    // ----------- S t a t e -------------
    //  --                 r3: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: padding
    //  -- sp[3*kPointerSize]: constructor function
    //  -- sp[4*kPointerSize]: number of arguments (tagged)
    //  -- sp[5*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ lw(a0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(a0);

    // Set up pointer to last argument.
    __ Addu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    Label enough_stack_space, stack_overflow;
    Generate_StackOverflowCheck(masm, a0, t0, t1, &stack_overflow);
    __ Branch(&enough_stack_space);

    __ bind(&stack_overflow);
    // Restore the context from the frame.
    __ lw(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);

    __ bind(&enough_stack_space);

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(t3, a0);
    // ----------- S t a t e -------------
    //  --                        a0: number of arguments (untagged)
    //  --                        a3: new target
    //  --                        t2: pointer to last argument
    //  --                        t3: counter
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  --        sp[2*kPointerSize]: padding
    //  -- a1 and sp[3*kPointerSize]: constructor function
    //  --        sp[4*kPointerSize]: number of arguments (tagged)
    //  --        sp[5*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Lsa(t0, t2, t3, kPointerSizeLog2);
    __ lw(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Addu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION);

    // ----------- S t a t e -------------
    //  --                 v0: constructor result
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
    __ lw(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(v0, RootIndex::kUndefinedValue, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(v0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    __ GetObjectType(v0, t2, t2);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ Branch(&leave_frame, greater_equal, t2, Operand(FIRST_JS_RECEIVER_TYPE));
    __ Branch(&use_receiver);

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ lw(v0, MemOperand(sp, 0 * kPointerSize));
    __ JumpIfRoot(v0, RootIndex::kTheHoleValue, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ lw(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(sp, sp, kPointerSize);
  __ Ret();
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(a1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// Clobbers scratch1 and scratch2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        Register scratch1, Register scratch2) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  LoadRealStackLimit(masm, scratch1);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause a2 to become negative.
  __ Subu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ sll(scratch2, argc, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(&okay, gt, scratch1, Operand(scratch2));

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

namespace {

// Used by JSEntryTrampoline to refer C++ parameter to JSEntryVariant.
constexpr int kPushedStackSpace =
    kCArgsSlotsSize + (kNumCalleeSaved + 1) * kPointerSize +
    kNumCalleeSavedFPU * kDoubleSize + 4 * kPointerSize +
    EntryFrameConstants::kCallerFPOffset;

// Called with the native C calling convention. The corresponding function
// signature is either:
//
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, Address new_target, Address target,
//       Address receiver, intptr_t argc, Address** argv)>;
// or
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, MicrotaskQueue* microtask_queue)>;
//
// Passes through a0, a1, a2, a3 and stack to JSEntryTrampoline.
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtins::Name entry_trampoline) {
  Label invoke, handler_entry, exit;

  int pushed_stack_space = kCArgsSlotsSize;
  {
    NoRootArrayScope no_root_array(masm);

    // Registers:
    // a0: root_register_value

    // Save callee saved registers on the stack.
    __ MultiPush(kCalleeSaved | ra.bit());
    pushed_stack_space +=
        kNumCalleeSaved * kPointerSize + kPointerSize /* ra */;

    // Save callee-saved FPU registers.
    __ MultiPushFPU(kCalleeSavedFPU);
    pushed_stack_space += kNumCalleeSavedFPU * kDoubleSize;

    // Set up the reserved register for 0.0.
    __ Move(kDoubleRegZero, 0.0);

    // Initialize the root register.
    // C calling convention. The first argument is passed in a0.
    __ mov(kRootRegister, a0);
  }

  // We build an EntryFrame.
  __ li(t3, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ li(t2, Operand(StackFrame::TypeToMarker(type)));
  __ li(t1, Operand(StackFrame::TypeToMarker(type)));
  __ li(t0, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                      masm->isolate()));
  __ lw(t0, MemOperand(t0));
  __ Push(t3, t2, t1, t0);
  pushed_stack_space += 4 * kPointerSize;

  // Set up frame pointer for the frame to be pushed.
  __ addiu(fp, sp, -EntryFrameConstants::kCallerFPOffset);
  pushed_stack_space += EntryFrameConstants::kCallerFPOffset;

  // Registers:
  // a0: root_register_value
  //
  // Stack:
  // caller fp          |
  // function slot      | entry frame
  // context slot       |
  // bad fp (0xFF...F)  |
  // callee saved registers + ra
  // 4 args slots

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp = ExternalReference::Create(
      IsolateAddressId::kJSEntrySPAddress, masm->isolate());
  __ li(t1, js_entry_sp);
  __ lw(t2, MemOperand(t1));
  __ Branch(&non_outermost_js, ne, t2, Operand(zero_reg));
  __ sw(fp, MemOperand(t1));
  __ li(t0, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ nop();  // Branch delay slot nop.
  __ bind(&non_outermost_js);
  __ li(t0, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(t0);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ li(t0, ExternalReference::Create(
                IsolateAddressId::kPendingExceptionAddress, masm->isolate()));
  __ sw(v0, MemOperand(t0));  // We come back from 'invoke'. result is in v0.
  __ LoadRoot(v0, RootIndex::kException);
  __ b(&exit);  // b exposes branch delay slot.
  __ nop();     // Branch delay slot nop.

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bal(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.
  //
  // Preserve a1, a2 and a3 passed by C++ and pass them to the trampoline.
  //
  // Stack:
  // handler frame
  // entry frame
  // callee saved registers + ra
  // 4 args slots
  //
  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->builtin_handle(entry_trampoline);
  DCHECK_EQ(kPushedStackSpace, pushed_stack_space);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // v0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(t1);
  __ Branch(&non_outermost_js_2, ne, t1,
            Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ li(t1, ExternalReference(js_entry_sp));
  __ sw(zero_reg, MemOperand(t1));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(t1);
  __ li(t0, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                      masm->isolate()));
  __ sw(t1, MemOperand(t0));

  // Reset the stack to the callee saved registers.
  __ addiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Restore callee-saved fpu registers.
  __ MultiPopFPU(kCalleeSavedFPU);

  // Restore callee saved registers from the stack.
  __ MultiPop(kCalleeSaved | ra.bit());
  // Return.
  __ Jump(ra);
}

}  // namespace

void Builtins::Generate_JSEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtins::kJSEntryTrampoline);
}

void Builtins::Generate_JSConstructEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::CONSTRUCT_ENTRY,
                          Builtins::kJSConstructEntryTrampoline);
}

void Builtins::Generate_JSRunMicrotasksEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtins::kRunMicrotasksTrampoline);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // ----------- S t a t e -------------
  //  -- a0: root_register_value (unused)
  //  -- a1: new.target
  //  -- a2: function
  //  -- a3: receiver_pointer
  //  -- [fp + kPushedStackSpace + 0 * kPointerSize]: argc
  //  -- [fp + kPushedStackSpace + 1 * kPointerSize]: argv
  // -----------------------------------

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ li(cp, context_address);
    __ lw(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(a2, a3);

    __ mov(a3, a1);
    __ mov(a1, a2);

    __ lw(s0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ lw(a0,
          MemOperand(s0, kPushedStackSpace + EntryFrameConstants::kArgcOffset));
    __ lw(s0,
          MemOperand(s0, kPushedStackSpace + EntryFrameConstants::kArgvOffset));

    // a0: argc
    // a1: function
    // a3: new.target
    // s0: argv

    // Check if we have enough stack space to push all arguments.
    // Clobbers a2 and t0.
    Generate_CheckStackOverflow(masm, a0, a2, t0);

    // Copy arguments to the stack in a loop.
    // a0: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
    __ Lsa(t2, s0, a0, kPointerSizeLog2);
    __ b(&entry);
    __ nop();  // Branch delay slot nop.
    // t2 points past last arg.
    __ bind(&loop);
    __ lw(t0, MemOperand(s0));  // Read next parameter.
    __ addiu(s0, s0, kPointerSize);
    __ lw(t0, MemOperand(t0));  // Dereference handle.
    __ push(t0);                // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, s0, Operand(t2));

    // a0: argc
    // a1: function
    // a3: new.target

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(t0, RootIndex::kUndefinedValue);
    __ mov(s0, t0);
    __ mov(s1, t0);
    __ mov(s2, t0);
    __ mov(s3, t0);
    __ mov(s4, t0);
    __ mov(s5, t0);
    // s6 holds the root address. Do not clobber.
    // s7 is cp. Do not init.

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }

  __ Jump(ra);
}

void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}

void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // a1: microtask_queue
  __ mov(RunMicrotasksDescriptor::MicrotaskQueueRegister(), a1);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void GetSharedFunctionInfoBytecode(MacroAssembler* masm,
                                          Register sfi_data,
                                          Register scratch1) {
  Label done;

  __ GetObjectType(sfi_data, scratch1, scratch1);
  __ Branch(&done, ne, scratch1, Operand(INTERPRETER_DATA_TYPE));
  __ lw(sfi_data,
        FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- ra : return address
  // -----------------------------------

  __ AssertGeneratorObject(a1);

  // Store input value into generator object.
  __ sw(v0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, v0, a3,
                      kRAHasNotBeenSaved, kDontSaveFPRegs);

  // Load suspended function and context.
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ lw(cp, FieldMemOperand(t0, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ li(t1, debug_hook);
  __ lb(t1, MemOperand(t1));
  __ Branch(&prepare_step_in_if_stepping, ne, t1, Operand(zero_reg));

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ li(t1, debug_suspended_generator);
  __ lw(t1, MemOperand(t1));
  __ Branch(&prepare_step_in_suspended_generator, eq, a1, Operand(t1));
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  LoadRealStackLimit(masm, kScratchReg);
  __ Branch(&stack_overflow, lo, sp, Operand(kScratchReg));

  // Push receiver.
  __ lw(t1, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ Push(t1);

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- t0    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.

  __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ lhu(a3,
         FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ lw(t1,
        FieldMemOperand(a1, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label done_loop, loop;
    __ Move(t2, zero_reg);
    __ bind(&loop);
    __ Subu(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ Lsa(kScratchReg, t1, t2, kPointerSizeLog2);
    __ lw(kScratchReg, FieldMemOperand(kScratchReg, FixedArray::kHeaderSize));
    __ Push(kScratchReg);
    __ Addu(t2, t2, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecode(masm, a3, a0);
    __ GetObjectType(a3, a3, a3);
    __ Assert(eq, AbortReason::kMissingBytecodeArray, a3,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ lw(a0, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
    __ lhu(a0, FieldMemOperand(
                   a0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, t0);
    static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
    __ lw(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
    __ Addu(a2, a2, Code::kHeaderSize - kHeapObjectTag);
    __ Jump(a2);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, t0);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(a1);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);  // This should be unreachable.
  }
}

static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  // Store code entry in the closure.
  __ sw(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ lw(args_count,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(args_count,
        FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  // Drop receiver + arguments.
  __ Addu(sp, sp, args_count);
}

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ Branch(&no_match, ne, smi_entry, Operand(Smi::FromEnum(marker)));
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- a3 : new target (preserved for callee if needed, and caller)
  //  -- a1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(!AreAliased(feedback_vector, a1, a3, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_weak_ref, fallthrough;

  Register closure = a1;
  Register optimized_code_entry = scratch1;

  __ lw(optimized_code_entry,
        FieldMemOperand(feedback_vector,
                        FeedbackVector::kOptimizedCodeWeakOrSmiOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret it as a weak cell to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_weak_ref);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ Branch(&fallthrough, eq, optimized_code_entry,
              Operand(Smi::FromEnum(OptimizationMarker::kNone)));

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
        __ Assert(
            eq, AbortReason::kExpectedOptimizationSentinel,
            optimized_code_entry,
            Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
      }
      __ jmp(&fallthrough);
    }
  }

  {
    // Optimized code slot is a weak reference.
    __ bind(&optimized_code_slot_is_weak_ref);

    __ LoadWeakValue(optimized_code_entry, optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ lw(scratch2, FieldMemOperand(optimized_code_entry,
                                    Code::kCodeDataContainerOffset));
    __ lw(scratch2, FieldMemOperand(
                        scratch2, CodeDataContainer::kKindSpecificFlagsOffset));
    __ And(scratch2, scratch2, Operand(1 << Code::kMarkedForDeoptimizationBit));
    __ Branch(&found_deoptimized_code, ne, scratch2, Operand(zero_reg));

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
    __ Addu(a2, optimized_code_entry, Code::kHeaderSize - kHeapObjectTag);
    __ Jump(a2);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // losure's code.
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
                                          Register scratch2, Label* if_return) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ li(bytecode_size_table, ExternalReference::bytecode_size_table_address());

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  STATIC_ASSERT(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  STATIC_ASSERT(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ Branch(&process_bytecode, hi, bytecode, Operand(3));
  __ And(scratch2, bytecode, Operand(1));
  __ Branch(&extra_wide, ne, scratch2, Operand(zero_reg));

  // Load the next bytecode and update table to the wide scaled table.
  __ Addu(bytecode_offset, bytecode_offset, Operand(1));
  __ Addu(scratch2, bytecode_array, bytecode_offset);
  __ lbu(bytecode, MemOperand(scratch2));
  __ Addu(bytecode_size_table, bytecode_size_table,
          Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ jmp(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ Addu(bytecode_offset, bytecode_offset, Operand(1));
  __ Addu(scratch2, bytecode_array, bytecode_offset);
  __ lbu(bytecode, MemOperand(scratch2));
  __ Addu(bytecode_size_table, bytecode_size_table,
          Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));

  __ bind(&process_bytecode);

// Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)          \
  __ Branch(if_return, eq, bytecode, \
            Operand(static_cast<int>(interpreter::Bytecode::k##NAME)));
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // Otherwise, load the size of the current bytecode and advance the offset.
  __ Lsa(scratch2, bytecode_size_table, bytecode, 2);
  __ lw(scratch2, MemOperand(scratch2));
  __ Addu(bytecode_offset, bytecode_offset, scratch2);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o a1: the JS function object being called.
//   o a3: the incoming new target or generator object
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  Register closure = a1;
  Register feedback_vector = a2;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ lw(a0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ lw(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));
  GetSharedFunctionInfoBytecode(masm, kInterpreterBytecodeArrayRegister, t0);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ GetObjectType(kInterpreterBytecodeArrayRegister, a0, a0);
  __ Branch(&compile_lazy, ne, a0, Operand(BYTECODE_ARRAY_TYPE));

  // Load the feedback vector from the closure.
  __ lw(feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ lw(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ lw(t0, FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
  __ lhu(t0, FieldMemOperand(t0, Map::kInstanceTypeOffset));
  __ Branch(&push_stack_frame, ne, t0, Operand(FEEDBACK_VECTOR_TYPE));

  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, t0, t3, t1);

  // Increment invocation count for the function.
  __ lw(t0, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));
  __ Addu(t0, t0, Operand(1));
  __ sw(t0, FieldMemOperand(feedback_vector,
                            FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ bind(&push_stack_frame);
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  // Reset code age and the OSR arming. The OSR field and BytecodeAgeOffset are
  // 8-bit fields next to each other, so we could just optimize by writing a
  // 16-bit. These static asserts guard our assumption is valid.
  STATIC_ASSERT(BytecodeArray::kBytecodeAgeOffset ==
                BytecodeArray::kOsrNestingLevelOffset + kCharSize);
  STATIC_ASSERT(BytecodeArray::kNoAgeBytecodeAge == 0);
  __ sh(zero_reg, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kOsrNestingLevelOffset));

  // Load initial bytecode offset.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(t0, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, t0);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size from the BytecodeArray object.
    __ lw(t0, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ Subu(t1, sp, Operand(t0));
    LoadRealStackLimit(masm, a2);
    __ Branch(&stack_overflow, lo, t1, Operand(a2));

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(t1, RootIndex::kUndefinedValue);
    __ Branch(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(t1);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ Subu(t0, t0, Operand(kPointerSize));
    __ Branch(&loop_header, ge, t0, Operand(zero_reg));
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r3.
  Label no_incoming_new_target_or_generator_register;
  __ lw(t1, FieldMemOperand(
                kInterpreterBytecodeArrayRegister,
                BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Branch(&no_incoming_new_target_or_generator_register, eq, t1,
            Operand(zero_reg));
  __ Lsa(t1, fp, t1, kPointerSizeLog2);
  __ sw(a3, MemOperand(t1));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));
  __ Addu(a0, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(t3, MemOperand(a0));
  __ Lsa(kScratchReg, kInterpreterDispatchTableRegister, t3, kPointerSizeLog2);
  __ lw(kJavaScriptCallCodeStartRegister, MemOperand(kScratchReg));
  __ Call(kJavaScriptCallCodeStartRegister);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ lw(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);
  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                &do_return);
  __ jmp(&do_dispatch);

  __ bind(&do_return);
  // The return value is in v0.
  LeaveInterpreterFrame(masm, t0);
  __ Jump(ra);

  __ bind(&compile_lazy);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
  // Unreachable code.
  __ break_(0xCC);

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  // Unreachable code.
  __ break_(0xCC);
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register scratch, Register scratch2) {
  // Find the address of the last argument.
  __ mov(scratch2, num_args);
  __ sll(scratch2, scratch2, kPointerSizeLog2);
  __ Subu(scratch2, index, Operand(scratch2));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ lw(scratch, MemOperand(index));
  __ Addu(index, index, Operand(-kPointerSize));
  __ push(scratch);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, index, Operand(scratch2));
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  __ Addu(t0, a0, Operand(1));  // Add one for receiver.

  Generate_StackOverflowCheck(masm, t0, t4, t1, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
    __ mov(t0, a0);  // No receiver.
  }

  // This function modifies a2, t4 and t1.
  Generate_InterpreterPushArgs(masm, t0, a2, t4, t1);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(a2);                   // Pass the spread in a register
    __ Subu(a0, a0, Operand(1));  // Subtract one for spread
  }

  // Call the target.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- a0 : argument count (not including receiver)
  // -- a3 : new target
  // -- a1 : constructor to call
  // -- a2 : allocation site feedback if available, undefined otherwise.
  // -- t4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ push(zero_reg);

  Generate_StackOverflowCheck(masm, a0, t1, t0, &stack_overflow);

  // This function modified t4, t1 and t0.
  Generate_InterpreterPushArgs(masm, a0, t4, t1, t0);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(a2);                   // Pass the spread in a register
    __ Subu(a0, a0, Operand(1));  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(a2, t0);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(a1);

    // Tail call to the array construct stub (still in the caller
    // context at this point).
    __ Jump(BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ lw(t0, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ lw(t0, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kFunctionDataOffset));
  __ GetObjectType(t0, kInterpreterDispatchTableRegister,
                   kInterpreterDispatchTableRegister);
  __ Branch(&builtin_trampoline, ne, kInterpreterDispatchTableRegister,
            Operand(INTERPRETER_DATA_TYPE));

  __ lw(t0, FieldMemOperand(t0, InterpreterData::kInterpreterTrampolineOffset));
  __ Addu(t0, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Branch(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ li(t0, ExternalReference::
                address_of_interpreter_entry_trampoline_instruction_start(
                    masm->isolate()));
  __ lw(t0, MemOperand(t0));

  __ bind(&trampoline_loaded);
  __ Addu(ra, t0, Operand(interpreter_entry_return_pc_offset.value()));

  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ lw(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, kScratchReg);
    __ Assert(ne,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              kScratchReg, Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a1, a1);
    __ Assert(eq,
              AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry,
              a1, Operand(BYTECODE_ARRAY_TYPE));
  }

  // Get the target bytecode offset from the frame.
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(t3, MemOperand(a1));
  __ Lsa(a1, kInterpreterDispatchTableRegister, t3, kPointerSizeLog2);
  __ lw(kJavaScriptCallCodeStartRegister, MemOperand(a1));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ lw(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, a1, a2, a3,
                                &if_return);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(a2, kInterpreterBytecodeOffsetRegister);
  __ sw(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

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
  //  -- a0 : argument count (preserved for callee)
  //  -- a1 : new target (preserved for callee)
  //  -- a3 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(t4, a0);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ Branch(&over, ne, t4, Operand(j));
      }
      for (int i = j - 1; i >= 0; --i) {
        __ lw(t4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                     i * kPointerSize));
        __ push(t4);
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(RootIndex::kUndefinedValue);
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
    __ JumpIfSmi(v0, &failed);

    __ Drop(2);
    __ pop(t4);
    __ SmiUntag(t4);
    scope.GenerateLeaveFrame();

    __ Addu(t4, t4, Operand(1));
    __ Lsa(sp, sp, t4, kPointerSizeLog2);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ lw(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Addu(a2, a2, Code::kHeaderSize - kHeapObjectTag);
  __ Jump(a2);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ sw(v0,
          MemOperand(
              sp, config->num_allocatable_general_registers() * kPointerSize +
                      BuiltinContinuationFrameConstants::kFixedFrameSize));
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ Pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }
  __ lw(fp, MemOperand(
                sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  __ Pop(t0);
  __ Addu(sp, sp,
          Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(ra);
  __ LoadEntryFromBuiltinIndex(t0);
  __ Jump(t0);
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

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), v0.code());
  __ lw(v0, MemOperand(sp, 0 * kPointerSize));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Addu(sp, sp, Operand(1 * kPointerSize));  // Remove accumulator.
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  __ Ret(eq, v0, Operand(Smi::zero()));

  // Drop the handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  __ LeaveFrame(StackFrame::STUB);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ lw(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ lw(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
                               DeoptimizationData::kOsrPcOffsetIndex) -
                               kHeapObjectTag));
  __ SmiUntag(a1);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Addu(v0, v0, a1);
  __ addiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into a1, argArray into a0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a2, RootIndex::kUndefinedValue);
    __ mov(a3, a2);
    // Lsa() cannot be used hare as scratch value used later.
    __ sll(scratch, a0, kPointerSizeLog2);
    __ Addu(a0, sp, Operand(scratch));
    __ lw(a1, MemOperand(a0));  // receiver
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // thisArg
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // argArray
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ sw(a2, MemOperand(sp));
    __ mov(a2, a3);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argArray
  //  -- a1    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(a2, RootIndex::kNullValue, &no_arguments);
  __ JumpIfRoot(a2, RootIndex::kUndefinedValue, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(a0, zero_reg);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // a0: actual number of arguments
  {
    Label done;
    __ Branch(&done, ne, a0, Operand(zero_reg));
    __ PushRoot(RootIndex::kUndefinedValue);
    __ Addu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack.
  // a0: actual number of arguments
  __ Lsa(kScratchReg, sp, a0, kPointerSizeLog2);
  __ lw(a1, MemOperand(kScratchReg));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ Lsa(a2, sp, a0, kPointerSizeLog2);

    __ bind(&loop);
    __ lw(kScratchReg, MemOperand(a2, -kPointerSize));
    __ sw(kScratchReg, MemOperand(a2));
    __ Subu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Subu(a0, a0, Operand(1));
    __ Pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a1, RootIndex::kUndefinedValue);
    __ mov(a2, a1);
    __ mov(a3, a1);
    __ sll(scratch, a0, kPointerSizeLog2);
    __ mov(a0, scratch);
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(zero_reg));
    __ Addu(a0, sp, Operand(a0));
    __ lw(a1, MemOperand(a0));  // target
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // thisArgument
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // argumentsList
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ sw(a2, MemOperand(sp));
    __ mov(a2, a3);
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a1    : target
  //  -- sp[0] : thisArgument
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
  //  -- a0     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a1, RootIndex::kUndefinedValue);
    __ mov(a2, a1);
    // Lsa() cannot be used hare as scratch value used later.
    __ sll(scratch, a0, kPointerSizeLog2);
    __ Addu(a0, sp, Operand(scratch));
    __ sw(a2, MemOperand(a0));  // receiver
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a1, MemOperand(a0));  // target
    __ mov(a3, a1);             // new.target defaults to target
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // argumentsList
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // new.target
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
  }

  // ----------- S t a t e -------------
  //  -- a2    : argumentsList
  //  -- a3    : new.target
  //  -- a1    : target
  //  -- sp[0] : receiver (undefined)
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

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ sll(a0, a0, kSmiTagSize);
  __ li(t0, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | t0.bit() | fp.bit() | ra.bit());
  __ Push(Smi::zero());  // Padding.
  __ Addu(fp, sp,
          Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ lw(a1, MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - kSmiTagSize);
  // Adjust for the receiver.
  __ Addu(sp, sp, Operand(kPointerSize));
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- a1 : target
  //  -- a0 : number of parameters on the stack (not including the receiver)
  //  -- a2 : arguments list (a FixedArray)
  //  -- t0 : len (number of elements to push from args)
  //  -- a3 : new.target (for [[Construct]])
  // -----------------------------------
  if (masm->emit_debug_code()) {
    // Allow a2 to be a FixedArray, or a FixedDoubleArray if t0 == 0.
    Label ok, fail;
    __ AssertNotSmi(a2);
    __ GetObjectType(a2, t8, t8);
    __ Branch(&ok, eq, t8, Operand(FIXED_ARRAY_TYPE));
    __ Branch(&fail, ne, t8, Operand(FIXED_DOUBLE_ARRAY_TYPE));
    __ Branch(&ok, eq, t0, Operand(0));
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  // Check for stack overflow.
  Label stack_overflow;
  Generate_StackOverflowCheck(masm, t0, kScratchReg, t1, &stack_overflow);

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ mov(t2, zero_reg);
    Label done, push, loop;
    __ LoadRoot(t1, RootIndex::kTheHoleValue);
    __ bind(&loop);
    __ Branch(&done, eq, t2, Operand(t0));
    __ Lsa(kScratchReg, a2, t2, kPointerSizeLog2);
    __ lw(kScratchReg, FieldMemOperand(kScratchReg, FixedArray::kHeaderSize));
    __ Branch(&push, ne, t1, Operand(kScratchReg));
    __ LoadRoot(kScratchReg, RootIndex::kUndefinedValue);
    __ bind(&push);
    __ Push(kScratchReg);
    __ Addu(t2, t2, Operand(1));
    __ Branch(&loop);
    __ bind(&done);
    __ Addu(a0, a0, t2);
  }

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
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a3 : the new.target (for [[Construct]] calls)
  //  -- a1 : the target to call (can be any Object)
  //  -- a2 : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(a3, &new_target_not_constructor);
    __ lw(t1, FieldMemOperand(a3, HeapObject::kMapOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
    __ And(t1, t1, Operand(Map::IsConstructorBit::kMask));
    __ Branch(&new_target_constructor, ne, t1, Operand(zero_reg));
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(a3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ bind(&new_target_constructor);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ lw(t3, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(t2, MemOperand(t3, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&arguments_adaptor, eq, t2,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  {
    __ lw(t2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ lw(t2, FieldMemOperand(t2, JSFunction::kSharedFunctionInfoOffset));
    __ lhu(t2, FieldMemOperand(
                   t2, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(t3, fp);
  }
  __ Branch(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Just get the length from the ArgumentsAdaptorFrame.
    __ lw(t2, MemOperand(t3, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiUntag(t2);
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ Subu(t2, t2, a2);
  __ Branch(&stack_done, le, t2, Operand(zero_reg));
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, t2, t0, t1, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ Addu(a0, a0, t2);
      __ bind(&loop);
      {
        __ Lsa(kScratchReg, t3, t2, kPointerSizeLog2);
        __ lw(kScratchReg, MemOperand(kScratchReg, 1 * kPointerSize));
        __ push(kScratchReg);
        __ Subu(t2, t2, Operand(1));
        __ Branch(&loop, ne, t2, Operand(zero_reg));
      }
    }
  }
  __ Branch(&stack_done);
  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ bind(&stack_done);

  // Tail-call to the {code} handler.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(a1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3, FieldMemOperand(a2, SharedFunctionInfo::kFlagsOffset));
  __ And(kScratchReg, a3,
         Operand(SharedFunctionInfo::IsClassConstructorBit::kMask));
  __ Branch(&class_constructor, ne, kScratchReg, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ lw(a3, FieldMemOperand(a2, SharedFunctionInfo::kFlagsOffset));
  __ And(kScratchReg, a3,
         Operand(SharedFunctionInfo::IsNativeBit::kMask |
                 SharedFunctionInfo::IsStrictBit::kMask));
  __ Branch(&done_convert, ne, kScratchReg, Operand(zero_reg));
  {
    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments (not including the receiver)
    //  -- a1 : the function to call (checked to be a JSFunction)
    //  -- a2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(a3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Lsa(kScratchReg, sp, a0, kPointerSizeLog2);
      __ lw(a3, MemOperand(kScratchReg));
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, t0, t0);
      __ Branch(&done_convert, hs, t0, Operand(FIRST_JS_RECEIVER_TYPE));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(a3, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(a3, RootIndex::kNullValue, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(a3);
        }
        __ Branch(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ sll(a0, a0, kSmiTagSize);  // Smi tagged.
        __ Push(a0, a1);
        __ mov(a0, a3);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(a3, v0);
        __ Pop(a0, a1);
        __ sra(a0, a0, kSmiTagSize);  // Un-tag.
      }
      __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ Lsa(kScratchReg, sp, a0, kPointerSizeLog2);
    __ sw(a3, MemOperand(kScratchReg));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ lhu(a2,
         FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(a0);
  ParameterCount expected(a2);
  __ InvokeFunctionCode(a1, no_reg, expected, actual, JUMP_FUNCTION);

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Patch the receiver to [[BoundThis]].
  {
    __ lw(kScratchReg, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ Lsa(t0, sp, a0, kPointerSizeLog2);
    __ sw(kScratchReg, MemOperand(t0));
  }

  // Load [[BoundArguments]] into a2 and length of that into t0.
  __ lw(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(t0);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- t0 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ sll(t1, t0, kPointerSizeLog2);
    __ Subu(sp, sp, Operand(t1));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    LoadRealStackLimit(masm, kScratchReg);
    __ Branch(&done, hs, sp, Operand(kScratchReg));
    // Restore the stack pointer.
    __ Addu(sp, sp, Operand(t1));
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
    __ mov(t1, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, gt, t1, Operand(a0));
    __ Lsa(t2, sp, t0, kPointerSizeLog2);
    __ lw(kScratchReg, MemOperand(t2));
    __ Lsa(t2, sp, t1, kPointerSizeLog2);
    __ sw(kScratchReg, MemOperand(t2));
    __ Addu(t0, t0, Operand(1));
    __ Addu(t1, t1, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(t0);
    __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Subu(t0, t0, Operand(1));
    __ Branch(&done_loop, lt, t0, Operand(zero_reg));
    __ Lsa(t1, a2, t0, kPointerSizeLog2);
    __ lw(kScratchReg, MemOperand(t1));
    __ Lsa(t1, sp, a0, kPointerSizeLog2);
    __ sw(kScratchReg, MemOperand(t1));
    __ Addu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ lw(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_smi;
  __ JumpIfSmi(a1, &non_callable);
  __ bind(&non_smi);
  __ GetObjectType(a1, t1, t2);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Check if target has a [[Call]] internal method.
  __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(Map::IsCallableBit::kMask));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));

  // Check if target is a proxy and call CallProxy external builtin
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_PROXY_TYPE));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver with the (original) target.
  __ Lsa(kScratchReg, sp, a0, kPointerSizeLog2);
  __ sw(a1, MemOperand(kScratchReg));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, a1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(a1);
  __ AssertFunction(a1);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kFlagsOffset));
  __ And(t0, t0, Operand(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ Branch(&call_generic_stub, eq, t0, Operand(zero_reg));

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(a1);
  __ AssertBoundFunction(a1);

  // Load [[BoundArguments]] into a2 and length of that into t0.
  __ lw(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(t0);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a3 : the new target (checked to be a constructor)
  //  -- t0 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ sll(t1, t0, kPointerSizeLog2);
    __ Subu(sp, sp, Operand(t1));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    LoadRealStackLimit(masm, kScratchReg);
    __ Branch(&done, hs, sp, Operand(kScratchReg));
    // Restore the stack pointer.
    __ Addu(sp, sp, Operand(t1));
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
    __ mov(t1, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, ge, t1, Operand(a0));
    __ Lsa(t2, sp, t0, kPointerSizeLog2);
    __ lw(kScratchReg, MemOperand(t2));
    __ Lsa(t2, sp, t1, kPointerSizeLog2);
    __ sw(kScratchReg, MemOperand(t2));
    __ Addu(t0, t0, Operand(1));
    __ Addu(t1, t1, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(t0);
    __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Subu(t0, t0, Operand(1));
    __ Branch(&done_loop, lt, t0, Operand(zero_reg));
    __ Lsa(t1, a2, t0, kPointerSizeLog2);
    __ lw(kScratchReg, MemOperand(t1));
    __ Lsa(t1, sp, a0, kPointerSizeLog2);
    __ sw(kScratchReg, MemOperand(t1));
    __ Addu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label skip_load;
    __ Branch(&skip_load, ne, a1, Operand(a3));
    __ lw(a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&skip_load);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ lw(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(a1, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ lw(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t3, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t3, t3, Operand(Map::IsConstructorBit::kMask));
  __ Branch(&non_constructor, eq, t3, Operand(zero_reg));

  // Dispatch based on instance type.
  __ lhu(t2, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Only dispatch to proxies after checking whether they are constructors.
  __ Branch(&non_proxy, ne, t2, Operand(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ Lsa(kScratchReg, sp, a0, kPointerSizeLog2);
    __ sw(a1, MemOperand(kScratchReg));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, a1);
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
  // State setup as expected by MacroAssembler::InvokePrologue.
  // ----------- S t a t e -------------
  //  -- a0: actual arguments count
  //  -- a1: function (passed through to callee)
  //  -- a2: expected arguments count
  //  -- a3: new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ Branch(&dont_adapt_arguments, eq, a2,
            Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  // We use Uless as the number of argument should always be greater than 0.
  __ Branch(&too_few, Uless, a0, Operand(a2));

  {  // Enough parameters: actual >= expected.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, t1, kScratchReg, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into t1.
    __ Lsa(a0, fp, a0, kPointerSizeLog2 - kSmiTagSize);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ sll(t1, a2, kPointerSizeLog2);
    __ subu(t1, a0, t1);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // t1: copy end address

    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));
    __ push(t0);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t1));
    __ addiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, t1, kScratchReg, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into t3.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ Lsa(a0, fp, a0, kPointerSizeLog2 - kSmiTagSize);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Addu(t3, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // t3: copy end address
    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Subu(sp, sp, kPointerSize);
    __ Subu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t3));
    __ sw(t0, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ LoadRoot(t0, RootIndex::kUndefinedValue);
    __ sll(t2, a2, kPointerSizeLog2);
    __ Subu(t1, fp, Operand(t2));
    // Adjust for frame.
    __ Subu(t1, t1,
            Operand(ArgumentsAdaptorFrameConstants::kFixedFrameSizeFromFp +
                    kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Subu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(t1));
    __ sw(t0, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
  // a3 : new target (passed through to callee)
  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ lw(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Addu(a2, a2, Code::kHeaderSize - kHeapObjectTag);
  __ Call(a2);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
  __ lw(a2, FieldMemOperand(a1, JSFunction::kCodeOffset));
  __ Addu(a2, a2, Code::kHeaderSize - kHeapObjectTag);
  __ Jump(a2);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in t0 by the jump table trampoline.
  // Convert to Smi for the runtime call.
  __ SmiTag(kWasmCompileLazyFuncIndexRegister);
  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameScope scope(masm, StackFrame::WASM_COMPILE_LAZY);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    constexpr RegList gp_regs = Register::ListOf<a0, a1, a2, a3>();
    constexpr RegList fp_regs =
        DoubleRegister::ListOf<f2, f4, f6, f8, f10, f12, f14>();
    __ MultiPush(gp_regs);
    __ MultiPushFPU(fp_regs);

    // Pass instance and function index as an explicit arguments to the runtime
    // function.
    __ Push(kWasmInstanceRegister, kWasmCompileLazyFuncIndexRegister);
    // Load the correct CEntry builtin from the instance object.
    __ lw(a2, FieldMemOperand(kWasmInstanceRegister,
                              WasmInstanceObject::kIsolateRootOffset));
    auto centry_id =
        Builtins::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit;
    __ lw(a2, MemOperand(a2, IsolateData::builtin_slot_offset(centry_id)));
    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ Move(kContextRegister, Smi::zero());
    __ CallRuntimeWithCEntry(Runtime::kWasmCompileLazy, a2);

    // Restore registers.
    __ MultiPopFPU(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Finally, jump to the entrypoint.
  __ Jump(kScratchReg, v0, 0);
}

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                               bool builtin_exit_frame) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // a0: number of arguments including receiver
  // a1: pointer to builtin function
  // fp: frame pointer    (restored after C call)
  // sp: stack pointer    (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_mode == kArgvInRegister:
  // a2: pointer to the first argument

  if (argv_mode == kArgvInRegister) {
    // Move argv into the correct register.
    __ mov(s1, a2);
  } else {
    // Compute the argv pointer in a callee-saved register.
    __ Lsa(s1, sp, a0, kPointerSizeLog2);
    __ Subu(s1, s1, kPointerSize);
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      save_doubles == kSaveFPRegs, 0,
      builtin_exit_frame ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);

  // s0: number of arguments  including receiver (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  // Prepare arguments for C routine.
  // a0 = argc
  __ mov(s0, a0);
  __ mov(s2, a1);

  // We are calling compiled C/C++ code. a0 and a1 hold our two arguments. We
  // also need to reserve the 4 argument slots on the stack.

  __ AssertStackIsAligned();

  // a0 = argc, a1 = argv, a2 = isolate
  __ li(a2, ExternalReference::isolate_address(masm->isolate()));
  __ mov(a1, s1);

  __ StoreReturnAddressAndCall(s2);

  // Result returned in v0 or v1:v0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ LoadRoot(t0, RootIndex::kException);
  __ Branch(&exception_returned, eq, t0, Operand(v0));

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ li(a2, pending_exception_address);
    __ lw(a2, MemOperand(a2));
    __ LoadRoot(t0, RootIndex::kTheHoleValue);
    // Cannot use check here as it attempts to generate call into runtime.
    __ Branch(&okay, eq, t0, Operand(a2));
    __ stop();
    __ bind(&okay);
  }

  // Exit C frame and return.
  // v0:v1: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == kArgvInRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // s0: still holds argc (callee-saved).
                      : s0;
  __ LeaveExitFrame(save_doubles == kSaveFPRegs, argc, EMIT_RETURN);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set v0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, a0);
    __ mov(a0, zero_reg);
    __ mov(a1, zero_reg);
    __ li(a2, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ li(cp, pending_handler_context_address);
  __ lw(cp, MemOperand(cp));
  __ li(sp, pending_handler_sp_address);
  __ lw(sp, MemOperand(sp));
  __ li(fp, pending_handler_fp_address);
  __ lw(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label zero;
  __ Branch(&zero, eq, cp, Operand(zero_reg));
  __ sw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&zero);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_untrusted_code_mitigations} to make the snapshot work
  // with both configurations. It is safe to always do this, because the
  // underlying register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  __ li(t9, pending_handler_entrypoint_address);
  __ lw(t9, MemOperand(t9));
  __ Jump(t9);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label done;
  Register result_reg = t0;

  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch2 = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch3 = GetRegisterThatIsNotOneOf(result_reg, scratch, scratch2);
  DoubleRegister double_scratch = kScratchDoubleReg;

  // Account for saved regs.
  const int kArgumentOffset = 4 * kPointerSize;

  __ Push(result_reg);
  __ Push(scratch, scratch2, scratch3);

  // Load double input.
  __ Ldc1(double_scratch, MemOperand(sp, kArgumentOffset));

  // Clear cumulative exception flags and save the FCSR.
  __ cfc1(scratch2, FCSR);
  __ ctc1(zero_reg, FCSR);

  // Try a conversion to a signed integer.
  __ Trunc_w_d(double_scratch, double_scratch);
  // Move the converted value into the result register.
  __ mfc1(scratch3, double_scratch);

  // Retrieve and restore the FCSR.
  __ cfc1(scratch, FCSR);
  __ ctc1(scratch2, FCSR);

  // Check for overflow and NaNs.
  __ And(
      scratch, scratch,
      kFCSROverflowFlagMask | kFCSRUnderflowFlagMask | kFCSRInvalidOpFlagMask);
  // If we had no exceptions then set result_reg and we are done.
  Label error;
  __ Branch(&error, ne, scratch, Operand(zero_reg));
  __ Move(result_reg, scratch3);
  __ Branch(&done);
  __ bind(&error);

  // Load the double value and perform a manual truncation.
  Register input_high = scratch2;
  Register input_low = scratch3;

  __ lw(input_low, MemOperand(sp, kArgumentOffset + Register::kMantissaOffset));
  __ lw(input_high,
        MemOperand(sp, kArgumentOffset + Register::kExponentOffset));

  Label normal_exponent;
  // Extract the biased exponent in result.
  __ Ext(result_reg, input_high, HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  __ Subu(scratch, result_reg, HeapNumber::kExponentMask);
  __ Movz(result_reg, zero_reg, scratch);
  __ Branch(&done, eq, scratch, Operand(zero_reg));

  // Express exponent as delta to (number of mantissa bits + 31).
  __ Subu(result_reg, result_reg,
          Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31));

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  __ Branch(&normal_exponent, le, result_reg, Operand(zero_reg));
  __ mov(result_reg, zero_reg);
  __ Branch(&done);

  __ bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  __ Addu(scratch, result_reg, Operand(kShiftBase + HeapNumber::kMantissaBits));

  // Save the sign.
  Register sign = result_reg;
  result_reg = no_reg;
  __ And(sign, input_high, Operand(HeapNumber::kSignMask));

  // On ARM shifts > 31 bits are valid and will result in zero. On MIPS we need
  // to check for this specific case.
  Label high_shift_needed, high_shift_done;
  __ Branch(&high_shift_needed, lt, scratch, Operand(32));
  __ mov(input_high, zero_reg);
  __ Branch(&high_shift_done);
  __ bind(&high_shift_needed);

  // Set the implicit 1 before the mantissa part in input_high.
  __ Or(input_high, input_high,
        Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  __ sllv(input_high, input_high, scratch);

  __ bind(&high_shift_done);

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done;
  __ li(kScratchReg, 32);
  __ subu(scratch, kScratchReg, scratch);
  __ Branch(&pos_shift, ge, scratch, Operand(zero_reg));

  // Negate scratch.
  __ Subu(scratch, zero_reg, scratch);
  __ sllv(input_low, input_low, scratch);
  __ Branch(&shift_done);

  __ bind(&pos_shift);
  __ srlv(input_low, input_low, scratch);

  __ bind(&shift_done);
  __ Or(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  __ mov(scratch, sign);
  result_reg = sign;
  sign = no_reg;
  __ Subu(result_reg, zero_reg, input_high);
  __ Movz(result_reg, input_high, scratch);

  __ bind(&done);
  __ sw(result_reg, MemOperand(sp, kArgumentOffset));
  __ Pop(scratch, scratch2, scratch3);
  __ Pop(result_reg);
  __ Ret();
}

void Builtins::Generate_InternalArrayConstructorImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argc
  //  -- a1 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ lw(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ SmiTst(a3, kScratchReg);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction,
              kScratchReg, Operand(zero_reg));
    __ GetObjectType(a3, a3, t0);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction, t0,
              Operand(MAP_TYPE));

    // Figure out the right elements kind.
    __ lw(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));

    // Load the map's "bit field 2" into a3. We only need the first byte,
    // but the following bit field extraction takes care of that anyway.
    __ lbu(a3, FieldMemOperand(a3, Map::kBitField2Offset));
    // Retrieve elements_kind from bit field 2.
    __ DecodeField<Map::ElementsKindBits>(a3);

    // Initial elements kind should be packed elements.
    __ Assert(eq, AbortReason::kInvalidElementsKindForInternalPackedArray, a3,
              Operand(PACKED_ELEMENTS));

    // No arguments should be passed.
    __ Assert(eq, AbortReason::kWrongNumberOfArgumentsForInternalPackedArray,
              a0, Operand(0));
  }

  __ Jump(
      BUILTIN_CODE(masm->isolate(), InternalArrayNoArgumentConstructor_Packed),
      RelocInfo::CODE_TARGET);
}

namespace {

int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  stack_space
// - space to be unwound on exit (includes the call JS arguments space and
// the additional space allocated for the fast call).
void CallApiFunctionAndReturn(MacroAssembler* masm, Register function_address,
                              ExternalReference thunk_ref, int stack_space,
                              MemOperand* stack_space_operand,
                              MemOperand return_value_operand) {
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  DCHECK(function_address == a1 || function_address == a2);

  Label profiler_enabled, end_profiler_check;
  __ li(t9, ExternalReference::is_profiling_address(isolate));
  __ lb(t9, MemOperand(t9, 0));
  __ Branch(&profiler_enabled, ne, t9, Operand(zero_reg));
  __ li(t9, ExternalReference::address_of_runtime_stats_flag());
  __ lw(t9, MemOperand(t9, 0));
  __ Branch(&profiler_enabled, ne, t9, Operand(zero_reg));
  {
    // Call the api function directly.
    __ mov(t9, function_address);
    __ Branch(&end_profiler_check);
  }
  __ bind(&profiler_enabled);
  {
    // Additional parameter is the address of the actual callback.
    __ li(t9, thunk_ref);
  }
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  __ li(s5, next_address);
  __ lw(s0, MemOperand(s5, kNextOffset));
  __ lw(s1, MemOperand(s5, kLimitOffset));
  __ lw(s2, MemOperand(s5, kLevelOffset));
  __ Addu(s2, s2, Operand(1));
  __ sw(s2, MemOperand(s5, kLevelOffset));

  __ StoreReturnAddressAndCall(t9);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  __ lw(v0, return_value_operand);
  __ bind(&return_value_loaded);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ sw(s0, MemOperand(s5, kNextOffset));
  if (__ emit_debug_code()) {
    __ lw(a1, MemOperand(s5, kLevelOffset));
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall, a1,
             Operand(s2));
  }
  __ Subu(s2, s2, Operand(1));
  __ sw(s2, MemOperand(s5, kLevelOffset));
  __ lw(kScratchReg, MemOperand(s5, kLimitOffset));
  __ Branch(&delete_allocated_handles, ne, s1, Operand(kScratchReg));

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);

  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ li(s0, Operand(stack_space));
  } else {
    DCHECK_EQ(stack_space, 0);
    // The ExitFrame contains four MIPS argument slots after the call so this
    // must be accounted for.
    // TODO(jgruber): Investigate if this is needed by the direct call.
    __ Drop(kCArgSlotCount);
    __ lw(s0, *stack_space_operand);
  }

  static constexpr bool kDontSaveDoubles = false;
  static constexpr bool kRegisterContainsSlotCount = false;
  __ LeaveExitFrame(kDontSaveDoubles, s0, NO_EMIT_RETURN,
                    kRegisterContainsSlotCount);

  // Check if the function scheduled an exception.
  __ LoadRoot(t0, RootIndex::kTheHoleValue);
  __ li(kScratchReg, ExternalReference::scheduled_exception_address(isolate));
  __ lw(t1, MemOperand(kScratchReg));
  __ Branch(&promote_scheduled_exception, ne, t0, Operand(t1));

  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ sw(s1, MemOperand(s5, kLimitOffset));
  __ mov(s0, v0);
  __ mov(a0, v0);
  __ PrepareCallCFunction(1, s1);
  __ li(a0, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ mov(v0, s0);
  __ jmp(&leave_exit_frame);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- a1                  : api function address
  //  -- a2                  : arguments count (not including the receiver)
  //  -- a3                  : call data
  //  -- a0                  : holder
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 4]  : first argument
  //  -- sp[(argc + 0) * 4]  : receiver
  // -----------------------------------

  Register api_function_address = a1;
  Register argc = a2;
  Register call_data = a3;
  Register holder = a0;
  Register scratch = t0;
  Register base = t1;  // For addressing MemOperands on the stack.

  DCHECK(!AreAliased(api_function_address, argc, call_data,
                     holder, scratch, base));

  using FCA = FunctionCallbackArguments;

  STATIC_ASSERT(FCA::kArgsLength == 6);
  STATIC_ASSERT(FCA::kNewTargetIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  // Set up FunctionCallbackInfo's implicit_args on the stack as follows:
  //
  // Target state:
  //   sp[0 * kPointerSize]: kHolder
  //   sp[1 * kPointerSize]: kIsolate
  //   sp[2 * kPointerSize]: undefined (kReturnValueDefaultValue)
  //   sp[3 * kPointerSize]: undefined (kReturnValue)
  //   sp[4 * kPointerSize]: kData
  //   sp[5 * kPointerSize]: undefined (kNewTarget)

  // Set up the base register for addressing through MemOperands. It will point
  // at the receiver (located at sp + argc * kPointerSize).
  __ Lsa(base, sp, argc, kPointerSizeLog2);

  // Reserve space on the stack.
  __ Subu(sp, sp, Operand(FCA::kArgsLength * kPointerSize));

  // kHolder.
  __ sw(holder, MemOperand(sp, 0 * kPointerSize));

  // kIsolate.
  __ li(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ sw(scratch, MemOperand(sp, 1 * kPointerSize));

  // kReturnValueDefaultValue and kReturnValue.
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ sw(scratch, MemOperand(sp, 2 * kPointerSize));
  __ sw(scratch, MemOperand(sp, 3 * kPointerSize));

  // kData.
  __ sw(call_data, MemOperand(sp, 4 * kPointerSize));

  // kNewTarget.
  __ sw(scratch, MemOperand(sp, 5 * kPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ mov(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  static constexpr int kApiStackSpace = 4;
  static constexpr bool kDontSaveDoubles = false;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kDontSaveDoubles, kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  // Arguments are after the return address (pushed by EnterExitFrame()).
  __ sw(scratch, MemOperand(sp, 1 * kPointerSize));

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ Subu(scratch, base, Operand(1 * kPointerSize));
  __ sw(scratch, MemOperand(sp, 2 * kPointerSize));

  // FunctionCallbackInfo::length_.
  __ sw(argc, MemOperand(sp, 3 * kPointerSize));

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  // Note: Unlike on other architectures, this stores the number of slots to
  // drop, not the number of bytes.
  __ Addu(scratch, argc, Operand(FCA::kArgsLength + 1 /* receiver */));
  __ sw(scratch, MemOperand(sp, 4 * kPointerSize));

  // v8::InvocationCallback's argument.
  DCHECK(!AreAliased(api_function_address, scratch, a0));
  __ Addu(a0, sp, Operand(1 * kPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();

  // There are two stack slots above the arguments we constructed on the stack.
  // TODO(jgruber): Document what these arguments are.
  static constexpr int kStackSlotsAboveFCA = 2;
  MemOperand return_value_operand(
      fp, (kStackSlotsAboveFCA + FCA::kReturnValueOffset) * kPointerSize);

  static constexpr int kUseStackSpaceOperand = 0;
  MemOperand stack_space_operand(sp, 4 * kPointerSize);

  AllowExternalCallThatCantCauseGC scope(masm);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kUseStackSpaceOperand, &stack_space_operand,
                           return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  STATIC_ASSERT(PropertyCallbackArguments::kShouldThrowOnErrorIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 6);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 7);

  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = t0;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = a2;

  // Here and below +1 is for name() pushed after the args_ array.
  using PCA = PropertyCallbackArguments;
  __ Subu(sp, sp, (PCA::kArgsLength + 1) * kPointerSize);
  __ sw(receiver, MemOperand(sp, (PCA::kThisIndex + 1) * kPointerSize));
  __ lw(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ sw(scratch, MemOperand(sp, (PCA::kDataIndex + 1) * kPointerSize));
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ sw(scratch, MemOperand(sp, (PCA::kReturnValueOffset + 1) * kPointerSize));
  __ sw(scratch, MemOperand(sp, (PCA::kReturnValueDefaultValueIndex + 1) *
                                    kPointerSize));
  __ li(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ sw(scratch, MemOperand(sp, (PCA::kIsolateIndex + 1) * kPointerSize));
  __ sw(holder, MemOperand(sp, (PCA::kHolderIndex + 1) * kPointerSize));
  // should_throw_on_error -> false
  DCHECK_EQ(0, Smi::kZero.ptr());
  __ sw(zero_reg,
        MemOperand(sp, (PCA::kShouldThrowOnErrorIndex + 1) * kPointerSize));
  __ lw(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ sw(scratch, MemOperand(sp, 0 * kPointerSize));

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mov(a0, sp);                              // a0 = Handle<Name>
  __ Addu(a1, a0, Operand(1 * kPointerSize));  // a1 = v8::PCI::args_

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ sw(a1, MemOperand(sp, 1 * kPointerSize));
  __ Addu(a1, sp, Operand(1 * kPointerSize));  // a1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();

  __ lw(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ lw(api_function_address,
        FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  MemOperand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  // The sole purpose of DirectCEntry is for movable callers (e.g. any general
  // purpose Code object) to be able to call into C functions that may trigger
  // GC and thus move the caller.
  //
  // DirectCEntry places the return address on the stack (updated by the GC),
  // making the call GC safe. The irregexp backend relies on this.

  // Make place for arguments to fit C calling convention. Callers use
  // EnterExitFrame/LeaveExitFrame so they handle stack restoring and we don't
  // have to do that here. Any caller must drop kCArgsSlotsSize stack space
  // after the call.
  __ Subu(sp, sp, Operand(kCArgsSlotsSize));

  __ sw(ra, MemOperand(sp, kCArgsSlotsSize));  // Store the return address.
  __ Call(t9);                                 // Call the C++ function.
  __ lw(t9, MemOperand(sp, kCArgsSlotsSize));  // Return to calling code.

  if (FLAG_debug_code && FLAG_enable_slow_asserts) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC. Dereference the address and check for
    // this.
    __ lw(t0, MemOperand(t9));
    __ Assert(ne, AbortReason::kReceivedInvalidReturnAddress, t0,
              Operand(reinterpret_cast<uint32_t>(kZapValue)));
  }

  __ Jump(t9);
}

void Builtins::Generate_MemCopyUint8Uint8(MacroAssembler* masm) {
  // This code assumes that cache lines are 32 bytes and if the cache line is
  // larger it will not work correctly.
  {
    Label lastb, unaligned, aligned, chkw, loop16w, chk1w, wordCopy_loop,
        skip_pref, lastbloop, leave, ua_chk16w, ua_loop16w, ua_skip_pref,
        ua_chkw, ua_chk1w, ua_wordCopy_loop, ua_smallCopy, ua_smallCopy_loop;

    // The size of each prefetch.
    uint32_t pref_chunk = 32;
    // The maximum size of a prefetch, it must not be less than pref_chunk.
    // If the real size of a prefetch is greater than max_pref_size and
    // the kPrefHintPrepareForStore hint is used, the code will not work
    // correctly.
    uint32_t max_pref_size = 128;
    DCHECK(pref_chunk < max_pref_size);

    // pref_limit is set based on the fact that we never use an offset
    // greater then 5 on a store pref and that a single pref can
    // never be larger then max_pref_size.
    uint32_t pref_limit = (5 * pref_chunk) + max_pref_size;
    int32_t pref_hint_load = kPrefHintLoadStreamed;
    int32_t pref_hint_store = kPrefHintPrepareForStore;
    uint32_t loadstore_chunk = 4;

    // The initial prefetches may fetch bytes that are before the buffer being
    // copied. Start copies with an offset of 4 so avoid this situation when
    // using kPrefHintPrepareForStore.
    DCHECK(pref_hint_store != kPrefHintPrepareForStore ||
           pref_chunk * 4 >= max_pref_size);

    // If the size is less than 8, go to lastb. Regardless of size,
    // copy dst pointer to v0 for the retuen value.
    __ slti(t2, a2, 2 * loadstore_chunk);
    __ bne(t2, zero_reg, &lastb);
    __ mov(v0, a0);  // In delay slot.

    // If src and dst have different alignments, go to unaligned, if they
    // have the same alignment (but are not actually aligned) do a partial
    // load/store to make them aligned. If they are both already aligned
    // we can start copying at aligned.
    __ xor_(t8, a1, a0);
    __ andi(t8, t8, loadstore_chunk - 1);  // t8 is a0/a1 word-displacement.
    __ bne(t8, zero_reg, &unaligned);
    __ subu(a3, zero_reg, a0);  // In delay slot.

    __ andi(a3, a3, loadstore_chunk - 1);  // Copy a3 bytes to align a0/a1.
    __ beq(a3, zero_reg, &aligned);        // Already aligned.
    __ subu(a2, a2, a3);  // In delay slot. a2 is the remining bytes count.

    if (kArchEndian == kLittle) {
      __ lwr(t8, MemOperand(a1));
      __ addu(a1, a1, a3);
      __ swr(t8, MemOperand(a0));
      __ addu(a0, a0, a3);
    } else {
      __ lwl(t8, MemOperand(a1));
      __ addu(a1, a1, a3);
      __ swl(t8, MemOperand(a0));
      __ addu(a0, a0, a3);
    }
    // Now dst/src are both aligned to (word) aligned addresses. Set a2 to
    // count how many bytes we have to copy after all the 64 byte chunks are
    // copied and a3 to the dst pointer after all the 64 byte chunks have been
    // copied. We will loop, incrementing a0 and a1 until a0 equals a3.
    __ bind(&aligned);
    __ andi(t8, a2, 0x3F);
    __ beq(a2, t8, &chkw);  // Less than 64?
    __ subu(a3, a2, t8);    // In delay slot.
    __ addu(a3, a0, a3);    // Now a3 is the final dst after loop.

    // When in the loop we prefetch with kPrefHintPrepareForStore hint,
    // in this case the a0+x should be past the "t0-32" address. This means:
    // for x=128 the last "safe" a0 address is "t0-160". Alternatively, for
    // x=64 the last "safe" a0 address is "t0-96". In the current version we
    // will use "pref hint, 128(a0)", so "t0-160" is the limit.
    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ addu(t0, a0, a2);          // t0 is the "past the end" address.
      __ Subu(t9, t0, pref_limit);  // t9 is the "last safe pref" address.
    }

    __ Pref(pref_hint_load, MemOperand(a1, 0 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 1 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 2 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 3 * pref_chunk));

    if (pref_hint_store != kPrefHintPrepareForStore) {
      __ Pref(pref_hint_store, MemOperand(a0, 1 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 2 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 3 * pref_chunk));
    }
    __ bind(&loop16w);
    __ lw(t0, MemOperand(a1));

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ sltu(v1, t9, a0);  // If a0 > t9, don't use next prefetch.
      __ Branch(USE_DELAY_SLOT, &skip_pref, gt, v1, Operand(zero_reg));
    }
    __ lw(t1, MemOperand(a1, 1, loadstore_chunk));  // Maybe in delay slot.

    __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
    __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

    __ bind(&skip_pref);
    __ lw(t2, MemOperand(a1, 2, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 3, loadstore_chunk));
    __ lw(t4, MemOperand(a1, 4, loadstore_chunk));
    __ lw(t5, MemOperand(a1, 5, loadstore_chunk));
    __ lw(t6, MemOperand(a1, 6, loadstore_chunk));
    __ lw(t7, MemOperand(a1, 7, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));

    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));

    __ lw(t0, MemOperand(a1, 8, loadstore_chunk));
    __ lw(t1, MemOperand(a1, 9, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 10, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 11, loadstore_chunk));
    __ lw(t4, MemOperand(a1, 12, loadstore_chunk));
    __ lw(t5, MemOperand(a1, 13, loadstore_chunk));
    __ lw(t6, MemOperand(a1, 14, loadstore_chunk));
    __ lw(t7, MemOperand(a1, 15, loadstore_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));

    __ sw(t0, MemOperand(a0, 8, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 9, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 10, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 11, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 12, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 13, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 14, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 15, loadstore_chunk));
    __ addiu(a0, a0, 16 * loadstore_chunk);
    __ bne(a0, a3, &loop16w);
    __ addiu(a1, a1, 16 * loadstore_chunk);  // In delay slot.
    __ mov(a2, t8);

    // Here we have src and dest word-aligned but less than 64-bytes to go.
    // Check for a 32 bytes chunk and copy if there is one. Otherwise jump
    // down to chk1w to handle the tail end of the copy.
    __ bind(&chkw);
    __ Pref(pref_hint_load, MemOperand(a1, 0 * pref_chunk));
    __ andi(t8, a2, 0x1F);
    __ beq(a2, t8, &chk1w);  // Less than 32?
    __ nop();                // In delay slot.
    __ lw(t0, MemOperand(a1));
    __ lw(t1, MemOperand(a1, 1, loadstore_chunk));
    __ lw(t2, MemOperand(a1, 2, loadstore_chunk));
    __ lw(t3, MemOperand(a1, 3, loadstore_chunk));
    __ lw(t4, MemOperand(a1, 4, loadstore_chunk));
    __ lw(t5, MemOperand(a1, 5, loadstore_chunk));
    __ lw(t6, MemOperand(a1, 6, loadstore_chunk));
    __ lw(t7, MemOperand(a1, 7, loadstore_chunk));
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    __ addiu(a0, a0, 8 * loadstore_chunk);

    // Here we have less than 32 bytes to copy. Set up for a loop to copy
    // one word at a time. Set a2 to count how many bytes we have to copy
    // after all the word chunks are copied and a3 to the dst pointer after
    // all the word chunks have been copied. We will loop, incrementing a0
    // and a1 until a0 equals a3.
    __ bind(&chk1w);
    __ andi(a2, t8, loadstore_chunk - 1);
    __ beq(a2, t8, &lastb);
    __ subu(a3, t8, a2);  // In delay slot.
    __ addu(a3, a0, a3);

    __ bind(&wordCopy_loop);
    __ lw(t3, MemOperand(a1));
    __ addiu(a0, a0, loadstore_chunk);
    __ addiu(a1, a1, loadstore_chunk);
    __ bne(a0, a3, &wordCopy_loop);
    __ sw(t3, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

    __ bind(&lastb);
    __ Branch(&leave, le, a2, Operand(zero_reg));
    __ addu(a3, a0, a2);

    __ bind(&lastbloop);
    __ lb(v1, MemOperand(a1));
    __ addiu(a0, a0, 1);
    __ addiu(a1, a1, 1);
    __ bne(a0, a3, &lastbloop);
    __ sb(v1, MemOperand(a0, -1));  // In delay slot.

    __ bind(&leave);
    __ jr(ra);
    __ nop();

    // Unaligned case. Only the dst gets aligned so we need to do partial
    // loads of the source followed by normal stores to the dst (once we
    // have aligned the destination).
    __ bind(&unaligned);
    __ andi(a3, a3, loadstore_chunk - 1);  // Copy a3 bytes to align a0/a1.
    __ beq(a3, zero_reg, &ua_chk16w);
    __ subu(a2, a2, a3);  // In delay slot.

    if (kArchEndian == kLittle) {
      __ lwr(v1, MemOperand(a1));
      __ lwl(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ addu(a1, a1, a3);
      __ swr(v1, MemOperand(a0));
      __ addu(a0, a0, a3);
    } else {
      __ lwl(v1, MemOperand(a1));
      __ lwr(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ addu(a1, a1, a3);
      __ swl(v1, MemOperand(a0));
      __ addu(a0, a0, a3);
    }

    // Now the dst (but not the source) is aligned. Set a2 to count how many
    // bytes we have to copy after all the 64 byte chunks are copied and a3 to
    // the dst pointer after all the 64 byte chunks have been copied. We will
    // loop, incrementing a0 and a1 until a0 equals a3.
    __ bind(&ua_chk16w);
    __ andi(t8, a2, 0x3F);
    __ beq(a2, t8, &ua_chkw);
    __ subu(a3, a2, t8);  // In delay slot.
    __ addu(a3, a0, a3);

    if (pref_hint_store == kPrefHintPrepareForStore) {
      __ addu(t0, a0, a2);
      __ Subu(t9, t0, pref_limit);
    }

    __ Pref(pref_hint_load, MemOperand(a1, 0 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 1 * pref_chunk));
    __ Pref(pref_hint_load, MemOperand(a1, 2 * pref_chunk));

    if (pref_hint_store != kPrefHintPrepareForStore) {
      __ Pref(pref_hint_store, MemOperand(a0, 1 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 2 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 3 * pref_chunk));
    }

    __ bind(&ua_loop16w);
    __ Pref(pref_hint_load, MemOperand(a1, 3 * pref_chunk));
    if (kArchEndian == kLittle) {
      __ lwr(t0, MemOperand(a1));
      __ lwr(t1, MemOperand(a1, 1, loadstore_chunk));
      __ lwr(t2, MemOperand(a1, 2, loadstore_chunk));

      if (pref_hint_store == kPrefHintPrepareForStore) {
        __ sltu(v1, t9, a0);
        __ Branch(USE_DELAY_SLOT, &ua_skip_pref, gt, v1, Operand(zero_reg));
      }
      __ lwr(t3, MemOperand(a1, 3, loadstore_chunk));  // Maybe in delay slot.

      __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

      __ bind(&ua_skip_pref);
      __ lwr(t4, MemOperand(a1, 4, loadstore_chunk));
      __ lwr(t5, MemOperand(a1, 5, loadstore_chunk));
      __ lwr(t6, MemOperand(a1, 6, loadstore_chunk));
      __ lwr(t7, MemOperand(a1, 7, loadstore_chunk));
      __ lwl(t0,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t1,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t2,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t3,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t4,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t5,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t6,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t7,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    } else {
      __ lwl(t0, MemOperand(a1));
      __ lwl(t1, MemOperand(a1, 1, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 2, loadstore_chunk));

      if (pref_hint_store == kPrefHintPrepareForStore) {
        __ sltu(v1, t9, a0);
        __ Branch(USE_DELAY_SLOT, &ua_skip_pref, gt, v1, Operand(zero_reg));
      }
      __ lwl(t3, MemOperand(a1, 3, loadstore_chunk));  // Maybe in delay slot.

      __ Pref(pref_hint_store, MemOperand(a0, 4 * pref_chunk));
      __ Pref(pref_hint_store, MemOperand(a0, 5 * pref_chunk));

      __ bind(&ua_skip_pref);
      __ lwl(t4, MemOperand(a1, 4, loadstore_chunk));
      __ lwl(t5, MemOperand(a1, 5, loadstore_chunk));
      __ lwl(t6, MemOperand(a1, 6, loadstore_chunk));
      __ lwl(t7, MemOperand(a1, 7, loadstore_chunk));
      __ lwr(t0,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t4,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t5,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t6,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t7,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ Pref(pref_hint_load, MemOperand(a1, 4 * pref_chunk));
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    if (kArchEndian == kLittle) {
      __ lwr(t0, MemOperand(a1, 8, loadstore_chunk));
      __ lwr(t1, MemOperand(a1, 9, loadstore_chunk));
      __ lwr(t2, MemOperand(a1, 10, loadstore_chunk));
      __ lwr(t3, MemOperand(a1, 11, loadstore_chunk));
      __ lwr(t4, MemOperand(a1, 12, loadstore_chunk));
      __ lwr(t5, MemOperand(a1, 13, loadstore_chunk));
      __ lwr(t6, MemOperand(a1, 14, loadstore_chunk));
      __ lwr(t7, MemOperand(a1, 15, loadstore_chunk));
      __ lwl(t0,
             MemOperand(a1, 9, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t1,
             MemOperand(a1, 10, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t2,
             MemOperand(a1, 11, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t3,
             MemOperand(a1, 12, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t4,
             MemOperand(a1, 13, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t5,
             MemOperand(a1, 14, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t6,
             MemOperand(a1, 15, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t7,
             MemOperand(a1, 16, loadstore_chunk, MemOperand::offset_minus_one));
    } else {
      __ lwl(t0, MemOperand(a1, 8, loadstore_chunk));
      __ lwl(t1, MemOperand(a1, 9, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 10, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 11, loadstore_chunk));
      __ lwl(t4, MemOperand(a1, 12, loadstore_chunk));
      __ lwl(t5, MemOperand(a1, 13, loadstore_chunk));
      __ lwl(t6, MemOperand(a1, 14, loadstore_chunk));
      __ lwl(t7, MemOperand(a1, 15, loadstore_chunk));
      __ lwr(t0,
             MemOperand(a1, 9, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 10, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 11, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 12, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t4,
             MemOperand(a1, 13, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t5,
             MemOperand(a1, 14, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t6,
             MemOperand(a1, 15, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t7,
             MemOperand(a1, 16, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ Pref(pref_hint_load, MemOperand(a1, 5 * pref_chunk));
    __ sw(t0, MemOperand(a0, 8, loadstore_chunk));
    __ sw(t1, MemOperand(a0, 9, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 10, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 11, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 12, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 13, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 14, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 15, loadstore_chunk));
    __ addiu(a0, a0, 16 * loadstore_chunk);
    __ bne(a0, a3, &ua_loop16w);
    __ addiu(a1, a1, 16 * loadstore_chunk);  // In delay slot.
    __ mov(a2, t8);

    // Here less than 64-bytes. Check for
    // a 32 byte chunk and copy if there is one. Otherwise jump down to
    // ua_chk1w to handle the tail end of the copy.
    __ bind(&ua_chkw);
    __ Pref(pref_hint_load, MemOperand(a1));
    __ andi(t8, a2, 0x1F);

    __ beq(a2, t8, &ua_chk1w);
    __ nop();  // In delay slot.
    if (kArchEndian == kLittle) {
      __ lwr(t0, MemOperand(a1));
      __ lwr(t1, MemOperand(a1, 1, loadstore_chunk));
      __ lwr(t2, MemOperand(a1, 2, loadstore_chunk));
      __ lwr(t3, MemOperand(a1, 3, loadstore_chunk));
      __ lwr(t4, MemOperand(a1, 4, loadstore_chunk));
      __ lwr(t5, MemOperand(a1, 5, loadstore_chunk));
      __ lwr(t6, MemOperand(a1, 6, loadstore_chunk));
      __ lwr(t7, MemOperand(a1, 7, loadstore_chunk));
      __ lwl(t0,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t1,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t2,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t3,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t4,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t5,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t6,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwl(t7,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    } else {
      __ lwl(t0, MemOperand(a1));
      __ lwl(t1, MemOperand(a1, 1, loadstore_chunk));
      __ lwl(t2, MemOperand(a1, 2, loadstore_chunk));
      __ lwl(t3, MemOperand(a1, 3, loadstore_chunk));
      __ lwl(t4, MemOperand(a1, 4, loadstore_chunk));
      __ lwl(t5, MemOperand(a1, 5, loadstore_chunk));
      __ lwl(t6, MemOperand(a1, 6, loadstore_chunk));
      __ lwl(t7, MemOperand(a1, 7, loadstore_chunk));
      __ lwr(t0,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t1,
             MemOperand(a1, 2, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t2,
             MemOperand(a1, 3, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t3,
             MemOperand(a1, 4, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t4,
             MemOperand(a1, 5, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t5,
             MemOperand(a1, 6, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t6,
             MemOperand(a1, 7, loadstore_chunk, MemOperand::offset_minus_one));
      __ lwr(t7,
             MemOperand(a1, 8, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ addiu(a1, a1, 8 * loadstore_chunk);
    __ sw(t0, MemOperand(a0));
    __ sw(t1, MemOperand(a0, 1, loadstore_chunk));
    __ sw(t2, MemOperand(a0, 2, loadstore_chunk));
    __ sw(t3, MemOperand(a0, 3, loadstore_chunk));
    __ sw(t4, MemOperand(a0, 4, loadstore_chunk));
    __ sw(t5, MemOperand(a0, 5, loadstore_chunk));
    __ sw(t6, MemOperand(a0, 6, loadstore_chunk));
    __ sw(t7, MemOperand(a0, 7, loadstore_chunk));
    __ addiu(a0, a0, 8 * loadstore_chunk);

    // Less than 32 bytes to copy. Set up for a loop to
    // copy one word at a time.
    __ bind(&ua_chk1w);
    __ andi(a2, t8, loadstore_chunk - 1);
    __ beq(a2, t8, &ua_smallCopy);
    __ subu(a3, t8, a2);  // In delay slot.
    __ addu(a3, a0, a3);

    __ bind(&ua_wordCopy_loop);
    if (kArchEndian == kLittle) {
      __ lwr(v1, MemOperand(a1));
      __ lwl(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    } else {
      __ lwl(v1, MemOperand(a1));
      __ lwr(v1,
             MemOperand(a1, 1, loadstore_chunk, MemOperand::offset_minus_one));
    }
    __ addiu(a0, a0, loadstore_chunk);
    __ addiu(a1, a1, loadstore_chunk);
    __ bne(a0, a3, &ua_wordCopy_loop);
    __ sw(v1, MemOperand(a0, -1, loadstore_chunk));  // In delay slot.

    // Copy the last 8 bytes.
    __ bind(&ua_smallCopy);
    __ beq(a2, zero_reg, &leave);
    __ addu(a3, a0, a2);  // In delay slot.

    __ bind(&ua_smallCopy_loop);
    __ lb(v1, MemOperand(a1));
    __ addiu(a0, a0, 1);
    __ addiu(a1, a1, 1);
    __ bne(a0, a3, &ua_smallCopy_loop);
    __ sb(v1, MemOperand(a0, -1));  // In delay slot.

    __ jr(ra);
    __ nop();
  }
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS
