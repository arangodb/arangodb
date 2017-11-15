// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/compiler/value-helper.h"

#include "src/compiler/node-matchers.h"
#include "src/compiler/representation-change.h"

namespace v8 {
namespace internal {
namespace compiler {

class RepresentationChangerTester : public HandleAndZoneScope,
                                    public GraphAndBuilders {
 public:
  explicit RepresentationChangerTester(int num_parameters = 0)
      : GraphAndBuilders(main_zone()),
        javascript_(main_zone()),
        jsgraph_(main_isolate(), main_graph_, &main_common_, &javascript_,
                 &main_simplified_, &main_machine_),
        changer_(&jsgraph_, main_isolate()) {
    Node* s = graph()->NewNode(common()->Start(num_parameters));
    graph()->SetStart(s);
  }

  JSOperatorBuilder javascript_;
  JSGraph jsgraph_;
  RepresentationChanger changer_;

  Isolate* isolate() { return main_isolate(); }
  Graph* graph() { return main_graph_; }
  CommonOperatorBuilder* common() { return &main_common_; }
  JSGraph* jsgraph() { return &jsgraph_; }
  RepresentationChanger* changer() { return &changer_; }

  // TODO(titzer): use ValueChecker / ValueUtil
  void CheckInt32Constant(Node* n, int32_t expected) {
    Int32Matcher m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, m.Value());
  }

  void CheckUint32Constant(Node* n, uint32_t expected) {
    Uint32Matcher m(n);
    CHECK(m.HasValue());
    CHECK_EQ(static_cast<int>(expected), static_cast<int>(m.Value()));
  }

  void CheckFloat64Constant(Node* n, double expected) {
    Float64Matcher m(n);
    CHECK(m.HasValue());
    CHECK_DOUBLE_EQ(expected, m.Value());
  }

  void CheckFloat32Constant(Node* n, float expected) {
    CHECK_EQ(IrOpcode::kFloat32Constant, n->opcode());
    float fval = OpParameter<float>(n->op());
    CHECK_FLOAT_EQ(expected, fval);
  }

  void CheckHeapConstant(Node* n, HeapObject* expected) {
    HeapObjectMatcher m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, *m.Value());
  }

  void CheckNumberConstant(Node* n, double expected) {
    NumberMatcher m(n);
    CHECK_EQ(IrOpcode::kNumberConstant, n->opcode());
    CHECK(m.HasValue());
    CHECK_DOUBLE_EQ(expected, m.Value());
  }

  Node* Parameter(int index = 0) {
    Node* n = graph()->NewNode(common()->Parameter(index), graph()->start());
    NodeProperties::SetType(n, Type::Any());
    return n;
  }

  Node* Return(Node* input) {
    Node* n = graph()->NewNode(common()->Return(), jsgraph()->Int32Constant(0),
                               input, graph()->start(), graph()->start());
    return n;
  }

  void CheckTypeError(MachineRepresentation from, Type* from_type,
                      MachineRepresentation to) {
    changer()->testing_type_errors_ = true;
    changer()->type_error_ = false;
    Node* n = Parameter(0);
    Node* use = Return(n);
    Node* c = changer()->GetRepresentationFor(n, from, from_type, use,
                                              UseInfo(to, Truncation::None()));
    CHECK(changer()->type_error_);
    CHECK_EQ(n, c);
  }

  void CheckNop(MachineRepresentation from, Type* from_type,
                MachineRepresentation to) {
    Node* n = Parameter(0);
    Node* use = Return(n);
    Node* c = changer()->GetRepresentationFor(n, from, from_type, use,
                                              UseInfo(to, Truncation::None()));
    CHECK_EQ(n, c);
  }
};


const MachineType kMachineTypes[] = {
    MachineType::Float32(), MachineType::Float64(),  MachineType::Int8(),
    MachineType::Uint8(),   MachineType::Int16(),    MachineType::Uint16(),
    MachineType::Int32(),   MachineType::Uint32(),   MachineType::Int64(),
    MachineType::Uint64(),  MachineType::AnyTagged()};


TEST(BoolToBit_constant) {
  RepresentationChangerTester r;

  Node* true_node = r.jsgraph()->TrueConstant();
  Node* true_use = r.Return(true_node);
  Node* true_bit = r.changer()->GetRepresentationFor(
      true_node, MachineRepresentation::kTagged, Type::None(), true_use,
      UseInfo(MachineRepresentation::kBit, Truncation::None()));
  r.CheckInt32Constant(true_bit, 1);

  Node* false_node = r.jsgraph()->FalseConstant();
  Node* false_use = r.Return(false_node);
  Node* false_bit = r.changer()->GetRepresentationFor(
      false_node, MachineRepresentation::kTagged, Type::None(), false_use,
      UseInfo(MachineRepresentation::kBit, Truncation::None()));
  r.CheckInt32Constant(false_bit, 0);
}

TEST(ToTagged_constant) {
  RepresentationChangerTester r;

  for (double i : ValueHelper::float64_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kFloat64, Type::None(), use,
        UseInfo(MachineRepresentation::kTagged, Truncation::None()));
    r.CheckNumberConstant(c, i);
  }

