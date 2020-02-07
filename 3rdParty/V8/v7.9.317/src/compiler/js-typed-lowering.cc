// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-typed-lowering.h"

#include "src/ast/modules.h"
#include "src/builtins/builtins-utils.h"
#include "src/codegen/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/allocation-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/types.h"
#include "src/execution/protectors.h"
#include "src/objects/js-generator.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper class to simplify the process of reducing a single binop node with a
// JSOperator. This class manages the rewriting of context, control, and effect
// dependencies during lowering of a binop and contains numerous helper
// functions for matching the types of inputs to an operation.
class JSBinopReduction final {
 public:
  JSBinopReduction(JSTypedLowering* lowering, Node* node)
      : lowering_(lowering), node_(node) {}

  bool GetCompareNumberOperationHint(NumberOperationHint* hint) {
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    switch (CompareOperationHintOf(node_->op())) {
      case CompareOperationHint::kSignedSmall:
        *hint = NumberOperationHint::kSignedSmall;
        return true;
      case CompareOperationHint::kNumber:
        *hint = NumberOperationHint::kNumber;
        return true;
      case CompareOperationHint::kNumberOrOddball:
        *hint = NumberOperationHint::kNumberOrOddball;
        return true;
      case CompareOperationHint::kAny:
      case CompareOperationHint::kNone:
      case CompareOperationHint::kString:
      case CompareOperationHint::kSymbol:
      case CompareOperationHint::kBigInt:
      case CompareOperationHint::kReceiver:
      case CompareOperationHint::kReceiverOrNullOrUndefined:
      case CompareOperationHint::kInternalizedString:
        break;
    }
    return false;
  }

  bool IsInternalizedStringCompareOperation() {
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    return (CompareOperationHintOf(node_->op()) ==
            CompareOperationHint::kInternalizedString) &&
           BothInputsMaybe(Type::InternalizedString());
  }

  bool IsReceiverCompareOperation() {
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    return (CompareOperationHintOf(node_->op()) ==
            CompareOperationHint::kReceiver) &&
           BothInputsMaybe(Type::Receiver());
  }

  bool IsReceiverOrNullOrUndefinedCompareOperation() {
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    return (CompareOperationHintOf(node_->op()) ==
            CompareOperationHint::kReceiverOrNullOrUndefined) &&
           BothInputsMaybe(Type::ReceiverOrNullOrUndefined());
  }

  bool IsStringCompareOperation() {
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    return (CompareOperationHintOf(node_->op()) ==
            CompareOperationHint::kString) &&
           BothInputsMaybe(Type::String());
  }

  bool IsSymbolCompareOperation() {
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    return (CompareOperationHintOf(node_->op()) ==
            CompareOperationHint::kSymbol) &&
           BothInputsMaybe(Type::Symbol());
  }

  // Check if a string addition will definitely result in creating a ConsString,
  // i.e. if the combined length of the resulting string exceeds the ConsString
  // minimum length.
  bool ShouldCreateConsString() {
    DCHECK_EQ(IrOpcode::kJSAdd, node_->opcode());
    DCHECK(OneInputIs(Type::String()));
    if (BothInputsAre(Type::String()) ||
        BinaryOperationHintOf(node_->op()) == BinaryOperationHint::kString) {
      HeapObjectBinopMatcher m(node_);
      JSHeapBroker* broker = lowering_->broker();
      if (m.right().HasValue() && m.right().Ref(broker).IsString()) {
        StringRef right_string = m.right().Ref(broker).AsString();
        if (right_string.length() >= ConsString::kMinLength) return true;
      }
      if (m.left().HasValue() && m.left().Ref(broker).IsString()) {
        StringRef left_string = m.left().Ref(broker).AsString();
        if (left_string.length() >= ConsString::kMinLength) {
          // The invariant for ConsString requires the left hand side to be
          // a sequential or external string if the right hand side is the
          // empty string. Since we don't know anything about the right hand
          // side here, we must ensure that the left hand side satisfy the
          // constraints independent of the right hand side.
          return left_string.IsSeqString() || left_string.IsExternalString();
        }
      }
    }
    return false;
  }

  // Inserts a CheckReceiver for the left input.
  void CheckLeftInputToReceiver() {
    Node* left_input = graph()->NewNode(simplified()->CheckReceiver(), left(),
                                        effect(), control());
    node_->ReplaceInput(0, left_input);
    update_effect(left_input);
  }

  // Inserts a CheckReceiverOrNullOrUndefined for the left input.
  void CheckLeftInputToReceiverOrNullOrUndefined() {
    Node* left_input =
        graph()->NewNode(simplified()->CheckReceiverOrNullOrUndefined(), left(),
                         effect(), control());
    node_->ReplaceInput(0, left_input);
    update_effect(left_input);
  }

  // Checks that both inputs are Receiver, and if we don't know
  // statically that one side is already a Receiver, insert a
  // CheckReceiver node.
  void CheckInputsToReceiver() {
    if (!left_type().Is(Type::Receiver())) {
      CheckLeftInputToReceiver();
    }
    if (!right_type().Is(Type::Receiver())) {
      Node* right_input = graph()->NewNode(simplified()->CheckReceiver(),
                                           right(), effect(), control());
      node_->ReplaceInput(1, right_input);
      update_effect(right_input);
    }
  }

  // Checks that both inputs are Receiver, Null or Undefined and if
  // we don't know statically that one side is already a Receiver,
  // Null or Undefined, insert CheckReceiverOrNullOrUndefined nodes.
  void CheckInputsToReceiverOrNullOrUndefined() {
    if (!left_type().Is(Type::ReceiverOrNullOrUndefined())) {
      CheckLeftInputToReceiverOrNullOrUndefined();
    }
    if (!right_type().Is(Type::ReceiverOrNullOrUndefined())) {
      Node* right_input =
          graph()->NewNode(simplified()->CheckReceiverOrNullOrUndefined(),
                           right(), effect(), control());
      node_->ReplaceInput(1, right_input);
      update_effect(right_input);
    }
  }

  // Inserts a CheckSymbol for the left input.
  void CheckLeftInputToSymbol() {
    Node* left_input = graph()->NewNode(simplified()->CheckSymbol(), left(),
                                        effect(), control());
    node_->ReplaceInput(0, left_input);
    update_effect(left_input);
  }

  // Checks that both inputs are Symbol, and if we don't know
  // statically that one side is already a Symbol, insert a
  // CheckSymbol node.
  void CheckInputsToSymbol() {
    if (!left_type().Is(Type::Symbol())) {
      CheckLeftInputToSymbol();
    }
    if (!right_type().Is(Type::Symbol())) {
      Node* right_input = graph()->NewNode(simplified()->CheckSymbol(), right(),
                                           effect(), control());
      node_->ReplaceInput(1, right_input);
      update_effect(right_input);
    }
  }

  // Checks that both inputs are String, and if we don't know
  // statically that one side is already a String, insert a
  // CheckString node.
  void CheckInputsToString() {
    if (!left_type().Is(Type::String())) {
      Node* left_input =
          graph()->NewNode(simplified()->CheckString(FeedbackSource()), left(),
                           effect(), control());
      node_->ReplaceInput(0, left_input);
      update_effect(left_input);
    }
    if (!right_type().Is(Type::String())) {
      Node* right_input =
          graph()->NewNode(simplified()->CheckString(FeedbackSource()), right(),
                           effect(), control());
      node_->ReplaceInput(1, right_input);
      update_effect(right_input);
    }
  }

  // Checks that both inputs are InternalizedString, and if we don't know
  // statically that one side is already an InternalizedString, insert a
  // CheckInternalizedString node.
  void CheckInputsToInternalizedString() {
    if (!left_type().Is(Type::UniqueName())) {
      Node* left_input = graph()->NewNode(
          simplified()->CheckInternalizedString(), left(), effect(), control());
      node_->ReplaceInput(0, left_input);
      update_effect(left_input);
    }
    if (!right_type().Is(Type::UniqueName())) {
      Node* right_input =
          graph()->NewNode(simplified()->CheckInternalizedString(), right(),
                           effect(), control());
      node_->ReplaceInput(1, right_input);
      update_effect(right_input);
    }
  }

  void ConvertInputsToNumber() {
    DCHECK(left_type().Is(Type::PlainPrimitive()));
    DCHECK(right_type().Is(Type::PlainPrimitive()));
    node_->ReplaceInput(0, ConvertPlainPrimitiveToNumber(left()));
    node_->ReplaceInput(1, ConvertPlainPrimitiveToNumber(right()));
  }

  void ConvertInputsToUI32(Signedness left_signedness,
                           Signedness right_signedness) {
    node_->ReplaceInput(0, ConvertToUI32(left(), left_signedness));
    node_->ReplaceInput(1, ConvertToUI32(right(), right_signedness));
  }

  void SwapInputs() {
    Node* l = left();
    Node* r = right();
    node_->ReplaceInput(0, r);
    node_->ReplaceInput(1, l);
  }

