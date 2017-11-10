// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/operation-typer.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/types.h"
#include "src/factory.h"
#include "src/isolate.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

OperationTyper::OperationTyper(Isolate* isolate, Zone* zone)
    : zone_(zone), cache_(TypeCache::Get()) {
  Factory* factory = isolate->factory();
  infinity_ = Type::NewConstant(factory->infinity_value(), zone);
  minus_infinity_ = Type::NewConstant(factory->minus_infinity_value(), zone);
  Type* truncating_to_zero = Type::MinusZeroOrNaN();
  DCHECK(!truncating_to_zero->Maybe(Type::Integral32()));

  singleton_false_ = Type::HeapConstant(factory->false_value(), zone);
  singleton_true_ = Type::HeapConstant(factory->true_value(), zone);
  singleton_the_hole_ = Type::HeapConstant(factory->the_hole_value(), zone);
  signed32ish_ = Type::Union(Type::Signed32(), truncating_to_zero, zone);
  unsigned32ish_ = Type::Union(Type::Unsigned32(), truncating_to_zero, zone);
}

Type* OperationTyper::Merge(Type* left, Type* right) {
  return Type::Union(left, right, zone());
}

Type* OperationTyper::WeakenRange(Type* previous_range, Type* current_range) {
  static const double kWeakenMinLimits[] = {0.0,
                                            -1073741824.0,
                                            -2147483648.0,
                                            -4294967296.0,
                                            -8589934592.0,
                                            -17179869184.0,
                                            -34359738368.0,
                                            -68719476736.0,
                                            -137438953472.0,
                                            -274877906944.0,
                                            -549755813888.0,
                                            -1099511627776.0,
                                            -2199023255552.0,
                                            -4398046511104.0,
                                            -8796093022208.0,
                                            -17592186044416.0,
                                            -35184372088832.0,
                                            -70368744177664.0,
                                            -140737488355328.0,
                                            -281474976710656.0,
                                            -562949953421312.0};
  static const double kWeakenMaxLimits[] = {0.0,
                                            1073741823.0,
                                            2147483647.0,
                                            4294967295.0,
                                            8589934591.0,
                                            17179869183.0,
                                            34359738367.0,
                                            68719476735.0,
                                            137438953471.0,
                                            274877906943.0,
                                            549755813887.0,
                                            1099511627775.0,
                                            2199023255551.0,
                                            4398046511103.0,
                                            8796093022207.0,
                                            17592186044415.0,
                                            35184372088831.0,
                                            70368744177663.0,
                                            140737488355327.0,
                                            281474976710655.0,
                                            562949953421311.0};
  STATIC_ASSERT(arraysize(kWeakenMinLimits) == arraysize(kWeakenMaxLimits));

  double current_min = current_range->Min();
  double new_min = current_min;
  // Find the closest lower entry in the list of allowed
  // minima (or negative infinity if there is no such entry).
  if (current_min != previous_range->Min()) {
    new_min = -V8_INFINITY;
    for (double const min : kWeakenMinLimits) {
      if (min <= current_min) {
        new_min = min;
        break;
      }
    }
  }

  double current_max = current_range->Max();
  double new_max = current_max;
  // Find the closest greater entry in the list of allowed
  // maxima (or infinity if there is no such entry).
  if (current_max != previous_range->Max()) {
    new_max = V8_INFINITY;
    for (double const max : kWeakenMaxLimits) {
      if (max >= current_max) {
        new_max = max;
        break;
      }
    }
  }

  return Type::Range(new_min, new_max, zone());
}

Type* OperationTyper::Rangify(Type* type) {
  if (type->IsRange()) return type;  // Shortcut.
  if (!type->Is(cache_.kInteger)) {
    return type;  // Give up on non-integer types.
  }
  double min = type->Min();
  double max = type->Max();
  // Handle the degenerate case of empty bitset types (such as
  // OtherUnsigned31 and OtherSigned32 on 64-bit architectures).
  if (std::isnan(min)) {
    DCHECK(std::isnan(max));
    return type;
  }
  return Type::Range(min, max, zone());
}

