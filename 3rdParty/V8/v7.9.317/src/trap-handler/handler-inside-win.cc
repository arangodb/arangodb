// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ BEFORE CHANGING THIS FILE!
//
// This file implements the out of bounds trap handler for
// WebAssembly. Exception handlers are notoriously difficult to get
// right, and getting it wrong can lead to security
// vulnerabilities. In order to minimize this risk, here are some
// rules to follow.
//
// 1. Do not introduce any new external dependencies. This file needs
//    to be self contained so it is easy to audit everything that a
//    trap handler might do.
//
// 2. Any changes must be reviewed by someone from the crash reporting
//    or security team. See OWNERS for suggested reviewers.
//
// For more information, see https://goo.gl/yMeyUY.
//
// This file contains most of the code that actually runs in an exception
// handler context. Some additional code is used both inside and outside the
// trap handler. This code can be found in handler-shared.cc.

#include "src/trap-handler/handler-inside-win.h"

#include <windows.h>

#include "src/trap-handler/trap-handler-internal.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {
namespace trap_handler {

// The below struct needed to access the offset in the Thread Environment Block
// to see if the thread local storage for the thread has been allocated yet.
//
// The ThreadLocalStorage pointer is located 12 pointers into the TEB (i.e. at
// offset 0x58 for 64-bit platforms, and 0x2c for 32-bit platforms). This is
// true for x64, x86, ARM, and ARM64 platforms (see the header files in the SDK
// named ksamd64.inc, ks386.inc, ksarm.h, and ksarm64.h respectively).
//
// These offsets are baked into compiled binaries, so can never be changed for
// backwards compatibility reasons.
struct TEB {
  PVOID reserved[11];
  PVOID thread_local_storage_pointer;
};

bool TryHandleWasmTrap(EXCEPTION_POINTERS* exception) {
  // VectoredExceptionHandlers need extreme caution. Do as little as possible
  // to determine if the exception should be handled or not. Exceptions can be
  // thrown very early in a threads life, before the thread has even completed
  // initializing. As a demonstrative example, there was a bug (#8966) where an
  // exception would be raised before the thread local copy of the
  // "__declspec(thread)" variables had been allocated, the handler tried to
  // access the thread-local "g_thread_in_wasm_code", which would then raise
  // another exception, and an infinite loop ensued.

  // First ensure this is an exception type of interest
  if (exception->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
    return false;
  }

  // See if thread-local storage for __declspec(thread) variables has been
  // allocated yet. This pointer is initially null in the TEB until the
  // loader has completed allocating the memory for thread_local variables
  // and copy constructed their initial values. (Note: Any functions that
  // need to run to initialize values may not have run yet, but that is not
  // the case for any thread_locals used here).
  TEB* pteb = reinterpret_cast<TEB*>(NtCurrentTeb());
  if (!pteb->thread_local_storage_pointer) {
    return false;
  }

  // Now safe to run more advanced logic, which may access thread_locals
  // Ensure the faulting thread was actually running Wasm code.
  if (!IsThreadInWasm()) {
    return false;
  }

  // Clear g_thread_in_wasm_code, primarily to protect against nested faults.
  g_thread_in_wasm_code = false;

  const EXCEPTION_RECORD* record = exception->ExceptionRecord;

  uintptr_t fault_addr = reinterpret_cast<uintptr_t>(record->ExceptionAddress);
  uintptr_t landing_pad = 0;

  if (TryFindLandingPad(fault_addr, &landing_pad)) {
    exception->ContextRecord->Rip = landing_pad;
    // We will return to wasm code, so restore the g_thread_in_wasm_code flag.
    g_thread_in_wasm_code = true;
    return true;
  }

  // If we get here, it's not a recoverable wasm fault, so we go to the next
  // handler. Leave the g_thread_in_wasm_code flag unset since we do not return
  // to wasm code.
  return false;
}

LONG HandleWasmTrap(EXCEPTION_POINTERS* exception) {
  if (TryHandleWasmTrap(exception)) {
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
