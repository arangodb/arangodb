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

// We want something like S2PointRegion but for S2Shape
class S2PointShape final : public S2Shape {
 public:
  ~S2PointShape() final = default;

  void Reset(const S2Point& other) { _point = other; }

  // The result is unit length!
  S2Point GetCentroid() const noexcept { return _point; }

  int num_edges() const final { return 1; }

  Edge edge(int edge_id) const final;

  int dimension() const final { return 0; }

  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }

  int num_chains() const final { return 1; }

  Chain chain(int chain_id) const final { return {chain_id, 1}; }

  Edge chain_edge(int chain_id, int offset) const final;

  ChainPosition chain_position(int edge_id) const final { return {edge_id, 0}; }

  bool Decode(Decoder& decoder);

 private:
  S2Point _point;
};

}  // namespace arangodb::geo
