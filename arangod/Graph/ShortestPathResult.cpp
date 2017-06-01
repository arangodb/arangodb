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

#include "Graph/ShortestPathResult.h"

#include "Aql/AqlValue.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::transaction;
using namespace arangodb::traverser;

ShortestPathResult::ShortestPathResult() : _readDocuments(0) {}

ShortestPathResult::~ShortestPathResult() {}

/// @brief Clears the path
void ShortestPathResult::clear() {
  _vertices.clear();
  _edges.clear();
}

AqlValue ShortestPathResult::edgeToAqlValue(TraverserCache* cache, size_t position) const {
  if (position == 0) {
    // First Edge is defined as NULL
    return AqlValue(arangodb::basics::VelocyPackHelper::NullValue());
  }
  TRI_ASSERT(position - 1 < _edges.size());
  return cache->fetchAqlResult(_edges[position - 1].get());
}

AqlValue ShortestPathResult::vertexToAqlValue(TraverserCache* cache, size_t position) const {
  TRI_ASSERT(position < _vertices.size());
  return cache->fetchAqlResult(_vertices[position]);
}
