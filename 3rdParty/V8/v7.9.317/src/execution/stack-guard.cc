// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/stack-guard.h"

#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/execution/interrupts-scope.h"
#include "src/execution/isolate.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/simulator.h"
#include "src/logging/counters.h"
#include "src/objects/backing-store.h"
#include "src/roots/roots-inl.h"
#include "src/utils/memcopy.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {

void StackGuard::set_interrupt_limits(const ExecutionAccess& lock) {
  DCHECK_NOT_NULL(isolate_);
  thread_local_.set_jslimit(kInterruptLimit);
  thread_local_.set_climit(kInterruptLimit);
}

void StackGuard::reset_limits(const ExecutionAccess& lock) {
  DCHECK_NOT_NULL(isolate_);
  thread_local_.set_jslimit(thread_local_.real_jslimit_);
  thread_local_.set_climit(thread_local_.real_climit_);
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

void StackGuard::PushInterruptsScope(InterruptsScope* scope) {
  ExecutionAccess access(isolate_);
  DCHECK_NE(scope->mode_, InterruptsScope::kNoop);
  if (scope->mode_ == InterruptsScope::kPostponeInterrupts) {
    // Intercept already requested interrupts.
    intptr_t intercepted =
        thread_local_.interrupt_flags_ & scope->intercept_mask_;
    scope->intercepted_flags_ = intercepted;
    thread_local_.interrupt_flags_ &= ~intercepted;
  } else {
    DCHECK_EQ(scope->mode_, InterruptsScope::kRunInterrupts);
    // Restore postponed interrupts.
    int restored_flags = 0;
    for (InterruptsScope* current = thread_local_.interrupt_scopes_;
         current != nullptr; current = current->prev_) {
      restored_flags |= (current->intercepted_flags_ & scope->intercept_mask_);
      current->intercepted_flags_ &= ~scope->intercept_mask_;
    }
    thread_local_.interrupt_flags_ |= restored_flags;

    if (has_pending_interrupts(access)) set_interrupt_limits(access);
  }
  if (!has_pending_interrupts(access)) reset_limits(access);
  // Add scope to the chain.
  scope->prev_ = thread_local_.interrupt_scopes_;
  thread_local_.interrupt_scopes_ = scope;
}

void StackGuard::PopInterruptsScope() {
  ExecutionAccess access(isolate_);
  InterruptsScope* top = thread_local_.interrupt_scopes_;
  DCHECK_NE(top->mode_, InterruptsScope::kNoop);
  if (top->mode_ == InterruptsScope::kPostponeInterrupts) {
    // Make intercepted interrupts active.
    DCHECK_EQ(thread_local_.interrupt_flags_ & top->intercept_mask_, 0);
    thread_local_.interrupt_flags_ |= top->intercepted_flags_;
  } else {
    DCHECK_EQ(top->mode_, InterruptsScope::kRunInterrupts);
    // Postpone existing interupts if needed.
    if (top->prev_) {
      for (int interrupt = 1; interrupt < ALL_INTERRUPTS;
           interrupt = interrupt << 1) {
        InterruptFlag flag = static_cast<InterruptFlag>(interrupt);
        if ((thread_local_.interrupt_flags_ & flag) &&
            top->prev_->Intercept(flag)) {
          thread_local_.interrupt_flags_ &= ~flag;
        }
      }
    }
  }
  if (has_pending_interrupts(access)) set_interrupt_limits(access);
  // Remove scope from chain.
  thread_local_.interrupt_scopes_ = top->prev_;
}

bool StackGuard::CheckInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  return (thread_local_.interrupt_flags_ & flag) != 0;
}

