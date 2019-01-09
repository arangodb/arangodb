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

#include <set>
using std::multiset;
using std::set;

#include <vector>
using std::vector;

#include "S2MultiPolyline.h"

#include <s2/s2cap.h>
#include <s2/s2cell.h>
#include <s2/s2latlng.h>
#include <s2/s2latlng_rect_bounder.h>
#include <s2/s2polyline.h>
#include <s2/util/coding/coder.h>

DECLARE_bool(s2debug);  // defined in s2.cc

S2MultiPolyline::S2MultiPolyline() : lines_() {}

S2MultiPolyline::S2MultiPolyline(vector<S2Polyline>&& lines)
    : lines_(std::move(lines)) {}

void S2MultiPolyline::Init(std::vector<S2Polyline>&& lines) {
  lines_.clear();
  lines_ = std::move(lines);
}

S2MultiPolyline::S2MultiPolyline(S2MultiPolyline const* src) {
  assert(false);
  /*lines_.resize(src->lines_.size());
  std::copy(src->lines_.begin(), src->lines_.end(), lines_.begin());*/
}

S2MultiPolyline* S2MultiPolyline::Clone() const {
  return new S2MultiPolyline(this);
}

S2LatLngRect S2MultiPolyline::GetRectBound() const {
  S2LatLngRectBounder bounder;
  for (size_t k = 0; k < lines_.size(); k++) {
    S2Polyline const& line = lines_[k];
    for (int i = 0; i < line.num_vertices(); ++i) {
      bounder.AddPoint(line.vertex(i));
    }
  }
  return bounder.GetBound();
}

S2Cap S2MultiPolyline::GetCapBound() const {
  return GetRectBound().GetCapBound();
}

bool S2MultiPolyline::MayIntersect(S2Cell const& cell) const {
  for (size_t k = 0; k < lines_.size(); k++) {
    S2Polyline const& line = lines_[k];
    if (line.MayIntersect(cell)) {
      return true;
    }
  }
  return false;
}

/*
void S2MultiPolyline::Encode(Encoder* const encoder) const {
  encoder->Ensure(10);  // sufficient
  encoder->put8(kCurrentEncodingVersionNumber);
  encoder->put32(lines_.size());
  for (size_t k = 0; k < lines_.size(); k++) {
    S2Polyline const& line = lines_[k];
    line.Encode(encoder);
  }
  assert(encoder->avail() >= 0);
}

bool S2MultiPolyline::Decode(Decoder* const decoder) {
  unsigned char version = decoder->get8();
  if (version > kCurrentEncodingVersionNumber) return false;

  lines_.clear();
  lines_.resize(decoder->get32());
  for (size_t k = 0; k < lines_.size(); k++) {
    S2Polyline& line = lines_[k];
    if (!line.Decode(decoder)) {
      return false;
    }
  }
  return decoder->avail() >= 0;
}
*/
