// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution.h"

#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/runtime-profiler.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

StackGuard::StackGuard()
    : isolate_(NULL) {
}


void StackGuard::set_interrupt_limits(const ExecutionAccess& lock) {
  DCHECK(isolate_ != NULL);
  thread_local_.set_jslimit(kInterruptLimit);
  thread_local_.set_climit(kInterruptLimit);
  isolate_->heap()->SetStackLimits();
}


void StackGuard::reset_limits(const ExecutionAccess& lock) {
  DCHECK(isolate_ != NULL);
  thread_local_.set_jslimit(thread_local_.real_jslimit_);
  thread_local_.set_climit(thread_local_.real_climit_);
  isolate_->heap()->SetStackLimits();
}


static void PrintDeserializedCodeInfo(Handle<JSFunction> function) {
  if (function->code() == function->shared()->code() &&
      function->shared()->deserialized()) {
    PrintF("[Running deserialized script");
    Object* script = function->shared()->script();
    if (script->IsScript()) {
      Object* name = Script::cast(script)->name();
      if (name->IsString()) {
        PrintF(": %s", String::cast(name)->ToCString().get());
      }
    }
    PrintF("]\n");
  }
}


namespace {

MUST_USE_RESULT MaybeHandle<Object> Invoke(
    Isolate* isolate, bool is_construct, Handle<Object> target,
    Handle<Object> receiver, int argc, Handle<Object> args[],
    Handle<Object> new_target, Execution::MessageHandling message_handling) {
  DCHECK(!receiver->IsJSGlobalObject());

#ifdef USE_SIMULATOR
  // Simulators use separate stacks for C++ and JS. JS stack overflow checks
  // are performed whenever a JS function is called. However, it can be the case
  // that the C++ stack grows faster than the JS stack, resulting in an overflow
  // there. Add a check here to make that less likely.
  StackLimitCheck check(isolate);
  if (check.HasOverflowed()) {
    isolate->StackOverflow();
    if (message_handling == Execution::MessageHandling::kReport) {
      isolate->ReportPendingMessages();
    }
    return MaybeHandle<Object>();
  }
#endif

  // api callbacks can be called directly.
  if (target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(target);
    if ((!is_construct || function->IsConstructor()) &&
        function->shared()->IsApiFunction()) {
      SaveContext save(isolate);
      isolate->set_context(function->context());
      DCHECK(function->context()->global_object()->IsJSGlobalObject());
      if (is_construct) receiver = isolate->factory()->the_hole_value();
      auto value = Builtins::InvokeApiFunction(
          isolate, is_construct, function, receiver, argc, args,
          Handle<HeapObject>::cast(new_target));
      bool has_exception = value.is_null();
      DCHECK(has_exception == isolate->has_pending_exception());
      if (has_exception) {
        if (message_handling == Execution::MessageHandling::kReport) {
          isolate->ReportPendingMessages();
        }
        return MaybeHandle<Object>();
      } else {
        isolate->clear_pending_message();
      }
      return value;
    }
  }

  // Entering JavaScript.
  VMState<JS> state(isolate);
  CHECK(AllowJavascriptExecution::IsAllowed(isolate));
  if (!ThrowOnJavascriptExecution::IsAllowed(isolate)) {
    isolate->ThrowIllegalOperation();
    if (message_handling == Execution::MessageHandling::kReport) {
      isolate->ReportPendingMessages();
    }
    return MaybeHandle<Object>();
  }

  // Placeholder for return value.
  Object* value = NULL;

  typedef Object* (*JSEntryFunction)(Object* new_target, Object* target,
                                     Object* receiver, int argc,
                                     Object*** args);

  Handle<Code> code = is_construct
      ? isolate->factory()->js_construct_entry_code()
      : isolate->factory()->js_entry_code();

  {
    // Save and restore context around invocation and block the
    // allocation of handles without explicit handle scopes.
    SaveContext save(isolate);
    SealHandleScope shs(isolate);
    JSEntryFunction stub_entry = FUNCTION_CAST<JSEntryFunction>(code->entry());

    if (FLAG_clear_exceptions_on_js_entry) isolate->clear_pending_exception();

    // Call the function through the right JS entry stub.
    Object* orig_func = *new_target;
    Object* func = *target;
    Object* recv = *receiver;
    Object*** argv = reinterpret_cast<Object***>(args);
    if (FLAG_profile_deserialization && target->IsJSFunction()) {
      PrintDeserializedCodeInfo(Handle<JSFunction>::cast(target));
    }
    RuntimeCallTimerScope timer(isolate, &RuntimeCallStats::JS_Execution);
    value = CALL_GENERATED_CODE(isolate, stub_entry, orig_func, func, recv,
                                argc, argv);
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    value->ObjectVerify();
  }
#endif

  // Update the pending exception flag and return the value.
  bool has_exception = value->IsException(isolate);
  DCHECK(has_exception == isolate->has_pending_exception());
  if (has_exception) {
    if (message_handling == Execution::MessageHandling::kReport) {
      isolate->ReportPendingMessages();
    }
    return MaybeHandle<Object>();
  } else {
    isolate->clear_pending_message();
  }

  return Handle<Object>(value, isolate);
}

MaybeHandle<Object> CallInternal(Isolate* isolate, Handle<Object> callable,
                                 Handle<Object> receiver, int argc,
                                 Handle<Object> argv[],
                                 Execution::MessageHandling message_handling) {
  // Convert calls on global objects to be calls on the global
  // receiver instead to avoid having a 'this' pointer which refers
  // directly to a global object.
  if (receiver->IsJSGlobalObject()) {
    receiver =
        handle(Handle<JSGlobalObject>::cast(receiver)->global_proxy(), isolate);
  }
  return Invoke(isolate, false, callable, receiver, argc, argv,
                isolate->factory()->undefined_value(), message_handling);
}

}  // namespace