void StackGuard::RequestInterrupt(InterruptFlag flag) {
  ExecutionAccess access(isolate_);
  // Check the chain of InterruptsScope for interception.
  if (thread_local_.interrupt_scopes_ &&
      thread_local_.interrupt_scopes_->Intercept(flag)) {
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
  // Clear the interrupt flag from the chain of InterruptsScope.
  for (InterruptsScope* current = thread_local_.interrupt_scopes_;
       current != nullptr; current = current->prev_) {
    current->intercepted_flags_ &= ~flag;
  }

  // Clear the interrupt flag from the active interrupt flags.
  thread_local_.interrupt_flags_ &= ~flag;
  if (!has_pending_interrupts(access)) reset_limits(access);
}

int StackGuard::FetchAndClearInterrupts() {
  ExecutionAccess access(isolate_);

  int result = 0;
  if ((thread_local_.interrupt_flags_ & TERMINATE_EXECUTION) != 0) {
    // The TERMINATE_EXECUTION interrupt is special, since it terminates
    // execution but should leave V8 in a resumable state. If it exists, we only
    // fetch and clear that bit. On resume, V8 can continue processing other
    // interrupts.
    result = TERMINATE_EXECUTION;
    thread_local_.interrupt_flags_ &= ~TERMINATE_EXECUTION;
    if (!has_pending_interrupts(access)) reset_limits(access);
  } else {
    result = static_cast<int>(thread_local_.interrupt_flags_);
    thread_local_.interrupt_flags_ = 0;
    reset_limits(access);
  }

  return result;
}

char* StackGuard::ArchiveStackGuard(char* to) {
  ExecutionAccess access(isolate_);
  MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  thread_local_ = {};
  return to + sizeof(ThreadLocal);
}

char* StackGuard::RestoreStackGuard(char* from) {
  ExecutionAccess access(isolate_);
  MemCopy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  return from + sizeof(ThreadLocal);
}

void StackGuard::FreeThreadResources() {
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  per_thread->set_stack_limit(thread_local_.real_climit_);
}

void StackGuard::ThreadLocal::Initialize(Isolate* isolate,
                                         const ExecutionAccess& lock) {
  const uintptr_t kLimitSize = FLAG_stack_size * KB;
  DCHECK_GT(GetCurrentStackPosition(), kLimitSize);
  uintptr_t limit = GetCurrentStackPosition() - kLimitSize;
  real_jslimit_ = SimulatorStack::JsLimitFromCLimit(isolate, limit);
  set_jslimit(SimulatorStack::JsLimitFromCLimit(isolate, limit));
  real_climit_ = limit;
  set_climit(limit);
  interrupt_scopes_ = nullptr;
  interrupt_flags_ = 0;
}

void StackGuard::InitThread(const ExecutionAccess& lock) {
  thread_local_.Initialize(isolate_, lock);
  Isolate::PerIsolateThreadData* per_thread =
      isolate_->FindOrAllocatePerThreadDataForThisThread();
  uintptr_t stored_limit = per_thread->stack_limit();
  // You should hold the ExecutionAccess lock when you call this.
  if (stored_limit != 0) {
    SetStackLimit(stored_limit);
  }
}

// --- C a l l s   t o   n a t i v e s ---

namespace {

bool TestAndClear(int* bitfield, int mask) {
  bool result = (*bitfield & mask);
  *bitfield &= ~mask;
  return result;
}

class ShouldBeZeroOnReturnScope final {
 public:
#ifndef DEBUG
  explicit ShouldBeZeroOnReturnScope(int*) {}
#else   // DEBUG
  explicit ShouldBeZeroOnReturnScope(int* v) : v_(v) {}
  ~ShouldBeZeroOnReturnScope() { DCHECK_EQ(*v_, 0); }

 private:
  int* v_;
#endif  // DEBUG
};

}  // namespace

Object StackGuard::HandleInterrupts() {
  TRACE_EVENT0("v8.execute", "V8.HandleInterrupts");

  if (FLAG_verify_predictable) {
    // Advance synthetic time by making a time request.
    isolate_->heap()->MonotonicallyIncreasingTimeInMs();
  }

  // Fetch and clear interrupt bits in one go. See comments inside the method
  // for special handling of TERMINATE_EXECUTION.
  int interrupt_flags = FetchAndClearInterrupts();

  // All interrupts should be fully processed when returning from this method.
  ShouldBeZeroOnReturnScope should_be_zero_on_return(&interrupt_flags);

  if (TestAndClear(&interrupt_flags, TERMINATE_EXECUTION)) {
    TRACE_EVENT0("v8.execute", "V8.TerminateExecution");
    return isolate_->TerminateExecution();
  }

  if (TestAndClear(&interrupt_flags, GC_REQUEST)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "V8.GCHandleGCRequest");
    isolate_->heap()->HandleGCRequest();
  }

  if (TestAndClear(&interrupt_flags, GROW_SHARED_MEMORY)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
                 "V8.WasmGrowSharedMemory");
    BackingStore::UpdateSharedWasmMemoryObjects(isolate_);
  }

  if (TestAndClear(&interrupt_flags, DEOPT_MARKED_ALLOCATION_SITES)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                 "V8.GCDeoptMarkedAllocationSites");
    isolate_->heap()->DeoptMarkedAllocationSites();
  }

  if (TestAndClear(&interrupt_flags, INSTALL_CODE)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.InstallOptimizedFunctions");
    DCHECK(isolate_->concurrent_recompilation_enabled());
    isolate_->optimizing_compile_dispatcher()->InstallOptimizedFunctions();
  }

  if (TestAndClear(&interrupt_flags, API_INTERRUPT)) {
    TRACE_EVENT0("v8.execute", "V8.InvokeApiInterruptCallbacks");
    // Callbacks must be invoked outside of ExecutionAccess lock.
    isolate_->InvokeApiInterruptCallbacks();
  }

  if (TestAndClear(&interrupt_flags, LOG_WASM_CODE)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "LogCode");
    isolate_->wasm_engine()->LogOutstandingCodesForIsolate(isolate_);
  }

  if (TestAndClear(&interrupt_flags, WASM_CODE_GC)) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "WasmCodeGC");
    isolate_->wasm_engine()->ReportLiveCodeFromStackForGC(isolate_);
  }

  isolate_->counters()->stack_interrupts()->Increment();

  return ReadOnlyRoots(isolate_).undefined_value();
}

}  // namespace internal
}  // namespace v8
