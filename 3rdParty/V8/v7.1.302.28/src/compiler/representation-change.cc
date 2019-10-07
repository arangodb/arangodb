// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/representation-change.h"

#include <sstream>

#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/type-cache.h"
#include "src/heap/factory-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

const char* Truncation::description() const {
  switch (kind()) {
    case TruncationKind::kNone:
      return "no-value-use";
    case TruncationKind::kBool:
      return "truncate-to-bool";
    case TruncationKind::kWord32:
      return "truncate-to-word32";
    case TruncationKind::kFloat64:
      switch (identify_zeros()) {
        case kIdentifyZeros:
          return "truncate-to-float64 (identify zeros)";
        case kDistinguishZeros:
          return "truncate-to-float64 (distinguish zeros)";
      }
    case TruncationKind::kAny:
      switch (identify_zeros()) {
        case kIdentifyZeros:
          return "no-truncation (but identify zeros)";
        case kDistinguishZeros:
          return "no-truncation (but distinguish zeros)";
      }
  }
  UNREACHABLE();
}

// Partial order for truncations:
//
//          kAny <-------+
//            ^          |
//            |          |
//          kFloat64     |
//          ^            |
//          /            |
//   kWord32           kBool
//         ^            ^
//         \            /
//          \          /
//           \        /
//            \      /
//             \    /
//             kNone
//
// TODO(jarin) We might consider making kBool < kFloat64.

// static
Truncation::TruncationKind Truncation::Generalize(TruncationKind rep1,
                                                  TruncationKind rep2) {
  if (LessGeneral(rep1, rep2)) return rep2;
  if (LessGeneral(rep2, rep1)) return rep1;
  // Handle the generalization of float64-representable values.
  if (LessGeneral(rep1, TruncationKind::kFloat64) &&
      LessGeneral(rep2, TruncationKind::kFloat64)) {
    return TruncationKind::kFloat64;
  }
  // Handle the generalization of any-representable values.
  if (LessGeneral(rep1, TruncationKind::kAny) &&
      LessGeneral(rep2, TruncationKind::kAny)) {
    return TruncationKind::kAny;
  }
  // All other combinations are illegal.
  FATAL("Tried to combine incompatible truncations");
  return TruncationKind::kNone;
}

// static
IdentifyZeros Truncation::GeneralizeIdentifyZeros(IdentifyZeros i1,
                                                  IdentifyZeros i2) {
  if (i1 == i2) {
    return i1;
  } else {
    return kDistinguishZeros;
  }
}

// static
bool Truncation::LessGeneral(TruncationKind rep1, TruncationKind rep2) {
  switch (rep1) {
    case TruncationKind::kNone:
      return true;
    case TruncationKind::kBool:
      return rep2 == TruncationKind::kBool || rep2 == TruncationKind::kAny;
    case TruncationKind::kWord32:
      return rep2 == TruncationKind::kWord32 ||
             rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kFloat64:
      return rep2 == TruncationKind::kFloat64 || rep2 == TruncationKind::kAny;
    case TruncationKind::kAny:
      return rep2 == TruncationKind::kAny;
  }
  UNREACHABLE();
}

// static
bool Truncation::LessGeneralIdentifyZeros(IdentifyZeros i1, IdentifyZeros i2) {
  return i1 == i2 || i1 == kIdentifyZeros;
}

namespace {

bool IsWord(MachineRepresentation rep) {
  return rep == MachineRepresentation::kWord8 ||
         rep == MachineRepresentation::kWord16 ||
         rep == MachineRepresentation::kWord32;
}

}  // namespace

RepresentationChanger::RepresentationChanger(JSGraph* jsgraph, Isolate* isolate)
    : cache_(TypeCache::Get()),
      jsgraph_(jsgraph),
      isolate_(isolate),
      testing_type_errors_(false),
      type_error_(false) {}

// Changes representation from {output_rep} to {use_rep}. The {truncation}
// parameter is only used for sanity checking - if the changer cannot figure
// out signedness for the word32->float64 conversion, then we check that the
// uses truncate to word32 (so they do not care about signedness).
Node* RepresentationChanger::GetRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Node* use_node, UseInfo use_info) {
  if (output_rep == MachineRepresentation::kNone && !output_type.IsNone()) {
    // The output representation should be set if the type is inhabited (i.e.,
    // if the value is possible).
    return TypeError(node, output_rep, output_type, use_info.representation());
  }

  // Handle the no-op shortcuts when no checking is necessary.
  if (use_info.type_check() == TypeCheckKind::kNone ||
      output_rep != MachineRepresentation::kWord32) {
    if (use_info.representation() == output_rep) {
      // Representations are the same. That's a no-op.
      return node;
    }
    if (IsWord(use_info.representation()) && IsWord(output_rep)) {
      // Both are words less than or equal to 32-bits.
      // Since loads of integers from memory implicitly sign or zero extend the
      // value to the full machine word size and stores implicitly truncate,
      // no representation change is necessary.
      return node;
    }
  }

  switch (use_info.representation()) {
    case MachineRepresentation::kTaggedSigned:
      DCHECK(use_info.type_check() == TypeCheckKind::kNone ||
             use_info.type_check() == TypeCheckKind::kSignedSmall);
      return GetTaggedSignedRepresentationFor(node, output_rep, output_type,
                                              use_node, use_info);
    case MachineRepresentation::kTaggedPointer:
      DCHECK(use_info.type_check() == TypeCheckKind::kNone ||
             use_info.type_check() == TypeCheckKind::kHeapObject);
      return GetTaggedPointerRepresentationFor(node, output_rep, output_type,
                                               use_node, use_info);
    case MachineRepresentation::kTagged:
      DCHECK_EQ(TypeCheckKind::kNone, use_info.type_check());
      return GetTaggedRepresentationFor(node, output_rep, output_type,
                                        use_info.truncation());
    case MachineRepresentation::kFloat32:
      DCHECK_EQ(TypeCheckKind::kNone, use_info.type_check());
      return GetFloat32RepresentationFor(node, output_rep, output_type,
                                         use_info.truncation());
    case MachineRepresentation::kFloat64:
      return GetFloat64RepresentationFor(node, output_rep, output_type,
                                         use_node, use_info);
    case MachineRepresentation::kBit:
      DCHECK_EQ(TypeCheckKind::kNone, use_info.type_check());
      return GetBitRepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return GetWord32RepresentationFor(node, output_rep, output_type, use_node,
                                        use_info);
    case MachineRepresentation::kWord64:
      DCHECK_EQ(TypeCheckKind::kNone, use_info.type_check());
      return GetWord64RepresentationFor(node, output_rep, output_type);
    case MachineRepresentation::kSimd128:
    case MachineRepresentation::kNone:
      return node;
  }
  UNREACHABLE();
}

