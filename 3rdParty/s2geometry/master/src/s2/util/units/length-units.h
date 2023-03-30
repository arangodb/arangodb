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


// Define common length units.  New length units can be added as needed
// provided there is a direct, multiplicative scaling to meters.
//
// When adding new units, you should add an appropriate GetMyUnit accessor
// template and (optionally) define a shorthand typedef specialization for
// float.  Also update the physical units unittest to check the conversion
// ratio.

#ifndef S2_UTIL_UNITS_LENGTH_UNITS_H_
#define S2_UTIL_UNITS_LENGTH_UNITS_H_

#include "s2/util/units/physical-units.h"

namespace util {

namespace units {

struct LengthBase {
  // the base unit type is meters
  static const char* const output_suffix;

  template <typename ValueType, class Unit>
  static ValueType GetInBaseUnit(
      const PhysicalUnit<ValueType, LengthBase, Unit> u) {
    return GetMeters(u);
  }
};

template <typename Float>
class Length {
  typedef UnitConversion<1, 1> MetersConversion;
  typedef UnitConversion<1, 1000> KilometersConversion;
  typedef UnitConversion<10000, 254> InchesConversion;
  typedef UnitConversion<10000, 3048> FeetConversion;
  typedef UnitConversion<1000, 1609344> MilesConversion;
  typedef UnitConversion<1000, 1> MillimetersConversion;
  typedef UnitConversion<1, 1852> NauticalMilesConversion;
  typedef UnitConversion<10000, 9144> YardsConversion;

 public:
  typedef PhysicalUnit<Float, LengthBase, MetersConversion> Meters;
  typedef PhysicalUnit<Float, LengthBase, KilometersConversion> Kilometers;
  typedef PhysicalUnit<Float, LengthBase, InchesConversion> Inches;
  typedef PhysicalUnit<Float, LengthBase, FeetConversion> Feet;
  typedef PhysicalUnit<Float, LengthBase, MilesConversion> Miles;
  typedef PhysicalUnit<Float, LengthBase, MillimetersConversion> Millimeters;
  typedef PhysicalUnit<Float, LengthBase, NauticalMilesConversion>
      NauticalMiles;
  typedef PhysicalUnit<Float, LengthBase, YardsConversion> Yards;
};

// Define some shorthand, standard typenames.  The standard length
// type is chosen to be float.  If you need greater precision for
// a specific application, either use the fully qualified typename
// Length<double>::* or declare local typedefs.  This would be an
// ideal place for a template typedef, if only C++ supported them.
// Note that units with different floating-point types do not
// support implicit conversion (use the precision_cast<...> method).
typedef Length<float>::Meters Meters;
typedef Length<float>::Kilometers Kilometers;
typedef Length<float>::Inches Inches;
typedef Length<float>::Feet Feet;
typedef Length<float>::Miles Miles;
typedef Length<float>::Millimeters Millimeters;
typedef Length<float>::NauticalMiles NauticalMiles;
typedef Length<float>::Yards Yards;

// Explicit unit accessors.  In general these are safer than using
// the value() accessor, particularly in cases where the unit type
// may not be immediately obvious (such as function returns).

template <typename ValueType, class Unit>
inline ValueType GetMeters(const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Meters(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetKilometers(
    const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Kilometers(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetInches(const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Inches(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetFeet(const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Feet(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetMiles(const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Miles(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetMillimeters(
    const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Millimeters(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetNauticalMiles(
    const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::NauticalMiles(u).value();
}

template <typename ValueType, class Unit>
inline ValueType GetYards(const PhysicalUnit<ValueType, LengthBase, Unit> u) {
  return typename Length<ValueType>::Yards(u).value();
}

}  // end namespace units

}  // end namespace util

#endif  // S2_UTIL_UNITS_LENGTH_UNITS_H_
