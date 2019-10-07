// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_GENERATOR_H_
#define V8_INTERPRETER_INTERPRETER_GENERATOR_H_

#include "src/interpreter/bytecode-operands.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {

struct AssemblerOptions;

namespace interpreter {

extern Handle<Code> GenerateBytecodeHandler(Isolate* isolate, Bytecode bytecode,
                                            OperandScale operand_scale,
                                            int builtin_index,
                                            const AssemblerOptions& options);

extern Handle<Code> GenerateDeserializeLazyHandler(
    Isolate* isolate, OperandScale operand_scale, int builtin_index,
    const AssemblerOptions& options);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_GENERATOR_H_