  // Remove all effect and control inputs and outputs to this node and change
  // to the pure operator {op}.
  Reduction ChangeToPureOperator(const Operator* op, Type type = Type::Any()) {
    DCHECK_EQ(0, op->EffectInputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(0, op->ControlInputCount());
    DCHECK_EQ(2, op->ValueInputCount());

    // Remove the effects from the node, and update its effect/control usages.
    if (node_->op()->EffectInputCount() > 0) {
      lowering_->RelaxEffectsAndControls(node_);
    }
    // Remove the inputs corresponding to context, effect, and control.
    NodeProperties::RemoveNonValueInputs(node_);
    // Finally, update the operator to the new one.
    NodeProperties::ChangeOp(node_, op);

    // TODO(jarin): Replace the explicit typing hack with a call to some method
    // that encapsulates changing the operator and re-typing.
    Type node_type = NodeProperties::GetType(node_);
    NodeProperties::SetType(node_, Type::Intersect(node_type, type, zone()));

    return lowering_->Changed(node_);
  }

  Reduction ChangeToSpeculativeOperator(const Operator* op, Type upper_bound) {
    DCHECK_EQ(1, op->EffectInputCount());
    DCHECK_EQ(1, op->EffectOutputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(1, op->ControlInputCount());
    DCHECK_EQ(0, op->ControlOutputCount());
    DCHECK_EQ(0, OperatorProperties::GetFrameStateInputCount(op));
    DCHECK_EQ(2, op->ValueInputCount());

    DCHECK_EQ(1, node_->op()->EffectInputCount());
    DCHECK_EQ(1, node_->op()->EffectOutputCount());
    DCHECK_EQ(1, node_->op()->ControlInputCount());
    DCHECK_EQ(2, node_->op()->ValueInputCount());

    // Reconnect the control output to bypass the IfSuccess node and
    // possibly disconnect from the IfException node.
    lowering_->RelaxControls(node_);

    // Remove the frame state and the context.
    if (OperatorProperties::HasFrameStateInput(node_->op())) {
      node_->RemoveInput(NodeProperties::FirstFrameStateIndex(node_));
    }
    node_->RemoveInput(NodeProperties::FirstContextIndex(node_));

    NodeProperties::ChangeOp(node_, op);

    // Update the type to number.
    Type node_type = NodeProperties::GetType(node_);
    NodeProperties::SetType(node_,
                            Type::Intersect(node_type, upper_bound, zone()));

    return lowering_->Changed(node_);
  }

  const Operator* NumberOp() {
    switch (node_->opcode()) {
      case IrOpcode::kJSAdd:
        return simplified()->NumberAdd();
      case IrOpcode::kJSSubtract:
        return simplified()->NumberSubtract();
      case IrOpcode::kJSMultiply:
        return simplified()->NumberMultiply();
      case IrOpcode::kJSDivide:
        return simplified()->NumberDivide();
      case IrOpcode::kJSModulus:
        return simplified()->NumberModulus();
      case IrOpcode::kJSExponentiate:
        return simplified()->NumberPow();
      case IrOpcode::kJSBitwiseAnd:
        return simplified()->NumberBitwiseAnd();
      case IrOpcode::kJSBitwiseOr:
        return simplified()->NumberBitwiseOr();
      case IrOpcode::kJSBitwiseXor:
        return simplified()->NumberBitwiseXor();
      case IrOpcode::kJSShiftLeft:
        return simplified()->NumberShiftLeft();
      case IrOpcode::kJSShiftRight:
        return simplified()->NumberShiftRight();
      case IrOpcode::kJSShiftRightLogical:
        return simplified()->NumberShiftRightLogical();
      default:
        break;
    }
    UNREACHABLE();
  }

  bool LeftInputIs(Type t) { return left_type().Is(t); }

  bool RightInputIs(Type t) { return right_type().Is(t); }

  bool OneInputIs(Type t) { return LeftInputIs(t) || RightInputIs(t); }

  bool BothInputsAre(Type t) { return LeftInputIs(t) && RightInputIs(t); }

  bool BothInputsMaybe(Type t) {
    return left_type().Maybe(t) && right_type().Maybe(t);
  }

  bool OneInputCannotBe(Type t) {
    return !left_type().Maybe(t) || !right_type().Maybe(t);
  }

  bool NeitherInputCanBe(Type t) {
    return !left_type().Maybe(t) && !right_type().Maybe(t);
  }

  Node* effect() { return NodeProperties::GetEffectInput(node_); }
  Node* control() { return NodeProperties::GetControlInput(node_); }
  Node* context() { return NodeProperties::GetContextInput(node_); }
  Node* left() { return NodeProperties::GetValueInput(node_, 0); }
  Node* right() { return NodeProperties::GetValueInput(node_, 1); }
  Type left_type() { return NodeProperties::GetType(node_->InputAt(0)); }
  Type right_type() { return NodeProperties::GetType(node_->InputAt(1)); }
  Type type() { return NodeProperties::GetType(node_); }

  SimplifiedOperatorBuilder* simplified() { return lowering_->simplified(); }
  Graph* graph() const { return lowering_->graph(); }
  JSGraph* jsgraph() { return lowering_->jsgraph(); }
  Isolate* isolate() { return jsgraph()->isolate(); }
  JSOperatorBuilder* javascript() { return lowering_->javascript(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }
  Zone* zone() const { return graph()->zone(); }

 private:
  JSTypedLowering* lowering_;  // The containing lowering instance.
  Node* node_;                 // The original node.

  Node* ConvertPlainPrimitiveToNumber(Node* node) {
    DCHECK(NodeProperties::GetType(node).Is(Type::PlainPrimitive()));
    // Avoid inserting too many eager ToNumber() operations.
    Reduction const reduction = lowering_->ReduceJSToNumberInput(node);
    if (reduction.Changed()) return reduction.replacement();
    if (NodeProperties::GetType(node).Is(Type::Number())) {
      return node;
    }
    return graph()->NewNode(simplified()->PlainPrimitiveToNumber(), node);
  }

  Node* ConvertToUI32(Node* node, Signedness signedness) {
    // Avoid introducing too many eager NumberToXXnt32() operations.
    Type type = NodeProperties::GetType(node);
    if (signedness == kSigned) {
      if (!type.Is(Type::Signed32())) {
        node = graph()->NewNode(simplified()->NumberToInt32(), node);
      }
    } else {
      DCHECK_EQ(kUnsigned, signedness);
      if (!type.Is(Type::Unsigned32())) {
        node = graph()->NewNode(simplified()->NumberToUint32(), node);
      }
    }
    return node;
  }

  void update_effect(Node* effect) {
    NodeProperties::ReplaceEffectInput(node_, effect);
  }
};


// TODO(turbofan): js-typed-lowering improvements possible
// - immediately put in type bounds for all new nodes
// - relax effects from generic but not-side-effecting operations

JSTypedLowering::JSTypedLowering(Editor* editor, JSGraph* jsgraph,
                                 JSHeapBroker* broker, Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      broker_(broker),
      empty_string_type_(Type::HeapConstant(broker, factory()->empty_string(),
                                            graph()->zone())),
      pointer_comparable_type_(
          Type::Union(Type::Oddball(),
                      Type::Union(Type::SymbolOrReceiver(), empty_string_type_,
                                  graph()->zone()),
                      graph()->zone())),
      type_cache_(TypeCache::Get()) {}

Reduction JSTypedLowering::ReduceJSBitwiseNot(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  Type input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::PlainPrimitive())) {
    // JSBitwiseNot(x) => NumberBitwiseXor(ToInt32(x), -1)
    node->InsertInput(graph()->zone(), 1, jsgraph()->SmiConstant(-1));
    NodeProperties::ChangeOp(node, javascript()->BitwiseXor());
    JSBinopReduction r(this, node);
    r.ConvertInputsToNumber();
    r.ConvertInputsToUI32(kSigned, kSigned);
    return r.ChangeToPureOperator(r.NumberOp(), Type::Signed32());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSDecrement(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  Type input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::PlainPrimitive())) {
    // JSDecrement(x) => NumberSubtract(ToNumber(x), 1)
    node->InsertInput(graph()->zone(), 1, jsgraph()->OneConstant());
    NodeProperties::ChangeOp(node, javascript()->Subtract());
    JSBinopReduction r(this, node);
    r.ConvertInputsToNumber();
    DCHECK_EQ(simplified()->NumberSubtract(), r.NumberOp());
    return r.ChangeToPureOperator(r.NumberOp(), Type::Number());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSIncrement(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  Type input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::PlainPrimitive())) {
    // JSIncrement(x) => NumberAdd(ToNumber(x), 1)
    node->InsertInput(graph()->zone(), 1, jsgraph()->OneConstant());
    BinaryOperationHint hint = BinaryOperationHint::kAny;  // Dummy.
    NodeProperties::ChangeOp(node, javascript()->Add(hint));
    JSBinopReduction r(this, node);
    r.ConvertInputsToNumber();
    DCHECK_EQ(simplified()->NumberAdd(), r.NumberOp());
    return r.ChangeToPureOperator(r.NumberOp(), Type::Number());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSNegate(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  Type input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::PlainPrimitive())) {
    // JSNegate(x) => NumberMultiply(ToNumber(x), -1)
    node->InsertInput(graph()->zone(), 1, jsgraph()->SmiConstant(-1));
    NodeProperties::ChangeOp(node, javascript()->Multiply());
    JSBinopReduction r(this, node);
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(r.NumberOp(), Type::Number());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSAdd(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Number())) {
    // JSAdd(x:number, y:number) => NumberAdd(x, y)
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }
  if (r.BothInputsAre(Type::PlainPrimitive()) &&
      r.NeitherInputCanBe(Type::StringOrReceiver())) {
    // JSAdd(x:-string, y:-string) => NumberAdd(ToNumber(x), ToNumber(y))
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }

  // Strength-reduce if one input is already known to be a string.
  if (r.LeftInputIs(Type::String())) {
    // JSAdd(x:string, y) => JSAdd(x, JSToString(y))
    Reduction const reduction = ReduceJSToStringInput(r.right());
    if (reduction.Changed()) {
      NodeProperties::ReplaceValueInput(node, reduction.replacement(), 1);
    }
  } else if (r.RightInputIs(Type::String())) {
    // JSAdd(x, y:string) => JSAdd(JSToString(x), y)
    Reduction const reduction = ReduceJSToStringInput(r.left());
    if (reduction.Changed()) {
      NodeProperties::ReplaceValueInput(node, reduction.replacement(), 0);
    }
  }

  // Always bake in String feedback into the graph.
  if (BinaryOperationHintOf(node->op()) == BinaryOperationHint::kString) {
    r.CheckInputsToString();
  }

  // Strength-reduce concatenation of empty strings if both sides are
  // primitives, as in that case the ToPrimitive on the other side is
  // definitely going to be a no-op.
  if (r.BothInputsAre(Type::Primitive())) {
    if (r.LeftInputIs(empty_string_type_)) {
      // JSAdd("", x:primitive) => JSToString(x)
      NodeProperties::ReplaceValueInputs(node, r.right());
      NodeProperties::ChangeOp(node, javascript()->ToString());
      NodeProperties::SetType(
          node, Type::Intersect(r.type(), Type::String(), graph()->zone()));
      Reduction const reduction = ReduceJSToString(node);
      return reduction.Changed() ? reduction : Changed(node);
    } else if (r.RightInputIs(empty_string_type_)) {
      // JSAdd(x:primitive, "") => JSToString(x)
      NodeProperties::ReplaceValueInputs(node, r.left());
      NodeProperties::ChangeOp(node, javascript()->ToString());
      NodeProperties::SetType(
          node, Type::Intersect(r.type(), Type::String(), graph()->zone()));
      Reduction const reduction = ReduceJSToString(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }

  // Lower to string addition if both inputs are known to be strings.
  if (r.BothInputsAre(Type::String())) {
    Node* context = NodeProperties::GetContextInput(node);
    Node* frame_state = NodeProperties::GetFrameStateInput(node);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    // Compute the resulting length.
    Node* left_length =
        graph()->NewNode(simplified()->StringLength(), r.left());
    Node* right_length =
        graph()->NewNode(simplified()->StringLength(), r.right());
    Node* length =
        graph()->NewNode(simplified()->NumberAdd(), left_length, right_length);

    PropertyCellRef string_length_protector(
        broker(), factory()->string_length_protector());
    if (string_length_protector.value().AsSmi() ==
        Protectors::kProtectorValid) {
      // We can just deoptimize if the {length} is out-of-bounds. Besides
      // generating a shorter code sequence than the version below, this
      // has the additional benefit of not holding on to the lazy {frame_state}
      // and thus potentially reduces the number of live ranges and allows for
      // more truncations.
      length = effect = graph()->NewNode(
          simplified()->CheckBounds(FeedbackSource()), length,
          jsgraph()->Constant(String::kMaxLength + 1), effect, control);
    } else {
      // Check if we would overflow the allowed maximum string length.
      Node* check =
          graph()->NewNode(simplified()->NumberLessThanOrEqual(), length,
                           jsgraph()->Constant(String::kMaxLength));
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* efalse = effect;
      {
        // Throw a RangeError in case of overflow.
        Node* vfalse = efalse = if_false = graph()->NewNode(
            javascript()->CallRuntime(Runtime::kThrowInvalidStringLength),
            context, frame_state, efalse, if_false);

        // Update potential {IfException} uses of {node} to point to the
        // %ThrowInvalidStringLength runtime call node instead.
        Node* on_exception = nullptr;
        if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
          NodeProperties::ReplaceControlInput(on_exception, vfalse);
          NodeProperties::ReplaceEffectInput(on_exception, efalse);
          if_false = graph()->NewNode(common()->IfSuccess(), vfalse);
          Revisit(on_exception);
        }

        // The above %ThrowInvalidStringLength runtime call is an unconditional
        // throw, making it impossible to return a successful completion in this
        // case. We simply connect the successful completion to the graph end.
        if_false = graph()->NewNode(common()->Throw(), efalse, if_false);
        // TODO(bmeurer): This should be on the AdvancedReducer somehow.
        NodeProperties::MergeControlToEnd(graph(), common(), if_false);
        Revisit(graph()->end());
      }
      control = graph()->NewNode(common()->IfTrue(), branch);
      length = effect =
          graph()->NewNode(common()->TypeGuard(type_cache_->kStringLengthType),
                           length, effect, control);
    }

    // TODO(bmeurer): Ideally this should always use StringConcat and decide to
    // optimize to NewConsString later during SimplifiedLowering, but for that
    // to work we need to know that it's safe to create a ConsString.
    Operator const* const op = r.ShouldCreateConsString()
                                   ? simplified()->NewConsString()
                                   : simplified()->StringConcat();
    Node* value = graph()->NewNode(op, length, r.left(), r.right());
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }

  // We never get here when we had String feedback.
  DCHECK_NE(BinaryOperationHint::kString, BinaryOperationHintOf(node->op()));
  if (r.OneInputIs(Type::String())) {
    StringAddFlags flags = STRING_ADD_CHECK_NONE;
    if (!r.LeftInputIs(Type::String())) {
      flags = STRING_ADD_CONVERT_LEFT;
    } else if (!r.RightInputIs(Type::String())) {
      flags = STRING_ADD_CONVERT_RIGHT;
    }
    Operator::Properties properties = node->op()->properties();
    if (r.NeitherInputCanBe(Type::Receiver())) {
      // Both sides are already strings, so we know that the
      // string addition will not cause any observable side
      // effects; it can still throw obviously.
      properties = Operator::kNoWrite | Operator::kNoDeopt;
    }

    // JSAdd(x:string, y) => CallStub[StringAdd](x, y)
    // JSAdd(x, y:string) => CallStub[StringAdd](x, y)
    Callable const callable = CodeFactory::StringAdd(isolate(), flags);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(),
        CallDescriptor::kNeedsFrameState, properties);
    DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
    return Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceNumberBinop(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::PlainPrimitive())) {
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(r.NumberOp(), Type::Number());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceInt32Binop(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::PlainPrimitive())) {
    r.ConvertInputsToNumber();
    r.ConvertInputsToUI32(kSigned, kSigned);
    return r.ChangeToPureOperator(r.NumberOp(), Type::Signed32());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceUI32Shift(Node* node, Signedness signedness) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::PlainPrimitive())) {
    r.ConvertInputsToNumber();
    r.ConvertInputsToUI32(signedness, kUnsigned);
    return r.ChangeToPureOperator(r.NumberOp(), signedness == kUnsigned
                                                    ? Type::Unsigned32()
                                                    : Type::Signed32());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSComparison(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::String())) {
    // If both inputs are definitely strings, perform a string comparison.
    const Operator* stringOp;
    switch (node->opcode()) {
      case IrOpcode::kJSLessThan:
        stringOp = simplified()->StringLessThan();
        break;
      case IrOpcode::kJSGreaterThan:
        stringOp = simplified()->StringLessThan();
        r.SwapInputs();  // a > b => b < a
        break;
      case IrOpcode::kJSLessThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        break;
      case IrOpcode::kJSGreaterThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        r.SwapInputs();  // a >= b => b <= a
        break;
      default:
        return NoChange();
    }
    r.ChangeToPureOperator(stringOp);
    return Changed(node);
  }

  const Operator* less_than;
  const Operator* less_than_or_equal;
  if (r.BothInputsAre(Type::Signed32()) ||
      r.BothInputsAre(Type::Unsigned32())) {
    less_than = simplified()->NumberLessThan();
    less_than_or_equal = simplified()->NumberLessThanOrEqual();
  } else if (r.OneInputCannotBe(Type::StringOrReceiver()) &&
             r.BothInputsAre(Type::PlainPrimitive())) {
    r.ConvertInputsToNumber();
    less_than = simplified()->NumberLessThan();
    less_than_or_equal = simplified()->NumberLessThanOrEqual();
  } else if (r.IsStringCompareOperation()) {
    r.CheckInputsToString();
    less_than = simplified()->StringLessThan();
    less_than_or_equal = simplified()->StringLessThanOrEqual();
  } else {
    return NoChange();
  }
  const Operator* comparison;
  switch (node->opcode()) {
    case IrOpcode::kJSLessThan:
      comparison = less_than;
      break;
    case IrOpcode::kJSGreaterThan:
      comparison = less_than;
      r.SwapInputs();  // a > b => b < a
      break;
    case IrOpcode::kJSLessThanOrEqual:
      comparison = less_than_or_equal;
      break;
    case IrOpcode::kJSGreaterThanOrEqual:
      comparison = less_than_or_equal;
      r.SwapInputs();  // a >= b => b <= a
      break;
    default:
      return NoChange();
  }
  return r.ChangeToPureOperator(comparison);
}

Reduction JSTypedLowering::ReduceJSEqual(Node* node) {
  JSBinopReduction r(this, node);

  if (r.BothInputsAre(Type::UniqueName())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.IsInternalizedStringCompareOperation()) {
    r.CheckInputsToInternalizedString();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual());
  }
  if (r.BothInputsAre(Type::Boolean())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.BothInputsAre(Type::Receiver())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.OneInputIs(Type::Undetectable())) {
    RelaxEffectsAndControls(node);
    node->RemoveInput(r.LeftInputIs(Type::Undetectable()) ? 0 : 1);
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsUndetectable());
    return Changed(node);
  }

  if (r.BothInputsAre(Type::Signed32()) ||
      r.BothInputsAre(Type::Unsigned32())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual());
  } else if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual());
  } else if (r.IsReceiverCompareOperation()) {
    r.CheckInputsToReceiver();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  } else if (r.IsReceiverOrNullOrUndefinedCompareOperation()) {
    // Check that both inputs are Receiver, Null or Undefined.
    r.CheckInputsToReceiverOrNullOrUndefined();

    // If one side is known to be a detectable receiver now, we
    // can simply perform reference equality here, since this
    // known detectable receiver is going to only match itself.
    if (r.OneInputIs(Type::DetectableReceiver())) {
      return r.ChangeToPureOperator(simplified()->ReferenceEqual());
    }

    // Known that both sides are Receiver, Null or Undefined, the
    // abstract equality operation can be performed like this:
    //
    //   if ObjectIsUndetectable(left)
    //     then ObjectIsUndetectable(right)
    //     else ReferenceEqual(left, right)
    //
    Node* left = r.left();
    Node* right = r.right();
    Node* effect = r.effect();
    Node* control = r.control();

    Node* check = graph()->NewNode(simplified()->ObjectIsUndetectable(), left);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* vtrue = graph()->NewNode(simplified()->ObjectIsUndetectable(), right);

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* vfalse =
        graph()->NewNode(simplified()->ReferenceEqual(), left, right);

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    Node* value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue, vfalse, control);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  } else if (r.IsStringCompareOperation()) {
    r.CheckInputsToString();
    return r.ChangeToPureOperator(simplified()->StringEqual());
  } else if (r.IsSymbolCompareOperation()) {
    r.CheckInputsToSymbol();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSStrictEqual(Node* node) {
  JSBinopReduction r(this, node);
  if (r.left() == r.right()) {
    // x === x is always true if x != NaN
    Node* replacement = graph()->NewNode(
        simplified()->BooleanNot(),
        graph()->NewNode(simplified()->ObjectIsNaN(), r.left()));
    ReplaceWithValue(node, replacement);
    return Replace(replacement);
  }
  if (r.OneInputCannotBe(Type::NumericOrString())) {
    // For values with canonical representation (i.e. neither String nor
    // Numeric) an empty type intersection means the values cannot be strictly
    // equal.
    if (!r.left_type().Maybe(r.right_type())) {
      Node* replacement = jsgraph()->FalseConstant();
      ReplaceWithValue(node, replacement);
      return Replace(replacement);
    }
  }

  if (r.BothInputsAre(Type::Unique())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.OneInputIs(pointer_comparable_type_)) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.IsInternalizedStringCompareOperation()) {
    r.CheckInputsToInternalizedString();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual());
  }

  NumberOperationHint hint;
  if (r.BothInputsAre(Type::Signed32()) ||
      r.BothInputsAre(Type::Unsigned32())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual());
  } else if (r.GetCompareNumberOperationHint(&hint)) {
    return r.ChangeToSpeculativeOperator(
        simplified()->SpeculativeNumberEqual(hint), Type::Boolean());
  } else if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual());
  } else if (r.IsReceiverCompareOperation()) {
    // For strict equality, it's enough to know that one input is a Receiver,
    // as a strict equality comparison with a Receiver can only yield true if
    // both sides refer to the same Receiver.
    r.CheckLeftInputToReceiver();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  } else if (r.IsReceiverOrNullOrUndefinedCompareOperation()) {
    // For strict equality, it's enough to know that one input is a Receiver,
    // Null or Undefined, as a strict equality comparison with a Receiver,
    // Null or Undefined can only yield true if both sides refer to the same
    // instance.
    r.CheckLeftInputToReceiverOrNullOrUndefined();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  } else if (r.IsStringCompareOperation()) {
    r.CheckInputsToString();
    return r.ChangeToPureOperator(simplified()->StringEqual());
  } else if (r.IsSymbolCompareOperation()) {
    // For strict equality, it's enough to know that one input is a Symbol,
    // as a strict equality comparison with a Symbol can only yield true if
    // both sides refer to the same Symbol.
    r.CheckLeftInputToSymbol();
    return r.ChangeToPureOperator(simplified()->ReferenceEqual());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToName(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::Name())) {
    // JSToName(x:name) => x
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToLength(Node* node) {
  Node* input = NodeProperties::GetValueInput(node, 0);
  Type input_type = NodeProperties::GetType(input);
  if (input_type.Is(type_cache_->kIntegerOrMinusZero)) {
    if (input_type.IsNone() || input_type.Max() <= 0.0) {
      input = jsgraph()->ZeroConstant();
    } else if (input_type.Min() >= kMaxSafeInteger) {
      input = jsgraph()->Constant(kMaxSafeInteger);
    } else {
      if (input_type.Min() <= 0.0) {
        input = graph()->NewNode(simplified()->NumberMax(),
                                 jsgraph()->ZeroConstant(), input);
      }
      if (input_type.Max() > kMaxSafeInteger) {
        input = graph()->NewNode(simplified()->NumberMin(),
                                 jsgraph()->Constant(kMaxSafeInteger), input);
      }
    }
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToNumberInput(Node* input) {
  // Try constant-folding of JSToNumber with constant inputs.
  Type input_type = NodeProperties::GetType(input);

  if (input_type.Is(Type::String())) {
    HeapObjectMatcher m(input);
    if (m.HasValue() && m.Ref(broker()).IsString()) {
      StringRef input_value = m.Ref(broker()).AsString();
      double number;
      ASSIGN_RETURN_NO_CHANGE_IF_DATA_MISSING(number, input_value.ToNumber());
      return Replace(jsgraph()->Constant(number));
    }
  }
  if (input_type.IsHeapConstant()) {
    HeapObjectRef input_value = input_type.AsHeapConstant()->Ref();
    double value;
    if (input_value.OddballToNumber().To(&value)) {
      return Replace(jsgraph()->Constant(value));
    }
  }
  if (input_type.Is(Type::Number())) {
    // JSToNumber(x:number) => x
    return Changed(input);
  }
  if (input_type.Is(Type::Undefined())) {
    // JSToNumber(undefined) => #NaN
    return Replace(jsgraph()->NaNConstant());
  }
  if (input_type.Is(Type::Null())) {
    // JSToNumber(null) => #0
    return Replace(jsgraph()->ZeroConstant());
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToNumber(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToNumberInput(input);
  if (reduction.Changed()) {
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::PlainPrimitive())) {
    RelaxEffectsAndControls(node);
    node->TrimInputCount(1);
    // For a PlainPrimitive, ToNumeric is the same as ToNumber.
    Type node_type = NodeProperties::GetType(node);
    NodeProperties::SetType(
        node, Type::Intersect(node_type, Type::Number(), graph()->zone()));
    NodeProperties::ChangeOp(node, simplified()->PlainPrimitiveToNumber());
    return Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToNumeric(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::NonBigIntPrimitive())) {
    // ToNumeric(x:primitive\bigint) => ToNumber(x)
    NodeProperties::ChangeOp(node, javascript()->ToNumber());
    Reduction const reduction = ReduceJSToNumber(node);
    return reduction.Changed() ? reduction : Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToStringInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToString) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToString(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToString(JSToString(x)) => JSToString(x)
  }
  Type input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::String())) {
    return Changed(input);  // JSToString(x:string) => x
  }
  if (input_type.Is(Type::Boolean())) {
    return Replace(graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged), input,
        jsgraph()->HeapConstant(factory()->true_string()),
        jsgraph()->HeapConstant(factory()->false_string())));
  }
  if (input_type.Is(Type::Undefined())) {
    return Replace(jsgraph()->HeapConstant(factory()->undefined_string()));
  }
  if (input_type.Is(Type::Null())) {
    return Replace(jsgraph()->HeapConstant(factory()->null_string()));
  }
  if (input_type.Is(Type::NaN())) {
    return Replace(jsgraph()->HeapConstant(factory()->NaN_string()));
  }
  if (input_type.Is(Type::Number())) {
    return Replace(graph()->NewNode(simplified()->NumberToString(), input));
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToString(Node* node) {
  DCHECK_EQ(IrOpcode::kJSToString, node->opcode());
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToStringInput(input);
  if (reduction.Changed()) {
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSToObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSToObject, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type receiver_type = NodeProperties::GetType(receiver);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (receiver_type.Is(Type::Receiver())) {
    ReplaceWithValue(node, receiver, effect, control);
    return Replace(receiver);
  }

  // Check whether {receiver} is a spec object.
  Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), receiver);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* rtrue = receiver;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* rfalse;
  {
    // Convert {receiver} using the ToObjectStub.
    Callable callable = Builtins::CallableFor(isolate(), Builtins::kToObject);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(),
        CallDescriptor::kNeedsFrameState, node->op()->properties());
    rfalse = efalse = if_false =
        graph()->NewNode(common()->Call(call_descriptor),
                         jsgraph()->HeapConstant(callable.code()), receiver,
                         context, frame_state, efalse, if_false);
  }

  // Update potential {IfException} uses of {node} to point to the above
  // ToObject stub call node instead. Note that the stub can only throw on
  // receivers that can be null or undefined.
  Node* on_exception = nullptr;
  if (receiver_type.Maybe(Type::NullOrUndefined()) &&
      NodeProperties::IsExceptionalCall(node, &on_exception)) {
    NodeProperties::ReplaceControlInput(on_exception, if_false);
    NodeProperties::ReplaceEffectInput(on_exception, efalse);
    if_false = graph()->NewNode(common()->IfSuccess(), if_false);
    Revisit(on_exception);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  // Morph the {node} into an appropriate Phi.
  ReplaceWithValue(node, node, effect, control);
  node->ReplaceInput(0, rtrue);
  node->ReplaceInput(1, rfalse);
  node->ReplaceInput(2, control);
  node->TrimInputCount(3);
  NodeProperties::ChangeOp(node,
                           common()->Phi(MachineRepresentation::kTagged, 2));
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type receiver_type = NodeProperties::GetType(receiver);
  NameRef name(broker(), NamedAccessOf(node->op()).name());
  NameRef length_str(broker(), factory()->length_string());
  // Optimize "length" property of strings.
  if (name.equals(length_str) && receiver_type.Is(Type::String())) {
    Node* value = graph()->NewNode(simplified()->StringLength(), receiver);
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSHasInPrototypeChain(Node* node) {
  DCHECK_EQ(IrOpcode::kJSHasInPrototypeChain, node->opcode());
  Node* value = NodeProperties::GetValueInput(node, 0);
  Type value_type = NodeProperties::GetType(value);
  Node* prototype = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // If {value} cannot be a receiver, then it cannot have {prototype} in
  // it's prototype chain (all Primitive values have a null prototype).
  if (value_type.Is(Type::Primitive())) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 = jsgraph()->FalseConstant();

  control = graph()->NewNode(common()->IfFalse(), branch0);

  // Loop through the {value}s prototype chain looking for the {prototype}.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
  NodeProperties::MergeControlToEnd(graph(), common(), terminate);
  Node* vloop = value = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), value, value, loop);
  NodeProperties::SetType(vloop, Type::NonInternal());

  // Load the {value} map and instance type.
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* value_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      effect, control);

  // Check if the {value} is a special receiver, because for special
  // receivers, i.e. proxies or API values that need access checks,
  // we have to use the %HasInPrototypeChain runtime function instead.
  Node* check1 = graph()->NewNode(
      simplified()->NumberLessThanOrEqual(), value_instance_type,
      jsgraph()->Constant(LAST_SPECIAL_RECEIVER_TYPE));
  Node* branch1 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check1, control);

  control = graph()->NewNode(common()->IfFalse(), branch1);

  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* etrue1 = effect;
  Node* vtrue1;

  // Check if the {value} is not a receiver at all.
  Node* check10 =
      graph()->NewNode(simplified()->NumberLessThan(), value_instance_type,
                       jsgraph()->Constant(FIRST_JS_RECEIVER_TYPE));
  Node* branch10 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check10, if_true1);

  // A primitive value cannot match the {prototype} we're looking for.
  if_true1 = graph()->NewNode(common()->IfTrue(), branch10);
  vtrue1 = jsgraph()->FalseConstant();

  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch10);
  Node* efalse1 = etrue1;
  Node* vfalse1;
  {
    // Slow path, need to call the %HasInPrototypeChain runtime function.
    vfalse1 = efalse1 = if_false1 = graph()->NewNode(
        javascript()->CallRuntime(Runtime::kHasInPrototypeChain), value,
        prototype, context, frame_state, efalse1, if_false1);

    // Replace any potential {IfException} uses of {node} to catch
    // exceptions from this %HasInPrototypeChain runtime call instead.
    Node* on_exception = nullptr;
    if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
      NodeProperties::ReplaceControlInput(on_exception, vfalse1);
      NodeProperties::ReplaceEffectInput(on_exception, efalse1);
      if_false1 = graph()->NewNode(common()->IfSuccess(), vfalse1);
      Revisit(on_exception);
    }
  }

  // Load the {value} prototype.
  Node* value_prototype = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapPrototype()), value_map,
      effect, control);

  // Check if we reached the end of {value}s prototype chain.
  Node* check2 = graph()->NewNode(simplified()->ReferenceEqual(),
                                  value_prototype, jsgraph()->NullConstant());
  Node* branch2 = graph()->NewNode(common()->Branch(), check2, control);

  Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
  Node* etrue2 = effect;
  Node* vtrue2 = jsgraph()->FalseConstant();

  control = graph()->NewNode(common()->IfFalse(), branch2);

  // Check if we reached the {prototype}.
  Node* check3 = graph()->NewNode(simplified()->ReferenceEqual(),
                                  value_prototype, prototype);
  Node* branch3 = graph()->NewNode(common()->Branch(), check3, control);

  Node* if_true3 = graph()->NewNode(common()->IfTrue(), branch3);
  Node* etrue3 = effect;
  Node* vtrue3 = jsgraph()->TrueConstant();

  control = graph()->NewNode(common()->IfFalse(), branch3);

  // Close the loop.
  vloop->ReplaceInput(1, value_prototype);
  eloop->ReplaceInput(1, effect);
  loop->ReplaceInput(1, control);

  control = graph()->NewNode(common()->Merge(5), if_true0, if_true1, if_true2,
                             if_true3, if_false1);
  effect = graph()->NewNode(common()->EffectPhi(5), etrue0, etrue1, etrue2,
                            etrue3, efalse1, control);

  // Morph the {node} into an appropriate Phi.
  ReplaceWithValue(node, node, effect, control);
  node->ReplaceInput(0, vtrue0);
  node->ReplaceInput(1, vtrue1);
  node->ReplaceInput(2, vtrue2);
  node->ReplaceInput(3, vtrue3);
  node->ReplaceInput(4, vfalse1);
  node->ReplaceInput(5, control);
  node->TrimInputCount(6);
  NodeProperties::ChangeOp(node,
                           common()->Phi(MachineRepresentation::kTagged, 5));
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSOrdinaryHasInstance(Node* node) {
  DCHECK_EQ(IrOpcode::kJSOrdinaryHasInstance, node->opcode());
  Node* constructor = NodeProperties::GetValueInput(node, 0);
  Type constructor_type = NodeProperties::GetType(constructor);
  Node* object = NodeProperties::GetValueInput(node, 1);
  Type object_type = NodeProperties::GetType(object);

  // Check if the {constructor} cannot be callable.
  // See ES6 section 7.3.19 OrdinaryHasInstance ( C, O ) step 1.
  if (!constructor_type.Maybe(Type::Callable())) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  // If the {constructor} cannot be a JSBoundFunction and then {object}
  // cannot be a JSReceiver, then this can be constant-folded to false.
  // See ES6 section 7.3.19 OrdinaryHasInstance ( C, O ) step 2 and 3.
  if (!object_type.Maybe(Type::Receiver()) &&
      !constructor_type.Maybe(Type::BoundFunction())) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  return NoChange();
}

