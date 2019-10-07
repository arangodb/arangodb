// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PIPELINE_H_
#define V8_COMPILER_PIPELINE_H_

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/globals.h"
#include "src/objects.h"
#include "src/objects/code.h"

namespace v8 {
namespace internal {

struct AssemblerOptions;
class OptimizedCompilationInfo;
class OptimizedCompilationJob;
class RegisterConfiguration;
class JumpOptimizationInfo;

namespace wasm {
enum ModuleOrigin : uint8_t;
struct FunctionBody;
class NativeModule;
class WasmEngine;
struct WasmModule;
}  // namespace wasm

namespace compiler {

class CallDescriptor;
class Graph;
class InstructionSequence;
class MachineGraph;
class NodeOriginTable;
class Schedule;
class SourcePositionTable;

class Pipeline : public AllStatic {
 public:
  // Returns a new compilation job for the given JavaScript function.
  static OptimizedCompilationJob* NewCompilationJob(Isolate* isolate,
                                                    Handle<JSFunction> function,
                                                    bool has_script);

  // Returns a new compilation job for the WebAssembly compilation info.
  static OptimizedCompilationJob* NewWasmCompilationJob(
      OptimizedCompilationInfo* info, wasm::WasmEngine* wasm_engine,
      MachineGraph* mcgraph, CallDescriptor* call_descriptor,
      SourcePositionTable* source_positions, NodeOriginTable* node_origins,
      wasm::FunctionBody function_body, wasm::WasmModule* wasm_module,
      wasm::NativeModule* native_module, int function_index,
      wasm::ModuleOrigin wasm_origin);

  // Run the pipeline on a machine graph and generate code.
  static MaybeHandle<Code> GenerateCodeForWasmStub(
      Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
      Code::Kind kind, const char* debug_name,
      const AssemblerOptions& assembler_options,
      SourcePositionTable* source_positions = nullptr);

  // Run the pipeline on a machine graph and generate code. The {schedule} must
  // be valid, hence the given {graph} does not need to be schedulable.
  static MaybeHandle<Code> GenerateCodeForCodeStub(
      Isolate* isolate, CallDescriptor* call_descriptor, Graph* graph,
      Schedule* schedule, Code::Kind kind, const char* debug_name,
      uint32_t stub_key, int32_t builtin_index, JumpOptimizationInfo* jump_opt,
      PoisoningMitigationLevel poisoning_level,
      const AssemblerOptions& options);

  // ---------------------------------------------------------------------------
  // The following methods are for testing purposes only. Avoid production use.
  // ---------------------------------------------------------------------------

  // Run the pipeline on JavaScript bytecode and generate code.
  static MaybeHandle<Code> GenerateCodeForTesting(
      OptimizedCompilationInfo* info, Isolate* isolate);

  // Run the pipeline on a machine graph and generate code. If {schedule} is
  // {nullptr}, then compute a new schedule for code generation.
  V8_EXPORT_PRIVATE static MaybeHandle<Code> GenerateCodeForTesting(
      OptimizedCompilationInfo* info, Isolate* isolate,
      CallDescriptor* call_descriptor, Graph* graph,
      const AssemblerOptions& options, Schedule* schedule = nullptr);

  // Run just the register allocator phases.
  V8_EXPORT_PRIVATE static bool AllocateRegistersForTesting(
      const RegisterConfiguration* config, InstructionSequence* sequence,
      bool run_verifier);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Pipeline);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PIPELINE_H_
