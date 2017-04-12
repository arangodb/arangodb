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

#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::transaction;

ShortestPathResult::ShortestPathResult() : _readDocuments(0) {}

ShortestPathResult::~ShortestPathResult() {}

/// @brief Clears the path
void ShortestPathResult::clear() {
  _vertices.clear();
  _edges.clear();
}

void ShortestPathResult::edgeToVelocyPack(transaction::Methods*,
                                          ManagedDocumentResult* mmdr,
                                          size_t position,
                                          VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  if (position == 0) {
    builder.add(basics::VelocyPackHelper::NullValue());
  } else {
    TRI_ASSERT(position - 1 < _edges.size());
    // FIXME ADD CACHE!
    std::string tmp = _edges[position - 1].toString();
    builder.add(VPackValue(tmp));
  }
}

void ShortestPathResult::vertexToVelocyPack(transaction::Methods* trx,
                                            ManagedDocumentResult* mmdr,
                                            size_t position,
                                            VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  StringRef v = _vertices[position];
  std::string collection = v.toString();
  size_t p = collection.find("/");
  TRI_ASSERT(p != std::string::npos);

  transaction::BuilderLeaser searchBuilder(trx);
  searchBuilder->add(VPackValue(collection.substr(p + 1)));
  collection = collection.substr(0, p);

  Result res = trx->documentFastPath(collection, mmdr, searchBuilder->slice(),
                                     builder, true);
  if (!res.ok()) {
    builder.clear();  // Just in case...
    builder.add(basics::VelocyPackHelper::NullValue());
  }
}
