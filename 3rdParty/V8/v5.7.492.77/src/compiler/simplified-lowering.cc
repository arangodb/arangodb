// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include <limits>

#include "src/address-map.h"
#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/diamond.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operation-typer.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/conversions-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace compiler {

// Macro for outputting trace information from representation inference.
#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_representation) PrintF(__VA_ARGS__); \
  } while (false)

// Representation selection and lowering of {Simplified} operators to machine
// operators are interwined. We use a fixpoint calculation to compute both the
// output representation and the best possible lowering for {Simplified} nodes.
// Representation change insertion ensures that all values are in the correct
// machine representation after this phase, as dictated by the machine
// operators themselves.
enum Phase {
  // 1.) PROPAGATE: Traverse the graph from the end, pushing usage information
  //     backwards from uses to definitions, around cycles in phis, according
  //     to local rules for each operator.
  //     During this phase, the usage information for a node determines the best
  //     possible lowering for each operator so far, and that in turn determines
  //     the output representation.
  //     Therefore, to be correct, this phase must iterate to a fixpoint before
  //     the next phase can begin.
  PROPAGATE,

  // 2.) RETYPE: Propagate types from type feedback forwards.
  RETYPE,

  // 3.) LOWER: perform lowering for all {Simplified} nodes by replacing some
  //     operators for some nodes, expanding some nodes to multiple nodes, or
  //     removing some (redundant) nodes.
  //     During this phase, use the {RepresentationChanger} to insert
  //     representation changes between uses that demand a particular
  //     representation and nodes that produce a different representation.
  LOWER
};

namespace {

MachineRepresentation MachineRepresentationFromArrayType(
    ExternalArrayType array_type) {
  switch (array_type) {
    case kExternalUint8Array:
    case kExternalUint8ClampedArray:
    case kExternalInt8Array:
      return MachineRepresentation::kWord8;
    case kExternalUint16Array:
    case kExternalInt16Array:
      return MachineRepresentation::kWord16;
    case kExternalUint32Array:
    case kExternalInt32Array:
      return MachineRepresentation::kWord32;
    case kExternalFloat32Array:
      return MachineRepresentation::kFloat32;
    case kExternalFloat64Array:
      return MachineRepresentation::kFloat64;
  }
  UNREACHABLE();
  return MachineRepresentation::kNone;
}

UseInfo CheckedUseInfoAsWord32FromHint(
    NumberOperationHint hint, CheckForMinusZeroMode minus_zero_mode =
                                  CheckForMinusZeroMode::kCheckForMinusZero) {
  switch (hint) {
    case NumberOperationHint::kSignedSmall:
      return UseInfo::CheckedSignedSmallAsWord32(minus_zero_mode);
    case NumberOperationHint::kSigned32:
      return UseInfo::CheckedSigned32AsWord32(minus_zero_mode);
    case NumberOperationHint::kNumber:
      return UseInfo::CheckedNumberAsWord32();
    case NumberOperationHint::kNumberOrOddball:
      return UseInfo::CheckedNumberOrOddballAsWord32();
  }
  UNREACHABLE();
  return UseInfo::None();
}

UseInfo CheckedUseInfoAsFloat64FromHint(NumberOperationHint hint) {
  switch (hint) {
    case NumberOperationHint::kSignedSmall:
    case NumberOperationHint::kSigned32:
      // Not used currently.
      UNREACHABLE();
      break;
    case NumberOperationHint::kNumber:
      return UseInfo::CheckedNumberAsFloat64();
    case NumberOperationHint::kNumberOrOddball:
      return UseInfo::CheckedNumberOrOddballAsFloat64();
  }
  UNREACHABLE();
  return UseInfo::None();
}

UseInfo TruncatingUseInfoFromRepresentation(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      return UseInfo::AnyTagged();
    case MachineRepresentation::kFloat64:
      return UseInfo::TruncatingFloat64();
    case MachineRepresentation::kFloat32:
      return UseInfo::Float32();
    case MachineRepresentation::kWord64:
      return UseInfo::TruncatingWord64();
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return UseInfo::TruncatingWord32();
    case MachineRepresentation::kBit:
      return UseInfo::Bool();
    case MachineRepresentation::kSimd128:  // Fall through.
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
  return UseInfo::None();
}


UseInfo UseInfoForBasePointer(const FieldAccess& access) {
  return access.tag() != 0 ? UseInfo::AnyTagged() : UseInfo::PointerInt();
}


UseInfo UseInfoForBasePointer(const ElementAccess& access) {
  return access.tag() != 0 ? UseInfo::AnyTagged() : UseInfo::PointerInt();
}

void ReplaceEffectControlUses(Node* node, Node* effect, Node* control) {
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      edge.UpdateTo(control);
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    } else {
      DCHECK(NodeProperties::IsValueEdge(edge) ||
             NodeProperties::IsContextEdge(edge));
    }
  }
}

void ChangeToPureOp(Node* node, const Operator* new_op) {
  if (node->op()->EffectInputCount() > 0) {
    DCHECK_LT(0, node->op()->ControlInputCount());
    // Disconnect the node from effect and control chains.
    Node* control = NodeProperties::GetControlInput(node);
    Node* effect = NodeProperties::GetEffectInput(node);
    ReplaceEffectControlUses(node, effect, control);
    node->TrimInputCount(new_op->ValueInputCount());
  } else {
    DCHECK_EQ(0, node->op()->ControlInputCount());
  }
  NodeProperties::ChangeOp(node, new_op);
}

#ifdef DEBUG
// Helpers for monotonicity checking.
class InputUseInfos {
 public:
  explicit InputUseInfos(Zone* zone) : input_use_infos_(zone) {}

  void SetAndCheckInput(Node* node, int index, UseInfo use_info) {
    if (input_use_infos_.empty()) {
      input_use_infos_.resize(node->InputCount(), UseInfo::None());
    }
    // Check that the new use informatin is a super-type of the old
    // one.
    CHECK(IsUseLessGeneral(input_use_infos_[index], use_info));
    input_use_infos_[index] = use_info;
  }

 private:
  ZoneVector<UseInfo> input_use_infos_;

  static bool IsUseLessGeneral(UseInfo use1, UseInfo use2) {
    return use1.truncation().IsLessGeneralThan(use2.truncation());
  }
};

#endif  // DEBUG

bool CanOverflowSigned32(const Operator* op, Type* left, Type* right,
                         Zone* type_zone) {
  // We assume the inputs are checked Signed32 (or known statically
  // to be Signed32). Technically, theinputs could also be minus zero, but
  // that cannot cause overflow.
  left = Type::Intersect(left, Type::Signed32(), type_zone);
  right = Type::Intersect(right, Type::Signed32(), type_zone);
  if (!left->IsInhabited() || !right->IsInhabited()) return false;
  switch (op->opcode()) {
    case IrOpcode::kSpeculativeNumberAdd:
      return (left->Max() + right->Max() > kMaxInt) ||
             (left->Min() + right->Min() < kMinInt);

    case IrOpcode::kSpeculativeNumberSubtract:
      return (left->Max() - right->Min() > kMaxInt) ||
             (left->Min() - right->Max() < kMinInt);

    default:
      UNREACHABLE();
  }
  return true;
}

}  // namespace

class RepresentationSelector {
 public:
  // Information for each node tracked during the fixpoint.
  class NodeInfo final {
   public:
    // Adds new use to the node. Returns true if something has changed
    // and the node has to be requeued.
    bool AddUse(UseInfo info) {
      Truncation old_truncation = truncation_;
      truncation_ = Truncation::Generalize(truncation_, info.truncation());
      return truncation_ != old_truncation;
    }

    void set_queued() { state_ = kQueued; }
    void set_visited() { state_ = kVisited; }
    void set_pushed() { state_ = kPushed; }
    void reset_state() { state_ = kUnvisited; }
    bool visited() const { return state_ == kVisited; }
    bool queued() const { return state_ == kQueued; }
    bool unvisited() const { return state_ == kUnvisited; }
    Truncation truncation() const { return truncation_; }
    void set_output(MachineRepresentation output) { representation_ = output; }

    MachineRepresentation representation() const { return representation_; }

    // Helpers for feedback typing.
    void set_feedback_type(Type* type) { feedback_type_ = type; }
    Type* feedback_type() const { return feedback_type_; }
    void set_weakened() { weakened_ = true; }
    bool weakened() const { return weakened_; }
    void set_restriction_type(Type* type) { restriction_type_ = type; }
    Type* restriction_type() const { return restriction_type_; }

   private:
    enum State : uint8_t { kUnvisited, kPushed, kVisited, kQueued };
    State state_ = kUnvisited;
    MachineRepresentation representation_ =
        MachineRepresentation::kNone;             // Output representation.
    Truncation truncation_ = Truncation::None();  // Information about uses.

    Type* restriction_type_ = Type::Any();
    Type* feedback_type_ = nullptr;
    bool weakened_ = false;
  };

  RepresentationSelector(JSGraph* jsgraph, Zone* zone,
                         RepresentationChanger* changer,
                         SourcePositionTable* source_positions)
      : jsgraph_(jsgraph),
        zone_(zone),
        count_(jsgraph->graph()->NodeCount()),
        info_(count_, zone),
#ifdef DEBUG
        node_input_use_infos_(count_, InputUseInfos(zone), zone),
#endif
        nodes_(zone),
        replacements_(zone),
        phase_(PROPAGATE),
        changer_(changer),
        queue_(zone),
        typing_stack_(zone),
        source_positions_(source_positions),
        type_cache_(TypeCache::Get()),
        op_typer_(jsgraph->isolate(), graph_zone()) {
  }