// static
MaybeHandle<Object> Execution::Call(Isolate* isolate, Handle<Object> callable,
                                    Handle<Object> receiver, int argc,
                                    Handle<Object> argv[]) {
  return CallInternal(isolate, callable, receiver, argc, argv,
                      MessageHandling::kReport);
}


// static
MaybeHandle<Object> Execution::New(Handle<JSFunction> constructor, int argc,
                                   Handle<Object> argv[]) {
  return New(constructor->GetIsolate(), constructor, constructor, argc, argv);
}


// static
MaybeHandle<Object> Execution::New(Isolate* isolate, Handle<Object> constructor,
                                   Handle<Object> new_target, int argc,
                                   Handle<Object> argv[]) {
  return Invoke(isolate, true, constructor,
                isolate->factory()->undefined_value(), argc, argv, new_target,
                MessageHandling::kReport);
}

MaybeHandle<Object> Execution::TryCall(Isolate* isolate,
                                       Handle<Object> callable,
                                       Handle<Object> receiver, int argc,
                                       Handle<Object> args[],
                                       MessageHandling message_handling,
                                       MaybeHandle<Object>* exception_out) {
  bool is_termination = false;
  MaybeHandle<Object> maybe_result;
  if (exception_out != NULL) *exception_out = MaybeHandle<Object>();
  DCHECK_IMPLIES(message_handling == MessageHandling::kKeepPending,
                 exception_out == nullptr);
  // Enter a try-block while executing the JavaScript code. To avoid
  // duplicate error printing it must be non-verbose.  Also, to avoid
  // creating message objects during stack overflow we shouldn't
  // capture messages.
  {
    v8::TryCatch catcher(reinterpret_cast<v8::Isolate*>(isolate));
    catcher.SetVerbose(false);
    catcher.SetCaptureMessage(false);

    maybe_result =
        CallInternal(isolate, callable, receiver, argc, args, message_handling);

    if (maybe_result.is_null()) {
      DCHECK(isolate->has_pending_exception());
      if (isolate->pending_exception() ==
          isolate->heap()->termination_exception()) {
        is_termination = true;
      } else {
        if (exception_out != nullptr) {
          DCHECK(catcher.HasCaught());
          DCHECK(isolate->external_caught_exception());
          *exception_out = v8::Utils::OpenHandle(*catcher.Exception());
        }
      }
      if (message_handling == MessageHandling::kReport) {
        isolate->OptionalRescheduleException(true);
      }
    }
  }

  // Re-request terminate execution interrupt to trigger later.
  if (is_termination) isolate->stack_guard()->RequestTerminateExecution();

  return maybe_result;
}


