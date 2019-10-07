// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/assembler.h"
#include "src/frame-constants.h"
#include "src/macro-assembler.h"
#include "src/ppc/assembler-ppc-inl.h"
#include "src/ppc/assembler-ppc.h"
#include "src/ppc/macro-assembler-ppc.h"

#include "src/ppc/frame-constants-ppc.h"

namespace v8 {
namespace internal {

Register JavaScriptFrame::fp_register() { return v8::internal::fp; }
Register JavaScriptFrame::context_register() { return cp; }
Register JavaScriptFrame::constant_pool_pointer_register() {
  DCHECK(FLAG_enable_embedded_constant_pool);
  return kConstantPoolRegister;
}

int InterpreterFrameConstants::RegisterStackSlotCount(int register_count) {
  return register_count;
}

int BuiltinContinuationFrameConstants::PaddingSlotCount(int register_count) {
  USE(register_count);
  return 0;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