Node* RepresentationChanger::GetTaggedSignedRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Node* use_node, UseInfo use_info) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kNumberConstant:
      if (output_type.Is(Type::SignedSmall())) {
        return node;
      }
      break;
    default:
      break;
  }
  // Select the correct X -> Tagged operator.
  const Operator* op;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kTaggedSigned),
        node);
  } else if (IsWord(output_rep)) {
    if (output_type.Is(Type::Signed31())) {
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type.Is(Type::Signed32())) {
      if (SmiValuesAre32Bits()) {
        op = simplified()->ChangeInt32ToTagged();
      } else if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
        op = simplified()->CheckedInt32ToTaggedSigned(use_info.feedback());
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kTaggedSigned);
      }
    } else if (output_type.Is(Type::Unsigned32()) &&
               use_info.type_check() == TypeCheckKind::kSignedSmall) {
      op = simplified()->CheckedUint32ToTaggedSigned(use_info.feedback());
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedSigned);
    }
  } else if (output_rep == MachineRepresentation::kWord64) {
    if (output_type.Is(Type::Signed31())) {
      // int64 -> int32 -> tagged signed
      node = InsertTruncateInt64ToInt32(node);
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type.Is(Type::Signed32()) && SmiValuesAre32Bits()) {
      // int64 -> int32 -> tagged signed
      node = InsertTruncateInt64ToInt32(node);
      op = simplified()->ChangeInt32ToTagged();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      if (output_type.Is(cache_.kPositiveSafeInteger)) {
        op = simplified()->CheckedUint64ToTaggedSigned(use_info.feedback());
      } else if (output_type.Is(cache_.kSafeInteger)) {
        op = simplified()->CheckedInt64ToTaggedSigned(use_info.feedback());
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kTaggedSigned);
      }
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedSigned);
    }
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type.Is(Type::Signed31())) {
      // float64 -> int32 -> tagged signed
      node = InsertChangeFloat64ToInt32(node);
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type.Is(Type::Signed32())) {
      // float64 -> int32 -> tagged signed
      node = InsertChangeFloat64ToInt32(node);
      if (SmiValuesAre32Bits()) {
        op = simplified()->ChangeInt32ToTagged();
      } else if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
        op = simplified()->CheckedInt32ToTaggedSigned(use_info.feedback());
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kTaggedSigned);
      }
    } else if (output_type.Is(Type::Unsigned32()) &&
               use_info.type_check() == TypeCheckKind::kSignedSmall) {
      // float64 -> uint32 -> tagged signed
      node = InsertChangeFloat64ToUint32(node);
      op = simplified()->CheckedUint32ToTaggedSigned(use_info.feedback());
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      op = simplified()->CheckedFloat64ToInt32(
          output_type.Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero,
          use_info.feedback());
      node = InsertConversion(node, op, use_node);
      if (SmiValuesAre32Bits()) {
        op = simplified()->ChangeInt32ToTagged();
      } else {
        op = simplified()->CheckedInt32ToTaggedSigned(use_info.feedback());
      }
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedSigned);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      op = machine()->ChangeFloat32ToFloat64();
      node = InsertConversion(node, op, use_node);
      op = simplified()->CheckedFloat64ToInt32(
          output_type.Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero,
          use_info.feedback());
      node = InsertConversion(node, op, use_node);
      if (SmiValuesAre32Bits()) {
        op = simplified()->ChangeInt32ToTagged();
      } else {
        op = simplified()->CheckedInt32ToTaggedSigned(use_info.feedback());
      }
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedSigned);
    }
  } else if (CanBeTaggedPointer(output_rep)) {
    if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      op = simplified()->CheckedTaggedToTaggedSigned(use_info.feedback());
    } else if (output_type.Is(Type::SignedSmall())) {
      op = simplified()->ChangeTaggedToTaggedSigned();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedSigned);
    }
  } else if (output_rep == MachineRepresentation::kBit) {
    if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      // TODO(turbofan): Consider adding a Bailout operator that just deopts.
      // Also use that for MachineRepresentation::kPointer case above.
      node = InsertChangeBitToTagged(node);
      op = simplified()->CheckedTaggedToTaggedSigned(use_info.feedback());
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedSigned);
    }
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kTaggedSigned);
  }
  return InsertConversion(node, op, use_node);
}

