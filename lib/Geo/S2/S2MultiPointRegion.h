////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef UTIL_GEOMETRY_S2MULTIPOINTREGION_H__
#define UTIL_GEOMETRY_S2MULTIPOINTREGION_H__

#include <s2/s2region.h>

/// An S2MultiPointRegion is a region that contains a one or more points.
/// Added to complete the GeoJson support
class S2MultiPointRegion final : public S2Region {
 public:
  // Create a region containing the given points, will take ownership
  explicit S2MultiPointRegion(std::vector<S2Point>* points);

  ~S2MultiPointRegion() { delete[] points_; }

  int num_points() const { return num_points_; }
  S2Point const& point(int k) const { return points_[k]; }

  ////////////////////////////////////////////////////////////////////////
  // S2Region interface (see s2region.h for details):

  virtual S2MultiPointRegion* Clone() const override;
  S2Cap GetCapBound() const override;
  S2LatLngRect GetRectBound() const override;
  bool Contains(S2Cell const& cell) const override { return false; }
  bool MayIntersect(S2Cell const& cell) const override;
  bool Contains(S2Point const& p) const override;
  // void Encode(Encoder* const encoder) const override;
  // bool Decode(Decoder* const decoder) override;

 private:
  // private constructor for clone
  explicit S2MultiPointRegion(S2MultiPointRegion const*);

  int num_points_;
  S2Point* points_;
};

#endif  // UTIL_GEOMETRY_S2POINTREGION_H__
