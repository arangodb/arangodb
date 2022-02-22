// Copyright 2003 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


// Basic physical unit classes.  These classes are not as fancy as
// some general purpose physical unit libraries, but provide a
// simple and efficient interface for unit tracking and conversion.
// In particular, compound units cannot be automatically constructed
// from or decomposed into simpler units (e.g. velocity = distance /
// time), but can be defined explicitly as opaque types.
//
// These classes define overloaded operators and non-explicit single
// argument ctors, which breaks the style guidelines, but an exception
// has been allowed in this case.
// - Douglas Greiman <dgreiman@google.com>
//
// Specific types of physical units are defined in other headers
// (e.g. angle-units.h).  Each unit type can be specialized to either
// float or double.  Non-floating-point types are disallowed, since
// the implicit conversion logic generally fails for integer division.
// Attempting to declare integer-based units results in a fairly
// informative compiler error.
//
// All units share common functionality, as demonstrated in this
// example:
//
//   #include "angle-units.h"
//
//   class Nomad {
//     Radians latitude_;
//     ...
//     // With -O2 optimization, the use of Radians in this method is
//     // completely equivalent to using float, but prevents unitless
//     // angles from being passed.
//     void SetLatitude(Radians angle) {
//       S2_CHECK(angle.abs() < Degrees(90.0));
//       latitude_ = angle;
//       latitude_radius_ = EARTH_RADIUS * cos(angle.value());
//     }
//
//     // This method contains an implicit unit conversion from degrees
//     // to radians.  In practice it would make more sense to use
//     // Radians as the argument type to avoid this extra work if
//     // possible (see the two calls of this method below).
//     void MoveNorth(Degrees angle) {
//       SetLatitude(latitude_ + angle);
//     }
//   };
//
//   void do_tests(float degrees_to_move, float radians_to_move) {
//     Nomad joe;
//
//     // The use of Degrees(30.0) to set the latitude in radians requires
//     // no runtime conversion.
//     joe.SetLatitude(Degrees(30.0));
//
//     // The Degrees(...) parameter will be converted to radians at
//     // runtime prior to addition in Nomad::MoveNorth().
//     joe.MoveNorth(Degrees(degrees_to_move));
//
//     // This is ok, but due to the poor choice of units for the MoveNorth
//     // method's argument, incurs two pointless multiply operations to
//     // convert from radians to degrees and back to radians.
//     joe.MoveNorth(Radians(radians_to_move));
//
//     // Implicit conversions from unitless values generate errors at
//     // compile time.
//     // joe.MoveNorth(degrees_to_move); // compile ERROR!
//   }
//

#ifndef S2_UTIL_UNITS_PHYSICAL_UNITS_H_
#define S2_UTIL_UNITS_PHYSICAL_UNITS_H_

#include <cmath>
#include <iosfwd>
#include <iostream>
#include <string>
#include <type_traits>

#include "s2/base/integral_types.h"
#include "s2/third_party/absl/base/macros.h"

namespace util {

namespace units {

// Static conversion scale and offset to convert from a standard base
// unit to a specific unit.  The scale and offset is specified as a
// rational number to allow static construction at compile time.
template <int ScaleNumerator, int ScaleDenominator,
          int OffsetNumerator = 0, int OffsetDenominator = 1>
struct UnitConversion {
  static const int SCALE_NUMERATOR = ScaleNumerator;
  static const int SCALE_DENOMINATOR = ScaleDenominator;
  static const int OFFSET_NUMERATOR = OffsetNumerator;
  static const int OFFSET_DENOMINATOR = OffsetDenominator;
};

template <class FromUnit, class ToUnit, typename Float>
struct UnitConverter {
  // Linear unit conversion: A' = A * s + t, where
  // - s is a static scale factor and
  // - t is a static offset
  // composed from two UnitConversion structs.
  // Cast one multiplicand to 64 bit to ensure that the integer expression
  // is computed in 64 bit. Otherwise Feet(Miles(x)) will overflow.
  constexpr static inline Float Convert(Float value) {
    // scaling and offset
    return static_cast<Float>(
      (static_cast<double>(value *
         (static_cast<double>(static_cast<uint64>(ToUnit::SCALE_NUMERATOR) *
                              FromUnit::SCALE_DENOMINATOR) /
          static_cast<double>(static_cast<uint64>(ToUnit::SCALE_DENOMINATOR) *
                              FromUnit::SCALE_NUMERATOR)))) -
      (static_cast<double>(static_cast<uint64>(ToUnit::SCALE_NUMERATOR) *
                           FromUnit::SCALE_DENOMINATOR *
                           FromUnit::OFFSET_NUMERATOR) /
       static_cast<double>(static_cast<uint64>(ToUnit::SCALE_DENOMINATOR) *
                           FromUnit::SCALE_NUMERATOR *
                           FromUnit::OFFSET_DENOMINATOR)) +
      (static_cast<double>(ToUnit::OFFSET_NUMERATOR) /
       static_cast<double>(ToUnit::OFFSET_DENOMINATOR)));
  }
};

// Some unit operations are only defined for base units that have linear
// transformations, as in T(a+b) = T(a) + T(b).   Temperatures units are
// an example of units that do not have linear transformations.  By
// default unit transformations are assumed to be linear; see
// temperature-units.h for an example of how to override this default.
template <class Base>
struct is_linear_unit_transformation : std::true_type { };

// Template class holding a single value with an associated physical
// unit.  The unit and conversion parameters are statically defined
// and optimized at compile time.  With optimization (-O2), use of a
// single physical unit type is as efficient as using a native
// floating point type.  Conversions between units are optimized to
// (typically) a single multiplication operation.  Unit conversions
// for constants are done at compile time and incur no runtime
// overhead (again at -O2).
//
// Template parameters:
//   Base is the base unit class, such as Angle or Length. If operator<<
//   is used, it must have:
//   - a public static field "output_suffix" and
//   - a public static method Float Base::GetInBaseUnit(PhysicalUnit). An
//     example can be found in length-units.h
//     LengthBase::GetInBaseUnit.
//   Unit is the UnitConversion class that defines a specific unit
//   (such as degrees) in terms of a reference unit (such as radians).
template <class Float, class Base, class Unit>
class PhysicalUnit {
 public:
  typedef PhysicalUnit<Float, Base, Unit> Type;
  typedef Float FloatType;

