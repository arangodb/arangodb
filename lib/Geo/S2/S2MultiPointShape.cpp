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

#include "Geo/S2/S2MultiPointShape.h"
#include "Geo/Coding.h"
#include "Basics/Exceptions.h"

namespace arangodb::geo {

S2Point S2MultiPointShape::GetCentroid() const noexcept {
  return getPointsCentroid(_impl);
}

S2Shape::Edge S2MultiPointShape::edge(int edge_id) const {
  TRI_ASSERT(0 <= edge_id);
  TRI_ASSERT(static_cast<size_t>(edge_id) <= _impl.size());
  return {_impl[static_cast<size_t>(edge_id)],
          _impl[static_cast<size_t>(edge_id)]};
}

S2Shape::Edge S2MultiPointShape::chain_edge(int chain_id, int offset) const {
  TRI_ASSERT(offset == 0);
  return edge(chain_id);
}

bool S2MultiPointShape::Decode(Decoder& decoder, uint8_t tag) {
  return decodeVectorPoint(decoder, tag, _impl);
}

}  // namespace arangodb::geo
