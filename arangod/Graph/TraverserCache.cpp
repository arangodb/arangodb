////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2017 ArangoDB GmbH, Cologne, Germany
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

#include "TraverserCache.h"

#include "Basics/StringHeap.h"
#include "Basics/VelocyPackHelper.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Cluster/ServerState.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

TraverserCache::TraverserCache(aql::Query* query)
    : _mmdr(new ManagedDocumentResult{}),
      _query(query),
      _trx(query->trx()),
      _insertedDocuments(0),
      _filteredDocuments(0),
      _stringHeap(new StringHeap{4096}) /* arbitrary block-size may be adjusted for performance */ {
}

TraverserCache::~TraverserCache() {}

VPackSlice TraverserCache::lookupToken(EdgeDocumentToken const& idToken) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto col = _trx->vocbase().lookupCollection(idToken.cid());

  if (col == nullptr) {
    // collection gone... should not happen
    LOG_TOPIC("3b2ba", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document. collection not found";
    TRI_ASSERT(col != nullptr);  // for maintainer mode
    return arangodb::velocypack::Slice::nullSlice();
  }

  if (!col->readDocument(_trx, idToken.localDocumentId(), *_mmdr.get())) {
    // We already had this token, inconsistent state. Return NULL in Production
    LOG_TOPIC("3acb3", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document, return 'null' instead. "
        << "This is most likely a caching issue. Try: 'db." << col->name()
        << ".unload(); db." << col->name() << ".load()' in arangosh to fix this.";
    TRI_ASSERT(false);  // for maintainer mode
    return arangodb::velocypack::Slice::nullSlice();
  }

  return VPackSlice(_mmdr->vpack());
}

VPackSlice TraverserCache::lookupInCollection(arangodb::velocypack::StringRef id) {
  // TRI_ASSERT(!ServerState::instance()->isCoordinator());
  size_t pos = id.find('/');
  if (pos == std::string::npos || pos + 1 == id.size()) {
    // Invalid input. If we get here somehow we managed to store invalid
    // _from/_to values or the traverser did a let an illegal start through
    TRI_ASSERT(false);  // for maintainer mode
    return arangodb::velocypack::Slice::nullSlice();
  }

  Result res = _trx->documentFastPathLocal(id.substr(0, pos).toString(),
                                           id.substr(pos + 1), *_mmdr, true);
  if (res.ok()) {
    ++_insertedDocuments;
    return VPackSlice(_mmdr->vpack());
  } else if (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
    ++_insertedDocuments;

    // Register a warning. It is okay though but helps the user
    std::string msg = "vertex '" + id.toString() + "' not found";
    _query->registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());

    // This is expected, we may have dangling edges. Interpret as NULL
    return arangodb::velocypack::Slice::nullSlice();
  } else {
    // ok we are in a rather bad state. Better throw and abort.
    THROW_ARANGO_EXCEPTION(res);
  }
}

void TraverserCache::insertEdgeIntoResult(EdgeDocumentToken const& idToken,
                                          VPackBuilder& builder) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  builder.add(lookupToken(idToken));
}

void TraverserCache::insertVertexIntoResult(arangodb::velocypack::StringRef idString, VPackBuilder& builder) {
  builder.add(lookupInCollection(idString));
}

aql::AqlValue TraverserCache::fetchEdgeAqlResult(EdgeDocumentToken const& idToken) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  return aql::AqlValue(lookupToken(idToken));
}

aql::AqlValue TraverserCache::fetchVertexAqlResult(arangodb::velocypack::StringRef idString) {
  return aql::AqlValue(lookupInCollection(idString));
}

arangodb::velocypack::StringRef TraverserCache::persistString(arangodb::velocypack::StringRef const idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  arangodb::velocypack::StringRef res = _stringHeap->registerString(idString.begin(), idString.length());
  _persistedStrings.emplace(res);
  return res;
}
