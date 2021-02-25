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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef UTIL_GEOMETRY_S2MULTIPOLYLINE_H__
#define UTIL_GEOMETRY_S2MULTIPOLYLINE_H__

#include <s2/s2polyline.h>
#include <s2/s2region.h>
#include <vector>

/// Represents a range of multiple independent polylines
/// Added to complete the GeoJson support
class S2MultiPolyline : public S2Region {
 public:
  // Creates an empty S2Polyline that should be initialized by calling Init()
  // or Decode().
  S2MultiPolyline();

  // Convenience constructors that call Init() with the given vertices.
  explicit S2MultiPolyline(std::vector<S2Polyline>&& lines);

  // Initialize a polyline that connects the given vertices. Empty polylines are
  // allowed.  Adjacent vertices should not be identical or antipodal.  All
  // vertices should be unit length.
  void Init(std::vector<S2Polyline>&& lines);

  ~S2MultiPolyline() = default;

  size_t num_lines() const { return lines_.size(); }
  S2Polyline const& line(size_t k) const {
    assert(k < lines_.size());
    // DCHECK_LT(k, lines_.size());
    return lines_[k];
  }

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  S2MultiPolyline* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  bool Contains(S2Cell const& cell) const override { return false; }
  bool MayIntersect(S2Cell const& cell) const override;

  // Polylines do not have a Contains(S2Point) method, because "containment"
  // is not numerically well-defined except at the polyline vertices.
  bool Contains(S2Point const& p) const override { return false; }

  /*void Encode(Encoder* const encoder) const override;
  bool Decode(Decoder* const decoder) override;*/

 private:
  // Internal constructor used only by Clone() that makes a deep copy of
  // its argument.
  explicit S2MultiPolyline(S2MultiPolyline const* src);

  // We store the vertices in an array rather than a vector because we don't
  // need any STL methods, and computing the number of vertices using size()
  // would be relatively expensive (due to division by sizeof(S2Point) == 24).
  std::vector<S2Polyline> lines_;
};

#endif  // UTIL_GEOMETRY_S2POLYLINE_H__