Reduction JSTypedLowering::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    context = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlotKnownPointer(Context::PREVIOUS_INDEX)),
        context, effect, control);
  }
  node->ReplaceInput(0, context);
  node->ReplaceInput(1, effect);
  node->AppendInput(jsgraph()->zone(), control);
  NodeProperties::ChangeOp(
      node,
      simplified()->LoadField(AccessBuilder::ForContextSlot(access.index())));
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSStoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* control = graph()->start();
  Node* value = NodeProperties::GetValueInput(node, 0);
  for (size_t i = 0; i < access.depth(); ++i) {
    context = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlotKnownPointer(Context::PREVIOUS_INDEX)),
        context, effect, control);
  }
  node->ReplaceInput(0, context);
  node->ReplaceInput(1, value);
  node->ReplaceInput(2, effect);
  NodeProperties::ChangeOp(
      node,
      simplified()->StoreField(AccessBuilder::ForContextSlot(access.index())));
  return Changed(node);
}

Node* JSTypedLowering::BuildGetModuleCell(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadModule ||
         node->opcode() == IrOpcode::kJSStoreModule);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  int32_t cell_index = OpParameter<int32_t>(node->op());
  Node* module = NodeProperties::GetValueInput(node, 0);
  Type module_type = NodeProperties::GetType(module);

  if (module_type.IsHeapConstant()) {
    SourceTextModuleRef module_constant =
        module_type.AsHeapConstant()->Ref().AsSourceTextModule();
    base::Optional<CellRef> cell_constant = module_constant.GetCell(cell_index);
    if (cell_constant.has_value()) return jsgraph()->Constant(*cell_constant);
  }

  FieldAccess field_access;
  int index;
  if (SourceTextModuleDescriptor::GetCellIndexKind(cell_index) ==
      SourceTextModuleDescriptor::kExport) {
    field_access = AccessBuilder::ForModuleRegularExports();
    index = cell_index - 1;
  } else {
    DCHECK_EQ(SourceTextModuleDescriptor::GetCellIndexKind(cell_index),
              SourceTextModuleDescriptor::kImport);
    field_access = AccessBuilder::ForModuleRegularImports();
    index = -cell_index - 1;
  }
  Node* array = effect = graph()->NewNode(simplified()->LoadField(field_access),
                                          module, effect, control);
  return graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForFixedArraySlot(index)), array,
      effect, control);
}

