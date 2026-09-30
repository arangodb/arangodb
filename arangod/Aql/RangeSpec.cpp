////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Range.h"
#include "Aql/RangeSpec.h"

#include <cmath>
#include <limits>
#include <algorithm>

namespace arangodb::aql::functions {

std::optional<RangeSpec> makeRangeSpec(double from, double to, double step) {
  if (!std::isfinite(from) || !std::isfinite(to) || !std::isfinite(step) ||
      step == 0.0 || (from < to && step < 0.0) || (from > to && step > 0.0)) {
    return std::nullopt;
  }

  // epsilon is the smallest representable gap between 1.0 and the next double;
  // times the operand, which gives one ulp (the gap on that scale);
  // `from`, `to`, and `step` are possible to contain 0.5 ulp empirically;
  // and the subtract and divide add 0.5 each, which can land ~2.5 ulp;
  // therefore, 4 * ulp can absorb the floating point errors.
  double const rawTol =
      4 * std::numeric_limits<double>::epsilon() *
      std::max({std::abs(from), std::abs(to), std::abs(step)});
  // Sometimes, the calculated tol (`rawTol`) is larger than `step`, which
  // produces extra elements; therefore, we cap the `tol` by `step` here.
  double const tol = std::copysign(std::min(rawTol, std::abs(step) / 2), step);

  // if `count` is an integer, it can be infinite -> UB; so we use double here
  double const count = std::floor((to - from + tol) / step) + 1.0;
  uint64_t const n = (count <= static_cast<double>(Range::MaterializationLimit))
                         ? static_cast<uint64_t>(count)
                         : Range::MaterializationLimit + 1;

  return RangeSpec{from, step, n};
}

}  // namespace arangodb::aql::functions
