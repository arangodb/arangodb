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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Geo/S2/S2MultiPolylineShape.h"
#include "Assertions/Assert.h"

#include <s2/encoded_s2point_vector.h>
#include <s2/s2shape_measures.h>
#include <s2/s2polyline_measures.h>

namespace arangodb::geo {

S2Point S2MultiPolylineShape::GetCentroid() const noexcept {
  S2Point centroid;
  int num_chains = this->num_chains();
  TRI_ASSERT(num_chains > 0);
  for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
    S2Shape::Chain chain = this->chain(chain_id);
    centroid += S2::GetCentroid(
        S2PointSpan{vertices_.get() + static_cast<size_t>(chain.start),
                    static_cast<size_t>(chain.length + 1)});
  }
  TRI_ASSERT(centroid == S2::GetCentroid(*this));
  return centroid;
}

int S2MultiPolylineShape::num_edges() const {
  return num_vertices_ - num_polylines_;
}

S2Shape::Edge S2MultiPolylineShape::edge(int edge_id) const {
  auto const pos = chain_position(edge_id);
  return chain_edge(pos.chain_id, pos.offset);
}

int S2MultiPolylineShape::num_chains() const { return num_polylines_; }

S2Shape::Chain S2MultiPolylineShape::chain(int chain_id) const {
  int const start =
      static_cast<int>(polylines_starts_[static_cast<size_t>(chain_id)]);
  int const last =
      (chain_id + 1 == num_polylines_
           ? num_edges()
           : static_cast<int>(
                 polylines_starts_[static_cast<size_t>(chain_id + 1)]));
  return {start, last - start};
}

S2Shape::Edge S2MultiPolylineShape::chain_edge(int chain_id, int offset) const {
  int const start =
      static_cast<int>(polylines_starts_[static_cast<size_t>(chain_id)]);
  return {vertices_[static_cast<size_t>(start + offset)],
          vertices_[static_cast<size_t>(start + offset + 1)]};
}

S2Shape::ChainPosition S2MultiPolylineShape::chain_position(int edge_id) const {
  if (num_polylines_ == 1) {
    return {0, edge_id};
  }
  const auto* start = polylines_starts_.get() + prev_polyline_;
  if (!(edge_id >= static_cast<int>(start[0]) &&
        edge_id < static_cast<int>(start[1]))) {
    if (edge_id == static_cast<int>(start[1])) {
      // This is the edge immediately following the previous loop.
      do {
        ++start;
      } while (edge_id == static_cast<int>(start[1]));
    } else {
      start = polylines_starts_.get();
      static constexpr int kMaxLinearSearchPolylines = 12;
      if (num_polylines_ <= kMaxLinearSearchPolylines) {
        while (static_cast<int>(start[1]) <= edge_id) {
          ++start;
        }
      } else {
        start =
            std::upper_bound(start + 1, start + num_polylines_, edge_id) - 1;
      }
    }
    prev_polyline_ = static_cast<int>(start - polylines_starts_.get());
  }
  return {static_cast<int>(start - polylines_starts_.get()),
          edge_id - static_cast<int>(start[0])};
}

bool S2MultiPolylineShape::Decode(Decoder& decoder) {
  uint32 num_polylines = 0;
  if (!decoder.get_varint32(&num_polylines)) {
    return false;
  }
  num_polylines_ = static_cast<int>(num_polylines);
  s2coding::EncodedS2PointVector vertices;
  if (!vertices.Init(&decoder)) {
    return false;
  }
  if (num_polylines_ == 0) {
    num_vertices_ = 0;
    vertices_.reset();
    return true;
  }
  auto size = vertices.size();
  vertices_ = std::make_unique<S2Point[]>(size);
  for (size_t i = 0; i != size; ++i) {
    vertices_[i] = vertices[static_cast<int>(i)];
  }
  num_vertices_ = static_cast<int>(size);
  if (num_polylines_ > 1) {
    s2coding::EncodedUintVector<uint32> loop_starts;
    if (!loop_starts.Init(&decoder)) {
      return false;
    }
    size = loop_starts.size();
    polylines_starts_ = std::make_unique<uint32[]>(size);
    for (size_t i = 0; i != size; ++i) {
      polylines_starts_[i] = loop_starts[static_cast<int>(i)];
    }
  }
  return true;
}

}  // namespace arangodb::geo