Reduction JSTypedLowering::ReduceJSLoadModule(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadModule, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* cell = BuildGetModuleCell(node);
  if (cell->op()->EffectOutputCount() > 0) effect = cell;
  Node* value = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForCellValue()),
                       cell, effect, control);

  ReplaceWithValue(node, value, effect, control);
  return Changed(value);
}

Reduction JSTypedLowering::ReduceJSStoreModule(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreModule, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* value = NodeProperties::GetValueInput(node, 1);
  DCHECK_EQ(SourceTextModuleDescriptor::GetCellIndexKind(
                OpParameter<int32_t>(node->op())),
            SourceTextModuleDescriptor::kExport);

  Node* cell = BuildGetModuleCell(node);
  if (cell->op()->EffectOutputCount() > 0) effect = cell;
  effect =
      graph()->NewNode(simplified()->StoreField(AccessBuilder::ForCellValue()),
                       cell, value, effect, control);

  ReplaceWithValue(node, effect, effect, control);
  return Changed(value);
}

namespace {

void ReduceBuiltin(JSGraph* jsgraph, Node* node, int builtin_index, int arity,
                   CallDescriptor::Flags flags) {
  // Patch {node} to a direct CEntry call.
  //
  // ----------- A r g u m e n t s -----------
  // -- 0: CEntry
  // --- Stack args ---
  // -- 1: receiver
  // -- [2, 2 + n[: the n actual arguments passed to the builtin
  // -- 2 + n: argc, including the receiver and implicit args (Smi)
  // -- 2 + n + 1: target
  // -- 2 + n + 2: new_target
  // --- Register args ---
  // -- 2 + n + 3: the C entry point
  // -- 2 + n + 4: argc (Int32)
  // -----------------------------------

  // The logic contained here is mirrored in Builtins::Generate_Adaptor.
  // Keep these in sync.

  const bool is_construct = (node->opcode() == IrOpcode::kJSConstruct);

  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = is_construct
                         ? NodeProperties::GetValueInput(node, arity + 1)
                         : jsgraph->UndefinedConstant();

  // CPP builtins are implemented in C++, and we can inline it.
  // CPP builtins create a builtin exit frame.
  DCHECK(Builtins::IsCpp(builtin_index));
  const bool has_builtin_exit_frame = true;

  Node* stub = jsgraph->CEntryStubConstant(1, kDontSaveFPRegs, kArgvOnStack,
                                           has_builtin_exit_frame);
  node->ReplaceInput(0, stub);

  Zone* zone = jsgraph->zone();
  if (is_construct) {
    // Unify representations between construct and call nodes.
    // Remove new target and add receiver as a stack parameter.
    Node* receiver = jsgraph->UndefinedConstant();
    node->RemoveInput(arity + 1);
    node->InsertInput(zone, 1, receiver);
  }

  const int argc = arity + BuiltinArguments::kNumExtraArgsWithReceiver;
  Node* argc_node = jsgraph->Constant(argc);

  static const int kStubAndReceiver = 2;
  int cursor = arity + kStubAndReceiver;
  node->InsertInput(zone, cursor++, jsgraph->PaddingConstant());
  node->InsertInput(zone, cursor++, argc_node);
  node->InsertInput(zone, cursor++, target);
  node->InsertInput(zone, cursor++, new_target);

  Address entry = Builtins::CppEntryOf(builtin_index);
  ExternalReference entry_ref = ExternalReference::Create(entry);
  Node* entry_node = jsgraph->ExternalConstant(entry_ref);

  node->InsertInput(zone, cursor++, entry_node);
  node->InsertInput(zone, cursor++, argc_node);

  static const int kReturnCount = 1;
  const char* debug_name = Builtins::name(builtin_index);
  Operator::Properties properties = node->op()->properties();
  auto call_descriptor = Linkage::GetCEntryStubCallDescriptor(
      zone, kReturnCount, argc, debug_name, properties, flags);

  NodeProperties::ChangeOp(node, jsgraph->common()->Call(call_descriptor));
}

bool NeedsArgumentAdaptorFrame(SharedFunctionInfoRef shared, int arity) {
  static const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
  const int num_decl_parms = shared.internal_formal_parameter_count();
  return (num_decl_parms != arity && num_decl_parms != sentinel);
}

}  // namespace

