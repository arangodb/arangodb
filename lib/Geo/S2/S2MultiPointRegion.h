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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef __clang__
#pragma clang diagnostic push
// Suppress the warning
//   3rdParty\s2geometry\dfefe0c\src\s2/util/math/vector3_hash.h(32,24): error :
//   'is_pod<double>' is deprecated: warning STL4025: std::is_pod and
//   std::is_pod_v are deprecated in C++20. The std::is_trivially_copyable
//   and/or std::is_standard_layout traits likely suit your use case. You can
//   define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING or
//   _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS to acknowledge that you have
//   received this warning. [-Werror,-Wdeprecated-declarations]
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// Suppress the warning
//   3rdParty\s2geometry\dfefe0c\src\s2/base/logging.h(82,21): error :
//   private field 'severity_' is not used [-Werror,-Wunused-private-field]
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#include <s2/s2region.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
