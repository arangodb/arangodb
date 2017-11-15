// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/type-hints.h"

namespace v8 {
namespace internal {

std::ostream& operator<<(std::ostream& os, BinaryOperationHint hint) {
  switch (hint) {
    case BinaryOperationHint::kNone:
      return os << "None";
    case BinaryOperationHint::kSignedSmall:
      return os << "SignedSmall";
    case BinaryOperationHint::kSigned32:
      return os << "Signed32";
    case BinaryOperationHint::kNumberOrOddball:
      return os << "NumberOrOddball";
    case BinaryOperationHint::kString:
      return os << "String";
    case BinaryOperationHint::kAny:
      return os << "Any";
  }
  UNREACHABLE();
  return os;
}

std::ostream& operator<<(std::ostream& os, CompareOperationHint hint) {
  switch (hint) {
    case CompareOperationHint::kNone:
      return os << "None";
    case CompareOperationHint::kSignedSmall:
      return os << "SignedSmall";
    case CompareOperationHint::kNumber:
      return os << "Number";
    case CompareOperationHint::kNumberOrOddball:
      return os << "NumberOrOddball";
    case CompareOperationHint::kInternalizedString:
      return os << "InternalizedString";
    case CompareOperationHint::kString:
      return os << "String";
    case CompareOperationHint::kAny:
      return os << "Any";
  }
  UNREACHABLE();
  return os;
}

std::ostream& operator<<(std::ostream& os, ToBooleanHint hint) {
  switch (hint) {
    case ToBooleanHint::kNone:
      return os << "None";
    case ToBooleanHint::kUndefined:
      return os << "Undefined";
    case ToBooleanHint::kBoolean:
      return os << "Boolean";
    case ToBooleanHint::kNull:
      return os << "Null";
    case ToBooleanHint::kSmallInteger:
      return os << "SmallInteger";
    case ToBooleanHint::kReceiver:
      return os << "Receiver";
    case ToBooleanHint::kString:
      return os << "String";
    case ToBooleanHint::kSymbol:
      return os << "Symbol";
    case ToBooleanHint::kHeapNumber:
      return os << "HeapNumber";
    case ToBooleanHint::kSimdValue:
      return os << "SimdValue";
    case ToBooleanHint::kAny:
      return os << "Any";
    case ToBooleanHint::kNeedsMap:
      return os << "NeedsMap";
  }
  UNREACHABLE();
  return os;
}

std::string ToString(ToBooleanHint hint) {
  switch (hint) {
    case ToBooleanHint::kNone:
      return "None";
    case ToBooleanHint::kUndefined:
      return "Undefined";
    case ToBooleanHint::kBoolean:
      return "Boolean";
    case ToBooleanHint::kNull:
      return "Null";
    case ToBooleanHint::kSmallInteger:
      return "SmallInteger";
    case ToBooleanHint::kReceiver:
      return "Receiver";
    case ToBooleanHint::kString:
      return "String";
    case ToBooleanHint::kSymbol:
      return "Symbol";
    case ToBooleanHint::kHeapNumber:
      return "HeapNumber";
    case ToBooleanHint::kSimdValue:
      return "SimdValue";
    case ToBooleanHint::kAny:
      return "Any";
    case ToBooleanHint::kNeedsMap:
      return "NeedsMap";
  }
  UNREACHABLE();
  return "";
}

std::ostream& operator<<(std::ostream& os, ToBooleanHints hints) {
  if (hints == ToBooleanHint::kAny) return os << "Any";
  if (hints == ToBooleanHint::kNone) return os << "None";
  bool first = true;
  for (ToBooleanHints::mask_type i = 0; i < sizeof(i) * 8; ++i) {
    ToBooleanHint const hint = static_cast<ToBooleanHint>(1u << i);
    if (hints & hint) {
      if (!first) os << "|";
      first = false;
      os << hint;
    }
  }
  return os;
}

std::string ToString(ToBooleanHints hints) {
  if (hints == ToBooleanHint::kAny) return "Any";
  if (hints == ToBooleanHint::kNone) return "None";
  std::string ret;
  bool first = true;
  for (ToBooleanHints::mask_type i = 0; i < sizeof(i) * 8; ++i) {
    ToBooleanHint const hint = static_cast<ToBooleanHint>(1u << i);
    if (hints & hint) {
      if (!first) ret += "|";
      first = false;
      ret += ToString(hint);
    }
  }
  return ret;
}

std::ostream& operator<<(std::ostream& os, const StringAddFlags& flags) {
  switch (flags) {
    case STRING_ADD_CHECK_NONE:
      return os << "CheckNone";
    case STRING_ADD_CHECK_LEFT:
      return os << "CheckLeft";
    case STRING_ADD_CHECK_RIGHT:
      return os << "CheckRight";
    case STRING_ADD_CHECK_BOTH:
      return os << "CheckBoth";
    case STRING_ADD_CONVERT_LEFT:
      return os << "ConvertLeft";
    case STRING_ADD_CONVERT_RIGHT:
      return os << "ConvertRight";
    case STRING_ADD_CONVERT:
      break;
  }
  UNREACHABLE();
  return os;
}

}  // namespace internal
}  // namespace v8
