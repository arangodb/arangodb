// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRAP_HANDLER_TRAP_HANDLER_H_
#define V8_TRAP_HANDLER_TRAP_HANDLER_H_

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include "src/base/build_config.h"
#include "src/flags.h"
#include "src/globals.h"

#if V8_OS_LINUX
#include <ucontext.h>
#endif

namespace v8 {
namespace internal {
namespace trap_handler {

// TODO(eholk): Support trap handlers on other platforms.
#if V8_TARGET_ARCH_X64 && V8_OS_LINUX && !V8_OS_ANDROID
#define V8_TRAP_HANDLER_SUPPORTED true
#else
#define V8_TRAP_HANDLER_SUPPORTED false
#endif

struct ProtectedInstructionData {
  // The offset of this instruction from the start of its code object.
  // Wasm code never grows larger than 2GB, so uint32_t is sufficient.
  uint32_t instr_offset;

  // The offset of the landing pad from the start of its code object.
  //
  // TODO(eholk): Using a single landing pad and store parameters here.
  uint32_t landing_offset;
};

const int kInvalidIndex = -1;

/// Adds the handler data to the place where the signal handler will find it.
///
/// This returns a number that can be used to identify the handler data to
/// ReleaseHandlerData, or -1 on failure.
int RegisterHandlerData(Address base, size_t size,
                        size_t num_protected_instructions,
                        const ProtectedInstructionData* protected_instructions);

/// Removes the data from the master list and frees any memory, if necessary.
/// TODO(mtrofin): We can switch to using size_t for index and not need
/// kInvalidIndex.
void ReleaseHandlerData(int index);

#if V8_OS_WIN
#define THREAD_LOCAL __declspec(thread)
#elif V8_OS_ANDROID
// TODO(eholk): fix this before enabling for trap handlers for Android.
#define THREAD_LOCAL
#else
#define THREAD_LOCAL __thread
#endif

extern bool g_is_trap_handler_enabled;
// Enables trap handling for WebAssembly bounds checks.
//
// use_v8_signal_handler indicates that V8 should install its own signal handler
// rather than relying on the embedder to do it.
bool EnableTrapHandler(bool use_v8_signal_handler);

inline bool IsTrapHandlerEnabled() {
  DCHECK_IMPLIES(g_is_trap_handler_enabled, V8_TRAP_HANDLER_SUPPORTED);
  return g_is_trap_handler_enabled;
}

extern THREAD_LOCAL int g_thread_in_wasm_code;

// Return the address of the thread-local {g_thread_in_wasm_code} variable. This
// pointer can be accessed and modified as long as the thread calling this
// function exists. Only use if from the same thread do avoid race conditions.
inline int* GetThreadInWasmThreadLocalAddress() {
  return &g_thread_in_wasm_code;
}

inline bool IsThreadInWasm() { return g_thread_in_wasm_code; }

inline void SetThreadInWasm() {
  if (IsTrapHandlerEnabled()) {
    DCHECK(!IsThreadInWasm());
    g_thread_in_wasm_code = true;
  }
}

inline void ClearThreadInWasm() {
  if (IsTrapHandlerEnabled()) {
    DCHECK(IsThreadInWasm());
    g_thread_in_wasm_code = false;
  }
}

bool RegisterDefaultTrapHandler();
V8_EXPORT_PRIVATE void RestoreOriginalSignalHandler();

#if V8_OS_LINUX
bool TryHandleSignal(int signum, siginfo_t* info, ucontext_t* context);
#endif  // V8_OS_LINUX

size_t GetRecoveredTrapCount();

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8

#endif  // V8_TRAP_HANDLER_TRAP_HANDLER_H_