Node* RepresentationChanger::GetTaggedPointerRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Node* use_node, UseInfo use_info) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kHeapConstant:
    case IrOpcode::kDelayedStringConstant:
      return node;  // No change necessary.
    case IrOpcode::kInt32Constant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kFloat32Constant:
      UNREACHABLE();
    default:
      break;
  }
  // Select the correct X -> TaggedPointer operator.
  Operator const* op;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kTaggedPointer),
        node);
  } else if (output_rep == MachineRepresentation::kBit) {
    if (output_type.Is(Type::Boolean())) {
      op = simplified()->ChangeBitToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (IsWord(output_rep)) {
    if (output_type.Is(Type::Unsigned32())) {
      // uint32 -> float64 -> tagged
      node = InsertChangeUint32ToFloat64(node);
    } else if (output_type.Is(Type::Signed32())) {
      // int32 -> float64 -> tagged
      node = InsertChangeInt32ToFloat64(node);
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedPointer);
    }
    op = simplified()->ChangeFloat64ToTaggedPointer();
  } else if (output_rep == MachineRepresentation::kWord64) {
    if (output_type.Is(cache_.kSafeInteger)) {
      // int64 -> float64 -> tagged pointer
      op = machine()->ChangeInt64ToFloat64();
      node = jsgraph()->graph()->NewNode(op, node);
      op = simplified()->ChangeFloat64ToTaggedPointer();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedPointer);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    if (output_type.Is(Type::Number())) {
      // float32 -> float64 -> tagged
      node = InsertChangeFloat32ToFloat64(node);
      op = simplified()->ChangeFloat64ToTaggedPointer();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedPointer);
    }
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type.Is(Type::Number())) {
      // float64 -> tagged
      op = simplified()->ChangeFloat64ToTaggedPointer();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTaggedPointer);
    }
  } else if (CanBeTaggedSigned(output_rep) &&
             use_info.type_check() == TypeCheckKind::kHeapObject) {
    if (!output_type.Maybe(Type::SignedSmall())) {
      return node;
    }
    // TODO(turbofan): Consider adding a Bailout operator that just deopts
    // for TaggedSigned output representation.
    op = simplified()->CheckedTaggedToTaggedPointer(use_info.feedback());
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kTaggedPointer);
  }
  return InsertConversion(node, op, use_node);
}

Node* RepresentationChanger::GetTaggedRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Truncation truncation) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kNumberConstant:
    case IrOpcode::kHeapConstant:
    case IrOpcode::kDelayedStringConstant:
      return node;  // No change necessary.
    case IrOpcode::kInt32Constant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kFloat32Constant:
      UNREACHABLE();
      break;
    default:
      break;
  }
  if (output_rep == MachineRepresentation::kTaggedSigned ||
      output_rep == MachineRepresentation::kTaggedPointer) {
    // this is a no-op.
    return node;
  }
  // Select the correct X -> Tagged operator.
  const Operator* op;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kTagged), node);
  } else if (output_rep == MachineRepresentation::kBit) {
    if (output_type.Is(Type::Boolean())) {
      op = simplified()->ChangeBitToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (IsWord(output_rep)) {
    if (output_type.Is(Type::Signed31())) {
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type.Is(Type::Signed32())) {
      op = simplified()->ChangeInt32ToTagged();
    } else if (output_type.Is(Type::Unsigned32()) ||
               truncation.IsUsedAsWord32()) {
      // Either the output is uint32 or the uses only care about the
      // low 32 bits (so we can pick uint32 safely).
      op = simplified()->ChangeUint32ToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (output_rep == MachineRepresentation::kWord64) {
    if (output_type.Is(Type::Signed31())) {
      // int64 -> int32 -> tagged signed
      node = InsertTruncateInt64ToInt32(node);
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type.Is(Type::Signed32())) {
      // int64 -> int32 -> tagged
      node = InsertTruncateInt64ToInt32(node);
      op = simplified()->ChangeInt32ToTagged();
    } else if (output_type.Is(Type::Unsigned32())) {
      // int64 -> uint32 -> tagged
      node = InsertTruncateInt64ToInt32(node);
      op = simplified()->ChangeUint32ToTagged();
    } else if (output_type.Is(cache_.kPositiveSafeInteger)) {
      // uint64 -> tagged
      op = simplified()->ChangeUint64ToTagged();
    } else if (output_type.Is(cache_.kSafeInteger)) {
      // int64 -> tagged
      op = simplified()->ChangeInt64ToTagged();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else if (output_rep ==
             MachineRepresentation::kFloat32) {  // float32 -> float64 -> tagged
    node = InsertChangeFloat32ToFloat64(node);
    op = simplified()->ChangeFloat64ToTagged(
        output_type.Maybe(Type::MinusZero())
            ? CheckForMinusZeroMode::kCheckForMinusZero
            : CheckForMinusZeroMode::kDontCheckForMinusZero);
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type.Is(Type::Signed31())) {  // float64 -> int32 -> tagged
      node = InsertChangeFloat64ToInt32(node);
      op = simplified()->ChangeInt31ToTaggedSigned();
    } else if (output_type.Is(
                   Type::Signed32())) {  // float64 -> int32 -> tagged
      node = InsertChangeFloat64ToInt32(node);
      op = simplified()->ChangeInt32ToTagged();
    } else if (output_type.Is(
                   Type::Unsigned32())) {  // float64 -> uint32 -> tagged
      node = InsertChangeFloat64ToUint32(node);
      op = simplified()->ChangeUint32ToTagged();
    } else if (output_type.Is(Type::Number())) {
      op = simplified()->ChangeFloat64ToTagged(
          output_type.Maybe(Type::MinusZero())
              ? CheckForMinusZeroMode::kCheckForMinusZero
              : CheckForMinusZeroMode::kDontCheckForMinusZero);
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kTagged);
    }
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kTagged);
  }
  return jsgraph()->graph()->NewNode(op, node);
}

Node* RepresentationChanger::GetFloat32RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Truncation truncation) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kNumberConstant:
      return jsgraph()->Float32Constant(
          DoubleToFloat32(OpParameter<double>(node->op())));
    case IrOpcode::kInt32Constant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kFloat32Constant:
      UNREACHABLE();
      break;
    default:
      break;
  }
  // Select the correct X -> Float32 operator.
  const Operator* op = nullptr;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kFloat32), node);
  } else if (IsWord(output_rep)) {
    if (output_type.Is(Type::Signed32())) {
      // int32 -> float64 -> float32
      op = machine()->ChangeInt32ToFloat64();
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    } else if (output_type.Is(Type::Unsigned32()) ||
               truncation.IsUsedAsWord32()) {
      // Either the output is uint32 or the uses only care about the
      // low 32 bits (so we can pick uint32 safely).

      // uint32 -> float64 -> float32
      op = machine()->ChangeUint32ToFloat64();
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    }
  } else if (IsAnyTagged(output_rep)) {
    if (output_type.Is(Type::NumberOrOddball())) {
      // tagged -> float64 -> float32
      if (output_type.Is(Type::Number())) {
        op = simplified()->ChangeTaggedToFloat64();
      } else {
        op = simplified()->TruncateTaggedToFloat64();
      }
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    }
  } else if (output_rep == MachineRepresentation::kFloat64) {
    op = machine()->TruncateFloat64ToFloat32();
  } else if (output_rep == MachineRepresentation::kWord64) {
    if (output_type.Is(cache_.kSafeInteger)) {
      // int64 -> float64 -> float32
      op = machine()->ChangeInt64ToFloat64();
      node = jsgraph()->graph()->NewNode(op, node);
      op = machine()->TruncateFloat64ToFloat32();
    }
  }
  if (op == nullptr) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat32);
  }
  return jsgraph()->graph()->NewNode(op, node);
}