  // Use 'explicit' to prevent unintentional construction from untyped (or
  // mistyped) values.  Note that this also prevents arguably reasonable
  // constructs such as Unit unit = 10.0; use either Unit unit(10.0) or
  // Unit unit =  Unit(10.0) instead.
  PhysicalUnit(): value_(static_cast<Float>(0.0)) {}
  constexpr explicit PhysicalUnit(Float value): value_(value) {}

  // Conversion from other units of the same Base type.
  //
  // Policy decision: not using 'explicit' allows much more natural
  // conversions between units of a given base type.  This can result in
  // unintended implicit type conversions, but these incur very little
  // overhead (inlined multiply at -O2) and should be inconsequential in
  // most circumstances.  Casts between different base types (including
  // different underlying value types) require explicit handling. The ClangTidy
  // warnings regarding this are therefore suppressed with NOLINT below.
  template <class Unit2>
  constexpr PhysicalUnit(PhysicalUnit<Float, Base, Unit2> other)  // NOLINT
      : value_(UnitConverter<Unit2, Unit, Float>::Convert(other.value())) {}

  // Copy operation from other units of the same Base type.
  template <class Unit2>
  Type operator = (PhysicalUnit<Float, Base, Unit2> other) {
    value_ = UnitConverter<Unit2, Unit, Float>::Convert(other.value());
    return *this;
  }

  // Value accessor.  Consider using an explicitly typed accessor whenever
  // the unit type is not immediately obvious (such as function return
  // values).  For example:
  //   float x = myclass.GetAngle().value();           // what unit is x?
  //   float x = GetDegrees(myclass.GetAngle());       // much clearer.
  //   float x = Degrees(myclass.GetAngle()).value();  // ok too.
  //   Degrees degrees = myclass.GetAngle();           // using a temporary is
  //   float x = degrees.value();                      // also good.
  constexpr Float value() const { return value_; }

  // Trivial arithematic operator wrapping.
  Type operator - () const {
    return Type(-value_);
  }
  Type operator * (const Float scale) const {
    return Type(value_ * scale);
  }
  Type operator + (const Type other) const {
    static_assert(is_linear_unit_transformation<Base>::value,
                  "operation not defined");
    return Type(value_ + other.value());
  }
  Type operator - (const Type other) const {
    static_assert(is_linear_unit_transformation<Base>::value,
                  "operation not defined");
    return Type(value_ - other.value());
  }
  Float operator / (const Type other) const {
    return value_ / other.value();
  }
  Type operator *= (const Float scale) {
    value_ *= scale;
    return *this;
  }
  Type operator += (const Type other) {
    static_assert(is_linear_unit_transformation<Base>::value,
                  "operation not defined");
    value_ += other.value();
    return *this;
  }
  Type operator -= (const Type other) {
    static_assert(is_linear_unit_transformation<Base>::value,
                  "operation not defined");
    value_ -= other.value();
    return *this;
  }

  // Simple comparisons.  Overloaded equality is intentionally omitted;
  // use equals() instead.
  bool operator < (const Type other) const {
    return value_ < other.value();
  }
  bool operator > (const Type other) const {
    return value_ > other.value();
  }
  bool operator <= (const Type other) const {
    return value_ <= other.value();
  }
  bool operator >= (const Type other) const {
    return value_ >= other.value();
  }

  // Test equality to within some epsilon.  Always false for
  // epsilon < 0.
  bool equals(const Type other,
              const Type epsilon) const {
    Float delta = value_ - other.value_;
    if (delta < static_cast<Float>(0.0)) {
       return -delta <= epsilon.value_;
    }
    return delta <= epsilon.value_;
  }

  // A little more sugar.
  Type abs() const {
    return (value_ < static_cast<Float>(0.0)) ? -(*this) : *this;
  }

  // Explicit precision casting within a base unit class.
  template <typename OtherFloat>
  PhysicalUnit<OtherFloat, Base, Unit> precision_cast() const {
    typedef PhysicalUnit<OtherFloat, Base, Unit> CastType;
    return CastType(static_cast<OtherFloat>(value_));
  }

 private:
  Float value_;
  // Enforce Float to be a floating point type, since unit conversions will
  // generally fail with integers.
  static_assert(std::is_floating_point<Float>::value,
                "Only_use_floating_point_types");
};

// Allow 2*u (in addition to u*2).
template <typename Scale, typename Float, class Base, class Unit>
inline PhysicalUnit<Float, Base, Unit> operator * (
         const Scale scale,
         const PhysicalUnit<Float, Base, Unit> value) {
  return value * static_cast<Float>(scale);
}

// we'll print the current value and the value converted to the natural
// base type
template <typename Float, typename Base, typename Unit>
std::ostream& operator<<(std::ostream& os,
                         PhysicalUnit<Float, Base, Unit> value) {
  return os << value.value()
            << " (" << Base::GetInBaseUnit(value)
            << Base::output_suffix << ")";
}

} // end namespace units

} // end namespace util

#endif  // S2_UTIL_UNITS_PHYSICAL_UNITS_H_
