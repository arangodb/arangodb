// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SELECT_LOWERING_H_
#define V8_COMPILER_SELECT_LOWERING_H_

#include "src/compiler/graph-assembler.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Lowers Select nodes to diamonds.
class SelectLowering final : public Reducer {
 public:
  SelectLowering(JSGraph* jsgraph, Zone* zone);
  ~SelectLowering() override;

  const char* reducer_name() const override { return "SelectLowering"; }

  Reduction Reduce(Node* node) override;

  Node* LowerSelect(Node* node);

 private:
  GraphAssembler* gasm() { return &graph_assembler_; }
  Node* start() { return start_; }

  GraphAssembler graph_assembler_;
  Node* start_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SELECT_LOWERING_H_
