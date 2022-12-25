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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <s2/s2region.h>
#include <s2/s2point.h>

#include <exception>
#include <vector>

namespace arangodb::geo {

class S2Points final : public S2Region {
 public:
  ~S2Points() final = default;

  // The result is not unit length, so you may want to normalize it.
  S2Point GetCentroid() const noexcept;

  template<typename Region>
  bool Intersects(Region const& other) const noexcept {
    for (auto const& point : _impl) {
      if (other.Contains(point)) {
        return true;
      }
    }
    return false;
  }

  S2Region* Clone() const final;
  S2Cap GetCapBound() const final;
  S2LatLngRect GetRectBound() const final;
  bool Contains(const S2Cell& cell) const final;
  bool MayIntersect(const S2Cell& cell) const final;
  bool Contains(const S2Point& p) const final;

  auto& Impl() noexcept { return _impl; }
  auto const& Impl() const noexcept { return _impl; }

 private:
  std::vector<S2Point> _impl;
};

}  // namespace arangodb::geo
