// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-graph.h"

#include "src/compiler/node-properties.h"
#include "src/external-reference.h"

namespace v8 {
namespace internal {
namespace compiler {

Node* MachineGraph::Int32Constant(int32_t value) {
  Node** loc = cache_.FindInt32Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Int32Constant(value));
  }
  return *loc;
}

Node* MachineGraph::Int64Constant(int64_t value) {
  Node** loc = cache_.FindInt64Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Int64Constant(value));
  }
  return *loc;
}

Node* MachineGraph::IntPtrConstant(intptr_t value) {
  return machine()->Is32() ? Int32Constant(static_cast<int32_t>(value))
                           : Int64Constant(static_cast<int64_t>(value));
}

Node* MachineGraph::RelocatableInt32Constant(int32_t value,
                                             RelocInfo::Mode rmode) {
  Node** loc = cache_.FindRelocatableInt32Constant(
      value, static_cast<RelocInfoMode>(rmode));
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->RelocatableInt32Constant(value, rmode));
  }
  return *loc;
}

Node* MachineGraph::RelocatableInt64Constant(int64_t value,
                                             RelocInfo::Mode rmode) {
  Node** loc = cache_.FindRelocatableInt64Constant(
      value, static_cast<RelocInfoMode>(rmode));
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->RelocatableInt64Constant(value, rmode));
  }
  return *loc;
}

Node* MachineGraph::RelocatableIntPtrConstant(intptr_t value,
                                              RelocInfo::Mode rmode) {
  return kPointerSize == 8
             ? RelocatableInt64Constant(value, rmode)
             : RelocatableInt32Constant(static_cast<int>(value), rmode);
}

Node* MachineGraph::Float32Constant(float value) {
  Node** loc = cache_.FindFloat32Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Float32Constant(value));
  }
  return *loc;
}

Node* MachineGraph::Float64Constant(double value) {
  Node** loc = cache_.FindFloat64Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Float64Constant(value));
  }
  return *loc;
}

Node* MachineGraph::PointerConstant(intptr_t value) {
  Node** loc = cache_.FindPointerConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->PointerConstant(value));
  }
  return *loc;
}

Node* MachineGraph::ExternalConstant(ExternalReference reference) {
  Node** loc = cache_.FindExternalConstant(reference);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->ExternalConstant(reference));
  }
  return *loc;
}

Node* MachineGraph::ExternalConstant(Runtime::FunctionId function_id) {
  return ExternalConstant(ExternalReference::Create(function_id));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