  // Forward propagation of types from type feedback.
  void RunTypePropagationPhase() {
    // Run type propagation.
    TRACE("--{Type propagation phase}--\n");
    phase_ = RETYPE;
    ResetNodeInfoState();

    DCHECK(typing_stack_.empty());
    typing_stack_.push({graph()->end(), 0});
    GetInfo(graph()->end())->set_pushed();
    while (!typing_stack_.empty()) {
      NodeState& current = typing_stack_.top();

      // If there is an unvisited input, push it and continue.
      bool pushed_unvisited = false;
      while (current.input_index < current.node->InputCount()) {
        Node* input = current.node->InputAt(current.input_index);
        NodeInfo* input_info = GetInfo(input);
        current.input_index++;
        if (input_info->unvisited()) {
          input_info->set_pushed();
          typing_stack_.push({input, 0});
          pushed_unvisited = true;
          break;
        }
      }
      if (pushed_unvisited) continue;

      // Process the top of the stack.
      Node* node = current.node;
      typing_stack_.pop();
      NodeInfo* info = GetInfo(node);
      info->set_visited();
      bool updated = UpdateFeedbackType(node);
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      VisitNode(node, info->truncation(), nullptr);
      TRACE("  ==> output ");
      PrintOutputInfo(info);
      TRACE("\n");
      if (updated) {
        for (Node* const user : node->uses()) {
          if (GetInfo(user)->visited()) {
            GetInfo(user)->set_queued();
            queue_.push(user);
          }
        }
      }
    }

    // Process the revisit queue.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      queue_.pop();
      NodeInfo* info = GetInfo(node);
      info->set_visited();
      bool updated = UpdateFeedbackType(node);
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      VisitNode(node, info->truncation(), nullptr);
      TRACE("  ==> output ");
      PrintOutputInfo(info);
      TRACE("\n");
      if (updated) {
        for (Node* const user : node->uses()) {
          if (GetInfo(user)->visited()) {
            GetInfo(user)->set_queued();
            queue_.push(user);
          }
        }
      }
    }
  }

  void ResetNodeInfoState() {
    // Clean up for the next phase.
    for (NodeInfo& info : info_) {
      info.reset_state();
    }
  }

  Type* TypeOf(Node* node) {
    Type* type = GetInfo(node)->feedback_type();
    return type == nullptr ? NodeProperties::GetType(node) : type;
  }

  Type* FeedbackTypeOf(Node* node) {
    Type* type = GetInfo(node)->feedback_type();
    return type == nullptr ? Type::None() : type;
  }

  Type* TypePhi(Node* node) {
    int arity = node->op()->ValueInputCount();
    Type* type = FeedbackTypeOf(node->InputAt(0));
    for (int i = 1; i < arity; ++i) {
      type = op_typer_.Merge(type, FeedbackTypeOf(node->InputAt(i)));
    }
    return type;
  }

  Type* TypeSelect(Node* node) {
    return op_typer_.Merge(FeedbackTypeOf(node->InputAt(1)),
                           FeedbackTypeOf(node->InputAt(2)));
  }

  bool UpdateFeedbackType(Node* node) {
    if (node->op()->ValueOutputCount() == 0) return false;

    NodeInfo* info = GetInfo(node);
    Type* type = info->feedback_type();
    Type* new_type = type;

    // For any non-phi node just wait until we get all inputs typed. We only
    // allow untyped inputs for phi nodes because phis are the only places
    // where cycles need to be broken.
    if (node->opcode() != IrOpcode::kPhi) {
      for (int i = 0; i < node->op()->ValueInputCount(); i++) {
        if (GetInfo(node->InputAt(i))->feedback_type() == nullptr) {
          return false;
        }
      }
    }

    switch (node->opcode()) {
#define DECLARE_CASE(Name)                                       \
  case IrOpcode::k##Name: {                                      \
    new_type = op_typer_.Name(FeedbackTypeOf(node->InputAt(0)),  \
                              FeedbackTypeOf(node->InputAt(1))); \
    break;                                                       \
  }
      SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(Name)                                                \
  case IrOpcode::k##Name: {                                               \
    new_type =                                                            \
        Type::Intersect(op_typer_.Name(FeedbackTypeOf(node->InputAt(0)),  \
                                       FeedbackTypeOf(node->InputAt(1))), \
                        info->restriction_type(), graph_zone());          \
    break;                                                                \
  }
      SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(Name)                                       \
  case IrOpcode::k##Name: {                                      \
    new_type = op_typer_.Name(FeedbackTypeOf(node->InputAt(0))); \
    break;                                                       \
  }
      SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

      case IrOpcode::kPlainPrimitiveToNumber:
        new_type = op_typer_.ToNumber(FeedbackTypeOf(node->InputAt(0)));
        break;

      case IrOpcode::kPhi: {
        new_type = TypePhi(node);
        if (type != nullptr) {
          new_type = Weaken(node, type, new_type);
        }
        break;
      }

      case IrOpcode::kTypeGuard: {
        new_type = op_typer_.TypeTypeGuard(node->op(),
                                           FeedbackTypeOf(node->InputAt(0)));
        break;
      }

      case IrOpcode::kSelect: {
        new_type = TypeSelect(node);
        break;
      }

      default:
        // Shortcut for operations that we do not handle.
        if (type == nullptr) {
          GetInfo(node)->set_feedback_type(NodeProperties::GetType(node));
          return true;
        }
        return false;
    }
    // We need to guarantee that the feedback type is a subtype of the upper
    // bound. Naively that should hold, but weakening can actually produce
    // a bigger type if we are unlucky with ordering of phi typing. To be
    // really sure, just intersect the upper bound with the feedback type.
    new_type = Type::Intersect(GetUpperBound(node), new_type, graph_zone());

    if (type != nullptr && new_type->Is(type)) return false;
    GetInfo(node)->set_feedback_type(new_type);
    if (FLAG_trace_representation) {
      PrintNodeFeedbackType(node);
    }
    return true;
  }

  void PrintNodeFeedbackType(Node* n) {
    OFStream os(stdout);
    os << "#" << n->id() << ":" << *n->op() << "(";
    int j = 0;
    for (Node* const i : n->inputs()) {
      if (j++ > 0) os << ", ";
      os << "#" << i->id() << ":" << i->op()->mnemonic();
    }
    os << ")";
    if (NodeProperties::IsTyped(n)) {
      os << "  [Static type: ";
      Type* static_type = NodeProperties::GetType(n);
      static_type->PrintTo(os);
      Type* feedback_type = GetInfo(n)->feedback_type();
      if (feedback_type != nullptr && feedback_type != static_type) {
        os << ", Feedback type: ";
        feedback_type->PrintTo(os);
      }
      os << "]";
    }
    os << std::endl;
  }

  Type* Weaken(Node* node, Type* previous_type, Type* current_type) {
    // If the types have nothing to do with integers, return the types.
    Type* const integer = type_cache_.kInteger;
    if (!previous_type->Maybe(integer)) {
      return current_type;
    }
    DCHECK(current_type->Maybe(integer));

    Type* current_integer =
        Type::Intersect(current_type, integer, graph_zone());
    Type* previous_integer =
        Type::Intersect(previous_type, integer, graph_zone());

    // Once we start weakening a node, we should always weaken.
    if (!GetInfo(node)->weakened()) {
      // Only weaken if there is range involved; we should converge quickly
      // for all other types (the exception is a union of many constants,
      // but we currently do not increase the number of constants in unions).
      Type* previous = previous_integer->GetRange();
      Type* current = current_integer->GetRange();
      if (current == nullptr || previous == nullptr) {
        return current_type;
      }
      // Range is involved => we are weakening.
      GetInfo(node)->set_weakened();
    }

    return Type::Union(current_type,
                       op_typer_.WeakenRange(previous_integer, current_integer),
                       graph_zone());
  }

  // Backward propagation of truncations.
  void RunTruncationPropagationPhase() {
    // Run propagation phase to a fixpoint.
    TRACE("--{Propagation phase}--\n");
    phase_ = PROPAGATE;
    EnqueueInitial(jsgraph_->graph()->end());
    // Process nodes from the queue until it is empty.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      NodeInfo* info = GetInfo(node);
      queue_.pop();
      info->set_visited();
      TRACE(" visit #%d: %s (trunc: %s)\n", node->id(), node->op()->mnemonic(),
            info->truncation().description());
      VisitNode(node, info->truncation(), nullptr);
    }
  }

  void Run(SimplifiedLowering* lowering) {
    RunTruncationPropagationPhase();

    RunTypePropagationPhase();

    // Run lowering and change insertion phase.
    TRACE("--{Simplified lowering phase}--\n");
    phase_ = LOWER;
    // Process nodes from the collected {nodes_} vector.
    for (NodeVector::iterator i = nodes_.begin(); i != nodes_.end(); ++i) {
      Node* node = *i;
      NodeInfo* info = GetInfo(node);
      TRACE(" visit #%d: %s\n", node->id(), node->op()->mnemonic());
      // Reuse {VisitNode()} so the representation rules are in one place.
      SourcePositionTable::Scope scope(
          source_positions_, source_positions_->GetSourcePosition(node));
      VisitNode(node, info->truncation(), lowering);
    }

    // Perform the final replacements.
    for (NodeVector::iterator i = replacements_.begin();
         i != replacements_.end(); ++i) {
      Node* node = *i;
      Node* replacement = *(++i);
      node->ReplaceUses(replacement);
      node->Kill();
      // We also need to replace the node in the rest of the vector.
      for (NodeVector::iterator j = i + 1; j != replacements_.end(); ++j) {
        ++j;
        if (*j == node) *j = replacement;
      }
    }
  }

  void EnqueueInitial(Node* node) {
    NodeInfo* info = GetInfo(node);
    info->set_queued();
    nodes_.push_back(node);
    queue_.push(node);
  }

  // Enqueue {use_node}'s {index} input if the {use} contains new information
  // for that input node. Add the input to {nodes_} if this is the first time
  // it's been visited.
  void EnqueueInput(Node* use_node, int index,
                    UseInfo use_info = UseInfo::None()) {
    Node* node = use_node->InputAt(index);
    if (phase_ != PROPAGATE) return;
    NodeInfo* info = GetInfo(node);
#ifdef DEBUG
    // Check monotonicity of input requirements.
    node_input_use_infos_[use_node->id()].SetAndCheckInput(use_node, index,
                                                           use_info);
#endif  // DEBUG
    if (info->unvisited()) {
      // First visit of this node.
      info->set_queued();
      nodes_.push_back(node);
      queue_.push(node);
      TRACE("  initial #%i: ", node->id());
      info->AddUse(use_info);
      PrintTruncation(info->truncation());
      return;
    }
    TRACE("   queue #%i?: ", node->id());
    PrintTruncation(info->truncation());
    if (info->AddUse(use_info)) {
      // New usage information for the node is available.
      if (!info->queued()) {
        queue_.push(node);
        info->set_queued();
        TRACE("   added: ");
      } else {
        TRACE(" inqueue: ");
      }
      PrintTruncation(info->truncation());
    }
  }

  bool lower() const { return phase_ == LOWER; }
  bool retype() const { return phase_ == RETYPE; }
  bool propagate() const { return phase_ == PROPAGATE; }

  void SetOutput(Node* node, MachineRepresentation representation,
                 Type* restriction_type = Type::Any()) {
    NodeInfo* const info = GetInfo(node);
    switch (phase_) {
      case PROPAGATE:
        info->set_restriction_type(restriction_type);
        break;
      case RETYPE:
        DCHECK(info->restriction_type()->Is(restriction_type));
        DCHECK(restriction_type->Is(info->restriction_type()));
        info->set_output(representation);
        break;
      case LOWER:
        DCHECK_EQ(info->representation(), representation);
        DCHECK(info->restriction_type()->Is(restriction_type));
        DCHECK(restriction_type->Is(info->restriction_type()));
        break;
    }
  }

  Type* GetUpperBound(Node* node) { return NodeProperties::GetType(node); }

  bool InputCannotBe(Node* node, Type* type) {
    DCHECK_EQ(1, node->op()->ValueInputCount());
    return !GetUpperBound(node->InputAt(0))->Maybe(type);
  }

  bool InputIs(Node* node, Type* type) {
    DCHECK_EQ(1, node->op()->ValueInputCount());
    return GetUpperBound(node->InputAt(0))->Is(type);
  }

  bool BothInputsAreSigned32(Node* node) {
    return BothInputsAre(node, Type::Signed32());
  }

  bool BothInputsAreUnsigned32(Node* node) {
    return BothInputsAre(node, Type::Unsigned32());
  }

  bool BothInputsAre(Node* node, Type* type) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    return GetUpperBound(node->InputAt(0))->Is(type) &&
           GetUpperBound(node->InputAt(1))->Is(type);
  }

  bool IsNodeRepresentationTagged(Node* node) {
    MachineRepresentation representation = GetInfo(node)->representation();
    return IsAnyTagged(representation);
  }

  bool OneInputCannotBe(Node* node, Type* type) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    return !GetUpperBound(node->InputAt(0))->Maybe(type) ||
           !GetUpperBound(node->InputAt(1))->Maybe(type);
  }

  void ConvertInput(Node* node, int index, UseInfo use) {
    Node* input = node->InputAt(index);
    // In the change phase, insert a change before the use if necessary.
    if (use.representation() == MachineRepresentation::kNone)
      return;  // No input requirement on the use.
    DCHECK_NOT_NULL(input);
    NodeInfo* input_info = GetInfo(input);
    MachineRepresentation input_rep = input_info->representation();
    if (input_rep != use.representation() ||
        use.type_check() != TypeCheckKind::kNone) {
      // Output representation doesn't match usage.
      TRACE("  change: #%d:%s(@%d #%d:%s) ", node->id(), node->op()->mnemonic(),
            index, input->id(), input->op()->mnemonic());
      TRACE(" from ");
      PrintOutputInfo(input_info);
      TRACE(" to ");
      PrintUseInfo(use);
      TRACE("\n");
      Node* n = changer_->GetRepresentationFor(
          input, input_info->representation(), TypeOf(input), node, use);
      node->ReplaceInput(index, n);
    }
  }

  void ProcessInput(Node* node, int index, UseInfo use) {
    switch (phase_) {
      case PROPAGATE:
        EnqueueInput(node, index, use);
        break;
      case RETYPE:
        break;
      case LOWER:
        ConvertInput(node, index, use);
        break;
    }
  }

  void ProcessRemainingInputs(Node* node, int index) {
    DCHECK_GE(index, NodeProperties::PastValueIndex(node));
    DCHECK_GE(index, NodeProperties::PastContextIndex(node));
    for (int i = std::max(index, NodeProperties::FirstEffectIndex(node));
         i < NodeProperties::PastEffectIndex(node); ++i) {
      EnqueueInput(node, i);  // Effect inputs: just visit
    }
    for (int i = std::max(index, NodeProperties::FirstControlIndex(node));
         i < NodeProperties::PastControlIndex(node); ++i) {
      EnqueueInput(node, i);  // Control inputs: just visit
    }
  }

  // The default, most general visitation case. For {node}, process all value,
  // context, frame state, effect, and control inputs, assuming that value
  // inputs should have {kRepTagged} representation and can observe all output
  // values {kTypeAny}.
  void VisitInputs(Node* node) {
    int tagged_count = node->op()->ValueInputCount() +
                       OperatorProperties::GetContextInputCount(node->op()) +
                       OperatorProperties::GetFrameStateInputCount(node->op());
    // Visit value, context and frame state inputs as tagged.
    for (int i = 0; i < tagged_count; i++) {
      ProcessInput(node, i, UseInfo::AnyTagged());
    }
    // Only enqueue other inputs (effects, control).
    for (int i = tagged_count; i < node->InputCount(); i++) {
      EnqueueInput(node, i);
    }
  }

  void VisitReturn(Node* node) {
    int tagged_limit = node->op()->ValueInputCount() +
                       OperatorProperties::GetContextInputCount(node->op()) +
                       OperatorProperties::GetFrameStateInputCount(node->op());
    // Visit integer slot count to pop
    ProcessInput(node, 0, UseInfo::TruncatingWord32());

    // Visit value, context and frame state inputs as tagged.
    for (int i = 1; i < tagged_limit; i++) {
      ProcessInput(node, i, UseInfo::AnyTagged());
    }
    // Only enqueue other inputs (effects, control).
    for (int i = tagged_limit; i < node->InputCount(); i++) {
      EnqueueInput(node, i);
    }
  }

  // Helper for an unused node.
  void VisitUnused(Node* node) {
    int value_count = node->op()->ValueInputCount() +
                      OperatorProperties::GetContextInputCount(node->op()) +
                      OperatorProperties::GetFrameStateInputCount(node->op());
    for (int i = 0; i < value_count; i++) {
      ProcessInput(node, i, UseInfo::None());
    }
    ProcessRemainingInputs(node, value_count);
    if (lower()) Kill(node);
  }

  // Helper for binops of the R x L -> O variety.
  void VisitBinop(Node* node, UseInfo left_use, UseInfo right_use,
                  MachineRepresentation output,
                  Type* restriction_type = Type::Any()) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    ProcessInput(node, 0, left_use);
    ProcessInput(node, 1, right_use);
    for (int i = 2; i < node->InputCount(); i++) {
      EnqueueInput(node, i);
    }
    SetOutput(node, output, restriction_type);
  }

  // Helper for binops of the I x I -> O variety.
  void VisitBinop(Node* node, UseInfo input_use, MachineRepresentation output,
                  Type* restriction_type = Type::Any()) {
    VisitBinop(node, input_use, input_use, output, restriction_type);
  }

  void VisitSpeculativeInt32Binop(Node* node) {
    DCHECK_EQ(2, node->op()->ValueInputCount());
    if (BothInputsAre(node, Type::NumberOrOddball())) {
      return VisitBinop(node, UseInfo::TruncatingWord32(),
                        MachineRepresentation::kWord32);
    }
    NumberOperationHint hint = NumberOperationHintOf(node->op());
    return VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                      MachineRepresentation::kWord32);
  }

  // Helper for unops of the I -> O variety.
  void VisitUnop(Node* node, UseInfo input_use, MachineRepresentation output) {
    DCHECK_EQ(1, node->op()->ValueInputCount());
    ProcessInput(node, 0, input_use);
    ProcessRemainingInputs(node, 1);
    SetOutput(node, output);
  }

  // Helper for leaf nodes.
  void VisitLeaf(Node* node, MachineRepresentation output) {
    DCHECK_EQ(0, node->InputCount());
    SetOutput(node, output);
  }

  // Helpers for specific types of binops.
  void VisitFloat64Binop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingFloat64(),
               MachineRepresentation::kFloat64);
  }
  void VisitWord32TruncatingBinop(Node* node) {
    VisitBinop(node, UseInfo::TruncatingWord32(),
               MachineRepresentation::kWord32);
  }

  // Infer representation for phi-like nodes.
  // The {node} parameter is only used to decide on the int64 representation.
  // Once the type system supports an external pointer type, the {node}
  // parameter can be removed.
  MachineRepresentation GetOutputInfoForPhi(Node* node, Type* type,
                                            Truncation use) {
    // Compute the representation.
    if (type->Is(Type::None())) {
      return MachineRepresentation::kNone;
    } else if (type->Is(Type::Signed32()) || type->Is(Type::Unsigned32())) {
      return MachineRepresentation::kWord32;
    } else if (type->Is(Type::NumberOrOddball()) && use.IsUsedAsWord32()) {
      return MachineRepresentation::kWord32;
    } else if (type->Is(Type::Boolean())) {
      return MachineRepresentation::kBit;
    } else if (type->Is(Type::NumberOrOddball()) && use.IsUsedAsFloat64()) {
      return MachineRepresentation::kFloat64;
    } else if (type->Is(
                   Type::Union(Type::SignedSmall(), Type::NaN(), zone()))) {
      // TODO(turbofan): For Phis that return either NaN or some Smi, it's
      // beneficial to not go all the way to double, unless the uses are
      // double uses. For tagging that just means some potentially expensive
      // allocation code; we might want to do the same for -0 as well?
      return MachineRepresentation::kTagged;
    } else if (type->Is(Type::Number())) {
      return MachineRepresentation::kFloat64;
    } else if (type->Is(Type::ExternalPointer())) {
      return MachineType::PointerRepresentation();
    }
    return MachineRepresentation::kTagged;
  }

  // Helper for handling selects.
  void VisitSelect(Node* node, Truncation truncation,
                   SimplifiedLowering* lowering) {
    ProcessInput(node, 0, UseInfo::Bool());

    MachineRepresentation output =
        GetOutputInfoForPhi(node, TypeOf(node), truncation);
    SetOutput(node, output);

    if (lower()) {
      // Update the select operator.
      SelectParameters p = SelectParametersOf(node->op());
      if (output != p.representation()) {
        NodeProperties::ChangeOp(node,
                                 lowering->common()->Select(output, p.hint()));
      }
    }
    // Convert inputs to the output representation of this phi, pass the
    // truncation truncation along.
    UseInfo input_use(output, truncation);
    ProcessInput(node, 1, input_use);
    ProcessInput(node, 2, input_use);
  }

  // Helper for handling phis.
  void VisitPhi(Node* node, Truncation truncation,
                SimplifiedLowering* lowering) {
    MachineRepresentation output =
        GetOutputInfoForPhi(node, TypeOf(node), truncation);
    // Only set the output representation if not running with type
    // feedback. (Feedback typing will set the representation.)
    SetOutput(node, output);

    int values = node->op()->ValueInputCount();
    if (lower()) {
      // Update the phi operator.
      if (output != PhiRepresentationOf(node->op())) {
        NodeProperties::ChangeOp(node, lowering->common()->Phi(output, values));
      }
    }

    // Convert inputs to the output representation of this phi, pass the
    // truncation along.
    UseInfo input_use(output, truncation);
    for (int i = 0; i < node->InputCount(); i++) {
      ProcessInput(node, i, i < values ? input_use : UseInfo::None());
    }
  }

  void VisitObjectIs(Node* node, Type* type, SimplifiedLowering* lowering) {
    Type* const input_type = TypeOf(node->InputAt(0));
    if (input_type->Is(type)) {
      VisitUnop(node, UseInfo::None(), MachineRepresentation::kBit);
      if (lower()) {
        DeferReplacement(node, lowering->jsgraph()->Int32Constant(1));
      }
    } else {
      VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
      if (lower() && !input_type->Maybe(type)) {
        DeferReplacement(node, lowering->jsgraph()->Int32Constant(0));
      }
    }
  }

  void VisitCall(Node* node, SimplifiedLowering* lowering) {
    const CallDescriptor* desc = CallDescriptorOf(node->op());
    int params = static_cast<int>(desc->ParameterCount());
    int value_input_count = node->op()->ValueInputCount();
    // Propagate representation information from call descriptor.
    for (int i = 0; i < value_input_count; i++) {
      if (i == 0) {
        // The target of the call.
        ProcessInput(node, i, UseInfo::Any());
      } else if ((i - 1) < params) {
        ProcessInput(node, i, TruncatingUseInfoFromRepresentation(
                                  desc->GetInputType(i).representation()));
      } else {
        ProcessInput(node, i, UseInfo::AnyTagged());
      }
    }
    ProcessRemainingInputs(node, value_input_count);

    if (desc->ReturnCount() > 0) {
      SetOutput(node, desc->GetReturnType(0).representation());
    } else {
      SetOutput(node, MachineRepresentation::kTagged);
    }
  }

  MachineSemantic DeoptValueSemanticOf(Type* type) {
    // We only need signedness to do deopt correctly.
    if (type->Is(Type::Signed32())) {
      return MachineSemantic::kInt32;
    } else if (type->Is(Type::Unsigned32())) {
      return MachineSemantic::kUint32;
    } else {
      return MachineSemantic::kAny;
    }
  }

  void VisitStateValues(Node* node) {
    if (propagate()) {
      for (int i = 0; i < node->InputCount(); i++) {
        EnqueueInput(node, i, UseInfo::Any());
      }
    } else if (lower()) {
      Zone* zone = jsgraph_->zone();
      ZoneVector<MachineType>* types =
          new (zone->New(sizeof(ZoneVector<MachineType>)))
              ZoneVector<MachineType>(node->InputCount(), zone);
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        NodeInfo* input_info = GetInfo(input);
        Type* input_type = TypeOf(input);
        MachineRepresentation rep = input_type->IsInhabited()
                                        ? input_info->representation()
                                        : MachineRepresentation::kNone;
        MachineType machine_type(rep, DeoptValueSemanticOf(input_type));
        DCHECK(machine_type.representation() !=
                   MachineRepresentation::kWord32 ||
               machine_type.semantic() == MachineSemantic::kInt32 ||
               machine_type.semantic() == MachineSemantic::kUint32);
        (*types)[i] = machine_type;
      }
      SparseInputMask mask = SparseInputMaskOf(node->op());
      NodeProperties::ChangeOp(
          node, jsgraph_->common()->TypedStateValues(types, mask));
    }
    SetOutput(node, MachineRepresentation::kTagged);
  }

  void VisitObjectState(Node* node) {
    if (propagate()) {
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        Type* input_type = TypeOf(input);
        // TODO(turbofan): Special treatment for ExternalPointer here,
        // to avoid incompatible truncations. We really need a story
        // for the JSFunction::entry field.
        UseInfo use_info = UseInfo::None();
        if (input_type->IsInhabited()) {
          if (input_type->Is(Type::ExternalPointer())) {
            use_info = UseInfo::PointerInt();
          } else {
            use_info = UseInfo::Any();
          }
        }
        EnqueueInput(node, i, use_info);
      }
    } else if (lower()) {
      Zone* zone = jsgraph_->zone();
      ZoneVector<MachineType>* types =
          new (zone->New(sizeof(ZoneVector<MachineType>)))
              ZoneVector<MachineType>(node->InputCount(), zone);
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        NodeInfo* input_info = GetInfo(input);
        Type* input_type = TypeOf(input);
        // TODO(turbofan): Special treatment for ExternalPointer here,
        // to avoid incompatible truncations. We really need a story
        // for the JSFunction::entry field.
        if (!input_type->IsInhabited()) {
          (*types)[i] = MachineType::None();
        } else if (input_type->Is(Type::ExternalPointer())) {
          (*types)[i] = MachineType::Pointer();
        } else {
          MachineRepresentation rep = input_type->IsInhabited()
                                          ? input_info->representation()
                                          : MachineRepresentation::kNone;
          MachineType machine_type(rep, DeoptValueSemanticOf(input_type));
          DCHECK(machine_type.representation() !=
                     MachineRepresentation::kWord32 ||
                 machine_type.semantic() == MachineSemantic::kInt32 ||
                 machine_type.semantic() == MachineSemantic::kUint32);
          DCHECK(machine_type.representation() != MachineRepresentation::kBit ||
                 input_type->Is(Type::Boolean()));
          (*types)[i] = machine_type;
        }
      }
      NodeProperties::ChangeOp(node,
                               jsgraph_->common()->TypedObjectState(types));
    }
    SetOutput(node, MachineRepresentation::kTagged);
  }

  const Operator* Int32Op(Node* node) {
    return changer_->Int32OperatorFor(node->opcode());
  }

  const Operator* Int32OverflowOp(Node* node) {
    return changer_->Int32OverflowOperatorFor(node->opcode());
  }

  const Operator* Uint32Op(Node* node) {
    return changer_->Uint32OperatorFor(node->opcode());
  }

  const Operator* Uint32OverflowOp(Node* node) {
    return changer_->Uint32OverflowOperatorFor(node->opcode());
  }

  const Operator* Float64Op(Node* node) {
    return changer_->Float64OperatorFor(node->opcode());
  }

  WriteBarrierKind WriteBarrierKindFor(
      BaseTaggedness base_taggedness,
      MachineRepresentation field_representation, Type* field_type,
      MachineRepresentation value_representation, Node* value) {
    if (base_taggedness == kTaggedBase &&
        CanBeTaggedPointer(field_representation)) {
      Type* value_type = NodeProperties::GetType(value);
      if (field_representation == MachineRepresentation::kTaggedSigned ||
          value_representation == MachineRepresentation::kTaggedSigned) {
        // Write barriers are only for stores of heap objects.
        return kNoWriteBarrier;
      }
      if (field_type->Is(Type::BooleanOrNullOrUndefined()) ||
          value_type->Is(Type::BooleanOrNullOrUndefined())) {
        // Write barriers are not necessary when storing true, false, null or
        // undefined, because these special oddballs are always in the root set.
        return kNoWriteBarrier;
      }
      if (value_type->IsHeapConstant()) {
        Heap::RootListIndex root_index;
        Heap* heap = jsgraph_->isolate()->heap();
        if (heap->IsRootHandle(value_type->AsHeapConstant()->Value(),
                               &root_index)) {
          if (heap->RootIsImmortalImmovable(root_index)) {
            // Write barriers are unnecessary for immortal immovable roots.
            return kNoWriteBarrier;
          }
        }
      }
      if (field_representation == MachineRepresentation::kTaggedPointer ||
          value_representation == MachineRepresentation::kTaggedPointer) {
        // Write barriers for heap objects are cheaper.
        return kPointerWriteBarrier;
      }
      NumberMatcher m(value);
      if (m.HasValue()) {
        if (IsSmiDouble(m.Value())) {
          // Storing a smi doesn't need a write barrier.
          return kNoWriteBarrier;
        }
        // The NumberConstant will be represented as HeapNumber.
        return kPointerWriteBarrier;
      }
      return kFullWriteBarrier;
    }
    return kNoWriteBarrier;
  }

  WriteBarrierKind WriteBarrierKindFor(
      BaseTaggedness base_taggedness,
      MachineRepresentation field_representation, int field_offset,
      Type* field_type, MachineRepresentation value_representation,
      Node* value) {
    if (base_taggedness == kTaggedBase &&
        field_offset == HeapObject::kMapOffset) {
      return kMapWriteBarrier;
    }
    return WriteBarrierKindFor(base_taggedness, field_representation,
                               field_type, value_representation, value);
  }

  Graph* graph() const { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() const {
    return jsgraph_->simplified();
  }

  void LowerToCheckedInt32Mul(Node* node, Truncation truncation,
                              Type* input0_type, Type* input1_type) {
    // If one of the inputs is positive and/or truncation is being applied,
    // there is no need to return -0.
    CheckForMinusZeroMode mz_mode =
        truncation.IsUsedAsWord32() ||
                (input0_type->Is(Type::OrderedNumber()) &&
                 input0_type->Min() > 0) ||
                (input1_type->Is(Type::OrderedNumber()) &&
                 input1_type->Min() > 0)
            ? CheckForMinusZeroMode::kDontCheckForMinusZero
            : CheckForMinusZeroMode::kCheckForMinusZero;

    NodeProperties::ChangeOp(node, simplified()->CheckedInt32Mul(mz_mode));
  }

  void ChangeToInt32OverflowOp(Node* node) {
    NodeProperties::ChangeOp(node, Int32OverflowOp(node));
  }

  void ChangeToUint32OverflowOp(Node* node) {
    NodeProperties::ChangeOp(node, Uint32OverflowOp(node));
  }

  void VisitSpeculativeAdditiveOp(Node* node, Truncation truncation,
                                  SimplifiedLowering* lowering) {
    // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we can
    // only eliminate an unused speculative number operation if we know that
    // the inputs are PlainPrimitive, which excludes everything that's might
    // have side effects or throws during a ToNumber conversion.
    if (BothInputsAre(node, Type::PlainPrimitive())) {
      if (truncation.IsUnused()) return VisitUnused(node);
    }

    if (BothInputsAre(node, type_cache_.kAdditiveSafeIntegerOrMinusZero) &&
        (GetUpperBound(node)->Is(Type::Signed32()) ||
         GetUpperBound(node)->Is(Type::Unsigned32()) ||
         truncation.IsUsedAsWord32())) {
      // => Int32Add/Sub
      VisitWord32TruncatingBinop(node);
      if (lower()) ChangeToPureOp(node, Int32Op(node));
      return;
    }

    // Try to use type feedback.
    NumberOperationHint hint = NumberOperationHintOf(node->op());

    if (hint == NumberOperationHint::kSignedSmall ||
        hint == NumberOperationHint::kSigned32) {
      Type* left_feedback_type = TypeOf(node->InputAt(0));
      Type* right_feedback_type = TypeOf(node->InputAt(1));
      // Handle the case when no int32 checks on inputs are necessary (but
      // an overflow check is needed on the output).
      // TODO(jarin) We should not look at the upper bound because the typer
      // could have already baked in some feedback into the upper bound.
      if (BothInputsAre(node, Type::Signed32()) ||
          (BothInputsAre(node, Type::Signed32OrMinusZero()) &&
           GetUpperBound(node)->Is(type_cache_.kSafeInteger))) {
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   MachineRepresentation::kWord32, Type::Signed32());
      } else {
        UseInfo left_use = CheckedUseInfoAsWord32FromHint(hint);
        // For CheckedInt32Add and CheckedInt32Sub, we don't need to do
        // a minus zero check for the right hand side, since we already
        // know that the left hand side is a proper Signed32 value,
        // potentially guarded by a check.
        UseInfo right_use = CheckedUseInfoAsWord32FromHint(
            hint, CheckForMinusZeroMode::kDontCheckForMinusZero);
        VisitBinop(node, left_use, right_use, MachineRepresentation::kWord32,
                   Type::Signed32());
      }
      if (lower()) {
        if (CanOverflowSigned32(node->op(), left_feedback_type,
                                right_feedback_type, graph_zone())) {
          ChangeToInt32OverflowOp(node);
        } else {
          ChangeToPureOp(node, Int32Op(node));
        }
      }
      return;
    }

    // default case => Float64Add/Sub
    VisitBinop(node, UseInfo::CheckedNumberOrOddballAsFloat64(),
               MachineRepresentation::kFloat64, Type::Number());
    if (lower()) {
      ChangeToPureOp(node, Float64Op(node));
    }
    return;
  }

  void VisitSpeculativeNumberModulus(Node* node, Truncation truncation,
                                     SimplifiedLowering* lowering) {
    // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
    // can only eliminate an unused speculative number operation if we know
    // that the inputs are PlainPrimitive, which excludes everything that's
    // might have side effects or throws during a ToNumber conversion.
    if (BothInputsAre(node, Type::PlainPrimitive())) {
      if (truncation.IsUnused()) return VisitUnused(node);
    }
    if (BothInputsAre(node, Type::Unsigned32OrMinusZeroOrNaN()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node)->Is(Type::Unsigned32()))) {
      // => unsigned Uint32Mod
      VisitWord32TruncatingBinop(node);
      if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
      return;
    }
    if (BothInputsAre(node, Type::Signed32OrMinusZeroOrNaN()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node)->Is(Type::Signed32()))) {
      // => signed Int32Mod
      VisitWord32TruncatingBinop(node);
      if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
      return;
    }

    // Try to use type feedback.
    NumberOperationHint hint = NumberOperationHintOf(node->op());

    // Handle the case when no uint32 checks on inputs are necessary
    // (but an overflow check is needed on the output).
    if (BothInputsAreUnsigned32(node)) {
      if (hint == NumberOperationHint::kSignedSmall ||
          hint == NumberOperationHint::kSigned32) {
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   MachineRepresentation::kWord32, Type::Unsigned32());
        if (lower()) ChangeToUint32OverflowOp(node);
        return;
      }
    }

    // Handle the case when no int32 checks on inputs are necessary
    // (but an overflow check is needed on the output).
    if (BothInputsAre(node, Type::Signed32())) {
      // If both the inputs the feedback are int32, use the overflow op.
      if (hint == NumberOperationHint::kSignedSmall ||
          hint == NumberOperationHint::kSigned32) {
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   MachineRepresentation::kWord32, Type::Signed32());
        if (lower()) ChangeToInt32OverflowOp(node);
        return;
      }
    }

    if (hint == NumberOperationHint::kSignedSmall ||
        hint == NumberOperationHint::kSigned32) {
      // If the result is truncated, we only need to check the inputs.
      if (truncation.IsUsedAsWord32()) {
        VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                   MachineRepresentation::kWord32);
        if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
      } else if (BothInputsAre(node, Type::Unsigned32OrMinusZeroOrNaN())) {
        VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                   MachineRepresentation::kWord32, Type::Unsigned32());
        if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
      } else {
        VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                   MachineRepresentation::kWord32, Type::Signed32());
        if (lower()) ChangeToInt32OverflowOp(node);
      }
      return;
    }

    if (TypeOf(node->InputAt(0))->Is(Type::Unsigned32()) &&
        TypeOf(node->InputAt(1))->Is(Type::Unsigned32()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node)->Is(Type::Unsigned32()))) {
      // We can only promise Float64 truncation here, as the decision is
      // based on the feedback types of the inputs.
      VisitBinop(node,
                 UseInfo(MachineRepresentation::kWord32, Truncation::Float64()),
                 MachineRepresentation::kWord32, Type::Number());
      if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
      return;
    }
    if (TypeOf(node->InputAt(0))->Is(Type::Signed32()) &&
        TypeOf(node->InputAt(1))->Is(Type::Signed32()) &&
        (truncation.IsUsedAsWord32() ||
         NodeProperties::GetType(node)->Is(Type::Signed32()))) {
      // We can only promise Float64 truncation here, as the decision is
      // based on the feedback types of the inputs.
      VisitBinop(node,
                 UseInfo(MachineRepresentation::kWord32, Truncation::Float64()),
                 MachineRepresentation::kWord32, Type::Number());
      if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
      return;
    }
    // default case => Float64Mod
    VisitBinop(node, UseInfo::CheckedNumberOrOddballAsFloat64(),
               MachineRepresentation::kFloat64, Type::Number());
    if (lower()) ChangeToPureOp(node, Float64Op(node));
    return;
  }

  void VisitOsrGuard(Node* node) {
    VisitInputs(node);

    // Insert a dynamic check for the OSR value type if necessary.
    switch (OsrGuardTypeOf(node->op())) {
      case OsrGuardType::kUninitialized:
        // At this point, we should always have a type for the OsrValue.
        UNREACHABLE();
        break;
      case OsrGuardType::kSignedSmall:
        if (lower()) {
          NodeProperties::ChangeOp(node,
                                   simplified()->CheckedTaggedToTaggedSigned());
        }
        return SetOutput(node, MachineRepresentation::kTaggedSigned);
      case OsrGuardType::kAny:  // Nothing to check.
        if (lower()) {
          DeferReplacement(node, node->InputAt(0));
        }
        return SetOutput(node, MachineRepresentation::kTagged);
    }
    UNREACHABLE();
  }

  // Dispatching routine for visiting the node {node} with the usage {use}.
  // Depending on the operator, propagate new usage info to the inputs.
  void VisitNode(Node* node, Truncation truncation,
                 SimplifiedLowering* lowering) {
    // Unconditionally eliminate unused pure nodes (only relevant if there's
    // a pure operation in between two effectful ones, where the last one
    // is unused).
    // Note: We must not do this for constants, as they are cached and we
    // would thus kill the cached {node} during lowering (i.e. replace all
    // uses with Dead), but at that point some node lowering might have
    // already taken the constant {node} from the cache (while it was in
    // a sane state still) and we would afterwards replace that use with
    // Dead as well.
    if (node->op()->ValueInputCount() > 0 &&
        node->op()->HasProperty(Operator::kPure)) {
      if (truncation.IsUnused()) return VisitUnused(node);
    }
    switch (node->opcode()) {
      //------------------------------------------------------------------
      // Common operators.
      //------------------------------------------------------------------
      case IrOpcode::kStart:
        // We use Start as a terminator for the frame state chain, so even
        // tho Start doesn't really produce a value, we have to say Tagged
        // here, otherwise the input conversion will fail.
        return VisitLeaf(node, MachineRepresentation::kTagged);
      case IrOpcode::kParameter:
        // TODO(titzer): use representation from linkage.
        return VisitUnop(node, UseInfo::None(), MachineRepresentation::kTagged);
      case IrOpcode::kInt32Constant:
        return VisitLeaf(node, MachineRepresentation::kWord32);
      case IrOpcode::kInt64Constant:
        return VisitLeaf(node, MachineRepresentation::kWord64);
      case IrOpcode::kExternalConstant:
        return VisitLeaf(node, MachineType::PointerRepresentation());
      case IrOpcode::kNumberConstant:
        return VisitLeaf(node, MachineRepresentation::kTagged);
      case IrOpcode::kHeapConstant:
        return VisitLeaf(node, MachineRepresentation::kTaggedPointer);
      case IrOpcode::kPointerConstant: {
        VisitLeaf(node, MachineType::PointerRepresentation());
        if (lower()) {
          intptr_t const value = OpParameter<intptr_t>(node);
          DeferReplacement(node, lowering->jsgraph()->IntPtrConstant(value));
        }
        return;
      }

      case IrOpcode::kBranch:
        ProcessInput(node, 0, UseInfo::Bool());
        EnqueueInput(node, NodeProperties::FirstControlIndex(node));
        return;
      case IrOpcode::kSwitch:
        ProcessInput(node, 0, UseInfo::TruncatingWord32());
        EnqueueInput(node, NodeProperties::FirstControlIndex(node));
        return;
      case IrOpcode::kSelect:
        return VisitSelect(node, truncation, lowering);
      case IrOpcode::kPhi:
        return VisitPhi(node, truncation, lowering);
      case IrOpcode::kCall:
        return VisitCall(node, lowering);

      //------------------------------------------------------------------
      // JavaScript operators.
      //------------------------------------------------------------------
      case IrOpcode::kJSToBoolean: {
        if (truncation.IsUsedAsBool()) {
          ProcessInput(node, 0, UseInfo::Bool());
          ProcessInput(node, 1, UseInfo::None());
          SetOutput(node, MachineRepresentation::kBit);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitInputs(node);
          SetOutput(node, MachineRepresentation::kTaggedPointer);
        }
        return;
      }
      case IrOpcode::kJSToNumber: {
        VisitInputs(node);
        // TODO(bmeurer): Optimize somewhat based on input type?
        if (truncation.IsUsedAsWord32()) {
          SetOutput(node, MachineRepresentation::kWord32);
          if (lower()) lowering->DoJSToNumberTruncatesToWord32(node, this);
        } else if (truncation.IsUsedAsFloat64()) {
          SetOutput(node, MachineRepresentation::kFloat64);
          if (lower()) lowering->DoJSToNumberTruncatesToFloat64(node, this);
        } else {
          SetOutput(node, MachineRepresentation::kTagged);
        }
        return;
      }

      //------------------------------------------------------------------
      // Simplified operators.
      //------------------------------------------------------------------
      case IrOpcode::kBooleanNot: {
        if (lower()) {
          NodeInfo* input_info = GetInfo(node->InputAt(0));
          if (input_info->representation() == MachineRepresentation::kBit) {
            // BooleanNot(x: kRepBit) => Word32Equal(x, #0)
            node->AppendInput(jsgraph_->zone(), jsgraph_->Int32Constant(0));
            NodeProperties::ChangeOp(node, lowering->machine()->Word32Equal());
          } else {
            DCHECK(CanBeTaggedPointer(input_info->representation()));
            // BooleanNot(x: kRepTagged) => WordEqual(x, #false)
            node->AppendInput(jsgraph_->zone(), jsgraph_->FalseConstant());
            NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
          }
        } else {
          // No input representation requirement; adapt during lowering.
          ProcessInput(node, 0, UseInfo::AnyTruncatingToBool());
          SetOutput(node, MachineRepresentation::kBit);
        }
        return;
      }
      case IrOpcode::kNumberEqual: {
        Type* const lhs_type = TypeOf(node->InputAt(0));
        Type* const rhs_type = TypeOf(node->InputAt(1));
        // Number comparisons reduce to integer comparisons for integer inputs.
        if ((lhs_type->Is(Type::Unsigned32()) &&
             rhs_type->Is(Type::Unsigned32())) ||
            (lhs_type->Is(Type::Unsigned32OrMinusZeroOrNaN()) &&
             rhs_type->Is(Type::Unsigned32OrMinusZeroOrNaN()) &&
             OneInputCannotBe(node, type_cache_.kZeroish))) {
          // => unsigned Int32Cmp
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kBit);
          if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
          return;
        }
        if ((lhs_type->Is(Type::Signed32()) &&
             rhs_type->Is(Type::Signed32())) ||
            (lhs_type->Is(Type::Signed32OrMinusZeroOrNaN()) &&
             rhs_type->Is(Type::Signed32OrMinusZeroOrNaN()) &&
             OneInputCannotBe(node, type_cache_.kZeroish))) {
          // => signed Int32Cmp
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kBit);
          if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
          return;
        }
        // => Float64Cmp
        VisitBinop(node, UseInfo::TruncatingFloat64(),
                   MachineRepresentation::kBit);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberLessThan:
      case IrOpcode::kNumberLessThanOrEqual: {
        // Number comparisons reduce to integer comparisons for integer inputs.
        if (TypeOf(node->InputAt(0))->Is(Type::Unsigned32()) &&
            TypeOf(node->InputAt(1))->Is(Type::Unsigned32())) {
          // => unsigned Int32Cmp
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kBit);
          if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
        } else if (TypeOf(node->InputAt(0))->Is(Type::Signed32()) &&
                   TypeOf(node->InputAt(1))->Is(Type::Signed32())) {
          // => signed Int32Cmp
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kBit);
          if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
        } else {
          // => Float64Cmp
          VisitBinop(node, UseInfo::TruncatingFloat64(),
                     MachineRepresentation::kBit);
          if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }

      case IrOpcode::kSpeculativeNumberAdd:
      case IrOpcode::kSpeculativeNumberSubtract:
        return VisitSpeculativeAdditiveOp(node, truncation, lowering);

      case IrOpcode::kSpeculativeNumberLessThan:
      case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      case IrOpcode::kSpeculativeNumberEqual: {
        // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
        // can only eliminate an unused speculative number operation if we know
        // that the inputs are PlainPrimitive, which excludes everything that's
        // might have side effects or throws during a ToNumber conversion.
        if (BothInputsAre(node, Type::PlainPrimitive())) {
          if (truncation.IsUnused()) return VisitUnused(node);
        }
        // Number comparisons reduce to integer comparisons for integer inputs.
        if (TypeOf(node->InputAt(0))->Is(Type::Unsigned32()) &&
            TypeOf(node->InputAt(1))->Is(Type::Unsigned32())) {
          // => unsigned Int32Cmp
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kBit);
          if (lower()) ChangeToPureOp(node, Uint32Op(node));
          return;
        } else if (TypeOf(node->InputAt(0))->Is(Type::Signed32()) &&
                   TypeOf(node->InputAt(1))->Is(Type::Signed32())) {
          // => signed Int32Cmp
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kBit);
          if (lower()) ChangeToPureOp(node, Int32Op(node));
          return;
        }
        // Try to use type feedback.
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        switch (hint) {
          case NumberOperationHint::kSignedSmall:
          case NumberOperationHint::kSigned32: {
            if (propagate()) {
              VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                         MachineRepresentation::kBit);
            } else if (retype()) {
              SetOutput(node, MachineRepresentation::kBit, Type::Any());
            } else {
              DCHECK(lower());
              Node* lhs = node->InputAt(0);
              Node* rhs = node->InputAt(1);
              if (IsNodeRepresentationTagged(lhs) &&
                  IsNodeRepresentationTagged(rhs)) {
                VisitBinop(node, UseInfo::CheckedSignedSmallAsTaggedSigned(),
                           MachineRepresentation::kBit);
                ChangeToPureOp(
                    node, changer_->TaggedSignedOperatorFor(node->opcode()));

              } else {
                VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                           MachineRepresentation::kBit);
                ChangeToPureOp(node, Int32Op(node));
              }
            }
            return;
          }
          case NumberOperationHint::kNumberOrOddball:
            // Abstract and strict equality don't perform ToNumber conversions
            // on Oddballs, so make sure we don't accidentially sneak in a
            // hint with Oddball feedback here.
            DCHECK_NE(IrOpcode::kSpeculativeNumberEqual, node->opcode());
          // Fallthrough
          case NumberOperationHint::kNumber:
            VisitBinop(node, CheckedUseInfoAsFloat64FromHint(hint),
                       MachineRepresentation::kBit);
            if (lower()) ChangeToPureOp(node, Float64Op(node));
            return;
        }
        UNREACHABLE();
        return;
      }

      case IrOpcode::kNumberAdd:
      case IrOpcode::kNumberSubtract: {
        if (BothInputsAre(node, type_cache_.kAdditiveSafeIntegerOrMinusZero) &&
            (GetUpperBound(node)->Is(Type::Signed32()) ||
             GetUpperBound(node)->Is(Type::Unsigned32()) ||
             truncation.IsUsedAsWord32())) {
          // => Int32Add/Sub
          VisitWord32TruncatingBinop(node);
          if (lower()) ChangeToPureOp(node, Int32Op(node));
        } else {
          // => Float64Add/Sub
          VisitFloat64Binop(node);
          if (lower()) ChangeToPureOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberMultiply: {
        // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
        // can only eliminate an unused speculative number operation if we know
        // that the inputs are PlainPrimitive, which excludes everything that's
        // might have side effects or throws during a ToNumber conversion.
        if (BothInputsAre(node, Type::PlainPrimitive())) {
          if (truncation.IsUnused()) return VisitUnused(node);
        }
        if (BothInputsAre(node, Type::Integral32()) &&
            (NodeProperties::GetType(node)->Is(Type::Signed32()) ||
             NodeProperties::GetType(node)->Is(Type::Unsigned32()) ||
             (truncation.IsUsedAsWord32() &&
              NodeProperties::GetType(node)->Is(
                  type_cache_.kSafeIntegerOrMinusZero)))) {
          // Multiply reduces to Int32Mul if the inputs are integers, and
          // (a) the output is either known to be Signed32, or
          // (b) the output is known to be Unsigned32, or
          // (c) the uses are truncating and the result is in the safe
          //     integer range.
          VisitWord32TruncatingBinop(node);
          if (lower()) ChangeToPureOp(node, Int32Op(node));
          return;
        }
        // Try to use type feedback.
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type* input0_type = TypeOf(node->InputAt(0));
        Type* input1_type = TypeOf(node->InputAt(1));

        // Handle the case when no int32 checks on inputs are necessary
        // (but an overflow check is needed on the output).
        if (BothInputsAre(node, Type::Signed32())) {
          // If both the inputs the feedback are int32, use the overflow op.
          if (hint == NumberOperationHint::kSignedSmall ||
              hint == NumberOperationHint::kSigned32) {
            VisitBinop(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32, Type::Signed32());
            if (lower()) {
              LowerToCheckedInt32Mul(node, truncation, input0_type,
                                     input1_type);
            }
            return;
          }
        }

        if (hint == NumberOperationHint::kSignedSmall ||
            hint == NumberOperationHint::kSigned32) {
          VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                     MachineRepresentation::kWord32, Type::Signed32());
          if (lower()) {
            LowerToCheckedInt32Mul(node, truncation, input0_type, input1_type);
          }
          return;
        }

        // Checked float64 x float64 => float64
        VisitBinop(node, UseInfo::CheckedNumberOrOddballAsFloat64(),
                   MachineRepresentation::kFloat64, Type::Number());
        if (lower()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberMultiply: {
        if (BothInputsAre(node, Type::Integral32()) &&
            (NodeProperties::GetType(node)->Is(Type::Signed32()) ||
             NodeProperties::GetType(node)->Is(Type::Unsigned32()) ||
             (truncation.IsUsedAsWord32() &&
              NodeProperties::GetType(node)->Is(
                  type_cache_.kSafeIntegerOrMinusZero)))) {
          // Multiply reduces to Int32Mul if the inputs are integers, and
          // (a) the output is either known to be Signed32, or
          // (b) the output is known to be Unsigned32, or
          // (c) the uses are truncating and the result is in the safe
          //     integer range.
          VisitWord32TruncatingBinop(node);
          if (lower()) ChangeToPureOp(node, Int32Op(node));
          return;
        }
        // Number x Number => Float64Mul
        VisitFloat64Binop(node);
        if (lower()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kSpeculativeNumberDivide: {
        // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
        // can only eliminate an unused speculative number operation if we know
        // that the inputs are PlainPrimitive, which excludes everything that's
        // might have side effects or throws during a ToNumber conversion.
        if (BothInputsAre(node, Type::PlainPrimitive())) {
          if (truncation.IsUnused()) return VisitUnused(node);
        }
        if (BothInputsAreUnsigned32(node) && truncation.IsUsedAsWord32()) {
          // => unsigned Uint32Div
          VisitWord32TruncatingBinop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Div(node));
          return;
        }
        if (BothInputsAreSigned32(node)) {
          if (NodeProperties::GetType(node)->Is(Type::Signed32())) {
            // => signed Int32Div
            VisitWord32TruncatingBinop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          }
          if (truncation.IsUsedAsWord32()) {
            // => signed Int32Div
            VisitWord32TruncatingBinop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          }
        }

        // Try to use type feedback.
        NumberOperationHint hint = NumberOperationHintOf(node->op());

        // Handle the case when no uint32 checks on inputs are necessary
        // (but an overflow check is needed on the output).
        if (BothInputsAreUnsigned32(node)) {
          if (hint == NumberOperationHint::kSignedSmall ||
              hint == NumberOperationHint::kSigned32) {
            VisitBinop(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32, Type::Unsigned32());
            if (lower()) ChangeToUint32OverflowOp(node);
            return;
          }
        }

        // Handle the case when no int32 checks on inputs are necessary
        // (but an overflow check is needed on the output).
        if (BothInputsAreSigned32(node)) {
          // If both the inputs the feedback are int32, use the overflow op.
          if (hint == NumberOperationHint::kSignedSmall ||
              hint == NumberOperationHint::kSigned32) {
            VisitBinop(node, UseInfo::TruncatingWord32(),
                       MachineRepresentation::kWord32, Type::Signed32());
            if (lower()) ChangeToInt32OverflowOp(node);
            return;
          }
        }

        if (hint == NumberOperationHint::kSignedSmall ||
            hint == NumberOperationHint::kSigned32) {
          // If the result is truncated, we only need to check the inputs.
          if (truncation.IsUsedAsWord32()) {
            VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                       MachineRepresentation::kWord32);
            if (lower()) DeferReplacement(node, lowering->Int32Div(node));
          } else {
            VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                       MachineRepresentation::kWord32, Type::Signed32());
            if (lower()) ChangeToInt32OverflowOp(node);
          }
          return;
        }

        // default case => Float64Div
        VisitBinop(node, UseInfo::CheckedNumberOrOddballAsFloat64(),
                   MachineRepresentation::kFloat64, Type::Number());
        if (lower()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberDivide: {
        if (BothInputsAreUnsigned32(node) && truncation.IsUsedAsWord32()) {
          // => unsigned Uint32Div
          VisitWord32TruncatingBinop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Div(node));
          return;
        }
        if (BothInputsAreSigned32(node)) {
          if (NodeProperties::GetType(node)->Is(Type::Signed32())) {
            // => signed Int32Div
            VisitWord32TruncatingBinop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          }
          if (truncation.IsUsedAsWord32()) {
            // => signed Int32Div
            VisitWord32TruncatingBinop(node);
            if (lower()) DeferReplacement(node, lowering->Int32Div(node));
            return;
          }
        }
        // Number x Number => Float64Div
        VisitFloat64Binop(node);
        if (lower()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kSpeculativeNumberModulus:
        return VisitSpeculativeNumberModulus(node, truncation, lowering);
      case IrOpcode::kNumberModulus: {
        if (BothInputsAre(node, Type::Unsigned32OrMinusZeroOrNaN()) &&
            (truncation.IsUsedAsWord32() ||
             NodeProperties::GetType(node)->Is(Type::Unsigned32()))) {
          // => unsigned Uint32Mod
          VisitWord32TruncatingBinop(node);
          if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
          return;
        }
        if (BothInputsAre(node, Type::Signed32OrMinusZeroOrNaN()) &&
            (truncation.IsUsedAsWord32() ||
             NodeProperties::GetType(node)->Is(Type::Signed32()))) {
          // => signed Int32Mod
          VisitWord32TruncatingBinop(node);
          if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
          return;
        }
        if (TypeOf(node->InputAt(0))->Is(Type::Unsigned32()) &&
            TypeOf(node->InputAt(1))->Is(Type::Unsigned32()) &&
            (truncation.IsUsedAsWord32() ||
             NodeProperties::GetType(node)->Is(Type::Unsigned32()))) {
          // We can only promise Float64 truncation here, as the decision is
          // based on the feedback types of the inputs.
          VisitBinop(node, UseInfo(MachineRepresentation::kWord32,
                                   Truncation::Float64()),
                     MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, lowering->Uint32Mod(node));
          return;
        }
        if (TypeOf(node->InputAt(0))->Is(Type::Signed32()) &&
            TypeOf(node->InputAt(1))->Is(Type::Signed32()) &&
            (truncation.IsUsedAsWord32() ||
             NodeProperties::GetType(node)->Is(Type::Signed32()))) {
          // We can only promise Float64 truncation here, as the decision is
          // based on the feedback types of the inputs.
          VisitBinop(node, UseInfo(MachineRepresentation::kWord32,
                                   Truncation::Float64()),
                     MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, lowering->Int32Mod(node));
          return;
        }
        // default case => Float64Mod
        VisitFloat64Binop(node);
        if (lower()) ChangeToPureOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberBitwiseOr:
      case IrOpcode::kNumberBitwiseXor:
      case IrOpcode::kNumberBitwiseAnd: {
        VisitWord32TruncatingBinop(node);
        if (lower()) NodeProperties::ChangeOp(node, Int32Op(node));
        return;
      }
      case IrOpcode::kSpeculativeNumberBitwiseOr:
      case IrOpcode::kSpeculativeNumberBitwiseXor:
      case IrOpcode::kSpeculativeNumberBitwiseAnd:
        VisitSpeculativeInt32Binop(node);
        if (lower()) {
          ChangeToPureOp(node, Int32Op(node));
        }
        return;
      case IrOpcode::kNumberShiftLeft: {
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shl(), rhs_type);
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberShiftLeft: {
        // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
        // can only eliminate an unused speculative number operation if we know
        // that the inputs are PlainPrimitive, which excludes everything that's
        // might have side effects or throws during a ToNumber conversion.
        if (BothInputsAre(node, Type::PlainPrimitive())) {
          if (truncation.IsUnused()) return VisitUnused(node);
        }
        if (BothInputsAre(node, Type::NumberOrOddball())) {
          Type* rhs_type = GetUpperBound(node->InputAt(1));
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
          if (lower()) {
            lowering->DoShift(node, lowering->machine()->Word32Shl(), rhs_type);
          }
          return;
        }
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                   MachineRepresentation::kWord32, Type::Signed32());
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shl(), rhs_type);
        }
        return;
      }
      case IrOpcode::kNumberShiftRight: {
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Sar(), rhs_type);
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberShiftRight: {
        // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
        // can only eliminate an unused speculative number operation if we know
        // that the inputs are PlainPrimitive, which excludes everything that's
        // might have side effects or throws during a ToNumber conversion.
        if (BothInputsAre(node, Type::PlainPrimitive())) {
          if (truncation.IsUnused()) return VisitUnused(node);
        }
        if (BothInputsAre(node, Type::NumberOrOddball())) {
          Type* rhs_type = GetUpperBound(node->InputAt(1));
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
          if (lower()) {
            lowering->DoShift(node, lowering->machine()->Word32Sar(), rhs_type);
          }
          return;
        }
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                   MachineRepresentation::kWord32, Type::Signed32());
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Sar(), rhs_type);
        }
        return;
      }
      case IrOpcode::kNumberShiftRightLogical: {
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shr(), rhs_type);
        }
        return;
      }
      case IrOpcode::kSpeculativeNumberShiftRightLogical: {
        // ToNumber(x) can throw if x is either a Receiver or a Symbol, so we
        // can only eliminate an unused speculative number operation if we know
        // that the inputs are PlainPrimitive, which excludes everything that's
        // might have side effects or throws during a ToNumber conversion.
        if (BothInputsAre(node, Type::PlainPrimitive())) {
          if (truncation.IsUnused()) return VisitUnused(node);
        }
        if (BothInputsAre(node, Type::NumberOrOddball())) {
          Type* rhs_type = GetUpperBound(node->InputAt(1));
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
          if (lower()) {
            lowering->DoShift(node, lowering->machine()->Word32Shr(), rhs_type);
          }
          return;
        }
        NumberOperationHint hint = NumberOperationHintOf(node->op());
        Type* rhs_type = GetUpperBound(node->InputAt(1));
        VisitBinop(node, CheckedUseInfoAsWord32FromHint(hint),
                   MachineRepresentation::kWord32, Type::Unsigned32());
        if (lower()) {
          lowering->DoShift(node, lowering->machine()->Word32Shr(), rhs_type);
        }
        return;
      }
      case IrOpcode::kNumberAbs: {
        if (TypeOf(node->InputAt(0))->Is(Type::Unsigned32())) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if (TypeOf(node->InputAt(0))->Is(Type::Signed32())) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, lowering->Int32Abs(node));
        } else if (TypeOf(node->InputAt(0))
                       ->Is(type_cache_.kPositiveIntegerOrMinusZeroOrNaN)) {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kFloat64);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kFloat64);
          if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kNumberClz32: {
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
        if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
        return;
      }
      case IrOpcode::kNumberImul: {
        VisitBinop(node, UseInfo::TruncatingWord32(),
                   UseInfo::TruncatingWord32(), MachineRepresentation::kWord32);
        if (lower()) NodeProperties::ChangeOp(node, Uint32Op(node));
        return;
      }
      case IrOpcode::kNumberFround: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat32);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberMax: {
        // TODO(turbofan): We should consider feedback types here as well.
        if (BothInputsAreUnsigned32(node)) {
          VisitWord32TruncatingBinop(node);
          if (lower()) {
            lowering->DoMax(node, lowering->machine()->Uint32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if (BothInputsAreSigned32(node)) {
          VisitWord32TruncatingBinop(node);
          if (lower()) {
            lowering->DoMax(node, lowering->machine()->Int32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if (BothInputsAre(node, Type::PlainNumber())) {
          VisitFloat64Binop(node);
          if (lower()) {
            lowering->DoMax(node, lowering->machine()->Float64LessThan(),
                            MachineRepresentation::kFloat64);
          }
        } else {
          VisitFloat64Binop(node);
          if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kNumberMin: {
        // TODO(turbofan): We should consider feedback types here as well.
        if (BothInputsAreUnsigned32(node)) {
          VisitWord32TruncatingBinop(node);
          if (lower()) {
            lowering->DoMin(node, lowering->machine()->Uint32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if (BothInputsAreSigned32(node)) {
          VisitWord32TruncatingBinop(node);
          if (lower()) {
            lowering->DoMin(node, lowering->machine()->Int32LessThan(),
                            MachineRepresentation::kWord32);
          }
        } else if (BothInputsAre(node, Type::PlainNumber())) {
          VisitFloat64Binop(node);
          if (lower()) {
            lowering->DoMin(node, lowering->machine()->Float64LessThan(),
                            MachineRepresentation::kFloat64);
          }
        } else {
          VisitFloat64Binop(node);
          if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        }
        return;
      }
      case IrOpcode::kNumberAtan2:
      case IrOpcode::kNumberPow: {
        VisitBinop(node, UseInfo::TruncatingFloat64(),
                   MachineRepresentation::kFloat64);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberAcos:
      case IrOpcode::kNumberAcosh:
      case IrOpcode::kNumberAsin:
      case IrOpcode::kNumberAsinh:
      case IrOpcode::kNumberAtan:
      case IrOpcode::kNumberAtanh:
      case IrOpcode::kNumberCeil:
      case IrOpcode::kNumberCos:
      case IrOpcode::kNumberCosh:
      case IrOpcode::kNumberExp:
      case IrOpcode::kNumberExpm1:
      case IrOpcode::kNumberFloor:
      case IrOpcode::kNumberLog:
      case IrOpcode::kNumberLog1p:
      case IrOpcode::kNumberLog2:
      case IrOpcode::kNumberLog10:
      case IrOpcode::kNumberCbrt:
      case IrOpcode::kNumberSin:
      case IrOpcode::kNumberSinh:
      case IrOpcode::kNumberTan:
      case IrOpcode::kNumberTanh:
      case IrOpcode::kNumberTrunc: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberRound: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) DeferReplacement(node, lowering->Float64Round(node));
        return;
      }
      case IrOpcode::kNumberSign: {
        if (InputIs(node, Type::Signed32())) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, lowering->Int32Sign(node));
        } else {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kFloat64);
          if (lower()) DeferReplacement(node, lowering->Float64Sign(node));
        }
        return;
      }
      case IrOpcode::kNumberSqrt: {
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      }
      case IrOpcode::kNumberToBoolean: {
        Type* const input_type = TypeOf(node->InputAt(0));
        if (input_type->Is(Type::Integral32())) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kBit);
          if (lower()) lowering->DoIntegral32ToBit(node);
        } else if (input_type->Is(Type::OrderedNumber())) {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kBit);
          if (lower()) lowering->DoOrderedNumberToBit(node);
        } else {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kBit);
          if (lower()) lowering->DoNumberToBit(node);
        }
        return;
      }
      case IrOpcode::kNumberToInt32: {
        // Just change representation if necessary.
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
        if (lower()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kNumberToUint32: {
        // Just change representation if necessary.
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kWord32);
        if (lower()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kNumberToUint8Clamped: {
        Type* const input_type = TypeOf(node->InputAt(0));
        if (input_type->Is(type_cache_.kUint8OrMinusZeroOrNaN)) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if (input_type->Is(Type::Unsigned32OrMinusZeroOrNaN())) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) lowering->DoUnsigned32ToUint8Clamped(node);
        } else if (input_type->Is(Type::Signed32OrMinusZeroOrNaN())) {
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) lowering->DoSigned32ToUint8Clamped(node);
        } else if (input_type->Is(type_cache_.kIntegerOrMinusZeroOrNaN)) {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kFloat64);
          if (lower()) lowering->DoIntegerToUint8Clamped(node);
        } else {
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kFloat64);
          if (lower()) lowering->DoNumberToUint8Clamped(node);
        }
        return;
      }
      case IrOpcode::kReferenceEqual: {
        VisitBinop(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        if (lower()) {
          NodeProperties::ChangeOp(node, lowering->machine()->WordEqual());
        }
        return;
      }
      case IrOpcode::kStringEqual:
      case IrOpcode::kStringLessThan:
      case IrOpcode::kStringLessThanOrEqual: {
        return VisitBinop(node, UseInfo::AnyTagged(),
                          MachineRepresentation::kTaggedPointer);
      }
      case IrOpcode::kStringCharAt: {
        VisitBinop(node, UseInfo::AnyTagged(), UseInfo::TruncatingWord32(),
                   MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kStringCharCodeAt: {
        // TODO(turbofan): Allow builtins to return untagged values.
        VisitBinop(node, UseInfo::AnyTagged(), UseInfo::TruncatingWord32(),
                   MachineRepresentation::kTaggedSigned);
        return;
      }
      case IrOpcode::kStringFromCharCode: {
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kStringFromCodePoint: {
        VisitUnop(node, UseInfo::TruncatingWord32(),
                  MachineRepresentation::kTaggedPointer);
        return;
      }

      case IrOpcode::kCheckBounds: {
        Type* index_type = TypeOf(node->InputAt(0));
        Type* length_type = TypeOf(node->InputAt(1));
        if (index_type->Is(Type::Unsigned32())) {
          VisitBinop(node, UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
          if (lower() && index_type->Max() < length_type->Min()) {
            // The bounds check is redundant if we already know that
            // the index is within the bounds of [0.0, length[.
            DeferReplacement(node, node->InputAt(0));
          }
        } else {
          VisitBinop(node, UseInfo::CheckedSigned32AsWord32(),
                     UseInfo::TruncatingWord32(),
                     MachineRepresentation::kWord32);
        }
        return;
      }
      case IrOpcode::kCheckHeapObject: {
        if (InputCannotBe(node, Type::SignedSmall())) {
          VisitUnop(node, UseInfo::AnyTagged(),
                    MachineRepresentation::kTaggedPointer);
        } else {
          VisitUnop(node, UseInfo::CheckedHeapObjectAsTaggedPointer(),
                    MachineRepresentation::kTaggedPointer);
        }
        if (lower()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kCheckIf: {
        ProcessInput(node, 0, UseInfo::Bool());
        ProcessRemainingInputs(node, 1);
        SetOutput(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kCheckInternalizedString: {
        if (InputIs(node, Type::InternalizedString())) {
          VisitUnop(node, UseInfo::AnyTagged(),
                    MachineRepresentation::kTaggedPointer);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop(node, UseInfo::AnyTagged(),
                    MachineRepresentation::kTaggedPointer);
        }
        return;
      }
      case IrOpcode::kCheckNumber: {
        if (InputIs(node, Type::Number())) {
          if (truncation.IsUsedAsWord32()) {
            VisitUnop(node, UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
          } else {
            // TODO(jarin,bmeurer): We need to go to Tagged here, because
            // otherwise we cannot distinguish the hole NaN (which might need to
            // be treated as undefined). We should have a dedicated Type for
            // that at some point, and maybe even a dedicated truncation.
            VisitUnop(node, UseInfo::AnyTagged(),
                      MachineRepresentation::kTagged);
          }
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        }
        return;
      }
      case IrOpcode::kCheckSmi: {
        if (SmiValuesAre32Bits() && truncation.IsUsedAsWord32()) {
          VisitUnop(node, UseInfo::CheckedSignedSmallAsWord32(),
                    MachineRepresentation::kWord32);
        } else {
          VisitUnop(node, UseInfo::CheckedSignedSmallAsTaggedSigned(),
                    MachineRepresentation::kTaggedSigned);
        }
        if (lower()) DeferReplacement(node, node->InputAt(0));
        return;
      }
      case IrOpcode::kCheckString: {
        if (InputIs(node, Type::String())) {
          VisitUnop(node, UseInfo::AnyTagged(),
                    MachineRepresentation::kTaggedPointer);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          VisitUnop(node, UseInfo::AnyTagged(),
                    MachineRepresentation::kTaggedPointer);
        }
        return;
      }

      case IrOpcode::kAllocate: {
        ProcessInput(node, 0, UseInfo::TruncatingWord32());
        ProcessRemainingInputs(node, 1);
        SetOutput(node, MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kLoadField: {
        if (truncation.IsUnused()) return VisitUnused(node);
        FieldAccess access = FieldAccessOf(node->op());
        MachineRepresentation const representation =
            access.machine_type.representation();
        VisitUnop(node, UseInfoForBasePointer(access), representation);
        return;
      }
      case IrOpcode::kStoreField: {
        FieldAccess access = FieldAccessOf(node->op());
        NodeInfo* input_info = GetInfo(node->InputAt(1));
        WriteBarrierKind write_barrier_kind = WriteBarrierKindFor(
            access.base_is_tagged, access.machine_type.representation(),
            access.offset, access.type, input_info->representation(),
            node->InputAt(1));
        ProcessInput(node, 0, UseInfoForBasePointer(access));
        ProcessInput(node, 1, TruncatingUseInfoFromRepresentation(
                                  access.machine_type.representation()));
        ProcessRemainingInputs(node, 2);
        SetOutput(node, MachineRepresentation::kNone);
        if (lower()) {
          if (write_barrier_kind < access.write_barrier_kind) {
            access.write_barrier_kind = write_barrier_kind;
            NodeProperties::ChangeOp(
                node, jsgraph_->simplified()->StoreField(access));
          }
        }
        return;
      }
      case IrOpcode::kLoadBuffer: {
        if (truncation.IsUnused()) return VisitUnused(node);
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, UseInfo::PointerInt());        // buffer
        ProcessInput(node, 1, UseInfo::TruncatingWord32());  // offset
        ProcessInput(node, 2, UseInfo::TruncatingWord32());  // length
        ProcessRemainingInputs(node, 3);

        MachineRepresentation output;
        if (truncation.IdentifiesUndefinedAndNaNAndZero()) {
          if (truncation.IdentifiesNaNAndZero()) {
            // If undefined is truncated to a non-NaN number, we can use
            // the load's representation.
            output = access.machine_type().representation();
          } else {
            // If undefined is truncated to a number, but the use can
            // observe NaN, we need to output at least the float32
            // representation.
            if (access.machine_type().representation() ==
                MachineRepresentation::kFloat32) {
              output = access.machine_type().representation();
            } else {
              output = MachineRepresentation::kFloat64;
            }
          }
        } else {
          // If undefined is not truncated away, we need to have the tagged
          // representation.
          output = MachineRepresentation::kTagged;
        }
        SetOutput(node, output);
        if (lower()) lowering->DoLoadBuffer(node, output, changer_);
        return;
      }
      case IrOpcode::kStoreBuffer: {
        BufferAccess access = BufferAccessOf(node->op());
        ProcessInput(node, 0, UseInfo::PointerInt());        // buffer
        ProcessInput(node, 1, UseInfo::TruncatingWord32());  // offset
        ProcessInput(node, 2, UseInfo::TruncatingWord32());  // length
        ProcessInput(node, 3,
                     TruncatingUseInfoFromRepresentation(
                         access.machine_type().representation()));  // value
        ProcessRemainingInputs(node, 4);
        SetOutput(node, MachineRepresentation::kNone);
        if (lower()) lowering->DoStoreBuffer(node);
        return;
      }
      case IrOpcode::kLoadElement: {
        if (truncation.IsUnused()) return VisitUnused(node);
        ElementAccess access = ElementAccessOf(node->op());
        VisitBinop(node, UseInfoForBasePointer(access),
                   UseInfo::TruncatingWord32(),
                   access.machine_type.representation());
        return;
      }
      case IrOpcode::kStoreElement: {
        ElementAccess access = ElementAccessOf(node->op());
        NodeInfo* input_info = GetInfo(node->InputAt(2));
        WriteBarrierKind write_barrier_kind = WriteBarrierKindFor(
            access.base_is_tagged, access.machine_type.representation(),
            access.type, input_info->representation(), node->InputAt(2));
        ProcessInput(node, 0, UseInfoForBasePointer(access));  // base
        ProcessInput(node, 1, UseInfo::TruncatingWord32());    // index
        ProcessInput(node, 2,
                     TruncatingUseInfoFromRepresentation(
                         access.machine_type.representation()));  // value
        ProcessRemainingInputs(node, 3);
        SetOutput(node, MachineRepresentation::kNone);
        if (lower()) {
          if (write_barrier_kind < access.write_barrier_kind) {
            access.write_barrier_kind = write_barrier_kind;
            NodeProperties::ChangeOp(
                node, jsgraph_->simplified()->StoreElement(access));
          }
        }
        return;
      }
      case IrOpcode::kLoadTypedElement: {
        MachineRepresentation const rep =
            MachineRepresentationFromArrayType(ExternalArrayTypeOf(node->op()));
        ProcessInput(node, 0, UseInfo::AnyTagged());         // buffer
        ProcessInput(node, 1, UseInfo::AnyTagged());         // base pointer
        ProcessInput(node, 2, UseInfo::PointerInt());        // external pointer
        ProcessInput(node, 3, UseInfo::TruncatingWord32());  // index
        ProcessRemainingInputs(node, 4);
        SetOutput(node, rep);
        return;
      }
      case IrOpcode::kStoreTypedElement: {
        MachineRepresentation const rep =
            MachineRepresentationFromArrayType(ExternalArrayTypeOf(node->op()));
        ProcessInput(node, 0, UseInfo::AnyTagged());         // buffer
        ProcessInput(node, 1, UseInfo::AnyTagged());         // base pointer
        ProcessInput(node, 2, UseInfo::PointerInt());        // external pointer
        ProcessInput(node, 3, UseInfo::TruncatingWord32());  // index
        ProcessInput(node, 4,
                     TruncatingUseInfoFromRepresentation(rep));  // value
        ProcessRemainingInputs(node, 5);
        SetOutput(node, MachineRepresentation::kNone);
        return;
      }
      case IrOpcode::kPlainPrimitiveToNumber: {
        if (InputIs(node, Type::Boolean())) {
          VisitUnop(node, UseInfo::Bool(), MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if (InputIs(node, Type::String())) {
          VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
          if (lower()) lowering->DoStringToNumber(node);
        } else if (truncation.IsUsedAsWord32()) {
          if (InputIs(node, Type::NumberOrOddball())) {
            VisitUnop(node, UseInfo::TruncatingWord32(),
                      MachineRepresentation::kWord32);
            if (lower()) DeferReplacement(node, node->InputAt(0));
          } else {
            VisitUnop(node, UseInfo::AnyTagged(),
                      MachineRepresentation::kWord32);
            if (lower()) {
              NodeProperties::ChangeOp(node,
                                       simplified()->PlainPrimitiveToWord32());
            }
          }
        } else if (truncation.IsUsedAsFloat64()) {
          if (InputIs(node, Type::NumberOrOddball())) {
            VisitUnop(node, UseInfo::TruncatingFloat64(),
                      MachineRepresentation::kFloat64);
            if (lower()) DeferReplacement(node, node->InputAt(0));
          } else {
            VisitUnop(node, UseInfo::AnyTagged(),
                      MachineRepresentation::kFloat64);
            if (lower()) {
              NodeProperties::ChangeOp(node,
                                       simplified()->PlainPrimitiveToFloat64());
            }
          }
        } else {
          VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        }
        return;
      }
      case IrOpcode::kObjectIsCallable: {
        // TODO(turbofan): Add Type::Callable to optimize this?
        VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsNumber: {
        VisitObjectIs(node, Type::Number(), lowering);
        return;
      }
      case IrOpcode::kObjectIsReceiver: {
        VisitObjectIs(node, Type::Receiver(), lowering);
        return;
      }
      case IrOpcode::kObjectIsSmi: {
        // TODO(turbofan): Optimize based on input representation.
        VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kObjectIsString: {
        VisitObjectIs(node, Type::String(), lowering);
        return;
      }
      case IrOpcode::kObjectIsUndetectable: {
        VisitObjectIs(node, Type::Undetectable(), lowering);
        return;
      }
      case IrOpcode::kNewRestParameterElements:
      case IrOpcode::kNewUnmappedArgumentsElements: {
        ProcessRemainingInputs(node, 0);
        SetOutput(node, MachineRepresentation::kTaggedPointer);
        return;
      }
      case IrOpcode::kArrayBufferWasNeutered: {
        VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kBit);
        return;
      }
      case IrOpcode::kCheckFloat64Hole: {
        if (truncation.IsUnused()) return VisitUnused(node);
        CheckFloat64HoleMode mode = CheckFloat64HoleModeOf(node->op());
        ProcessInput(node, 0, UseInfo::TruncatingFloat64());
        ProcessRemainingInputs(node, 1);
        SetOutput(node, MachineRepresentation::kFloat64);
        if (truncation.IsUsedAsFloat64() &&
            mode == CheckFloat64HoleMode::kAllowReturnHole) {
          if (lower()) DeferReplacement(node, node->InputAt(0));
        }
        return;
      }
      case IrOpcode::kCheckTaggedHole: {
        VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        return;
      }
      case IrOpcode::kConvertTaggedHoleToUndefined: {
        if (InputIs(node, Type::NumberOrOddball()) &&
            truncation.IsUsedAsWord32()) {
          // Propagate the Word32 truncation.
          VisitUnop(node, UseInfo::TruncatingWord32(),
                    MachineRepresentation::kWord32);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if (InputIs(node, Type::NumberOrOddball()) &&
                   truncation.IsUsedAsFloat64()) {
          // Propagate the Float64 truncation.
          VisitUnop(node, UseInfo::TruncatingFloat64(),
                    MachineRepresentation::kFloat64);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else if (InputIs(node, Type::NonInternal())) {
          VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
          if (lower()) DeferReplacement(node, node->InputAt(0));
        } else {
          // TODO(turbofan): Add a (Tagged) truncation that identifies hole
          // and undefined, i.e. for a[i] === obj cases.
          VisitUnop(node, UseInfo::AnyTagged(), MachineRepresentation::kTagged);
        }
        return;
      }
      case IrOpcode::kCheckMaps:
      case IrOpcode::kTransitionElementsKind: {
        VisitInputs(node);
        return SetOutput(node, MachineRepresentation::kNone);
      }
      case IrOpcode::kEnsureWritableFastElements:
        return VisitBinop(node, UseInfo::AnyTagged(),
                          MachineRepresentation::kTaggedPointer);
      case IrOpcode::kMaybeGrowFastElements: {
        ProcessInput(node, 0, UseInfo::AnyTagged());         // object
        ProcessInput(node, 1, UseInfo::AnyTagged());         // elements
        ProcessInput(node, 2, UseInfo::TruncatingWord32());  // index
        ProcessInput(node, 3, UseInfo::TruncatingWord32());  // length
        ProcessRemainingInputs(node, 4);
        SetOutput(node, MachineRepresentation::kTaggedPointer);
        return;
      }

      case IrOpcode::kNumberSilenceNaN:
        VisitUnop(node, UseInfo::TruncatingFloat64(),
                  MachineRepresentation::kFloat64);
        if (lower()) NodeProperties::ChangeOp(node, Float64Op(node));
        return;
      case IrOpcode::kStateValues:
        return VisitStateValues(node);
      case IrOpcode::kObjectState:
        return VisitObjectState(node);
      case IrOpcode::kTypeGuard: {
        // We just get rid of the sigma here. In principle, it should be
        // possible to refine the truncation and representation based on
        // the sigma's type.
        MachineRepresentation output =
            GetOutputInfoForPhi(node, TypeOf(node->InputAt(0)), truncation);
        VisitUnop(node, UseInfo(output, truncation), output);
        if (lower()) DeferReplacement(node, node->InputAt(0));
        return;
      }

      case IrOpcode::kOsrGuard:
        return VisitOsrGuard(node);

      case IrOpcode::kFinishRegion:
        VisitInputs(node);
        // Assume the output is tagged pointer.
        return SetOutput(node, MachineRepresentation::kTaggedPointer);

      case IrOpcode::kReturn:
        VisitReturn(node);
        // Assume the output is tagged.
        return SetOutput(node, MachineRepresentation::kTagged);

      // Operators with all inputs tagged and no or tagged output have uniform
      // handling.
      case IrOpcode::kEnd:
      case IrOpcode::kIfSuccess:
      case IrOpcode::kIfException:
      case IrOpcode::kIfTrue:
      case IrOpcode::kIfFalse:
      case IrOpcode::kDeoptimize:
      case IrOpcode::kEffectPhi:
      case IrOpcode::kTerminate:
      case IrOpcode::kFrameState:
      case IrOpcode::kCheckpoint:
      case IrOpcode::kLoop:
      case IrOpcode::kMerge:
      case IrOpcode::kThrow:
      case IrOpcode::kBeginRegion:
      case IrOpcode::kProjection:
      case IrOpcode::kOsrValue:
// All JavaScript operators except JSToNumber have uniform handling.
#define OPCODE_CASE(name) case IrOpcode::k##name:
        JS_SIMPLE_BINOP_LIST(OPCODE_CASE)
        JS_OTHER_UNOP_LIST(OPCODE_CASE)
        JS_OBJECT_OP_LIST(OPCODE_CASE)
        JS_CONTEXT_OP_LIST(OPCODE_CASE)
        JS_OTHER_OP_LIST(OPCODE_CASE)
#undef OPCODE_CASE
      case IrOpcode::kJSToInteger:
      case IrOpcode::kJSToLength:
      case IrOpcode::kJSToName:
      case IrOpcode::kJSToObject:
      case IrOpcode::kJSToString:
        VisitInputs(node);
        // Assume the output is tagged.
        return SetOutput(node, MachineRepresentation::kTagged);

      default:
        V8_Fatal(
            __FILE__, __LINE__,
            "Representation inference: unsupported opcode %i (%s), node #%i\n.",
            node->opcode(), node->op()->mnemonic(), node->id());
        break;
    }
    UNREACHABLE();
  }

  void DeferReplacement(Node* node, Node* replacement) {
    TRACE("defer replacement #%d:%s with #%d:%s\n", node->id(),
          node->op()->mnemonic(), replacement->id(),
          replacement->op()->mnemonic());

    // Disconnect the node from effect and control chains, if necessary.
    if (node->op()->EffectInputCount() > 0) {
      DCHECK_LT(0, node->op()->ControlInputCount());
      // Disconnect the node from effect and control chains.
      Node* control = NodeProperties::GetControlInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      ReplaceEffectControlUses(node, effect, control);
    }

    replacements_.push_back(node);
    replacements_.push_back(replacement);

    node->NullAllInputs();  // Node is now dead.
  }

  void Kill(Node* node) {
    TRACE("killing #%d:%s\n", node->id(), node->op()->mnemonic());

    if (node->op()->EffectInputCount() == 1) {
      DCHECK_LT(0, node->op()->ControlInputCount());
      // Disconnect the node from effect and control chains.
      Node* control = NodeProperties::GetControlInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      ReplaceEffectControlUses(node, effect, control);
    } else {
      DCHECK_EQ(0, node->op()->EffectInputCount());
      DCHECK_EQ(0, node->op()->ControlOutputCount());
      DCHECK_EQ(0, node->op()->EffectOutputCount());
    }

    node->ReplaceUses(jsgraph_->Dead());

    node->NullAllInputs();  // The {node} is now dead.
  }

  void PrintOutputInfo(NodeInfo* info) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << info->representation();
    }
  }

  void PrintRepresentation(MachineRepresentation rep) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << rep;
    }
  }

  void PrintTruncation(Truncation truncation) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << truncation.description() << std::endl;
    }
  }

  void PrintUseInfo(UseInfo info) {
    if (FLAG_trace_representation) {
      OFStream os(stdout);
      os << info.representation() << ":" << info.truncation().description();
    }
  }

 private:
  JSGraph* jsgraph_;
  Zone* zone_;                      // Temporary zone.
  size_t const count_;              // number of nodes in the graph
  ZoneVector<NodeInfo> info_;       // node id -> usage information
#ifdef DEBUG
  ZoneVector<InputUseInfos> node_input_use_infos_;  // Debug information about
                                                    // requirements on inputs.
#endif                                              // DEBUG
  NodeVector nodes_;                // collected nodes
  NodeVector replacements_;         // replacements to be done after lowering
  Phase phase_;                     // current phase of algorithm
  RepresentationChanger* changer_;  // for inserting representation changes
  ZoneQueue<Node*> queue_;          // queue for traversing the graph

  struct NodeState {
    Node* node;
    int input_index;
  };
  ZoneStack<NodeState> typing_stack_;  // stack for graph typing.
  // TODO(danno): RepresentationSelector shouldn't know anything about the
  // source positions table, but must for now since there currently is no other
  // way to pass down source position information to nodes created during
  // lowering. Once this phase becomes a vanilla reducer, it should get source
  // position information via the SourcePositionWrapper like all other reducers.
  SourcePositionTable* source_positions_;
  TypeCache const& type_cache_;
  OperationTyper op_typer_;  // helper for the feedback typer

  NodeInfo* GetInfo(Node* node) {
    DCHECK(node->id() < count_);
    return &info_[node->id()];
  }
  Zone* zone() { return zone_; }
  Zone* graph_zone() { return jsgraph_->zone(); }
};

SimplifiedLowering::SimplifiedLowering(JSGraph* jsgraph, Zone* zone,
                                       SourcePositionTable* source_positions)
    : jsgraph_(jsgraph),
      zone_(zone),
      type_cache_(TypeCache::Get()),
      source_positions_(source_positions) {}

void SimplifiedLowering::LowerAllNodes() {
  RepresentationChanger changer(jsgraph(), jsgraph()->isolate());
  RepresentationSelector selector(jsgraph(), zone_, &changer,
                                  source_positions_);
  selector.Run(this);
}

void SimplifiedLowering::DoJSToNumberTruncatesToFloat64(
    Node* node, RepresentationSelector* selector) {
  DCHECK_EQ(IrOpcode::kJSToNumber, node->opcode());
  Node* value = node->InputAt(0);
  Node* context = node->InputAt(1);
  Node* frame_state = node->InputAt(2);
  Node* effect = node->InputAt(3);
  Node* control = node->InputAt(4);
  Node* throwing;

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0;
  {
    vtrue0 = graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), value);
    vtrue0 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    throwing = vfalse0 = efalse0 =
        graph()->NewNode(ToNumberOperator(), ToNumberCode(), value, context,
                         frame_state, efalse0, if_false0);
    if_false0 = graph()->NewNode(common()->IfSuccess(), throwing);

    Node* check1 = graph()->NewNode(simplified()->ObjectIsSmi(), vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1;
    {
      vtrue1 =
          graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), vfalse0);
      vtrue1 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), vtrue1);
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                         vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kFloat64, 2),
                           vtrue0, vfalse0, control);

  // Replace effect and control uses appropriately.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        edge.from()->ReplaceUses(control);
        edge.from()->Kill();
      } else if (edge.from()->opcode() == IrOpcode::kIfException) {
        edge.UpdateTo(throwing);
      } else {
        UNREACHABLE();
      }
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    }
  }

  selector->DeferReplacement(node, value);
}

