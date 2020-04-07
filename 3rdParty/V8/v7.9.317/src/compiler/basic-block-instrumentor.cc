// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/basic-block-instrumentor.h"

#include <sstream>

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/schedule.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// Find the first place to insert new nodes in a block that's already been
// scheduled that won't upset the register allocator.
static NodeVector::iterator FindInsertionPoint(BasicBlock* block) {
  NodeVector::iterator i = block->begin();
  for (; i != block->end(); ++i) {
    const Operator* op = (*i)->op();
    if (OperatorProperties::IsBasicBlockBegin(op)) continue;
    switch (op->opcode()) {
      case IrOpcode::kParameter:
      case IrOpcode::kPhi:
      case IrOpcode::kEffectPhi:
        continue;
    }
    break;
  }
  return i;
}


// TODO(dcarney): need to mark code as non-serializable.
static const Operator* PointerConstant(CommonOperatorBuilder* common,
                                       intptr_t ptr) {
  return kSystemPointerSize == 8
             ? common->Int64Constant(ptr)
             : common->Int32Constant(static_cast<int32_t>(ptr));
}

BasicBlockProfiler::Data* BasicBlockInstrumentor::Instrument(
    OptimizedCompilationInfo* info, Graph* graph, Schedule* schedule,
    Isolate* isolate) {
  // Basic block profiling disables concurrent compilation, so handle deref is
  // fine.
  AllowHandleDereference allow_handle_dereference;
  // Skip the exit block in profiles, since the register allocator can't handle
  // it and entry into it means falling off the end of the function anyway.
  size_t n_blocks = static_cast<size_t>(schedule->RpoBlockCount()) - 1;
  BasicBlockProfiler::Data* data = BasicBlockProfiler::Get()->NewData(n_blocks);
  // Set the function name.
  data->SetFunctionName(info->GetDebugName());
  // Capture the schedule string before instrumentation.
  {
    std::ostringstream os;
    os << *schedule;
    data->SetSchedule(&os);
  }
  // Add the increment instructions to the start of every block.
  CommonOperatorBuilder common(graph->zone());
  Node* zero = graph->NewNode(common.Int32Constant(0));
  Node* one = graph->NewNode(common.Int32Constant(1));
  MachineOperatorBuilder machine(graph->zone());
  BasicBlockVector* blocks = schedule->rpo_order();
  size_t block_number = 0;
  for (BasicBlockVector::iterator it = blocks->begin(); block_number < n_blocks;
       ++it, ++block_number) {
    BasicBlock* block = (*it);
    data->SetBlockRpoNumber(block_number, block->rpo_number());
    // TODO(dcarney): wire effect and control deps for load and store.
    // Construct increment operation.
    Node* base = graph->NewNode(
        PointerConstant(&common, data->GetCounterAddress(block_number)));
    Node* load = graph->NewNode(machine.Load(MachineType::Uint32()), base, zero,
                                graph->start(), graph->start());
    Node* inc = graph->NewNode(machine.Int32Add(), load, one);
    Node* store =
        graph->NewNode(machine.Store(StoreRepresentation(
                           MachineRepresentation::kWord32, kNoWriteBarrier)),
                       base, zero, inc, graph->start(), graph->start());
    // Insert the new nodes.
    static const int kArraySize = 6;
    Node* to_insert[kArraySize] = {zero, one, base, load, inc, store};
    int insertion_start = block_number == 0 ? 0 : 2;
    NodeVector::iterator insertion_point = FindInsertionPoint(block);
    block->InsertNodes(insertion_point, &to_insert[insertion_start],
                       &to_insert[kArraySize]);
    // Tell the scheduler about the new nodes.
    for (int i = insertion_start; i < kArraySize; ++i) {
      schedule->SetBlockForNode(block, to_insert[i]);
    }
  }
  return data;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