  for (int i : ValueHelper::int32_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kWord32, Type::Signed32(), use,
        UseInfo(MachineRepresentation::kTagged, Truncation::None()));
    r.CheckNumberConstant(c, i);
  }

  for (uint32_t i : ValueHelper::uint32_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kWord32, Type::Unsigned32(), use,
        UseInfo(MachineRepresentation::kTagged, Truncation::None()));
    r.CheckNumberConstant(c, i);
  }
}

TEST(ToFloat64_constant) {
  RepresentationChangerTester r;

  for (double i : ValueHelper::float64_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kTagged, Type::None(), use,
        UseInfo(MachineRepresentation::kFloat64, Truncation::None()));
    r.CheckFloat64Constant(c, i);
  }

  for (int i : ValueHelper::int32_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kWord32, Type::Signed32(), use,
        UseInfo(MachineRepresentation::kFloat64, Truncation::None()));
    r.CheckFloat64Constant(c, i);
  }

  for (uint32_t i : ValueHelper::uint32_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kWord32, Type::Unsigned32(), use,
        UseInfo(MachineRepresentation::kFloat64, Truncation::None()));
    r.CheckFloat64Constant(c, i);
  }
}


static bool IsFloat32Int32(int32_t val) {
  return val >= -(1 << 23) && val <= (1 << 23);
}


static bool IsFloat32Uint32(uint32_t val) { return val <= (1 << 23); }


TEST(ToFloat32_constant) {
  RepresentationChangerTester r;

  for (double i : ValueHelper::float32_vector()) {
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kTagged, Type::None(), use,
        UseInfo(MachineRepresentation::kFloat32, Truncation::None()));
    r.CheckFloat32Constant(c, i);
  }

  for (int i : ValueHelper::int32_vector()) {
    if (!IsFloat32Int32(i)) continue;
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kWord32, Type::Signed32(), use,
        UseInfo(MachineRepresentation::kFloat32, Truncation::None()));
    r.CheckFloat32Constant(c, static_cast<float>(i));
  }

  for (uint32_t i : ValueHelper::uint32_vector()) {
    if (!IsFloat32Uint32(i)) continue;
    Node* n = r.jsgraph()->Constant(i);
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kWord32, Type::Unsigned32(), use,
        UseInfo(MachineRepresentation::kFloat32, Truncation::None()));
    r.CheckFloat32Constant(c, static_cast<float>(i));
  }
}

TEST(ToInt32_constant) {
  RepresentationChangerTester r;
  {
    FOR_INT32_INPUTS(i) {
      Node* n = r.jsgraph()->Constant(*i);
      Node* use = r.Return(n);
      Node* c = r.changer()->GetRepresentationFor(
          n, MachineRepresentation::kTagged, Type::Signed32(), use,
          UseInfo(MachineRepresentation::kWord32, Truncation::None()));
      r.CheckInt32Constant(c, *i);
    }
  }
}

TEST(ToUint32_constant) {
  RepresentationChangerTester r;
  FOR_UINT32_INPUTS(i) {
    Node* n = r.jsgraph()->Constant(static_cast<double>(*i));
    Node* use = r.Return(n);
    Node* c = r.changer()->GetRepresentationFor(
        n, MachineRepresentation::kTagged, Type::Unsigned32(), use,
        UseInfo(MachineRepresentation::kWord32, Truncation::None()));
    r.CheckUint32Constant(c, *i);
  }
}