void StackGuard::SetStackLimit(uintptr_t limit) {
  ExecutionAccess access(isolate_);
  // If the current limits are special (e.g. due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(isolate_, limit);
  if (thread_local_.jslimit() == thread_local_.real_jslimit_) {
    thread_local_.set_jslimit(jslimit);
  }
  if (thread_local_.climit() == thread_local_.real_climit_) {
    thread_local_.set_climit(limit);
  }
  thread_local_.real_climit_ = limit;
  thread_local_.real_jslimit_ = jslimit;
}


void StackGuard::AdjustStackLimitForSimulator() {
  ExecutionAccess access(isolate_);
  uintptr_t climit = thread_local_.real_climit_;
  // If the current limits are special (e.g. due to a pending interrupt) then
  // leave them alone.
  uintptr_t jslimit = SimulatorStack::JsLimitFromCLimit(isolate_, climit);
  if (thread_local_.jslimit() == thread_local_.real_jslimit_) {
    thread_local_.set_jslimit(jslimit);
    isolate_->heap()->SetStackLimits();
  }
}


void StackGuard::EnableInterrupts() {
  ExecutionAccess access(isolate_);
  if (has_pending_interrupts(access)) {
    set_interrupt_limits(access);
  }
}


void StackGuard::DisableInterrupts() {
  ExecutionAccess access(isolate_);
  reset_limits(access);
}


void StackGuard::PushPostponeInterruptsScope(PostponeInterruptsScope* scope) {
  ExecutionAccess access(isolate_);
  // Intercept already requested interrupts.
  int intercepted = thread_local_.interrupt_flags_ & scope->intercept_mask_;
  scope->intercepted_flags_ = intercepted;
  thread_local_.interrupt_flags_ &= ~intercepted;
  if (!has_pending_interrupts(access)) reset_limits(access);
  // Add scope to the chain.
  scope->prev_ = thread_local_.postpone_interrupts_;
  thread_local_.postpone_interrupts_ = scope;
}


void StackGuard::PopPostponeInterruptsScope() {
  ExecutionAccess access(isolate_);
  PostponeInterruptsScope* top = thread_local_.postpone_interrupts_;
  // Make intercepted interrupts active.
  DCHECK((thread_local_.interrupt_flags_ & top->intercept_mask_) == 0);
  thread_local_.interrupt_flags_ |= top->intercepted_flags_;
  if (has_pending_interrupts(access)) set_interrupt_limits(access);
  // Remove scope from chain.
  thread_local_.postpone_interrupts_ = top->prev_;
}


bool StackGuard::CheckInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  return thread_local_.interrupt_flags_ & flag;
}


void StackGuard::RequestInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Check the chain of PostponeInterruptsScopes for interception.
  if (thread_local_.postpone_interrupts_ &&
      thread_local_.postpone_interrupts_->Intercept(flag)) {
    return;
  }

  // Not intercepted.  Set as active interrupt flag.
  thread_local_.interrupt_flags_ |= flag;
  set_interrupt_limits(access);

  // If this isolate is waiting in a futex, notify it to wake up.
  isolate_->futex_wait_list_node()->NotifyWake();
}


void StackGuard::ClearInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Clear the interrupt flag from the chain of PostponeInterruptsScopes.
  for (PostponeInterruptsScope* current = thread_local_.postpone_interrupts_;
       current != NULL;
       current = current->prev_) {
    current->intercepted_flags_ &= ~flag;
  }

  // Clear the interrupt flag from the active interrupt flags.
  thread_local_.interrupt_flags_ &= ~flag;
  if (!has_pending_interrupts(access)) reset_limits(access);
}


bool StackGuard::CheckAndClearInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  bool result = (thread_local_.interrupt_flags_ & flag);
  thread_local_.interrupt_flags_ &= ~flag;
  if (!has_pending_interrupts(access)) reset_limits(access);
  return result;
}


