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

#include <s2/s2shape.h>
#include <s2/s2point.h>

#include <exception>
#include <vector>

namespace arangodb::geo {

class S2MultiPolylineShape final : public S2Shape {
 public:
  ~S2MultiPolylineShape() final = default;

  S2Point GetCentroid() const noexcept;

  int num_edges() const final;

  Edge edge(int edge_id) const final;

  int dimension() const final { return 1; }

  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }

  int num_chains() const final;

  Chain chain(int chain_id) const final;

  Edge chain_edge(int chain_id, int offset) const final;

  ChainPosition chain_position(int edge_id) const final;

  bool Decode(Decoder& decoder);

 private:
  int num_polylines_{0};
  mutable int prev_polyline_{0};
  int num_vertices_{0};
  std::unique_ptr<S2Point[]> vertices_;
  std::unique_ptr<uint32_t[]> polylines_starts_;
};

}  // namespace arangodb::geo
