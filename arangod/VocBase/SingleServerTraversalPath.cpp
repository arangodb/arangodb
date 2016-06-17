////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/SingleServerTraversalPath.h"
#include "Aql/AqlValue.h"

using namespace arangodb::aql;
using namespace arangodb::traverser;

void SingleServerTraversalPath::pathToVelocyPack(arangodb::Transaction* trx,
                                                 VPackBuilder& result) {
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _path.edges) {
    auto cached = _traverser->_edges.find(it);
    // All edges are cached!!
    TRI_ASSERT(cached != _traverser->_edges.end());
    result.addExternal((*cached).second);
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _path.vertices) {
    result.add(_traverser->fetchVertexData(it).slice());
  }
  result.close();
  result.close();
}

void SingleServerTraversalPath::lastEdgeToVelocyPack(arangodb::Transaction* trx, VPackBuilder& result) {
  if (_path.edges.empty()) {
    result.add(arangodb::basics::VelocyPackHelper::NullValue());
    return;
  }
  auto cached = _traverser->_edges.find(_path.edges.back());
  // All edges are cached!!
  TRI_ASSERT(cached != _traverser->_edges.end());
  result.addExternal((*cached).second);
}

AqlValue SingleServerTraversalPath::lastVertexToAqlValue(arangodb::Transaction* trx) {
  return _traverser->fetchVertexData(_path.vertices.back());
}