Reduction JSTypedLowering::ReduceJSConstructForwardVarargs(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructForwardVarargs, node->opcode());
  ConstructForwardVarargsParameters p =
      ConstructForwardVarargsParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  int const start_index = static_cast<int>(p.start_index());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type target_type = NodeProperties::GetType(target);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);

  // Check if {target} is a JSFunction.
  if (target_type.IsHeapConstant() &&
      target_type.AsHeapConstant()->Ref().IsJSFunction()) {
    // Only optimize [[Construct]] here if {function} is a Constructor.
    JSFunctionRef function = target_type.AsHeapConstant()->Ref().AsJSFunction();
    if (!function.map().is_constructor()) return NoChange();
    // Patch {node} to an indirect call via ConstructFunctionForwardVarargs.
    Callable callable = CodeFactory::ConstructFunctionForwardVarargs(isolate());
    node->RemoveInput(arity + 1);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, new_target);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->Constant(start_index));
    node->InsertInput(graph()->zone(), 5, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  graph()->zone(), callable.descriptor(), arity + 1,
                  CallDescriptor::kNeedsFrameState)));
    return Changed(node);
  }

  return NoChange();
}

Reduction JSTypedLowering::ReduceJSConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstruct, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type target_type = NodeProperties::GetType(target);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);

  // Check if {target} is a known JSFunction.
  if (target_type.IsHeapConstant() &&
      target_type.AsHeapConstant()->Ref().IsJSFunction()) {
    JSFunctionRef function = target_type.AsHeapConstant()->Ref().AsJSFunction();

    // Only optimize [[Construct]] here if {function} is a Constructor.
    if (!function.map().is_constructor()) return NoChange();

    if (!function.serialized()) {
      TRACE_BROKER_MISSING(broker(), "data for function " << function);
      return NoChange();
    }

    // Patch {node} to an indirect call via the {function}s construct stub.
    bool use_builtin_construct_stub = function.shared().construct_as_builtin();
    CodeRef code(broker(),
                 use_builtin_construct_stub
                     ? BUILTIN_CODE(isolate(), JSBuiltinsConstructStub)
                     : BUILTIN_CODE(isolate(), JSConstructStubGeneric));
    node->RemoveInput(arity + 1);
    node->InsertInput(graph()->zone(), 0, jsgraph()->Constant(code));
    node->InsertInput(graph()->zone(), 2, new_target);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    node->InsertInput(graph()->zone(), 5, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  graph()->zone(), ConstructStubDescriptor{}, 1 + arity,
                  CallDescriptor::kNeedsFrameState)));
    return Changed(node);
  }

  return NoChange();
}