Node* RepresentationChanger::GetFloat64RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Node* use_node, UseInfo use_info) {
  // Eagerly fold representation changes for constants.
  if ((use_info.type_check() == TypeCheckKind::kNone)) {
    // TODO(jarin) Handle checked constant conversions.
    switch (node->opcode()) {
      case IrOpcode::kNumberConstant:
        return jsgraph()->Float64Constant(OpParameter<double>(node->op()));
      case IrOpcode::kInt32Constant:
      case IrOpcode::kFloat64Constant:
      case IrOpcode::kFloat32Constant:
        UNREACHABLE();
        break;
      default:
        break;
    }
  }
  // Select the correct X -> Float64 operator.
  const Operator* op = nullptr;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kFloat64), node);
  } else if (IsWord(output_rep)) {
    if (output_type.Is(Type::Signed32()) ||
        (output_type.Is(Type::Signed32OrMinusZero()) &&
         use_info.truncation().IdentifiesZeroAndMinusZero())) {
      op = machine()->ChangeInt32ToFloat64();
    } else if (output_type.Is(Type::Unsigned32()) ||
               use_info.truncation().IsUsedAsWord32()) {
      // Either the output is uint32 or the uses only care about the
      // low 32 bits (so we can pick uint32 safely).
      op = machine()->ChangeUint32ToFloat64();
    }
  } else if (output_rep == MachineRepresentation::kBit) {
    op = machine()->ChangeUint32ToFloat64();
  } else if (output_rep == MachineRepresentation::kTagged ||
             output_rep == MachineRepresentation::kTaggedSigned ||
             output_rep == MachineRepresentation::kTaggedPointer) {
    if (output_type.Is(Type::Undefined())) {
      return jsgraph()->Float64Constant(
          std::numeric_limits<double>::quiet_NaN());

    } else if (output_rep == MachineRepresentation::kTaggedSigned) {
      node = InsertChangeTaggedSignedToInt32(node);
      op = machine()->ChangeInt32ToFloat64();
    } else if (output_type.Is(Type::Number())) {
      op = simplified()->ChangeTaggedToFloat64();
    } else if (output_type.Is(Type::NumberOrOddball())) {
      // TODO(jarin) Here we should check that truncation is Number.
      op = simplified()->TruncateTaggedToFloat64();
    } else if (use_info.type_check() == TypeCheckKind::kNumber ||
               (use_info.type_check() == TypeCheckKind::kNumberOrOddball &&
                !output_type.Maybe(Type::BooleanOrNullOrNumber()))) {
      op = simplified()->CheckedTaggedToFloat64(CheckTaggedInputMode::kNumber,
                                                use_info.feedback());
    } else if (use_info.type_check() == TypeCheckKind::kNumberOrOddball) {
      op = simplified()->CheckedTaggedToFloat64(
          CheckTaggedInputMode::kNumberOrOddball, use_info.feedback());
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    op = machine()->ChangeFloat32ToFloat64();
  } else if (output_rep == MachineRepresentation::kWord64) {
    if (output_type.Is(cache_.kSafeInteger)) {
      op = machine()->ChangeInt64ToFloat64();
    }
  }
  if (op == nullptr) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kFloat64);
  }
  return InsertConversion(node, op, use_node);
}

Node* RepresentationChanger::MakeTruncatedInt32Constant(double value) {
  return jsgraph()->Int32Constant(DoubleToInt32(value));
}

Node* RepresentationChanger::InsertUnconditionalDeopt(Node* node,
                                                      DeoptimizeReason reason) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  effect =
      jsgraph()->graph()->NewNode(simplified()->CheckIf(reason),
                                  jsgraph()->Int32Constant(0), effect, control);
  Node* unreachable = effect = jsgraph()->graph()->NewNode(
      jsgraph()->common()->Unreachable(), effect, control);
  NodeProperties::ReplaceEffectInput(node, effect);
  return unreachable;
}

