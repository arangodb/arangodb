////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/debugging.h"
#include "Metrics/Metric.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace arangodb::metrics {

template<typename T>
class Scale {
 public:
  using Value = T;

  Scale(T const& low, T const& high, size_t n) : _n{n}, _low{low}, _high{high} {
    TRI_ASSERT(n > 1);
    _delim.resize(n - 1);
  }

  virtual ~Scale() = default;

  /**
   * @brief number of buckets
   */
  [[nodiscard]] size_t n() const noexcept { return _n; }
  [[nodiscard]] T low() const { return _low; }
  [[nodiscard]] T high() const { return _high; }
  [[nodiscard]] std::string delim(size_t s) const {
    return (s < _n - 1) ? std::to_string(_delim[s]) : "+Inf";
  }

  [[nodiscard]] std::vector<T> const& delims() const { return _delim; }

  /**
   * @brief dump to builder
   */
  virtual void toVelocyPack(VPackBuilder& b) const {
    TRI_ASSERT(b.isOpenObject());
    b.add("lower-limit", VPackValue(_low));
    b.add("upper-limit", VPackValue(_high));
    b.add("value-type", VPackValue(typeid(T).name()));
    b.add(VPackValue("range"));
    VPackArrayBuilder abb(&b);
    for (auto const& i : _delim) {
      b.add(VPackValue(i));
    }
  }

  /**
   * @brief dump to std::ostream
   */
  std::ostream& print(std::ostream& output) const {
    VPackBuilder b;
    {
      VPackObjectBuilder bb(&b);
      toVelocyPack(b);
    }
    return output << b.toJson();
  }

 protected:
  std::vector<T> _delim;
  size_t _n;
  T _low;
  T _high;
};

template<typename T>
std::ostream& operator<<(std::ostream& o, Scale<T> const& s) {
  return s.print(o);
}

enum class ScaleType {
  Fixed,
  Linear,
  Logarithmic,
};

}  // namespace arangodb::metrics