void SimplifiedLowering::DoJSToNumberTruncatesToWord32(
    Node* node, RepresentationSelector* selector) {
  DCHECK_EQ(IrOpcode::kJSToNumber, node->opcode());
  Node* value = node->InputAt(0);
  Node* context = node->InputAt(1);
  Node* frame_state = node->InputAt(2);
  Node* effect = node->InputAt(3);
  Node* control = node->InputAt(4);
  Node* throwing;

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 =
      graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), value);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0 = effect;
  Node* vfalse0;
  {
    throwing = vfalse0 = efalse0 =
        graph()->NewNode(ToNumberOperator(), ToNumberCode(), value, context,
                         frame_state, efalse0, if_false0);
    if_false0 = graph()->NewNode(common()->IfSuccess(), throwing);

    Node* check1 = graph()->NewNode(simplified()->ObjectIsSmi(), vfalse0);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;
    Node* vtrue1 =
        graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(), vfalse0);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;
    Node* vfalse1;
    {
      vfalse1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForHeapNumberValue()), efalse0,
          efalse1, if_false1);
      vfalse1 = graph()->NewNode(machine()->TruncateFloat64ToWord32(), vfalse1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue1, vfalse1, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue0, vfalse0, control);

  // Replace effect and control uses appropriately.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      if (edge.from()->opcode() == IrOpcode::kIfSuccess) {
        edge.from()->ReplaceUses(control);
        edge.from()->Kill();
      } else if (edge.from()->opcode() == IrOpcode::kIfException) {
        edge.UpdateTo(throwing);
      } else {
        UNREACHABLE();
      }
    } else if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
    }
  }

  selector->DeferReplacement(node, value);
}