Node* RepresentationChanger::GetWord32RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type,
    Node* use_node, UseInfo use_info) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat32Constant:
    case IrOpcode::kFloat64Constant:
      UNREACHABLE();
      break;
    case IrOpcode::kNumberConstant: {
      double const fv = OpParameter<double>(node->op());
      if (use_info.type_check() == TypeCheckKind::kNone ||
          ((use_info.type_check() == TypeCheckKind::kSignedSmall ||
            use_info.type_check() == TypeCheckKind::kSigned32) &&
           IsInt32Double(fv))) {
        return MakeTruncatedInt32Constant(fv);
      }
      break;
    }
    default:
      break;
  }

  // Select the correct X -> Word32 operator.
  const Operator* op = nullptr;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kWord32), node);
  } else if (output_rep == MachineRepresentation::kBit) {
    CHECK(output_type.Is(Type::Boolean()));
    if (use_info.truncation().IsUsedAsWord32()) {
      return node;
    } else {
      CHECK(Truncation::Any(kIdentifyZeros)
                .IsLessGeneralThan(use_info.truncation()));
      CHECK_NE(use_info.type_check(), TypeCheckKind::kNone);
      Node* unreachable =
          InsertUnconditionalDeopt(use_node, DeoptimizeReason::kNotASmi);
      return jsgraph()->graph()->NewNode(
          jsgraph()->common()->DeadValue(MachineRepresentation::kWord32),
          unreachable);
    }
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type.Is(Type::Signed32())) {
      op = machine()->ChangeFloat64ToInt32();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
               use_info.type_check() == TypeCheckKind::kSigned32) {
      op = simplified()->CheckedFloat64ToInt32(
          output_type.Maybe(Type::MinusZero())
              ? use_info.minus_zero_check()
              : CheckForMinusZeroMode::kDontCheckForMinusZero,
          use_info.feedback());
    } else if (output_type.Is(Type::Unsigned32())) {
      op = machine()->ChangeFloat64ToUint32();
    } else if (use_info.truncation().IsUsedAsWord32()) {
      op = machine()->TruncateFloat64ToWord32();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord32);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    node = InsertChangeFloat32ToFloat64(node);  // float32 -> float64 -> int32
    if (output_type.Is(Type::Signed32())) {
      op = machine()->ChangeFloat64ToInt32();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
               use_info.type_check() == TypeCheckKind::kSigned32) {
      op = simplified()->CheckedFloat64ToInt32(
          output_type.Maybe(Type::MinusZero())
              ? use_info.minus_zero_check()
              : CheckForMinusZeroMode::kDontCheckForMinusZero,
          use_info.feedback());
    } else if (output_type.Is(Type::Unsigned32())) {
      op = machine()->ChangeFloat64ToUint32();
    } else if (use_info.truncation().IsUsedAsWord32()) {
      op = machine()->TruncateFloat64ToWord32();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord32);
    }
  } else if (IsAnyTagged(output_rep)) {
    if (output_rep == MachineRepresentation::kTaggedSigned &&
        output_type.Is(Type::SignedSmall())) {
      op = simplified()->ChangeTaggedSignedToInt32();
    } else if (output_type.Is(Type::Signed32())) {
      op = simplified()->ChangeTaggedToInt32();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall) {
      op = simplified()->CheckedTaggedSignedToInt32(use_info.feedback());
    } else if (use_info.type_check() == TypeCheckKind::kSigned32) {
      op = simplified()->CheckedTaggedToInt32(
          output_type.Maybe(Type::MinusZero())
              ? use_info.minus_zero_check()
              : CheckForMinusZeroMode::kDontCheckForMinusZero,
          use_info.feedback());
    } else if (output_type.Is(Type::Unsigned32())) {
      op = simplified()->ChangeTaggedToUint32();
    } else if (use_info.truncation().IsUsedAsWord32()) {
      if (output_type.Is(Type::NumberOrOddball())) {
        op = simplified()->TruncateTaggedToWord32();
      } else if (use_info.type_check() == TypeCheckKind::kNumber) {
        op = simplified()->CheckedTruncateTaggedToWord32(
            CheckTaggedInputMode::kNumber, use_info.feedback());
      } else if (use_info.type_check() == TypeCheckKind::kNumberOrOddball) {
        op = simplified()->CheckedTruncateTaggedToWord32(
            CheckTaggedInputMode::kNumberOrOddball, use_info.feedback());
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kWord32);
      }
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord32);
    }
  } else if (output_rep == MachineRepresentation::kWord32) {
    if (use_info.truncation().IdentifiesZeroAndMinusZero()) {
      if (output_type.Is(Type::Signed32OrMinusZero()) ||
          output_type.Is(Type::Unsigned32OrMinusZero())) {
        return node;
      }
    }
    // Only the checked case should get here, the non-checked case is
    // handled in GetRepresentationFor.
    if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
        use_info.type_check() == TypeCheckKind::kSigned32) {
      if (output_type.Is(Type::Signed32())) {
        return node;
      } else if (output_type.Is(Type::Unsigned32())) {
        op = simplified()->CheckedUint32ToInt32(use_info.feedback());
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kWord32);
      }
    } else if (use_info.type_check() == TypeCheckKind::kNumber ||
               use_info.type_check() == TypeCheckKind::kNumberOrOddball) {
      return node;
    }
  } else if (output_rep == MachineRepresentation::kWord8 ||
             output_rep == MachineRepresentation::kWord16) {
    DCHECK_EQ(MachineRepresentation::kWord32, use_info.representation());
    DCHECK(use_info.type_check() == TypeCheckKind::kSignedSmall ||
           use_info.type_check() == TypeCheckKind::kSigned32);
    return node;
  } else if (output_rep == MachineRepresentation::kWord64) {
    if (output_type.Is(Type::Signed32()) ||
        output_type.Is(Type::Unsigned32())) {
      op = machine()->TruncateInt64ToInt32();
    } else if (output_type.Is(cache_.kSafeInteger) &&
               use_info.truncation().IsUsedAsWord32()) {
      op = machine()->TruncateInt64ToInt32();
    } else if (use_info.type_check() == TypeCheckKind::kSignedSmall ||
               use_info.type_check() == TypeCheckKind::kSigned32) {
      if (output_type.Is(cache_.kPositiveSafeInteger)) {
        op = simplified()->CheckedUint64ToInt32(use_info.feedback());
      } else if (output_type.Is(cache_.kSafeInteger)) {
        op = simplified()->CheckedInt64ToInt32(use_info.feedback());
      } else {
        return TypeError(node, output_rep, output_type,
                         MachineRepresentation::kWord32);
      }
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord32);
    }
  }

  if (op == nullptr) {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kWord32);
  }
  return InsertConversion(node, op, use_node);
}