static void CheckChange(IrOpcode::Value expected, MachineRepresentation from,
                        Type* from_type, UseInfo use_info) {
  RepresentationChangerTester r;

  Node* n = r.Parameter();
  Node* use = r.Return(n);
  Node* c =
      r.changer()->GetRepresentationFor(n, from, from_type, use, use_info);

  CHECK_NE(c, n);
  CHECK_EQ(expected, c->opcode());
  CHECK_EQ(n, c->InputAt(0));

  if (expected == IrOpcode::kCheckedFloat64ToInt32) {
    CheckForMinusZeroMode mode =
        from_type->Maybe(Type::MinusZero())
            ? use_info.minus_zero_check()
            : CheckForMinusZeroMode::kDontCheckForMinusZero;
    CHECK_EQ(mode, CheckMinusZeroModeOf(c->op()));
  }
}

static void CheckChange(IrOpcode::Value expected, MachineRepresentation from,
                        Type* from_type, MachineRepresentation to) {
  CheckChange(expected, from, from_type, UseInfo(to, Truncation::None()));
}

static void CheckTwoChanges(IrOpcode::Value expected2,
                            IrOpcode::Value expected1,
                            MachineRepresentation from, Type* from_type,
                            MachineRepresentation to) {
  RepresentationChangerTester r;

  Node* n = r.Parameter();
  Node* use = r.Return(n);
  Node* c1 = r.changer()->GetRepresentationFor(n, from, from_type, use,
                                               UseInfo(to, Truncation::None()));

  CHECK_NE(c1, n);
  CHECK_EQ(expected1, c1->opcode());
  Node* c2 = c1->InputAt(0);
  CHECK_NE(c2, n);
  CHECK_EQ(expected2, c2->opcode());
  CHECK_EQ(n, c2->InputAt(0));
}

static void CheckChange(IrOpcode::Value expected, MachineRepresentation from,
                        Type* from_type, MachineRepresentation to,
                        UseInfo use_info) {
  RepresentationChangerTester r;

  Node* n = r.Parameter();
  Node* use = r.Return(n);
  Node* c =
      r.changer()->GetRepresentationFor(n, from, from_type, use, use_info);

  CHECK_NE(c, n);
  CHECK_EQ(expected, c->opcode());
  CHECK_EQ(n, c->InputAt(0));
}