Reduction JSTypedLowering::ReduceJSCallForwardVarargs(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallForwardVarargs, node->opcode());
  CallForwardVarargsParameters p = CallForwardVarargsParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  int const start_index = static_cast<int>(p.start_index());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type target_type = NodeProperties::GetType(target);

  // Check if {target} is a JSFunction.
  if (target_type.Is(Type::Function())) {
    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    // Patch {node} to an indirect call via CallFunctionForwardVarargs.
    Callable callable = CodeFactory::CallFunctionForwardVarargs(isolate());
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->Constant(arity));
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(start_index));
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  graph()->zone(), callable.descriptor(), arity + 1, flags)));
    return Changed(node);
  }

  return NoChange();
}

Reduction JSTypedLowering::ReduceJSCall(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  ConvertReceiverMode convert_mode = p.convert_mode();
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type target_type = NodeProperties::GetType(target);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Type receiver_type = NodeProperties::GetType(receiver);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Try to infer receiver {convert_mode} from {receiver} type.
  if (receiver_type.Is(Type::NullOrUndefined())) {
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else if (!receiver_type.Maybe(Type::NullOrUndefined())) {
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  // Check if {target} is a known JSFunction.
  if (target_type.IsHeapConstant() &&
      target_type.AsHeapConstant()->Ref().IsJSFunction()) {
    JSFunctionRef function = target_type.AsHeapConstant()->Ref().AsJSFunction();

    if (!function.serialized()) {
      TRACE_BROKER_MISSING(broker(), "data for function " << function);
      return NoChange();
    }
    SharedFunctionInfoRef shared = function.shared();

    // Do not inline the call if we need to check whether to break at entry.
    if (shared.HasBreakInfo()) return NoChange();

    // Class constructors are callable, but [[Call]] will raise an exception.
    // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
    if (IsClassConstructor(shared.kind())) return NoChange();

    // Check if we need to convert the {receiver}, but bailout if it would
    // require data from a foreign native context.
    if (is_sloppy(shared.language_mode()) && !shared.native() &&
        !receiver_type.Is(Type::Receiver())) {
      if (!function.native_context().equals(
              broker()->target_native_context())) {
        return NoChange();
      }
      Node* global_proxy =
          jsgraph()->Constant(function.native_context().global_proxy_object());
      receiver = effect =
          graph()->NewNode(simplified()->ConvertReceiver(convert_mode),
                           receiver, global_proxy, effect, control);
      NodeProperties::ReplaceValueInput(node, receiver, 1);
    }

    // Load the context from the {target}.
    Node* context = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSFunctionContext()), target,
        effect, control);
    NodeProperties::ReplaceContextInput(node, context);

    // Update the effect dependency for the {node}.
    NodeProperties::ReplaceEffectInput(node, effect);

    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    Node* new_target = jsgraph()->UndefinedConstant();

    if (NeedsArgumentAdaptorFrame(shared, arity)) {
      // Check if it's safe to skip the arguments adaptor for {shared},
      // that is whether the target function anyways cannot observe the
      // actual arguments. Details can be found in this document at
      // https://bit.ly/v8-faster-calls-with-arguments-mismatch and
      // on the tracking bug at https://crbug.com/v8/8895
      if (shared.is_safe_to_skip_arguments_adaptor()) {
        // Currently we only support skipping arguments adaptor frames
        // for strict mode functions, since there's Function.arguments
        // legacy accessor, which is still available in sloppy mode.
        DCHECK_EQ(LanguageMode::kStrict, shared.language_mode());

        // Massage the arguments to match the expected number of arguments.
        int expected_argument_count = shared.internal_formal_parameter_count();
        for (; arity > expected_argument_count; --arity) {
          node->RemoveInput(arity + 1);
        }
        for (; arity < expected_argument_count; ++arity) {
          node->InsertInput(graph()->zone(), arity + 2,
                            jsgraph()->UndefinedConstant());
        }

        // Patch {node} to a direct call.
        node->InsertInput(graph()->zone(), arity + 2, new_target);
        node->InsertInput(graph()->zone(), arity + 3,
                          jsgraph()->Constant(arity));
        NodeProperties::ChangeOp(node,
                                 common()->Call(Linkage::GetJSCallDescriptor(
                                     graph()->zone(), false, 1 + arity,
                                     flags | CallDescriptor::kCanUseRoots)));
      } else {
        // Patch {node} to an indirect call via the ArgumentsAdaptorTrampoline.
        Callable callable = CodeFactory::ArgumentAdaptor(isolate());
        node->InsertInput(graph()->zone(), 0,
                          jsgraph()->HeapConstant(callable.code()));
        node->InsertInput(graph()->zone(), 2, new_target);
        node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
        node->InsertInput(
            graph()->zone(), 4,
            jsgraph()->Constant(shared.internal_formal_parameter_count()));
        NodeProperties::ChangeOp(
            node,
            common()->Call(Linkage::GetStubCallDescriptor(
                graph()->zone(), callable.descriptor(), 1 + arity, flags)));
      }
    } else if (shared.HasBuiltinId() && Builtins::IsCpp(shared.builtin_id())) {
      // Patch {node} to a direct CEntry call.
      ReduceBuiltin(jsgraph(), node, shared.builtin_id(), arity, flags);
    } else if (shared.HasBuiltinId() &&
               Builtins::KindOf(shared.builtin_id()) == Builtins::TFJ) {
      // Patch {node} to a direct code object call.
      Callable callable = Builtins::CallableFor(
          isolate(), static_cast<Builtins::Name>(shared.builtin_id()));
      CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;

      const CallInterfaceDescriptor& descriptor = callable.descriptor();
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          graph()->zone(), descriptor, 1 + arity, flags);
      Node* stub_code = jsgraph()->HeapConstant(callable.code());
      node->InsertInput(graph()->zone(), 0, stub_code);  // Code object.
      node->InsertInput(graph()->zone(), 2, new_target);
      node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
      NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
    } else {
      // Patch {node} to a direct call.
      node->InsertInput(graph()->zone(), arity + 2, new_target);
      node->InsertInput(graph()->zone(), arity + 3, jsgraph()->Constant(arity));
      NodeProperties::ChangeOp(node,
                               common()->Call(Linkage::GetJSCallDescriptor(
                                   graph()->zone(), false, 1 + arity,
                                   flags | CallDescriptor::kCanUseRoots)));
    }
    return Changed(node);
  }

  // Check if {target} is a JSFunction.
  if (target_type.Is(Type::Function())) {
    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    // Patch {node} to an indirect call via the CallFunction builtin.
    Callable callable = CodeFactory::CallFunction(isolate(), convert_mode);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->Constant(arity));
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  graph()->zone(), callable.descriptor(), 1 + arity, flags)));
    return Changed(node);
  }

  // Maybe we did at least learn something about the {receiver}.
  if (p.convert_mode() != convert_mode) {
    NodeProperties::ChangeOp(
        node, javascript()->Call(p.arity(), p.frequency(), p.feedback(),
                                 convert_mode, p.speculation_mode()));
    return Changed(node);
  }

  return NoChange();
}