Node* RepresentationChanger::InsertConversion(Node* node, const Operator* op,
                                              Node* use_node) {
  if (op->ControlInputCount() > 0) {
    // If the operator can deoptimize (which means it has control
    // input), we need to connect it to the effect and control chains.
    Node* effect = NodeProperties::GetEffectInput(use_node);
    Node* control = NodeProperties::GetControlInput(use_node);
    Node* conversion = jsgraph()->graph()->NewNode(op, node, effect, control);
    NodeProperties::ReplaceEffectInput(use_node, conversion);
    return conversion;
  }
  return jsgraph()->graph()->NewNode(op, node);
}

Node* RepresentationChanger::GetBitRepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kHeapConstant: {
      HeapObjectMatcher m(node);
      if (m.Is(factory()->false_value())) {
        return jsgraph()->Int32Constant(0);
      } else if (m.Is(factory()->true_value())) {
        return jsgraph()->Int32Constant(1);
      }
      break;
    }
    default:
      break;
  }
  // Select the correct X -> Bit operator.
  const Operator* op;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kBit), node);
  } else if (output_rep == MachineRepresentation::kTagged ||
             output_rep == MachineRepresentation::kTaggedPointer) {
    if (output_type.Is(Type::BooleanOrNullOrUndefined())) {
      // true is the only trueish Oddball.
      op = simplified()->ChangeTaggedToBit();
    } else {
      if (output_rep == MachineRepresentation::kTagged &&
          output_type.Maybe(Type::SignedSmall())) {
        op = simplified()->TruncateTaggedToBit();
      } else {
        // The {output_type} either doesn't include the Smi range,
        // or the {output_rep} is known to be TaggedPointer.
        op = simplified()->TruncateTaggedPointerToBit();
      }
    }
  } else if (output_rep == MachineRepresentation::kTaggedSigned) {
    node = jsgraph()->graph()->NewNode(machine()->WordEqual(), node,
                                       jsgraph()->IntPtrConstant(0));
    return jsgraph()->graph()->NewNode(machine()->Word32Equal(), node,
                                       jsgraph()->Int32Constant(0));
  } else if (IsWord(output_rep)) {
    node = jsgraph()->graph()->NewNode(machine()->Word32Equal(), node,
                                       jsgraph()->Int32Constant(0));
    return jsgraph()->graph()->NewNode(machine()->Word32Equal(), node,
                                       jsgraph()->Int32Constant(0));
  } else if (output_rep == MachineRepresentation::kWord64) {
    node = jsgraph()->graph()->NewNode(machine()->Word64Equal(), node,
                                       jsgraph()->Int64Constant(0));
    return jsgraph()->graph()->NewNode(machine()->Word32Equal(), node,
                                       jsgraph()->Int32Constant(0));
  } else if (output_rep == MachineRepresentation::kFloat32) {
    node = jsgraph()->graph()->NewNode(machine()->Float32Abs(), node);
    return jsgraph()->graph()->NewNode(machine()->Float32LessThan(),
                                       jsgraph()->Float32Constant(0.0), node);
  } else if (output_rep == MachineRepresentation::kFloat64) {
    node = jsgraph()->graph()->NewNode(machine()->Float64Abs(), node);
    return jsgraph()->graph()->NewNode(machine()->Float64LessThan(),
                                       jsgraph()->Float64Constant(0.0), node);
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kBit);
  }
  return jsgraph()->graph()->NewNode(op, node);
}