char* StackGuard::ArchiveStackGuard(char* to) {
  ExecutionAccess access(isolate_);
  MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  ThreadLocal blank;

  // Set the stack limits using the old thread_local_.
  // TODO(isolates): This was the old semantics of constructing a ThreadLocal
  //                 (as the ctor called SetStackLimits, which looked at the
  //                 current thread_local_ from StackGuard)-- but is this
  //                 really what was intended?
  isolate_->heap()->SetStackLimits();
  thread_local_ = blank;

  return to + sizeof(ThreadLocal);
}


char* StackGuard::RestoreStackGuard(char* from) {
  ExecutionAccess access(isolate_);
  MemCopy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  isolate_->heap()->SetStackLimits();
  return from + sizeof(ThreadLocal);
}


void StackGuard::FreeThreadResources() {
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  per_thread->set_stack_limit(thread_local_.real_climit_);
}


void StackGuard::ThreadLocal::Clear() {
  real_jslimit_ = kIllegalLimit;
  set_jslimit(kIllegalLimit);
  real_climit_ = kIllegalLimit;
  set_climit(kIllegalLimit);
  postpone_interrupts_ = NULL;
  interrupt_flags_ = 0;
}


bool StackGuard::ThreadLocal::Initialize(Isolate* isolate) {
  bool should_set_stack_limits = false;
  if (real_climit_ == kIllegalLimit) {
    const uintptr_t kLimitSize = FLAG_stack_size * KB;
    DCHECK(GetCurrentStackPosition() > kLimitSize);
    uintptr_t limit = GetCurrentStackPosition() - kLimitSize;
    real_jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
    set_jslimit(SimulatorStack::JsLimitFromCLimit(isolate, limit));
    real_climit_ = limit;
    set_climit(limit);
    should_set_stack_limits = true;
  }
  postpone_interrupts_ = NULL;
  interrupt_flags_ = 0;
  return should_set_stack_limits;
}


void StackGuard::ClearThread(const ExecutionAccess& lock) {
  thread_local_.Clear();
  isolate_->heap()->SetStackLimits();
}


void StackGuard::InitThread(const ExecutionAccess& lock) {
  if (thread_local_.Initialize(isolate_)) isolate_->heap()->SetStackLimits();
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  uintptr_t stored_limit = per_thread->stack_limit();
  // You should hold the ExecutionAccess lock when you call this.
  if (stored_limit != 0) {
    SetStackLimit(stored_limit);
  }
}


// --- C a l l s   t o   n a t i v e s ---


void StackGuard::HandleGCInterrupt() {
  if (CheckAndClearInterrupt(GC_REQUEST)) {
    isolate_->heap()->HandleGCRequest();
  }
}


Object* StackGuard::HandleInterrupts() {
  if (FLAG_verify_predictable) {
    // Advance synthetic time by making a time request.
    isolate_->heap()->MonotonicallyIncreasingTimeInMs();
  }

  if (CheckAndClearInterrupt(GC_REQUEST)) {
    isolate_->heap()->HandleGCRequest();
  }

  if (CheckDebugBreak() || CheckDebugCommand()) {
    isolate_->debug()->HandleDebugBreak();
  }

  if (CheckAndClearInterrupt(TERMINATE_EXECUTION)) {
    return isolate_->TerminateExecution();
  }

  if (CheckAndClearInterrupt(DEOPT_MARKED_ALLOCATION_SITES)) {
    isolate_->heap()->DeoptMarkedAllocationSites();
  }

  if (CheckAndClearInterrupt(INSTALL_CODE)) {
    DCHECK(isolate_->concurrent_recompilation_enabled());
    isolate_->optimizing_compile_dispatcher()->InstallOptimizedFunctions();
  }

  if (CheckAndClearInterrupt(API_INTERRUPT)) {
    // Callbacks must be invoked outside of ExecusionAccess lock.
    isolate_->InvokeApiInterruptCallbacks();
  }

  isolate_->counters()->stack_interrupts()->Increment();
  isolate_->counters()->runtime_profiler_ticks()->Increment();
  isolate_->runtime_profiler()->MarkCandidatesForOptimization();

  return isolate_->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
