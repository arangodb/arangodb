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

#include "Geo/S2/S2PointShape.h"
#include "Geo/Coding.h"
#include "Basics/Exceptions.h"

namespace arangodb::geo {

S2Shape::Edge S2PointShape::edge(int edge_id) const {
  TRI_ASSERT(edge_id == 0);
  return {_point, _point};
}

S2Shape::Edge S2PointShape::chain_edge(int chain_id, int offset) const {
  TRI_ASSERT(offset == 0);
  return edge(chain_id);
}

bool S2PointShape::Decode(Decoder& decoder) {
  return decodePoint(decoder, _point);
}

}  // namespace arangodb::geo