Node* RepresentationChanger::GetWord64RepresentationFor(
    Node* node, MachineRepresentation output_rep, Type output_type) {
  // Eagerly fold representation changes for constants.
  switch (node->opcode()) {
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat32Constant:
    case IrOpcode::kFloat64Constant:
      UNREACHABLE();
      break;
    case IrOpcode::kNumberConstant: {
      double const fv = OpParameter<double>(node->op());
      int64_t const iv = static_cast<int64_t>(fv);
      if (static_cast<double>(iv) == fv) {
        return jsgraph()->Int64Constant(iv);
      }
      break;
    }
    default:
      break;
  }

  // Select the correct X -> Word64 operator.
  const Operator* op;
  if (output_type.Is(Type::None())) {
    // This is an impossible value; it should not be used at runtime.
    return jsgraph()->graph()->NewNode(
        jsgraph()->common()->DeadValue(MachineRepresentation::kWord64), node);
  } else if (output_rep == MachineRepresentation::kBit) {
    return node;  // Sloppy comparison -> word64
  } else if (IsWord(output_rep)) {
    if (output_type.Is(Type::Unsigned32())) {
      op = machine()->ChangeUint32ToUint64();
    } else if (output_type.Is(Type::Signed32())) {
      op = machine()->ChangeInt32ToInt64();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord64);
    }
  } else if (output_rep == MachineRepresentation::kFloat32) {
    if (output_type.Is(cache_.kInt64)) {
      // float32 -> float64 -> int64
      node = InsertChangeFloat32ToFloat64(node);
      op = machine()->ChangeFloat64ToInt64();
    } else if (output_type.Is(cache_.kUint64)) {
      // float32 -> float64 -> uint64
      node = InsertChangeFloat32ToFloat64(node);
      op = machine()->ChangeFloat64ToUint64();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord64);
    }
  } else if (output_rep == MachineRepresentation::kFloat64) {
    if (output_type.Is(cache_.kInt64)) {
      op = machine()->ChangeFloat64ToInt64();
    } else if (output_type.Is(cache_.kUint64)) {
      op = machine()->ChangeFloat64ToUint64();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord64);
    }
  } else if (output_rep == MachineRepresentation::kTaggedSigned) {
    if (output_type.Is(Type::SignedSmall())) {
      op = simplified()->ChangeTaggedSignedToInt64();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord64);
    }
  } else if (CanBeTaggedPointer(output_rep)) {
    if (output_type.Is(cache_.kInt64)) {
      op = simplified()->ChangeTaggedToInt64();
    } else {
      return TypeError(node, output_rep, output_type,
                       MachineRepresentation::kWord64);
    }
  } else {
    return TypeError(node, output_rep, output_type,
                     MachineRepresentation::kWord64);
  }
  return jsgraph()->graph()->NewNode(op, node);
}

const Operator* RepresentationChanger::Int32OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberAdd:  // Fall through.
    case IrOpcode::kSpeculativeSafeIntegerAdd:
    case IrOpcode::kNumberAdd:
      return machine()->Int32Add();
    case IrOpcode::kSpeculativeNumberSubtract:  // Fall through.
    case IrOpcode::kSpeculativeSafeIntegerSubtract:
    case IrOpcode::kNumberSubtract:
      return machine()->Int32Sub();
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kNumberMultiply:
      return machine()->Int32Mul();
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kNumberDivide:
      return machine()->Int32Div();
    case IrOpcode::kSpeculativeNumberModulus:
    case IrOpcode::kNumberModulus:
      return machine()->Int32Mod();
    case IrOpcode::kSpeculativeNumberBitwiseOr:  // Fall through.
    case IrOpcode::kNumberBitwiseOr:
      return machine()->Word32Or();
    case IrOpcode::kSpeculativeNumberBitwiseXor:  // Fall through.
    case IrOpcode::kNumberBitwiseXor:
      return machine()->Word32Xor();
    case IrOpcode::kSpeculativeNumberBitwiseAnd:  // Fall through.
    case IrOpcode::kNumberBitwiseAnd:
      return machine()->Word32And();
    case IrOpcode::kNumberEqual:
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Word32Equal();
    case IrOpcode::kNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Int32LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Int32LessThanOrEqual();
    default:
      UNREACHABLE();
  }
}

const Operator* RepresentationChanger::Int32OverflowOperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeSafeIntegerAdd:
      return simplified()->CheckedInt32Add();
    case IrOpcode::kSpeculativeSafeIntegerSubtract:
      return simplified()->CheckedInt32Sub();
    case IrOpcode::kSpeculativeNumberDivide:
      return simplified()->CheckedInt32Div();
    case IrOpcode::kSpeculativeNumberModulus:
      return simplified()->CheckedInt32Mod();
    default:
      UNREACHABLE();
  }
}

const Operator* RepresentationChanger::Int64OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberAdd:  // Fall through.
    case IrOpcode::kSpeculativeSafeIntegerAdd:
    case IrOpcode::kNumberAdd:
      return machine()->Int64Add();
    case IrOpcode::kSpeculativeNumberSubtract:  // Fall through.
    case IrOpcode::kSpeculativeSafeIntegerSubtract:
    case IrOpcode::kNumberSubtract:
      return machine()->Int64Sub();
    default:
      UNREACHABLE();
  }
}

const Operator* RepresentationChanger::TaggedSignedOperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Is32() ? machine()->Int32LessThan()
                               : machine()->Int64LessThan();
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Is32() ? machine()->Int32LessThanOrEqual()
                               : machine()->Int64LessThanOrEqual();
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Is32() ? machine()->Word32Equal()
                               : machine()->Word64Equal();
    default:
      UNREACHABLE();
  }
}

const Operator* RepresentationChanger::Uint32OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kNumberAdd:
      return machine()->Int32Add();
    case IrOpcode::kNumberSubtract:
      return machine()->Int32Sub();
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kNumberMultiply:
      return machine()->Int32Mul();
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kNumberDivide:
      return machine()->Uint32Div();
    case IrOpcode::kSpeculativeNumberModulus:
    case IrOpcode::kNumberModulus:
      return machine()->Uint32Mod();
    case IrOpcode::kNumberEqual:
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Word32Equal();
    case IrOpcode::kNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Uint32LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Uint32LessThanOrEqual();
    case IrOpcode::kNumberClz32:
      return machine()->Word32Clz();
    case IrOpcode::kNumberImul:
      return machine()->Int32Mul();
    default:
      UNREACHABLE();
  }
}

const Operator* RepresentationChanger::Uint32OverflowOperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberDivide:
      return simplified()->CheckedUint32Div();
    case IrOpcode::kSpeculativeNumberModulus:
      return simplified()->CheckedUint32Mod();
    default:
      UNREACHABLE();
  }
}

