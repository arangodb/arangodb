// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/simplified-operator.h"

#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedLoweringTest : public GraphTest {
 public:
  explicit SimplifiedLoweringTest(int num_parameters = 1)
      : GraphTest(num_parameters),
        num_parameters_(num_parameters),
        machine_(zone()),
        javascript_(zone()),
        simplified_(zone()),
        jsgraph_(isolate(), graph(), common(), &javascript_, &simplified_,
                 &machine_) {}
  ~SimplifiedLoweringTest() override = default;

  void LowerGraph(Node* node) {
    // Make sure we always start with an empty graph.
    graph()->SetStart(graph()->NewNode(common()->Start(num_parameters())));
    graph()->SetEnd(graph()->NewNode(common()->End(1), graph()->start()));

    // Return {node} directly, so that we can match it with
    // "IsReturn(expected)".
    Node* zero = graph()->NewNode(common()->NumberConstant(0));
    Node* ret = graph()->NewNode(common()->Return(), zero, node,
                                 graph()->start(), graph()->start());
    NodeProperties::MergeControlToEnd(graph(), common(), ret);

    {
      // Simplified lowering needs to run w/o the typer decorator so make sure
      // the object is not live at the same time.
      Typer typer(js_heap_broker(), Typer::kNoFlags, graph());
      typer.Run();
    }

    SimplifiedLowering lowering(jsgraph(), js_heap_broker(), zone(),
                                source_positions(), node_origins(),
                                PoisoningMitigationLevel::kDontPoison);
    lowering.LowerAllNodes();
  }

  int num_parameters() const { return num_parameters_; }
  JSGraph* jsgraph() { return &jsgraph_; }

 private:
  const int num_parameters_;
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
  SimplifiedOperatorBuilder simplified_;
  JSGraph jsgraph_;
};

const int kSmiValues[] = {Smi::kMinValue,
                          Smi::kMinValue + 1,
                          Smi::kMinValue + 2,
                          3,
                          2,
                          1,
                          0,
                          -1,
                          -2,
                          -3,
                          Smi::kMaxValue - 2,
                          Smi::kMaxValue - 1,
                          Smi::kMaxValue};

TEST_F(SimplifiedLoweringTest, SmiConstantToIntPtrConstant) {
  TRACED_FOREACH(int, x, kSmiValues) {
    LowerGraph(jsgraph()->Constant(x));
    intptr_t smi = bit_cast<intptr_t>(Smi::FromInt(x));
    EXPECT_THAT(graph()->end()->InputAt(1),
                IsReturn(IsIntPtrConstant(smi), start(), start()));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