void SimplifiedLowering::DoLoadBuffer(Node* node,
                                      MachineRepresentation output_rep,
                                      RepresentationChanger* changer) {
  DCHECK_EQ(IrOpcode::kLoadBuffer, node->opcode());
  DCHECK_NE(MachineRepresentation::kNone, output_rep);
  MachineType const access_type = BufferAccessOf(node->op()).machine_type();
  if (output_rep != access_type.representation()) {
    Node* const buffer = node->InputAt(0);
    Node* const offset = node->InputAt(1);
    Node* const length = node->InputAt(2);
    Node* const effect = node->InputAt(3);
    Node* const control = node->InputAt(4);
    Node* const index =
        machine()->Is64()
            ? graph()->NewNode(machine()->ChangeUint32ToUint64(), offset)
            : offset;

    Node* check = graph()->NewNode(machine()->Uint32LessThan(), offset, length);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = graph()->NewNode(machine()->Load(access_type), buffer, index,
                                   effect, if_true);
    Type* element_type =
        Type::Intersect(NodeProperties::GetType(node), Type::Number(), zone());
    Node* vtrue = changer->GetRepresentationFor(
        etrue, access_type.representation(), element_type, node,
        UseInfo(output_rep, Truncation::None()));

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse;
    if (output_rep == MachineRepresentation::kTagged) {
      vfalse = jsgraph()->UndefinedConstant();
    } else if (output_rep == MachineRepresentation::kFloat64) {
      vfalse =
          jsgraph()->Float64Constant(std::numeric_limits<double>::quiet_NaN());
    } else if (output_rep == MachineRepresentation::kFloat32) {
      vfalse =
          jsgraph()->Float32Constant(std::numeric_limits<float>::quiet_NaN());
    } else {
      vfalse = jsgraph()->Int32Constant(0);
    }

    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    Node* ephi = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);

    // Replace effect uses of {node} with the {ephi}.
    NodeProperties::ReplaceUses(node, node, ephi);

    // Turn the {node} into a Phi.
    node->ReplaceInput(0, vtrue);
    node->ReplaceInput(1, vfalse);
    node->ReplaceInput(2, merge);
    node->TrimInputCount(3);
    NodeProperties::ChangeOp(node, common()->Phi(output_rep, 2));
  } else {
    NodeProperties::ChangeOp(node, machine()->CheckedLoad(access_type));
  }
}