Reduction JSTypedLowering::ReduceJSForInNext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInNext, node->opcode());
  ForInMode const mode = ForInModeOf(node->op());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* cache_array = NodeProperties::GetValueInput(node, 1);
  Node* cache_type = NodeProperties::GetValueInput(node, 2);
  Node* index = NodeProperties::GetValueInput(node, 3);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Load the map of the {receiver}.
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);

  switch (mode) {
    case ForInMode::kUseEnumCacheKeys:
    case ForInMode::kUseEnumCacheKeysAndIndices: {
      // Ensure that the expected map still matches that of the {receiver}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(),
                                     receiver_map, cache_type);
      effect =
          graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kWrongMap),
                           check, effect, control);

      // Since the change to LoadElement() below is effectful, we connect
      // node to all effect uses.
      ReplaceWithValue(node, node, node, control);

      // Morph the {node} into a LoadElement.
      node->ReplaceInput(0, cache_array);
      node->ReplaceInput(1, index);
      node->ReplaceInput(2, effect);
      node->ReplaceInput(3, control);
      node->TrimInputCount(4);
      NodeProperties::ChangeOp(
          node,
          simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()));
      NodeProperties::SetType(node, Type::InternalizedString());
      break;
    }
    case ForInMode::kGeneric: {
      // Load the next {key} from the {cache_array}.
      Node* key = effect = graph()->NewNode(
          simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
          cache_array, index, effect, control);

      // Check if the expected map still matches that of the {receiver}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(),
                                     receiver_map, cache_type);
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

      Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
      Node* etrue;
      Node* vtrue;
      {
        // Don't need filtering since expected map still matches that of the
        // {receiver}.
        etrue = effect;
        vtrue = key;
      }

      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* efalse;
      Node* vfalse;
      {
        // Filter the {key} to check if it's still a valid property of the
        // {receiver} (does the ToName conversion implicitly).
        Callable const callable =
            Builtins::CallableFor(isolate(), Builtins::kForInFilter);
        auto call_descriptor = Linkage::GetStubCallDescriptor(
            graph()->zone(), callable.descriptor(),
            callable.descriptor().GetStackParameterCount(),
            CallDescriptor::kNeedsFrameState);
        vfalse = efalse = if_false =
            graph()->NewNode(common()->Call(call_descriptor),
                             jsgraph()->HeapConstant(callable.code()), key,
                             receiver, context, frame_state, effect, if_false);

        // Update potential {IfException} uses of {node} to point to the above
        // ForInFilter stub call node instead.
        Node* if_exception = nullptr;
        if (NodeProperties::IsExceptionalCall(node, &if_exception)) {
          if_false = graph()->NewNode(common()->IfSuccess(), vfalse);
          NodeProperties::ReplaceControlInput(if_exception, vfalse);
          NodeProperties::ReplaceEffectInput(if_exception, efalse);
          Revisit(if_exception);
        }
      }

      control = graph()->NewNode(common()->Merge(2), if_true, if_false);
      effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
      ReplaceWithValue(node, node, effect, control);

      // Morph the {node} into a Phi.
      node->ReplaceInput(0, vtrue);
      node->ReplaceInput(1, vfalse);
      node->ReplaceInput(2, control);
      node->TrimInputCount(3);
      NodeProperties::ChangeOp(
          node, common()->Phi(MachineRepresentation::kTagged, 2));
    }
  }

  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSForInPrepare(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInPrepare, node->opcode());
  ForInMode const mode = ForInModeOf(node->op());
  Node* enumerator = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* cache_type = enumerator;
  Node* cache_array = nullptr;
  Node* cache_length = nullptr;

  switch (mode) {
    case ForInMode::kUseEnumCacheKeys:
    case ForInMode::kUseEnumCacheKeysAndIndices: {
      // Check that the {enumerator} is a Map.
      effect = graph()->NewNode(
          simplified()->CheckMaps(CheckMapsFlag::kNone,
                                  ZoneHandleSet<Map>(factory()->meta_map())),
          enumerator, effect, control);

      // Load the enum cache from the {enumerator} map.
      Node* descriptor_array = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapDescriptors()),
          enumerator, effect, control);
      Node* enum_cache = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForDescriptorArrayEnumCache()),
          descriptor_array, effect, control);
      cache_array = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForEnumCacheKeys()),
          enum_cache, effect, control);

      // Load the enum length of the {enumerator} map.
      Node* bit_field3 = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapBitField3()), enumerator,
          effect, control);
      STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
      cache_length =
          graph()->NewNode(simplified()->NumberBitwiseAnd(), bit_field3,
                           jsgraph()->Constant(Map::EnumLengthBits::kMask));
      break;
    }
    case ForInMode::kGeneric: {
      // Check if the {enumerator} is a Map or a FixedArray.
      Node* check = effect = graph()->NewNode(
          simplified()->CompareMaps(ZoneHandleSet<Map>(factory()->meta_map())),
          enumerator, effect, control);
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

      Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
      Node* etrue = effect;
      Node* cache_array_true;
      Node* cache_length_true;
      {
        // Load the enum cache from the {enumerator} map.
        Node* descriptor_array = etrue = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForMapDescriptors()),
            enumerator, etrue, if_true);
        Node* enum_cache = etrue =
            graph()->NewNode(simplified()->LoadField(
                                 AccessBuilder::ForDescriptorArrayEnumCache()),
                             descriptor_array, etrue, if_true);
        cache_array_true = etrue = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForEnumCacheKeys()),
            enum_cache, etrue, if_true);

        // Load the enum length of the {enumerator} map.
        Node* bit_field3 = etrue = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForMapBitField3()),
            enumerator, etrue, if_true);
        STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
        cache_length_true =
            graph()->NewNode(simplified()->NumberBitwiseAnd(), bit_field3,
                             jsgraph()->Constant(Map::EnumLengthBits::kMask));
      }

      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* efalse = effect;
      Node* cache_array_false;
      Node* cache_length_false;
      {
        // The {enumerator} is the FixedArray with the keys to iterate.
        cache_array_false = enumerator;
        cache_length_false = efalse = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForFixedArrayLength()),
            cache_array_false, efalse, if_false);
      }

      // Rewrite the uses of the {node}.
      control = graph()->NewNode(common()->Merge(2), if_true, if_false);
      effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
      cache_array =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           cache_array_true, cache_array_false, control);
      cache_length =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           cache_length_true, cache_length_false, control);
      break;
    }
  }

  // Update the uses of {node}.
  for (Edge edge : node->use_edges()) {
    Node* const user = edge.from();
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
      Revisit(user);
    } else if (NodeProperties::IsControlEdge(edge)) {
      edge.UpdateTo(control);
      Revisit(user);
    } else {
      DCHECK(NodeProperties::IsValueEdge(edge));
      switch (ProjectionIndexOf(user->op())) {
        case 0:
          Replace(user, cache_type);
          break;
        case 1:
          Replace(user, cache_array);
          break;
        case 2:
          Replace(user, cache_length);
          break;
        default:
          UNREACHABLE();
      }
    }
  }
  node->Kill();
  return Replace(effect);
}

Reduction JSTypedLowering::ReduceJSLoadMessage(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadMessage, node->opcode());
  ExternalReference const ref =
      ExternalReference::address_of_pending_message_obj(isolate());
  node->ReplaceInput(0, jsgraph()->ExternalConstant(ref));
  NodeProperties::ChangeOp(node, simplified()->LoadMessage());
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSStoreMessage(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreMessage, node->opcode());
  ExternalReference const ref =
      ExternalReference::address_of_pending_message_obj(isolate());
  Node* value = NodeProperties::GetValueInput(node, 0);
  node->ReplaceInput(0, jsgraph()->ExternalConstant(ref));
  node->ReplaceInput(1, value);
  NodeProperties::ChangeOp(node, simplified()->StoreMessage());
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSGeneratorStore(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorStore, node->opcode());
  Node* generator = NodeProperties::GetValueInput(node, 0);
  Node* continuation = NodeProperties::GetValueInput(node, 1);
  Node* offset = NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  int value_count = GeneratorStoreValueCountOf(node->op());

  FieldAccess array_field =
      AccessBuilder::ForJSGeneratorObjectParametersAndRegisters();
  FieldAccess context_field = AccessBuilder::ForJSGeneratorObjectContext();
  FieldAccess continuation_field =
      AccessBuilder::ForJSGeneratorObjectContinuation();
  FieldAccess input_or_debug_pos_field =
      AccessBuilder::ForJSGeneratorObjectInputOrDebugPos();

  Node* array = effect = graph()->NewNode(simplified()->LoadField(array_field),
                                          generator, effect, control);

  for (int i = 0; i < value_count; ++i) {
    Node* value = NodeProperties::GetValueInput(node, 3 + i);
    if (value != jsgraph()->OptimizedOutConstant()) {
      effect = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForFixedArraySlot(i)), array,
          value, effect, control);
    }
  }

  effect = graph()->NewNode(simplified()->StoreField(context_field), generator,
                            context, effect, control);
  effect = graph()->NewNode(simplified()->StoreField(continuation_field),
                            generator, continuation, effect, control);
  effect = graph()->NewNode(simplified()->StoreField(input_or_debug_pos_field),
                            generator, offset, effect, control);

  ReplaceWithValue(node, effect, effect, control);
  return Changed(effect);
}

Reduction JSTypedLowering::ReduceJSGeneratorRestoreContinuation(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreContinuation, node->opcode());
  Node* generator = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  FieldAccess continuation_field =
      AccessBuilder::ForJSGeneratorObjectContinuation();

  Node* continuation = effect = graph()->NewNode(
      simplified()->LoadField(continuation_field), generator, effect, control);
  Node* executing = jsgraph()->Constant(JSGeneratorObject::kGeneratorExecuting);
  effect = graph()->NewNode(simplified()->StoreField(continuation_field),
                            generator, executing, effect, control);

  ReplaceWithValue(node, continuation, effect, control);
  return Changed(continuation);
}

Reduction JSTypedLowering::ReduceJSGeneratorRestoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreContext, node->opcode());

  const Operator* new_op =
      simplified()->LoadField(AccessBuilder::ForJSGeneratorObjectContext());

  // Mutate the node in-place.
  DCHECK(OperatorProperties::HasContextInput(node->op()));
  DCHECK(!OperatorProperties::HasContextInput(new_op));
  node->RemoveInput(NodeProperties::FirstContextIndex(node));

  NodeProperties::ChangeOp(node, new_op);
  return Changed(node);
}

Reduction JSTypedLowering::ReduceJSGeneratorRestoreRegister(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreRegister, node->opcode());
  Node* generator = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  int index = RestoreRegisterIndexOf(node->op());

  FieldAccess array_field =
      AccessBuilder::ForJSGeneratorObjectParametersAndRegisters();
  FieldAccess element_field = AccessBuilder::ForFixedArraySlot(index);

  Node* array = effect = graph()->NewNode(simplified()->LoadField(array_field),
                                          generator, effect, control);
  Node* element = effect = graph()->NewNode(
      simplified()->LoadField(element_field), array, effect, control);
  Node* stale = jsgraph()->StaleRegisterConstant();
  effect = graph()->NewNode(simplified()->StoreField(element_field), array,
                            stale, effect, control);

  ReplaceWithValue(node, element, effect, control);
  return Changed(element);
}

