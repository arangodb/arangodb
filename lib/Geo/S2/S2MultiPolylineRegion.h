////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Geo/Coding.h"

#include <s2/s2region.h>
#include <s2/s2polyline.h>

#include <vector>

namespace arangodb::geo {
namespace rect {

bool intersects(S2LatLngRect const& rect, S2Polyline const& polyline);

}  // namespace rect

class S2MultiPolylineRegion final : public S2Region {
 public:
  ~S2MultiPolylineRegion() final = default;

  // The result is not unit length, so you may want to normalize it.
  S2Point GetCentroid() const noexcept;

  template<typename Region>
  bool Intersects(Region const& other) const noexcept {
    for (auto const& line : _impl) {
      if constexpr (std::is_same_v<Region, S2LatLngRect>) {
        if (rect::intersects(other, line)) {
          return true;
        }
      } else if (other.Intersects(line)) {
        return true;
      }
    }
    return false;
  }

  S2Region* Clone() const final;
  S2Cap GetCapBound() const final;
  S2LatLngRect GetRectBound() const final;
  bool Contains(S2Cell const& cell) const final;
  bool MayIntersect(S2Cell const& cell) const final;
  bool Contains(S2Point const& p) const final;

  void Encode(Encoder& encoder, coding::Options options) const;
  bool Decode(Decoder& decoder, uint8_t tag, std::vector<S2Point>& cache);

  auto& Impl() noexcept { return _impl; }
  auto const& Impl() const noexcept { return _impl; }

 private:
  std::vector<S2Polyline> _impl;
};

}  // namespace arangodb::geo