void SimplifiedLowering::DoStoreBuffer(Node* node) {
  DCHECK_EQ(IrOpcode::kStoreBuffer, node->opcode());
  MachineRepresentation const rep =
      BufferAccessOf(node->op()).machine_type().representation();
  NodeProperties::ChangeOp(node, machine()->CheckedStore(rep));
}

Node* SimplifiedLowering::Float64Round(Node* const node) {
  Node* const one = jsgraph()->Float64Constant(1.0);
  Node* const one_half = jsgraph()->Float64Constant(0.5);
  Node* const input = node->InputAt(0);

  // Round up towards Infinity, and adjust if the difference exceeds 0.5.
  Node* result = graph()->NewNode(machine()->Float64RoundUp().placeholder(),
                                  node->InputAt(0));
  return graph()->NewNode(
      common()->Select(MachineRepresentation::kFloat64),
      graph()->NewNode(
          machine()->Float64LessThanOrEqual(),
          graph()->NewNode(machine()->Float64Sub(), result, one_half), input),
      result, graph()->NewNode(machine()->Float64Sub(), result, one));
}

Node* SimplifiedLowering::Float64Sign(Node* const node) {
  Node* const minus_one = jsgraph()->Float64Constant(-1.0);
  Node* const zero = jsgraph()->Float64Constant(0.0);
  Node* const one = jsgraph()->Float64Constant(1.0);

  Node* const input = node->InputAt(0);

  return graph()->NewNode(
      common()->Select(MachineRepresentation::kFloat64),
      graph()->NewNode(machine()->Float64LessThan(), input, zero), minus_one,
      graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), zero, input), one,
          input));
}