Reduction JSTypedLowering::ReduceJSGeneratorRestoreInputOrDebugPos(Node* node) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreInputOrDebugPos, node->opcode());

  FieldAccess input_or_debug_pos_field =
      AccessBuilder::ForJSGeneratorObjectInputOrDebugPos();
  const Operator* new_op = simplified()->LoadField(input_or_debug_pos_field);

  // Mutate the node in-place.
  DCHECK(OperatorProperties::HasContextInput(node->op()));
  DCHECK(!OperatorProperties::HasContextInput(new_op));
  node->RemoveInput(NodeProperties::FirstContextIndex(node));

  NodeProperties::ChangeOp(node, new_op);
  return Changed(node);
}

Reduction JSTypedLowering::ReduceObjectIsArray(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Type value_type = NodeProperties::GetType(value);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Constant-fold based on {value} type.
  if (value_type.Is(Type::Array())) {
    Node* value = jsgraph()->TrueConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  } else if (!value_type.Maybe(Type::ArrayOrProxy())) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  int count = 0;
  Node* values[5];
  Node* effects[5];
  Node* controls[4];

  // Check if the {value} is a Smi.
  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  control =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  // The {value} is a Smi.
  controls[count] = graph()->NewNode(common()->IfTrue(), control);
  effects[count] = effect;
  values[count] = jsgraph()->FalseConstant();
  count++;

  control = graph()->NewNode(common()->IfFalse(), control);

  // Load the {value}s instance type.
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* value_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      effect, control);

  // Check if the {value} is a JSArray.
  check = graph()->NewNode(simplified()->NumberEqual(), value_instance_type,
                           jsgraph()->Constant(JS_ARRAY_TYPE));
  control = graph()->NewNode(common()->Branch(), check, control);

  // The {value} is a JSArray.
  controls[count] = graph()->NewNode(common()->IfTrue(), control);
  effects[count] = effect;
  values[count] = jsgraph()->TrueConstant();
  count++;

  control = graph()->NewNode(common()->IfFalse(), control);

  // Check if the {value} is a JSProxy.
  check = graph()->NewNode(simplified()->NumberEqual(), value_instance_type,
                           jsgraph()->Constant(JS_PROXY_TYPE));
  control =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  // The {value} is neither a JSArray nor a JSProxy.
  controls[count] = graph()->NewNode(common()->IfFalse(), control);
  effects[count] = effect;
  values[count] = jsgraph()->FalseConstant();
  count++;

  control = graph()->NewNode(common()->IfTrue(), control);

  // Let the %ArrayIsArray runtime function deal with the JSProxy {value}.
  value = effect = control =
      graph()->NewNode(javascript()->CallRuntime(Runtime::kArrayIsArray), value,
                       context, frame_state, effect, control);
  NodeProperties::SetType(value, Type::Boolean());

  // Update potential {IfException} uses of {node} to point to the above
  // %ArrayIsArray runtime call node instead.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    NodeProperties::ReplaceControlInput(on_exception, control);
    NodeProperties::ReplaceEffectInput(on_exception, effect);
    control = graph()->NewNode(common()->IfSuccess(), control);
    Revisit(on_exception);
  }

  // The {value} is neither a JSArray nor a JSProxy.
  controls[count] = control;
  effects[count] = effect;
  values[count] = value;
  count++;

  control = graph()->NewNode(common()->Merge(count), count, controls);
  effects[count] = control;
  values[count] = control;
  effect = graph()->NewNode(common()->EffectPhi(count), count + 1, effects);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                           count + 1, values);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSTypedLowering::ReduceJSParseInt(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Type value_type = NodeProperties::GetType(value);
  Node* radix = NodeProperties::GetValueInput(node, 1);
  Type radix_type = NodeProperties::GetType(radix);
  // We need kTenOrUndefined and kZeroOrUndefined because
  // the type representing {0,10} would become the range 1-10.
  if (value_type.Is(type_cache_->kSafeInteger) &&
      (radix_type.Is(type_cache_->kTenOrUndefined) ||
       radix_type.Is(type_cache_->kZeroOrUndefined))) {
    // Number.parseInt(a:safe-integer) -> a
    // Number.parseInt(a:safe-integer,b:#0\/undefined) -> a
    // Number.parseInt(a:safe-integer,b:#10\/undefined) -> a
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSTypedLowering::ReduceJSResolvePromise(Node* node) {
  DCHECK_EQ(IrOpcode::kJSResolvePromise, node->opcode());
  Node* resolution = NodeProperties::GetValueInput(node, 1);
  Type resolution_type = NodeProperties::GetType(resolution);
  // We can strength-reduce JSResolvePromise to JSFulfillPromise
  // if the {resolution} is known to be a primitive, as in that
  // case we don't perform the implicit chaining (via "then").
  if (resolution_type.Is(Type::Primitive())) {
    // JSResolvePromise(p,v:primitive) -> JSFulfillPromise(p,v)
    node->RemoveInput(3);  // frame state
    NodeProperties::ChangeOp(node, javascript()->FulfillPromise());
    return Changed(node);
  }
  return NoChange();
}

Reduction JSTypedLowering::Reduce(Node* node) {
  DisallowHeapAccess no_heap_access;

  switch (node->opcode()) {
    case IrOpcode::kJSEqual:
      return ReduceJSEqual(node);
    case IrOpcode::kJSStrictEqual:
      return ReduceJSStrictEqual(node);
    case IrOpcode::kJSLessThan:         // fall through
    case IrOpcode::kJSGreaterThan:      // fall through
    case IrOpcode::kJSLessThanOrEqual:  // fall through
    case IrOpcode::kJSGreaterThanOrEqual:
      return ReduceJSComparison(node);
    case IrOpcode::kJSBitwiseOr:
    case IrOpcode::kJSBitwiseXor:
    case IrOpcode::kJSBitwiseAnd:
      return ReduceInt32Binop(node);
    case IrOpcode::kJSShiftLeft:
    case IrOpcode::kJSShiftRight:
      return ReduceUI32Shift(node, kSigned);
    case IrOpcode::kJSShiftRightLogical:
      return ReduceUI32Shift(node, kUnsigned);
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSSubtract:
    case IrOpcode::kJSMultiply:
    case IrOpcode::kJSDivide:
    case IrOpcode::kJSModulus:
    case IrOpcode::kJSExponentiate:
      return ReduceNumberBinop(node);
    case IrOpcode::kJSBitwiseNot:
      return ReduceJSBitwiseNot(node);
    case IrOpcode::kJSDecrement:
      return ReduceJSDecrement(node);
    case IrOpcode::kJSIncrement:
      return ReduceJSIncrement(node);
    case IrOpcode::kJSNegate:
      return ReduceJSNegate(node);
    case IrOpcode::kJSHasInPrototypeChain:
      return ReduceJSHasInPrototypeChain(node);
    case IrOpcode::kJSOrdinaryHasInstance:
      return ReduceJSOrdinaryHasInstance(node);
    case IrOpcode::kJSToLength:
      return ReduceJSToLength(node);
    case IrOpcode::kJSToName:
      return ReduceJSToName(node);
    case IrOpcode::kJSToNumber:
    case IrOpcode::kJSToNumberConvertBigInt:
      return ReduceJSToNumber(node);
    case IrOpcode::kJSToNumeric:
      return ReduceJSToNumeric(node);
    case IrOpcode::kJSToString:
      return ReduceJSToString(node);
    case IrOpcode::kJSToObject:
      return ReduceJSToObject(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSStoreContext:
      return ReduceJSStoreContext(node);
    case IrOpcode::kJSLoadModule:
      return ReduceJSLoadModule(node);
    case IrOpcode::kJSStoreModule:
      return ReduceJSStoreModule(node);
    case IrOpcode::kJSConstructForwardVarargs:
      return ReduceJSConstructForwardVarargs(node);
    case IrOpcode::kJSConstruct:
      return ReduceJSConstruct(node);
    case IrOpcode::kJSCallForwardVarargs:
      return ReduceJSCallForwardVarargs(node);
    case IrOpcode::kJSCall:
      return ReduceJSCall(node);
    case IrOpcode::kJSForInPrepare:
      return ReduceJSForInPrepare(node);
    case IrOpcode::kJSForInNext:
      return ReduceJSForInNext(node);
    case IrOpcode::kJSLoadMessage:
      return ReduceJSLoadMessage(node);
    case IrOpcode::kJSStoreMessage:
      return ReduceJSStoreMessage(node);
    case IrOpcode::kJSGeneratorStore:
      return ReduceJSGeneratorStore(node);
    case IrOpcode::kJSGeneratorRestoreContinuation:
      return ReduceJSGeneratorRestoreContinuation(node);
    case IrOpcode::kJSGeneratorRestoreContext:
      return ReduceJSGeneratorRestoreContext(node);
    case IrOpcode::kJSGeneratorRestoreRegister:
      return ReduceJSGeneratorRestoreRegister(node);
    case IrOpcode::kJSGeneratorRestoreInputOrDebugPos:
      return ReduceJSGeneratorRestoreInputOrDebugPos(node);
    case IrOpcode::kJSObjectIsArray:
      return ReduceObjectIsArray(node);
    case IrOpcode::kJSParseInt:
      return ReduceJSParseInt(node);
    case IrOpcode::kJSResolvePromise:
      return ReduceJSResolvePromise(node);
    default:
      break;
  }
  return NoChange();
}


Factory* JSTypedLowering::factory() const { return jsgraph()->factory(); }


Graph* JSTypedLowering::graph() const { return jsgraph()->graph(); }


Isolate* JSTypedLowering::isolate() const { return jsgraph()->isolate(); }


JSOperatorBuilder* JSTypedLowering::javascript() const {
  return jsgraph()->javascript();
}


CommonOperatorBuilder* JSTypedLowering::common() const {
  return jsgraph()->common();
}

SimplifiedOperatorBuilder* JSTypedLowering::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