const Operator* RepresentationChanger::Float64OperatorFor(
    IrOpcode::Value opcode) {
  switch (opcode) {
    case IrOpcode::kSpeculativeNumberAdd:
    case IrOpcode::kSpeculativeSafeIntegerAdd:
    case IrOpcode::kNumberAdd:
      return machine()->Float64Add();
    case IrOpcode::kSpeculativeNumberSubtract:
    case IrOpcode::kSpeculativeSafeIntegerSubtract:
    case IrOpcode::kNumberSubtract:
      return machine()->Float64Sub();
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kNumberMultiply:
      return machine()->Float64Mul();
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kNumberDivide:
      return machine()->Float64Div();
    case IrOpcode::kSpeculativeNumberModulus:
    case IrOpcode::kNumberModulus:
      return machine()->Float64Mod();
    case IrOpcode::kNumberEqual:
    case IrOpcode::kSpeculativeNumberEqual:
      return machine()->Float64Equal();
    case IrOpcode::kNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      return machine()->Float64LessThan();
    case IrOpcode::kNumberLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return machine()->Float64LessThanOrEqual();
    case IrOpcode::kNumberAbs:
      return machine()->Float64Abs();
    case IrOpcode::kNumberAcos:
      return machine()->Float64Acos();
    case IrOpcode::kNumberAcosh:
      return machine()->Float64Acosh();
    case IrOpcode::kNumberAsin:
      return machine()->Float64Asin();
    case IrOpcode::kNumberAsinh:
      return machine()->Float64Asinh();
    case IrOpcode::kNumberAtan:
      return machine()->Float64Atan();
    case IrOpcode::kNumberAtanh:
      return machine()->Float64Atanh();
    case IrOpcode::kNumberAtan2:
      return machine()->Float64Atan2();
    case IrOpcode::kNumberCbrt:
      return machine()->Float64Cbrt();
    case IrOpcode::kNumberCeil:
      return machine()->Float64RoundUp().placeholder();
    case IrOpcode::kNumberCos:
      return machine()->Float64Cos();
    case IrOpcode::kNumberCosh:
      return machine()->Float64Cosh();
    case IrOpcode::kNumberExp:
      return machine()->Float64Exp();
    case IrOpcode::kNumberExpm1:
      return machine()->Float64Expm1();
    case IrOpcode::kNumberFloor:
      return machine()->Float64RoundDown().placeholder();
    case IrOpcode::kNumberFround:
      return machine()->TruncateFloat64ToFloat32();
    case IrOpcode::kNumberLog:
      return machine()->Float64Log();
    case IrOpcode::kNumberLog1p:
      return machine()->Float64Log1p();
    case IrOpcode::kNumberLog2:
      return machine()->Float64Log2();
    case IrOpcode::kNumberLog10:
      return machine()->Float64Log10();
    case IrOpcode::kNumberMax:
      return machine()->Float64Max();
    case IrOpcode::kNumberMin:
      return machine()->Float64Min();
    case IrOpcode::kNumberPow:
      return machine()->Float64Pow();
    case IrOpcode::kNumberSin:
      return machine()->Float64Sin();
    case IrOpcode::kNumberSinh:
      return machine()->Float64Sinh();
    case IrOpcode::kNumberSqrt:
      return machine()->Float64Sqrt();
    case IrOpcode::kNumberTan:
      return machine()->Float64Tan();
    case IrOpcode::kNumberTanh:
      return machine()->Float64Tanh();
    case IrOpcode::kNumberTrunc:
      return machine()->Float64RoundTruncate().placeholder();
    case IrOpcode::kNumberSilenceNaN:
      return machine()->Float64SilenceNaN();
    default:
      UNREACHABLE();
  }
}

Node* RepresentationChanger::TypeError(Node* node,
                                       MachineRepresentation output_rep,
                                       Type output_type,
                                       MachineRepresentation use) {
  type_error_ = true;
  if (!testing_type_errors_) {
    std::ostringstream out_str;
    out_str << output_rep << " (";
    output_type.PrintTo(out_str);
    out_str << ")";

    std::ostringstream use_str;
    use_str << use;

    FATAL(
        "RepresentationChangerError: node #%d:%s of "
        "%s cannot be changed to %s",
        node->id(), node->op()->mnemonic(), out_str.str().c_str(),
        use_str.str().c_str());
  }
  return node;
}

Node* RepresentationChanger::InsertChangeBitToTagged(Node* node) {
  return jsgraph()->graph()->NewNode(simplified()->ChangeBitToTagged(), node);
}

Node* RepresentationChanger::InsertChangeFloat32ToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat32ToFloat64(), node);
}

Node* RepresentationChanger::InsertChangeFloat64ToUint32(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat64ToUint32(), node);
}

Node* RepresentationChanger::InsertChangeFloat64ToInt32(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeFloat64ToInt32(), node);
}

Node* RepresentationChanger::InsertChangeInt32ToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeInt32ToFloat64(), node);
}

Node* RepresentationChanger::InsertChangeTaggedSignedToInt32(Node* node) {
  return jsgraph()->graph()->NewNode(simplified()->ChangeTaggedSignedToInt32(),
                                     node);
}

Node* RepresentationChanger::InsertChangeTaggedToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(simplified()->ChangeTaggedToFloat64(),
                                     node);
}

Node* RepresentationChanger::InsertChangeUint32ToFloat64(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->ChangeUint32ToFloat64(), node);
}

Node* RepresentationChanger::InsertTruncateInt64ToInt32(Node* node) {
  return jsgraph()->graph()->NewNode(machine()->TruncateInt64ToInt32(), node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