Node* SimplifiedLowering::Int32Abs(Node* const node) {
  Node* const input = node->InputAt(0);

  // Generate case for absolute integer value.
  //
  //    let sign = input >> 31 in
  //    (input ^ sign) - sign

  Node* sign = graph()->NewNode(machine()->Word32Sar(), input,
                                jsgraph()->Int32Constant(31));
  return graph()->NewNode(machine()->Int32Sub(),
                          graph()->NewNode(machine()->Word32Xor(), input, sign),
                          sign);
}

Node* SimplifiedLowering::Int32Div(Node* const node) {
  Int32BinopMatcher m(node);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(-1)) {
    return graph()->NewNode(machine()->Int32Sub(), zero, lhs);
  } else if (m.right().Is(0)) {
    return rhs;
  } else if (machine()->Int32DivIsSafe() || m.right().HasValue()) {
    return graph()->NewNode(machine()->Int32Div(), lhs, rhs, graph()->start());
  }

  // General case for signed integer division.
  //
  //    if 0 < rhs then
  //      lhs / rhs
  //    else
  //      if rhs < -1 then
  //        lhs / rhs
  //      else if rhs == 0 then
  //        0
  //      else
  //        0 - lhs
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true0);

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(machine()->Int32LessThan(), rhs, minus_one);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Div(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
      Node* branch2 = graph()->NewNode(common()->Branch(), check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* true2 = zero;

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(machine()->Int32Sub(), zero, lhs);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}


Node* SimplifiedLowering::Int32Mod(Node* const node) {
  Int32BinopMatcher m(node);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(-1) || m.right().Is(0)) {
    return zero;
  } else if (m.right().HasValue()) {
    return graph()->NewNode(machine()->Int32Mod(), lhs, rhs, graph()->start());
  }

  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if 0 < rhs then
  //     msk = rhs - 1
  //     if rhs & msk != 0 then
  //       lhs % rhs
  //     else
  //       if lhs < 0 then
  //         -(-lhs & msk)
  //       else
  //         lhs & msk
  //   else
  //     if rhs < -1 then
  //       lhs % rhs
  //     else
  //       zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(machine()->Int32LessThan(), zero, rhs);
  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), check0,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(machine()->Int32Add(), rhs, minus_one);

    Node* check1 = graph()->NewNode(machine()->Word32And(), rhs, msk);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(machine()->Int32LessThan(), lhs, zero);
      Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check2, if_false1);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* true2 = graph()->NewNode(
          machine()->Int32Sub(), zero,
          graph()->NewNode(machine()->Word32And(),
                           graph()->NewNode(machine()->Int32Sub(), zero, lhs),
                           msk));

      Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(machine()->Word32And(), lhs, msk);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(machine()->Int32LessThan(), rhs, minus_one);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Int32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1 = zero;

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