namespace {

// Returns the array's least element, ignoring NaN.
// There must be at least one non-NaN element.
// Any -0 is converted to 0.
double array_min(double a[], size_t n) {
  DCHECK(n != 0);
  double x = +V8_INFINITY;
  for (size_t i = 0; i < n; ++i) {
    if (!std::isnan(a[i])) {
      x = std::min(a[i], x);
    }
  }
  DCHECK(!std::isnan(x));
  return x == 0 ? 0 : x;  // -0 -> 0
}

// Returns the array's greatest element, ignoring NaN.
// There must be at least one non-NaN element.
// Any -0 is converted to 0.
double array_max(double a[], size_t n) {
  DCHECK(n != 0);
  double x = -V8_INFINITY;
  for (size_t i = 0; i < n; ++i) {
    if (!std::isnan(a[i])) {
      x = std::max(a[i], x);
    }
  }
  DCHECK(!std::isnan(x));
  return x == 0 ? 0 : x;  // -0 -> 0
}

}  // namespace

Type* OperationTyper::AddRanger(double lhs_min, double lhs_max, double rhs_min,
                                double rhs_max) {
  double results[4];
  results[0] = lhs_min + rhs_min;
  results[1] = lhs_min + rhs_max;
  results[2] = lhs_max + rhs_min;
  results[3] = lhs_max + rhs_max;
  // Since none of the inputs can be -0, the result cannot be -0 either.
  // However, it can be nan (the sum of two infinities of opposite sign).
  // On the other hand, if none of the "results" above is nan, then the
  // actual result cannot be nan either.
  int nans = 0;
  for (int i = 0; i < 4; ++i) {
    if (std::isnan(results[i])) ++nans;
  }
  if (nans == 4) return Type::NaN();
  Type* type =
      Type::Range(array_min(results, 4), array_max(results, 4), zone());
  if (nans > 0) type = Type::Union(type, Type::NaN(), zone());
  // Examples:
  //   [-inf, -inf] + [+inf, +inf] = NaN
  //   [-inf, -inf] + [n, +inf] = [-inf, -inf] \/ NaN
  //   [-inf, +inf] + [n, +inf] = [-inf, +inf] \/ NaN
  //   [-inf, m] + [n, +inf] = [-inf, +inf] \/ NaN
  return type;
}

Type* OperationTyper::SubtractRanger(double lhs_min, double lhs_max,
                                     double rhs_min, double rhs_max) {
  double results[4];
  results[0] = lhs_min - rhs_min;
  results[1] = lhs_min - rhs_max;
  results[2] = lhs_max - rhs_min;
  results[3] = lhs_max - rhs_max;
  // Since none of the inputs can be -0, the result cannot be -0.
  // However, it can be nan (the subtraction of two infinities of same sign).
  // On the other hand, if none of the "results" above is nan, then the actual
  // result cannot be nan either.
  int nans = 0;
  for (int i = 0; i < 4; ++i) {
    if (std::isnan(results[i])) ++nans;
  }
  if (nans == 4) return Type::NaN();  // [inf..inf] - [inf..inf] (all same sign)
  Type* type =
      Type::Range(array_min(results, 4), array_max(results, 4), zone());
  return nans == 0 ? type : Type::Union(type, Type::NaN(), zone());
  // Examples:
  //   [-inf, +inf] - [-inf, +inf] = [-inf, +inf] \/ NaN
  //   [-inf, -inf] - [-inf, -inf] = NaN
  //   [-inf, -inf] - [n, +inf] = [-inf, -inf] \/ NaN
  //   [m, +inf] - [-inf, n] = [-inf, +inf] \/ NaN
}

Type* OperationTyper::MultiplyRanger(Type* lhs, Type* rhs) {
  double results[4];
  double lmin = lhs->AsRange()->Min();
  double lmax = lhs->AsRange()->Max();
  double rmin = rhs->AsRange()->Min();
  double rmax = rhs->AsRange()->Max();
  results[0] = lmin * rmin;
  results[1] = lmin * rmax;
  results[2] = lmax * rmin;
  results[3] = lmax * rmax;
  // If the result may be nan, we give up on calculating a precise type, because
  // the discontinuity makes it too complicated.  Note that even if none of the
  // "results" above is nan, the actual result may still be, so we have to do a
  // different check:
  bool maybe_nan = (lhs->Maybe(cache_.kSingletonZero) &&
                    (rmin == -V8_INFINITY || rmax == +V8_INFINITY)) ||
                   (rhs->Maybe(cache_.kSingletonZero) &&
                    (lmin == -V8_INFINITY || lmax == +V8_INFINITY));
  if (maybe_nan) return cache_.kIntegerOrMinusZeroOrNaN;  // Giving up.
  bool maybe_minuszero = (lhs->Maybe(cache_.kSingletonZero) && rmin < 0) ||
                         (rhs->Maybe(cache_.kSingletonZero) && lmin < 0);
  Type* range =
      Type::Range(array_min(results, 4), array_max(results, 4), zone());
  return maybe_minuszero ? Type::Union(range, Type::MinusZero(), zone())
                         : range;
}

