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
#include "Metrics/Scale.h"

#include <cmath>

namespace arangodb::metrics {

template<typename T>
class LogScale final : public Scale<T> {
 public:
  using Value = T;
  static constexpr ScaleType kScaleType = ScaleType::Logarithmic;

  static constexpr T getHighFromSmallestBucket(T smallestBucketSize, T base,
                                               T low, size_t n) {
    return static_cast<T>((smallestBucketSize - low) * std::pow(base, n - 1) +
                          low);
  }
  struct SupplySmallestBucket {};
  static constexpr auto kSupplySmallestBucket = SupplySmallestBucket{};

  LogScale(SupplySmallestBucket, T const& base, T const& low,
           T const& smallestBucketSize, size_t n)
      : LogScale{base, low,
                 getHighFromSmallestBucket(smallestBucketSize, base, low, n),
                 n} {}

  LogScale(T const& base, T const& low, T const& high, size_t n)
      : Scale<T>{low, high, n}, _base{base} {
    TRI_ASSERT(base > T(0));
    double nn = -1.0 * (n - 1);
    for (auto& i : this->_delim) {
      i = static_cast<T>(
          static_cast<double>(high - low) *
              std::pow(static_cast<double>(base), static_cast<double>(nn++)) +
          static_cast<double>(low));
    }
    _div = this->_delim.front() - low;
    TRI_ASSERT(_div > T(0));
    _lbase = std::log(_base);
  }

  /**
   * @brief Dump to builder
   * @param b Envelope
   */
  void toVelocyPack(VPackBuilder& b) const final {
    b.add("scale-type", VPackValue("logarithmic"));
    b.add("base", VPackValue(_base));
    Scale<T>::toVelocyPack(b);
  }

  /**
   * @brief index for val
   * @param val value
   * @return    index
   */
  size_t pos(T val) const {
    return static_cast<size_t>(
        1 + std::floor(log((val - this->_low) / _div) / _lbase));
  }

  T base() const { return _base; }

 private:
  T _base;
  T _div;
  double _lbase;
};

}  // namespace arangodb::metrics
