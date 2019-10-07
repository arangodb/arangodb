// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GRAPH_H_
#define V8_COMPILER_JS_GRAPH_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-graph.h"
#include "src/compiler/node-properties.h"
#include "src/globals.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedOperatorBuilder;
class Typer;

// Implements a facade on a Graph, enhancing the graph with JS-specific
// notions, including various builders for operators, canonicalized global
// constants, and various helper methods.
class V8_EXPORT_PRIVATE JSGraph : public MachineGraph {
 public:
  JSGraph(Isolate* isolate, Graph* graph, CommonOperatorBuilder* common,
          JSOperatorBuilder* javascript, SimplifiedOperatorBuilder* simplified,
          MachineOperatorBuilder* machine)
      : MachineGraph(graph, common, machine),
        isolate_(isolate),
        javascript_(javascript),
        simplified_(simplified) {
  }

  // CEntryStubs are cached depending on the result size and other flags.
  Node* CEntryStubConstant(int result_size,
                           SaveFPRegsMode save_doubles = kDontSaveFPRegs,
                           ArgvMode argv_mode = kArgvOnStack,
                           bool builtin_exit_frame = false);

  // Used for padding frames. (alias: the hole)
  Node* PaddingConstant() { return TheHoleConstant(); }

  // Used for stubs and runtime functions with no context. (alias: SMI zero)
  Node* NoContextConstant() { return ZeroConstant(); }

  // Creates a HeapConstant node, possibly canonicalized, and may access the
  // heap to inspect the object.
  Node* HeapConstant(Handle<HeapObject> value);

  // Creates a Constant node of the appropriate type for the given object.
  // Accesses the heap to inspect the object and determine whether one of the
  // canonicalized globals or a number constant should be returned.
  Node* Constant(Handle<Object> value);

  // Like above, but doesn't access the heap directly.
  Node* Constant(const ObjectRef& value);

  // Creates a NumberConstant node, usually canonicalized.
  Node* Constant(double value);

  // Creates a HeapConstant node for either true or false.
  Node* BooleanConstant(bool is_true) {
    return is_true ? TrueConstant() : FalseConstant();
  }

  Node* SmiConstant(int32_t immediate) {
    DCHECK(Smi::IsValid(immediate));
    return Constant(immediate);
  }

  JSOperatorBuilder* javascript() const { return javascript_; }
  SimplifiedOperatorBuilder* simplified() const { return simplified_; }
  Isolate* isolate() const { return isolate_; }
  Factory* factory() const { return isolate()->factory(); }

  // Adds all the cached nodes to the given list.
  void GetCachedNodes(NodeVector* nodes);

// Cached global nodes.
#define CACHED_GLOBAL_LIST(V)       \
  V(AllocateInNewSpaceStubConstant) \
  V(AllocateInOldSpaceStubConstant) \
  V(ArrayConstructorStubConstant)   \
  V(ToNumberBuiltinConstant)        \
  V(EmptyFixedArrayConstant)        \
  V(EmptyStringConstant)            \
  V(FixedArrayMapConstant)          \
  V(PropertyArrayMapConstant)       \
  V(FixedDoubleArrayMapConstant)    \
  V(HeapNumberMapConstant)          \
  V(OptimizedOutConstant)           \
  V(StaleRegisterConstant)          \
  V(UndefinedConstant)              \
  V(TheHoleConstant)                \
  V(TrueConstant)                   \
  V(FalseConstant)                  \
  V(NullConstant)                   \
  V(ZeroConstant)                   \
  V(OneConstant)                    \
  V(NaNConstant)                    \
  V(MinusOneConstant)               \
  V(EmptyStateValues)               \
  V(SingleDeadTypedStateValues)

// Cached global node accessor methods.
#define DECLARE_GETTER(name) Node* name();
  CACHED_GLOBAL_LIST(DECLARE_GETTER)
#undef DECLARE_FIELD

 private:
  Isolate* isolate_;
  JSOperatorBuilder* javascript_;
  SimplifiedOperatorBuilder* simplified_;

#define CACHED_CENTRY_LIST(V) \
  V(CEntryStub1Constant)      \
  V(CEntryStub2Constant)      \
  V(CEntryStub3Constant)      \
  V(CEntryStub1WithBuiltinExitFrameConstant)

// Canonicalized global node fields.
#define DECLARE_FIELD(name) Node* name##_ = nullptr;
  CACHED_GLOBAL_LIST(DECLARE_FIELD)
  CACHED_CENTRY_LIST(DECLARE_FIELD)
#undef DECLARE_FIELD

  // Internal helper to canonicalize a number constant.
  Node* NumberConstant(double value);

  DISALLOW_COPY_AND_ASSIGN(JSGraph);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_GRAPH_H_