Node* SimplifiedLowering::Int32Sign(Node* const node) {
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const zero = jsgraph()->Int32Constant(0);
  Node* const one = jsgraph()->Int32Constant(1);

  Node* const input = node->InputAt(0);

  return graph()->NewNode(
      common()->Select(MachineRepresentation::kWord32),
      graph()->NewNode(machine()->Int32LessThan(), input, zero), minus_one,
      graph()->NewNode(
          common()->Select(MachineRepresentation::kWord32),
          graph()->NewNode(machine()->Int32LessThan(), zero, input), one,
          zero));
}

Node* SimplifiedLowering::Uint32Div(Node* const node) {
  Uint32BinopMatcher m(node);
  Node* const zero = jsgraph()->Uint32Constant(0);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(0)) {
    return zero;
  } else if (machine()->Uint32DivIsSafe() || m.right().HasValue()) {
    return graph()->NewNode(machine()->Uint32Div(), lhs, rhs, graph()->start());
  }

  Node* check = graph()->NewNode(machine()->Word32Equal(), rhs, zero);
  Diamond d(graph(), common(), check, BranchHint::kFalse);
  Node* div = graph()->NewNode(machine()->Uint32Div(), lhs, rhs, d.if_false);
  return d.Phi(MachineRepresentation::kWord32, zero, div);
}