Type* OperationTyper::ToNumber(Type* type) {
  if (type->Is(Type::Number())) return type;
  if (type->Is(Type::NullOrUndefined())) {
    if (type->Is(Type::Null())) return cache_.kSingletonZero;
    if (type->Is(Type::Undefined())) return Type::NaN();
    return Type::Union(Type::NaN(), cache_.kSingletonZero, zone());
  }
  if (type->Is(Type::Boolean())) {
    if (type->Is(singleton_false_)) return cache_.kSingletonZero;
    if (type->Is(singleton_true_)) return cache_.kSingletonOne;
    return cache_.kZeroOrOne;
  }
  if (type->Is(Type::NumberOrOddball())) {
    if (type->Is(Type::NumberOrUndefined())) {
      type = Type::Union(type, Type::NaN(), zone());
    } else if (type->Is(Type::NullOrNumber())) {
      type = Type::Union(type, cache_.kSingletonZero, zone());
    } else if (type->Is(Type::BooleanOrNullOrNumber())) {
      type = Type::Union(type, cache_.kZeroOrOne, zone());
    } else {
      type = Type::Union(type, cache_.kZeroOrOneOrNaN, zone());
    }
    return Type::Intersect(type, Type::Number(), zone());
  }
  return Type::Number();
}

Type* OperationTyper::NumberAbs(Type* type) {
  DCHECK(type->Is(Type::Number()));

  if (!type->IsInhabited()) {
    return Type::None();
  }

  bool const maybe_nan = type->Maybe(Type::NaN());
  bool const maybe_minuszero = type->Maybe(Type::MinusZero());
  type = Type::Intersect(type, Type::PlainNumber(), zone());
  double const max = type->Max();
  double const min = type->Min();
  if (min < 0) {
    if (type->Is(cache_.kInteger)) {
      type = Type::Range(0.0, std::max(std::fabs(min), std::fabs(max)), zone());
    } else {
      type = Type::PlainNumber();
    }
  }
  if (maybe_minuszero) {
    type = Type::Union(type, cache_.kSingletonZero, zone());
  }
  if (maybe_nan) {
    type = Type::Union(type, Type::NaN(), zone());
  }
  return type;
}

