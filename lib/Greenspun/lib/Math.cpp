////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>
#include <cmath>

#include "Greenspun/Extractor.h"
#include "Math.h"

namespace arangodb::greenspun {

template <typename F>
auto Math_applySingle(F&& f) {
  return [f = std::forward<F>(f)](Machine& ctx, VPackSlice const paramsList,
                                  VPackBuilder& result) -> EvalResult {

    auto res = extract<double>(paramsList);
    if (res.fail()) {
      return res.error();
    }

    auto&& [value] = res.value();
    result.add(VPackValue(f(value)));
    return {};
  };
}

template <typename F>
auto Math_applyTwo(F&& f) {
  return [f = std::forward<F>(f)](Machine& ctx, VPackSlice const paramsList,
                                  VPackBuilder& result) -> EvalResult {

    auto res = extract<double, double>(paramsList);
    if (res.fail()) {
      return res.error();
    }

    auto&& [a, b] = res.value();
    result.add(VPackValue(f(a, b)));
    return {};
  };
}

#define SET_MATH_SINGLE_FUNC(name) ctx.setFunction(#name, Math_applySingle([](double x) { return std:: name(x); }))
#define SET_MATH_DOUBLE_FUNC(name) ctx.setFunction(#name, Math_applyTwo([](double a, double b) { return std:: name(a, b); }))

void RegisterAllMathFunctions(Machine& ctx) {
  SET_MATH_SINGLE_FUNC(abs);
  SET_MATH_DOUBLE_FUNC(fmod);

  SET_MATH_SINGLE_FUNC(exp);
  SET_MATH_SINGLE_FUNC(expm1);
  SET_MATH_SINGLE_FUNC(exp2);
  SET_MATH_SINGLE_FUNC(log);
  SET_MATH_SINGLE_FUNC(log10);
  SET_MATH_SINGLE_FUNC(log2);
  SET_MATH_SINGLE_FUNC(log1p);

  SET_MATH_DOUBLE_FUNC(pow);
  SET_MATH_SINGLE_FUNC(sqrt);
  SET_MATH_SINGLE_FUNC(cbrt);
  SET_MATH_DOUBLE_FUNC(hypot);


  SET_MATH_SINGLE_FUNC(sin);
  SET_MATH_SINGLE_FUNC(cos);
  SET_MATH_SINGLE_FUNC(tan);
  SET_MATH_SINGLE_FUNC(asin);
  SET_MATH_SINGLE_FUNC(acos);
  SET_MATH_SINGLE_FUNC(atan);
  SET_MATH_DOUBLE_FUNC(atan2);

  SET_MATH_SINGLE_FUNC(sinh);
  SET_MATH_SINGLE_FUNC(cosh);
  SET_MATH_SINGLE_FUNC(tanh);
  SET_MATH_SINGLE_FUNC(asinh);
  SET_MATH_SINGLE_FUNC(acosh);
  SET_MATH_SINGLE_FUNC(atanh);

  SET_MATH_SINGLE_FUNC(ceil);
  SET_MATH_SINGLE_FUNC(floor);
  SET_MATH_SINGLE_FUNC(trunc);
  SET_MATH_SINGLE_FUNC(round);
}

}  // namespace arangodb::greenspun