Node* SimplifiedLowering::Uint32Mod(Node* const node) {
  Uint32BinopMatcher m(node);
  Node* const minus_one = jsgraph()->Int32Constant(-1);
  Node* const zero = jsgraph()->Uint32Constant(0);
  Node* const lhs = m.left().node();
  Node* const rhs = m.right().node();

  if (m.right().Is(0)) {
    return zero;
  } else if (m.right().HasValue()) {
    return graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, graph()->start());
  }

  // General case for unsigned integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if rhs then
  //     msk = rhs - 1
  //     if rhs & msk != 0 then
  //       lhs % rhs
  //     else
  //       lhs & msk
  //   else
  //     zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kWord32, 2);

  Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kTrue), rhs,
                                   graph()->start());

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(machine()->Int32Add(), rhs, minus_one);

    Node* check1 = graph()->NewNode(machine()->Word32And(), rhs, msk);
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(machine()->Uint32Mod(), lhs, rhs, if_true1);

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* false1 = graph()->NewNode(machine()->Word32And(), lhs, msk);

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* false0 = zero;

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

void SimplifiedLowering::DoMax(Node* node, Operator const* op,
                               MachineRepresentation rep) {
  Node* const lhs = node->InputAt(0);
  Node* const rhs = node->InputAt(1);

  node->ReplaceInput(0, graph()->NewNode(op, lhs, rhs));
  DCHECK_EQ(rhs, node->InputAt(1));
  node->AppendInput(graph()->zone(), lhs);
  NodeProperties::ChangeOp(node, common()->Select(rep));
}

void SimplifiedLowering::DoMin(Node* node, Operator const* op,
                               MachineRepresentation rep) {
  Node* const lhs = node->InputAt(0);
  Node* const rhs = node->InputAt(1);

  node->InsertInput(graph()->zone(), 0, graph()->NewNode(op, lhs, rhs));
  DCHECK_EQ(lhs, node->InputAt(1));
  DCHECK_EQ(rhs, node->InputAt(2));
  NodeProperties::ChangeOp(node, common()->Select(rep));
}

void SimplifiedLowering::DoShift(Node* node, Operator const* op,
                                 Type* rhs_type) {
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  if (!rhs_type->Is(type_cache_.kZeroToThirtyOne)) {
    node->ReplaceInput(1, graph()->NewNode(machine()->Word32And(), rhs,
                                           jsgraph()->Int32Constant(0x1f)));
  }
  DCHECK(op->HasProperty(Operator::kPure));
  ChangeToPureOp(node, op);
}

void SimplifiedLowering::DoStringToNumber(Node* node) {
  Operator::Properties properties = Operator::kEliminatable;
  Callable callable = CodeFactory::StringToNumber(isolate());
  CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), 0, flags, properties);
  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(callable.code()));
  node->AppendInput(graph()->zone(), jsgraph()->NoContextConstant());
  node->AppendInput(graph()->zone(), graph()->start());
  NodeProperties::ChangeOp(node, common()->Call(desc));
}

void SimplifiedLowering::DoIntegral32ToBit(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const zero = jsgraph()->Int32Constant(0);
  Operator const* const op = machine()->Word32Equal();

  node->ReplaceInput(0, graph()->NewNode(op, input, zero));
  node->AppendInput(graph()->zone(), zero);
  NodeProperties::ChangeOp(node, op);
}

void SimplifiedLowering::DoOrderedNumberToBit(Node* node) {
  Node* const input = node->InputAt(0);

  node->ReplaceInput(0, graph()->NewNode(machine()->Float64Equal(), input,
                                         jsgraph()->Float64Constant(0.0)));
  node->AppendInput(graph()->zone(), jsgraph()->Int32Constant(0));
  NodeProperties::ChangeOp(node, machine()->Word32Equal());
}

void SimplifiedLowering::DoNumberToBit(Node* node) {
  Node* const input = node->InputAt(0);

  node->ReplaceInput(0, jsgraph()->Float64Constant(0.0));
  node->AppendInput(graph()->zone(),
                    graph()->NewNode(machine()->Float64Abs(), input));
  NodeProperties::ChangeOp(node, machine()->Float64LessThan());
}

void SimplifiedLowering::DoIntegerToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const min = jsgraph()->Float64Constant(0.0);
  Node* const max = jsgraph()->Float64Constant(255.0);

  node->ReplaceInput(
      0, graph()->NewNode(machine()->Float64LessThan(), min, input));
  node->AppendInput(
      graph()->zone(),
      graph()->NewNode(
          common()->Select(MachineRepresentation::kFloat64),
          graph()->NewNode(machine()->Float64LessThan(), input, max), input,
          max));
  node->AppendInput(graph()->zone(), min);
  NodeProperties::ChangeOp(node,
                           common()->Select(MachineRepresentation::kFloat64));
}

void SimplifiedLowering::DoNumberToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const min = jsgraph()->Float64Constant(0.0);
  Node* const max = jsgraph()->Float64Constant(255.0);

  node->ReplaceInput(
      0, graph()->NewNode(
             common()->Select(MachineRepresentation::kFloat64),
             graph()->NewNode(machine()->Float64LessThan(), min, input),
             graph()->NewNode(
                 common()->Select(MachineRepresentation::kFloat64),
                 graph()->NewNode(machine()->Float64LessThan(), input, max),
                 input, max),
             min));
  NodeProperties::ChangeOp(node,
                           machine()->Float64RoundTiesEven().placeholder());
}

void SimplifiedLowering::DoSigned32ToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const min = jsgraph()->Int32Constant(0);
  Node* const max = jsgraph()->Int32Constant(255);

  node->ReplaceInput(
      0, graph()->NewNode(machine()->Int32LessThanOrEqual(), input, max));
  node->AppendInput(
      graph()->zone(),
      graph()->NewNode(common()->Select(MachineRepresentation::kWord32),
                       graph()->NewNode(machine()->Int32LessThan(), input, min),
                       min, input));
  node->AppendInput(graph()->zone(), max);
  NodeProperties::ChangeOp(node,
                           common()->Select(MachineRepresentation::kWord32));
}

void SimplifiedLowering::DoUnsigned32ToUint8Clamped(Node* node) {
  Node* const input = node->InputAt(0);
  Node* const max = jsgraph()->Uint32Constant(255u);

  node->ReplaceInput(
      0, graph()->NewNode(machine()->Uint32LessThanOrEqual(), input, max));
  node->AppendInput(graph()->zone(), input);
  node->AppendInput(graph()->zone(), max);
  NodeProperties::ChangeOp(node,
                           common()->Select(MachineRepresentation::kWord32));
}

Node* SimplifiedLowering::ToNumberCode() {
  if (!to_number_code_.is_set()) {
    Callable callable = CodeFactory::ToNumber(isolate());
    to_number_code_.set(jsgraph()->HeapConstant(callable.code()));
  }
  return to_number_code_.get();
}

Operator const* SimplifiedLowering::ToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable = CodeFactory::ToNumber(isolate());
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0, flags,
        Operator::kNoProperties);
    to_number_operator_.set(common()->Call(desc));
  }
  return to_number_operator_.get();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