Type* OperationTyper::NumberAcos(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberAcosh(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberAsin(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberAsinh(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberAtan(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberAtanh(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberCbrt(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberCeil(Type* type) {
  DCHECK(type->Is(Type::Number()));
  if (type->Is(cache_.kIntegerOrMinusZeroOrNaN)) return type;
  // TODO(bmeurer): We could infer a more precise type here.
  return cache_.kIntegerOrMinusZeroOrNaN;
}

Type* OperationTyper::NumberClz32(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return cache_.kZeroToThirtyTwo;
}

Type* OperationTyper::NumberCos(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberCosh(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberExp(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Union(Type::PlainNumber(), Type::NaN(), zone());
}

Type* OperationTyper::NumberExpm1(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Union(Type::PlainNumber(), Type::NaN(), zone());
}

Type* OperationTyper::NumberFloor(Type* type) {
  DCHECK(type->Is(Type::Number()));
  if (type->Is(cache_.kIntegerOrMinusZeroOrNaN)) return type;
  type = Type::Intersect(type, Type::MinusZeroOrNaN(), zone());
  type = Type::Union(type, cache_.kInteger, zone());
  return type;
}

Type* OperationTyper::NumberFround(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberLog(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberLog1p(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberLog2(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberLog10(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberRound(Type* type) {
  DCHECK(type->Is(Type::Number()));
  if (type->Is(cache_.kIntegerOrMinusZeroOrNaN)) return type;
  // TODO(bmeurer): We could infer a more precise type here.
  return cache_.kIntegerOrMinusZeroOrNaN;
}

Type* OperationTyper::NumberSign(Type* type) {
  DCHECK(type->Is(Type::Number()));
  if (type->Is(cache_.kZeroish)) return type;
  bool maybe_minuszero = type->Maybe(Type::MinusZero());
  bool maybe_nan = type->Maybe(Type::NaN());
  type = Type::Intersect(type, Type::PlainNumber(), zone());
  if (type->Max() < 0.0) {
    type = cache_.kSingletonMinusOne;
  } else if (type->Max() <= 0.0) {
    type = cache_.kMinusOneOrZero;
  } else if (type->Min() > 0.0) {
    type = cache_.kSingletonOne;
  } else if (type->Min() >= 0.0) {
    type = cache_.kZeroOrOne;
  } else {
    type = Type::Range(-1.0, 1.0, zone());
  }
  if (maybe_minuszero) type = Type::Union(type, Type::MinusZero(), zone());
  if (maybe_nan) type = Type::Union(type, Type::NaN(), zone());
  return type;
}

Type* OperationTyper::NumberSin(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberSinh(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberSqrt(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberTan(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberTanh(Type* type) {
  DCHECK(type->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberTrunc(Type* type) {
  DCHECK(type->Is(Type::Number()));
  if (type->Is(cache_.kIntegerOrMinusZeroOrNaN)) return type;
  // TODO(bmeurer): We could infer a more precise type here.
  return cache_.kIntegerOrMinusZeroOrNaN;
}

Type* OperationTyper::NumberToBoolean(Type* type) {
  DCHECK(type->Is(Type::Number()));
  if (!type->IsInhabited()) return Type::None();
  if (type->Is(cache_.kZeroish)) return singleton_false_;
  if (type->Is(Type::PlainNumber()) && (type->Max() < 0 || 0 < type->Min())) {
    return singleton_true_;  // Ruled out nan, -0 and +0.
  }
  return Type::Boolean();
}

Type* OperationTyper::NumberToInt32(Type* type) {
  DCHECK(type->Is(Type::Number()));

  if (type->Is(Type::Signed32())) return type;
  if (type->Is(cache_.kZeroish)) return cache_.kSingletonZero;
  if (type->Is(signed32ish_)) {
    return Type::Intersect(Type::Union(type, cache_.kSingletonZero, zone()),
                           Type::Signed32(), zone());
  }
  return Type::Signed32();
}

Type* OperationTyper::NumberToUint32(Type* type) {
  DCHECK(type->Is(Type::Number()));

  if (type->Is(Type::Unsigned32())) return type;
  if (type->Is(cache_.kZeroish)) return cache_.kSingletonZero;
  if (type->Is(unsigned32ish_)) {
    return Type::Intersect(Type::Union(type, cache_.kSingletonZero, zone()),
                           Type::Unsigned32(), zone());
  }
  return Type::Unsigned32();
}

Type* OperationTyper::NumberToUint8Clamped(Type* type) {
  DCHECK(type->Is(Type::Number()));

  if (type->Is(cache_.kUint8)) return type;
  return cache_.kUint8;
}

Type* OperationTyper::NumberSilenceNaN(Type* type) {
  DCHECK(type->Is(Type::Number()));
  // TODO(jarin): This is a terrible hack; we definitely need a dedicated type
  // for the hole (tagged and/or double). Otherwise if the input is the hole
  // NaN constant, we'd just eliminate this node in JSTypedLowering.
  if (type->Maybe(Type::NaN())) return Type::Number();
  return type;
}

Type* OperationTyper::NumberAdd(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) {
    return Type::None();
  }

  // Addition can return NaN if either input can be NaN or we try to compute
  // the sum of two infinities of opposite sign.
  bool maybe_nan = lhs->Maybe(Type::NaN()) || rhs->Maybe(Type::NaN());

  // Addition can yield minus zero only if both inputs can be minus zero.
  bool maybe_minuszero = true;
  if (lhs->Maybe(Type::MinusZero())) {
    lhs = Type::Union(lhs, cache_.kSingletonZero, zone());
  } else {
    maybe_minuszero = false;
  }
  if (rhs->Maybe(Type::MinusZero())) {
    rhs = Type::Union(rhs, cache_.kSingletonZero, zone());
  } else {
    maybe_minuszero = false;
  }

  // We can give more precise types for integers.
  Type* type = Type::None();
  lhs = Type::Intersect(lhs, Type::PlainNumber(), zone());
  rhs = Type::Intersect(rhs, Type::PlainNumber(), zone());
  if (lhs->IsInhabited() && rhs->IsInhabited()) {
    if (lhs->Is(cache_.kInteger) && rhs->Is(cache_.kInteger)) {
      type = AddRanger(lhs->Min(), lhs->Max(), rhs->Min(), rhs->Max());
    } else {
      if ((lhs->Maybe(minus_infinity_) && rhs->Maybe(infinity_)) ||
          (rhs->Maybe(minus_infinity_) && lhs->Maybe(infinity_))) {
        maybe_nan = true;
      }
      type = Type::PlainNumber();
    }
  }

  // Take into account the -0 and NaN information computed earlier.
  if (maybe_minuszero) type = Type::Union(type, Type::MinusZero(), zone());
  if (maybe_nan) type = Type::Union(type, Type::NaN(), zone());
  return type;
}

Type* OperationTyper::NumberSubtract(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) {
    return Type::None();
  }

  // Subtraction can return NaN if either input can be NaN or we try to
  // compute the sum of two infinities of opposite sign.
  bool maybe_nan = lhs->Maybe(Type::NaN()) || rhs->Maybe(Type::NaN());

  // Subtraction can yield minus zero if {lhs} can be minus zero and {rhs}
  // can be zero.
  bool maybe_minuszero = false;
  if (lhs->Maybe(Type::MinusZero())) {
    lhs = Type::Union(lhs, cache_.kSingletonZero, zone());
    maybe_minuszero = rhs->Maybe(cache_.kSingletonZero);
  }
  if (rhs->Maybe(Type::MinusZero())) {
    rhs = Type::Union(rhs, cache_.kSingletonZero, zone());
  }

  // We can give more precise types for integers.
  Type* type = Type::None();
  lhs = Type::Intersect(lhs, Type::PlainNumber(), zone());
  rhs = Type::Intersect(rhs, Type::PlainNumber(), zone());
  if (lhs->IsInhabited() && rhs->IsInhabited()) {
    if (lhs->Is(cache_.kInteger) && rhs->Is(cache_.kInteger)) {
      type = SubtractRanger(lhs->Min(), lhs->Max(), rhs->Min(), rhs->Max());
    } else {
      if ((lhs->Maybe(infinity_) && rhs->Maybe(infinity_)) ||
          (rhs->Maybe(minus_infinity_) && lhs->Maybe(minus_infinity_))) {
        maybe_nan = true;
      }
      type = Type::PlainNumber();
    }
  }

  // Take into account the -0 and NaN information computed earlier.
  if (maybe_minuszero) type = Type::Union(type, Type::MinusZero(), zone());
  if (maybe_nan) type = Type::Union(type, Type::NaN(), zone());
  return type;
}

Type* OperationTyper::NumberMultiply(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) {
    return Type::None();
  }

  lhs = Rangify(lhs);
  rhs = Rangify(rhs);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  if (lhs->IsRange() && rhs->IsRange()) {
    return MultiplyRanger(lhs, rhs);
  }
  return Type::Number();
}

Type* OperationTyper::NumberDivide(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) {
    return Type::None();
  }

  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  // Division is tricky, so all we do is try ruling out -0 and NaN.
  bool maybe_minuszero = !lhs->Is(cache_.kPositiveIntegerOrNaN) ||
                         !rhs->Is(cache_.kPositiveIntegerOrNaN);
  bool maybe_nan =
      lhs->Maybe(Type::NaN()) || rhs->Maybe(cache_.kZeroish) ||
      ((lhs->Min() == -V8_INFINITY || lhs->Max() == +V8_INFINITY) &&
       (rhs->Min() == -V8_INFINITY || rhs->Max() == +V8_INFINITY));

  // Take into account the -0 and NaN information computed earlier.
  Type* type = Type::PlainNumber();
  if (maybe_minuszero) type = Type::Union(type, Type::MinusZero(), zone());
  if (maybe_nan) type = Type::Union(type, Type::NaN(), zone());
  return type;
}

Type* OperationTyper::NumberModulus(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  // Modulus can yield NaN if either {lhs} or {rhs} are NaN, or
  // {lhs} is not finite, or the {rhs} is a zero value.
  bool maybe_nan = lhs->Maybe(Type::NaN()) || rhs->Maybe(cache_.kZeroish) ||
                   lhs->Min() == -V8_INFINITY || lhs->Max() == +V8_INFINITY;

  // Deal with -0 inputs, only the signbit of {lhs} matters for the result.
  bool maybe_minuszero = false;
  if (lhs->Maybe(Type::MinusZero())) {
    maybe_minuszero = true;
    lhs = Type::Union(lhs, cache_.kSingletonZero, zone());
  }
  if (rhs->Maybe(Type::MinusZero())) {
    rhs = Type::Union(rhs, cache_.kSingletonZero, zone());
  }

  // Rule out NaN and -0, and check what we can do with the remaining type info.
  Type* type = Type::None();
  lhs = Type::Intersect(lhs, Type::PlainNumber(), zone());
  rhs = Type::Intersect(rhs, Type::PlainNumber(), zone());

  // We can only derive a meaningful type if both {lhs} and {rhs} are inhabited,
  // and the {rhs} is not 0, otherwise the result is NaN independent of {lhs}.
  if (lhs->IsInhabited() && !rhs->Is(cache_.kSingletonZero)) {
    // Determine the bounds of {lhs} and {rhs}.
    double const lmin = lhs->Min();
    double const lmax = lhs->Max();
    double const rmin = rhs->Min();
    double const rmax = rhs->Max();

    // The sign of the result is the sign of the {lhs}.
    if (lmin < 0.0) maybe_minuszero = true;

    // For integer inputs {lhs} and {rhs} we can infer a precise type.
    if (lhs->Is(cache_.kInteger) && rhs->Is(cache_.kInteger)) {
      double labs = std::max(std::abs(lmin), std::abs(lmax));
      double rabs = std::max(std::abs(rmin), std::abs(rmax)) - 1;
      double abs = std::min(labs, rabs);
      double min = 0.0, max = 0.0;
      if (lmin >= 0.0) {
        // {lhs} positive.
        min = 0.0;
        max = abs;
      } else if (lmax <= 0.0) {
        // {lhs} negative.
        min = 0.0 - abs;
        max = 0.0;
      } else {
        // {lhs} positive or negative.
        min = 0.0 - abs;
        max = abs;
      }
      type = Type::Range(min, max, zone());
    } else {
      type = Type::PlainNumber();
    }
  }

  // Take into account the -0 and NaN information computed earlier.
  if (maybe_minuszero) type = Type::Union(type, Type::MinusZero(), zone());
  if (maybe_nan) type = Type::Union(type, Type::NaN(), zone());
  return type;
}

Type* OperationTyper::NumberBitwiseOr(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) return Type::None();

  lhs = NumberToInt32(lhs);
  rhs = NumberToInt32(rhs);

  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  // Or-ing any two values results in a value no smaller than their minimum.
  // Even no smaller than their maximum if both values are non-negative.
  double min =
      lmin >= 0 && rmin >= 0 ? std::max(lmin, rmin) : std::min(lmin, rmin);
  double max = kMaxInt;

  // Or-ing with 0 is essentially a conversion to int32.
  if (rmin == 0 && rmax == 0) {
    min = lmin;
    max = lmax;
  }
  if (lmin == 0 && lmax == 0) {
    min = rmin;
    max = rmax;
  }

  if (lmax < 0 || rmax < 0) {
    // Or-ing two values of which at least one is negative results in a negative
    // value.
    max = std::min(max, -1.0);
  }
  return Type::Range(min, max, zone());
}

Type* OperationTyper::NumberBitwiseAnd(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) return Type::None();

  lhs = NumberToInt32(lhs);
  rhs = NumberToInt32(rhs);

  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  double min = kMinInt;
  // And-ing any two values results in a value no larger than their maximum.
  // Even no larger than their minimum if both values are non-negative.
  double max =
      lmin >= 0 && rmin >= 0 ? std::min(lmax, rmax) : std::max(lmax, rmax);
  // And-ing with a non-negative value x causes the result to be between
  // zero and x.
  if (lmin >= 0) {
    min = 0;
    max = std::min(max, lmax);
  }
  if (rmin >= 0) {
    min = 0;
    max = std::min(max, rmax);
  }
  return Type::Range(min, max, zone());
}

Type* OperationTyper::NumberBitwiseXor(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) return Type::None();

  lhs = NumberToInt32(lhs);
  rhs = NumberToInt32(rhs);

  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  if ((lmin >= 0 && rmin >= 0) || (lmax < 0 && rmax < 0)) {
    // Xor-ing negative or non-negative values results in a non-negative value.
    return Type::Unsigned31();
  }
  if ((lmax < 0 && rmin >= 0) || (lmin >= 0 && rmax < 0)) {
    // Xor-ing a negative and a non-negative value results in a negative value.
    // TODO(jarin) Use a range here.
    return Type::Negative32();
  }
  return Type::Signed32();
}

Type* OperationTyper::NumberShiftLeft(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) return Type::None();

  lhs = NumberToInt32(lhs);
  rhs = NumberToUint32(rhs);

  int32_t min_lhs = lhs->Min();
  int32_t max_lhs = lhs->Max();
  uint32_t min_rhs = rhs->Min();
  uint32_t max_rhs = rhs->Max();
  if (max_rhs > 31) {
    // rhs can be larger than the bitmask
    max_rhs = 31;
    min_rhs = 0;
  }

  if (max_lhs > (kMaxInt >> max_rhs) || min_lhs < (kMinInt >> max_rhs)) {
    // overflow possible
    return Type::Signed32();
  }

  double min =
      std::min(static_cast<int32_t>(static_cast<uint32_t>(min_lhs) << min_rhs),
               static_cast<int32_t>(static_cast<uint32_t>(min_lhs) << max_rhs));
  double max =
      std::max(static_cast<int32_t>(static_cast<uint32_t>(max_lhs) << min_rhs),
               static_cast<int32_t>(static_cast<uint32_t>(max_lhs) << max_rhs));

  if (max == kMaxInt && min == kMinInt) return Type::Signed32();
  return Type::Range(min, max, zone());
}

Type* OperationTyper::NumberShiftRight(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited() || !rhs->IsInhabited()) return Type::None();

  lhs = NumberToInt32(lhs);
  rhs = NumberToUint32(rhs);

  int32_t min_lhs = lhs->Min();
  int32_t max_lhs = lhs->Max();
  uint32_t min_rhs = rhs->Min();
  uint32_t max_rhs = rhs->Max();
  if (max_rhs > 31) {
    // rhs can be larger than the bitmask
    max_rhs = 31;
    min_rhs = 0;
  }
  double min = std::min(min_lhs >> min_rhs, min_lhs >> max_rhs);
  double max = std::max(max_lhs >> min_rhs, max_lhs >> max_rhs);

  if (max == kMaxInt && min == kMinInt) return Type::Signed32();
  return Type::Range(min, max, zone());
}

Type* OperationTyper::NumberShiftRightLogical(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));

  if (!lhs->IsInhabited()) return Type::None();

  lhs = NumberToUint32(lhs);

  // Logical right-shifting any value cannot make it larger.
  return Type::Range(0.0, lhs->Max(), zone());
}

Type* OperationTyper::NumberAtan2(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));
  return Type::Number();
}

Type* OperationTyper::NumberImul(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));
  // TODO(turbofan): We should be able to do better here.
  return Type::Signed32();
}

Type* OperationTyper::NumberMax(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) {
    return Type::NaN();
  }
  Type* type = Type::None();
  // TODO(turbofan): Improve minus zero handling here.
  if (lhs->Maybe(Type::NaN()) || rhs->Maybe(Type::NaN())) {
    type = Type::Union(type, Type::NaN(), zone());
  }
  lhs = Type::Intersect(lhs, Type::OrderedNumber(), zone());
  rhs = Type::Intersect(rhs, Type::OrderedNumber(), zone());
  if (lhs->Is(cache_.kInteger) && rhs->Is(cache_.kInteger)) {
    double max = std::max(lhs->Max(), rhs->Max());
    double min = std::max(lhs->Min(), rhs->Min());
    type = Type::Union(type, Type::Range(min, max, zone()), zone());
  } else {
    type = Type::Union(type, Type::Union(lhs, rhs, zone()), zone());
  }
  return type;
}

Type* OperationTyper::NumberMin(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) {
    return Type::NaN();
  }
  Type* type = Type::None();
  // TODO(turbofan): Improve minus zero handling here.
  if (lhs->Maybe(Type::NaN()) || rhs->Maybe(Type::NaN())) {
    type = Type::Union(type, Type::NaN(), zone());
  }
  lhs = Type::Intersect(lhs, Type::OrderedNumber(), zone());
  rhs = Type::Intersect(rhs, Type::OrderedNumber(), zone());
  if (lhs->Is(cache_.kInteger) && rhs->Is(cache_.kInteger)) {
    double max = std::min(lhs->Max(), rhs->Max());
    double min = std::min(lhs->Min(), rhs->Min());
    type = Type::Union(type, Type::Range(min, max, zone()), zone());
  } else {
    type = Type::Union(type, Type::Union(lhs, rhs, zone()), zone());
  }
  return type;
}

Type* OperationTyper::NumberPow(Type* lhs, Type* rhs) {
  DCHECK(lhs->Is(Type::Number()));
  DCHECK(rhs->Is(Type::Number()));
  // TODO(turbofan): We should be able to do better here.
  return Type::Number();
}

#define SPECULATIVE_NUMBER_BINOP(Name)                                     \
  Type* OperationTyper::Speculative##Name(Type* lhs, Type* rhs) {          \
    lhs = ToNumber(Type::Intersect(lhs, Type::NumberOrOddball(), zone())); \
    rhs = ToNumber(Type::Intersect(rhs, Type::NumberOrOddball(), zone())); \
    return Name(lhs, rhs);                                                 \
  }
SPECULATIVE_NUMBER_BINOP(NumberAdd)
SPECULATIVE_NUMBER_BINOP(NumberSubtract)
SPECULATIVE_NUMBER_BINOP(NumberMultiply)
SPECULATIVE_NUMBER_BINOP(NumberDivide)
SPECULATIVE_NUMBER_BINOP(NumberModulus)
SPECULATIVE_NUMBER_BINOP(NumberBitwiseOr)
SPECULATIVE_NUMBER_BINOP(NumberBitwiseAnd)
SPECULATIVE_NUMBER_BINOP(NumberBitwiseXor)
SPECULATIVE_NUMBER_BINOP(NumberShiftLeft)
SPECULATIVE_NUMBER_BINOP(NumberShiftRight)
SPECULATIVE_NUMBER_BINOP(NumberShiftRightLogical)
#undef SPECULATIVE_NUMBER_BINOP

Type* OperationTyper::ToPrimitive(Type* type) {
  if (type->Is(Type::Primitive()) && !type->Maybe(Type::Receiver())) {
    return type;
  }
  return Type::Primitive();
}

Type* OperationTyper::Invert(Type* type) {
  DCHECK(type->Is(Type::Boolean()));
  DCHECK(type->IsInhabited());
  if (type->Is(singleton_false())) return singleton_true();
  if (type->Is(singleton_true())) return singleton_false();
  return type;
}

OperationTyper::ComparisonOutcome OperationTyper::Invert(
    ComparisonOutcome outcome) {
  ComparisonOutcome result(0);
  if ((outcome & kComparisonUndefined) != 0) result |= kComparisonUndefined;
  if ((outcome & kComparisonTrue) != 0) result |= kComparisonFalse;
  if ((outcome & kComparisonFalse) != 0) result |= kComparisonTrue;
  return result;
}

Type* OperationTyper::FalsifyUndefined(ComparisonOutcome outcome) {
  if ((outcome & kComparisonFalse) != 0 ||
      (outcome & kComparisonUndefined) != 0) {
    return (outcome & kComparisonTrue) != 0 ? Type::Boolean()
                                            : singleton_false();
  }
  // Type should be non empty, so we know it should be true.
  DCHECK((outcome & kComparisonTrue) != 0);
  return singleton_true();
}

Type* OperationTyper::TypeTypeGuard(const Operator* sigma_op, Type* input) {
  return Type::Intersect(input, TypeGuardTypeOf(sigma_op), zone());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