TEST(SingleChanges) {
  CheckChange(IrOpcode::kChangeTaggedToBit, MachineRepresentation::kTagged,
              Type::Boolean(), MachineRepresentation::kBit);
  CheckChange(IrOpcode::kChangeBitToTagged, MachineRepresentation::kBit,
              Type::Boolean(), MachineRepresentation::kTagged);

  CheckChange(IrOpcode::kChangeInt31ToTaggedSigned,
              MachineRepresentation::kWord32, Type::Signed31(),
              MachineRepresentation::kTagged);
  CheckChange(IrOpcode::kChangeInt32ToTagged, MachineRepresentation::kWord32,
              Type::Signed32(), MachineRepresentation::kTagged);
  CheckChange(IrOpcode::kChangeUint32ToTagged, MachineRepresentation::kWord32,
              Type::Unsigned32(), MachineRepresentation::kTagged);
  CheckChange(IrOpcode::kChangeFloat64ToTagged, MachineRepresentation::kFloat64,
              Type::Number(), MachineRepresentation::kTagged);
  CheckTwoChanges(IrOpcode::kChangeFloat64ToInt32,
                  IrOpcode::kChangeInt31ToTaggedSigned,
                  MachineRepresentation::kFloat64, Type::Signed31(),
                  MachineRepresentation::kTagged);
  CheckTwoChanges(IrOpcode::kChangeFloat64ToInt32,
                  IrOpcode::kChangeInt32ToTagged,
                  MachineRepresentation::kFloat64, Type::Signed32(),
                  MachineRepresentation::kTagged);
  CheckTwoChanges(IrOpcode::kChangeFloat64ToUint32,
                  IrOpcode::kChangeUint32ToTagged,
                  MachineRepresentation::kFloat64, Type::Unsigned32(),
                  MachineRepresentation::kTagged);

  CheckChange(IrOpcode::kChangeTaggedToInt32, MachineRepresentation::kTagged,
              Type::Signed32(), MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kChangeTaggedToUint32, MachineRepresentation::kTagged,
              Type::Unsigned32(), MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kChangeTaggedToFloat64, MachineRepresentation::kTagged,
              Type::Number(), MachineRepresentation::kFloat64);
  CheckChange(IrOpcode::kTruncateTaggedToFloat64,
              MachineRepresentation::kTagged, Type::NumberOrUndefined(),
              MachineRepresentation::kFloat64);
  CheckChange(IrOpcode::kChangeTaggedToFloat64, MachineRepresentation::kTagged,
              Type::Signed31(), MachineRepresentation::kFloat64);

  // Int32,Uint32 <-> Float64 are actually machine conversions.
  CheckChange(IrOpcode::kChangeInt32ToFloat64, MachineRepresentation::kWord32,
              Type::Signed32(), MachineRepresentation::kFloat64);
  CheckChange(IrOpcode::kChangeUint32ToFloat64, MachineRepresentation::kWord32,
              Type::Unsigned32(), MachineRepresentation::kFloat64);
  CheckChange(IrOpcode::kChangeFloat64ToInt32, MachineRepresentation::kFloat64,
              Type::Signed32(), MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kChangeFloat64ToUint32, MachineRepresentation::kFloat64,
              Type::Unsigned32(), MachineRepresentation::kWord32);

  CheckChange(IrOpcode::kTruncateFloat64ToFloat32,
              MachineRepresentation::kFloat64, Type::Number(),
              MachineRepresentation::kFloat32);

  // Int32,Uint32 <-> Float32 require two changes.
  CheckTwoChanges(IrOpcode::kChangeInt32ToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32,
                  MachineRepresentation::kWord32, Type::Signed32(),
                  MachineRepresentation::kFloat32);
  CheckTwoChanges(IrOpcode::kChangeUint32ToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32,
                  MachineRepresentation::kWord32, Type::Unsigned32(),
                  MachineRepresentation::kFloat32);
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToInt32,
                  MachineRepresentation::kFloat32, Type::Signed32(),
                  MachineRepresentation::kWord32);
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToUint32,
                  MachineRepresentation::kFloat32, Type::Unsigned32(),
                  MachineRepresentation::kWord32);

  // Float32 <-> Tagged require two changes.
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToTagged,
                  MachineRepresentation::kFloat32, Type::Number(),
                  MachineRepresentation::kTagged);
  CheckTwoChanges(IrOpcode::kChangeTaggedToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32,
                  MachineRepresentation::kTagged, Type::Number(),
                  MachineRepresentation::kFloat32);
}


TEST(SignednessInWord32) {
  RepresentationChangerTester r;

  CheckChange(IrOpcode::kChangeTaggedToInt32, MachineRepresentation::kTagged,
              Type::Signed32(), MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kChangeTaggedToUint32, MachineRepresentation::kTagged,
              Type::Unsigned32(), MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kChangeInt32ToFloat64, MachineRepresentation::kWord32,
              Type::Signed32(), MachineRepresentation::kFloat64);
  CheckChange(IrOpcode::kChangeFloat64ToInt32, MachineRepresentation::kFloat64,
              Type::Signed32(), MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kTruncateFloat64ToWord32,
              MachineRepresentation::kFloat64, Type::Number(),
              MachineRepresentation::kWord32);
  CheckChange(IrOpcode::kCheckedTruncateTaggedToWord32,
              MachineRepresentation::kTagged, Type::NonInternal(),
              MachineRepresentation::kWord32,
              UseInfo::CheckedNumberOrOddballAsWord32());

  CheckTwoChanges(IrOpcode::kChangeInt32ToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32,
                  MachineRepresentation::kWord32, Type::Signed32(),
                  MachineRepresentation::kFloat32);
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kTruncateFloat64ToWord32,
                  MachineRepresentation::kFloat32, Type::Number(),
                  MachineRepresentation::kWord32);
}

