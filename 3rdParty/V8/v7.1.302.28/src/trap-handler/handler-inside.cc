// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE READ BEFORE CHANGING THIS FILE!
//
// This file implements the out of bounds signal handler for
// WebAssembly. Signal handlers are notoriously difficult to get
// right, and getting it wrong can lead to security
// vulnerabilities. In order to minimize this risk, here are some
// rules to follow.
//
// 1. Do not introduce any new external dependencies. This file needs
//    to be self contained so it is easy to audit everything that a
//    signal handler might do.
//
// 2. Any changes must be reviewed by someone from the crash reporting
//    or security team. See OWNERS for suggested reviewers.
//
// For more information, see https://goo.gl/yMeyUY.
//
// This file contains most of the code that actually runs in a signal handler
// context. Some additional code is used both inside and outside the signal
// handler. This code can be found in handler-shared.cc.

#include "src/trap-handler/trap-handler-internal.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {
namespace trap_handler {

#if V8_TRAP_HANDLER_SUPPORTED

// This function contains the platform independent portions of fault
// classification.
bool TryFindLandingPad(uintptr_t fault_addr, uintptr_t* landing_pad) {
  // TODO(eholk): broad code range check

  // Taking locks in a signal handler is risky because a fault in the signal
  // handler could lead to a deadlock when attempting to acquire the lock
  // again. We guard against this case with g_thread_in_wasm_code. The lock
  // may only be taken when not executing Wasm code (an assert in
  // MetadataLock's constructor ensures this). This signal handler will bail
  // out before trying to take the lock if g_thread_in_wasm_code is not set.
  MetadataLock lock_holder;

  for (size_t i = 0; i < gNumCodeObjects; ++i) {
    const CodeProtectionInfo* data = gCodeObjects[i].code_info;
    if (data == nullptr) {
      continue;
    }
    const Address base = data->base;

    if (fault_addr >= base && fault_addr < base + data->size) {
      // Hurray, we found the code object. Check for protected addresses.
      const ptrdiff_t offset = fault_addr - base;

      for (unsigned i = 0; i < data->num_protected_instructions; ++i) {
        if (data->instructions[i].instr_offset == offset) {
          // Hurray again, we found the actual instruction.
          *landing_pad = data->instructions[i].landing_offset + base;

          gRecoveredTrapCount.store(
              gRecoveredTrapCount.load(std::memory_order_relaxed) + 1,
              std::memory_order_relaxed);

          return true;
        }
      }
    }
  }
  return false;
}
#endif  // V8_TRAP_HANDLER_SUPPORTED

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
