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

// We don't use S2PointVectorShape,
// because it doesn't provide direct access to vector of S2Point
class S2MultiPointShape final : public S2Shape {
 public:
  ~S2MultiPointShape() final = default;

  S2Point GetCentroid() const noexcept;

  int num_edges() const final { return static_cast<int>(_impl.size()); }

  Edge edge(int edge_id) const final;

  int dimension() const final { return 0; }

  ReferencePoint GetReferencePoint() const final {
    return ReferencePoint::Contained(false);
  }

  int num_chains() const final { return static_cast<int>(_impl.size()); }

  Chain chain(int chain_id) const final { return {chain_id, 1}; }

  Edge chain_edge(int chain_id, int offset) const final;

  ChainPosition chain_position(int edge_id) const final { return {edge_id, 0}; }

  bool Decode(Decoder& decoder, uint8_t tag);

 private:
  std::vector<S2Point> _impl;
};

}  // namespace arangodb::geo