static void TestMinusZeroCheck(IrOpcode::Value expected, Type* from_type) {
  RepresentationChangerTester r;

  CheckChange(expected, MachineRepresentation::kFloat64, from_type,
              UseInfo::CheckedSignedSmallAsWord32(
                  CheckForMinusZeroMode::kCheckForMinusZero));

  CheckChange(expected, MachineRepresentation::kFloat64, from_type,
              UseInfo::CheckedSignedSmallAsWord32(
                  CheckForMinusZeroMode::kDontCheckForMinusZero));

  CheckChange(expected, MachineRepresentation::kFloat64, from_type,
              UseInfo::CheckedSigned32AsWord32(
                  CheckForMinusZeroMode::kCheckForMinusZero));

  CheckChange(expected, MachineRepresentation::kFloat64, from_type,
              UseInfo::CheckedSigned32AsWord32(
                  CheckForMinusZeroMode::kDontCheckForMinusZero));
}

TEST(MinusZeroCheck) {
  TestMinusZeroCheck(IrOpcode::kCheckedFloat64ToInt32, Type::NumberOrOddball());
  // PlainNumber cannot be minus zero so the minus zero check should be
  // eliminated.
  TestMinusZeroCheck(IrOpcode::kCheckedFloat64ToInt32, Type::PlainNumber());
}

TEST(Nops) {
  RepresentationChangerTester r;

  // X -> X is always a nop for any single representation X.
  for (size_t i = 0; i < arraysize(kMachineTypes); i++) {
    r.CheckNop(kMachineTypes[i].representation(), Type::Number(),
               kMachineTypes[i].representation());
  }

  // 32-bit floats.
  r.CheckNop(MachineRepresentation::kFloat32, Type::Number(),
             MachineRepresentation::kFloat32);

  // 32-bit words can be used as smaller word sizes and vice versa, because
  // loads from memory implicitly sign or zero extend the value to the
  // full machine word size, and stores implicitly truncate.
  r.CheckNop(MachineRepresentation::kWord32, Type::Signed32(),
             MachineRepresentation::kWord8);
  r.CheckNop(MachineRepresentation::kWord32, Type::Signed32(),
             MachineRepresentation::kWord16);
  r.CheckNop(MachineRepresentation::kWord32, Type::Signed32(),
             MachineRepresentation::kWord32);
  r.CheckNop(MachineRepresentation::kWord8, Type::Signed32(),
             MachineRepresentation::kWord32);
  r.CheckNop(MachineRepresentation::kWord16, Type::Signed32(),
             MachineRepresentation::kWord32);

  // kRepBit (result of comparison) is implicitly a wordish thing.
  r.CheckNop(MachineRepresentation::kBit, Type::Boolean(),
             MachineRepresentation::kWord8);
  r.CheckNop(MachineRepresentation::kBit, Type::Boolean(),
             MachineRepresentation::kWord16);
  r.CheckNop(MachineRepresentation::kBit, Type::Boolean(),
             MachineRepresentation::kWord32);
  r.CheckNop(MachineRepresentation::kBit, Type::Boolean(),
             MachineRepresentation::kWord64);
}


TEST(TypeErrors) {
  RepresentationChangerTester r;

  // Floats cannot be implicitly converted to/from comparison conditions.
  r.CheckTypeError(MachineRepresentation::kBit, Type::Number(),
                   MachineRepresentation::kFloat32);
  r.CheckTypeError(MachineRepresentation::kBit, Type::Boolean(),
                   MachineRepresentation::kFloat32);

  // Word64 is internal and shouldn't be implicitly converted.
  r.CheckTypeError(MachineRepresentation::kWord64, Type::Internal(),
                   MachineRepresentation::kTagged);
  r.CheckTypeError(MachineRepresentation::kTagged, Type::Number(),
                   MachineRepresentation::kWord64);
  r.CheckTypeError(MachineRepresentation::kTagged, Type::Boolean(),
                   MachineRepresentation::kWord64);

  // Word64 / Word32 shouldn't be implicitly converted.
  r.CheckTypeError(MachineRepresentation::kWord64, Type::Internal(),
                   MachineRepresentation::kWord32);
  r.CheckTypeError(MachineRepresentation::kWord32, Type::Number(),
                   MachineRepresentation::kWord64);
  r.CheckTypeError(MachineRepresentation::kWord32, Type::Signed32(),
                   MachineRepresentation::kWord64);
  r.CheckTypeError(MachineRepresentation::kWord32, Type::Unsigned32(),
                   MachineRepresentation::kWord64);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
