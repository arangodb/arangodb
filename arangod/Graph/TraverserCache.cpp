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
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"

#include "Aql/AqlValue.h"
#include "Cluster/ServerState.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

TraverserCache::TraverserCache(transaction::Methods* trx)
    : _mmdr(new ManagedDocumentResult{}),
      _trx(trx), _insertedDocuments(0),
      _filteredDocuments(0),
      _stringHeap(new StringHeap{4096}) /* arbitrary block-size may be adjusted for performance */ {
}

TraverserCache::~TraverserCache() {}

arangodb::velocypack::Slice TraverserCache::lookupToken(EdgeDocumentToken const* token) {
  return lookupInCollection(static_cast<SingleServerEdgeDocumentToken const*>(token));
}


VPackSlice TraverserCache::lookupInCollection(StringRef id) {
  size_t pos = id.find('/');
  if (pos == std::string::npos) {
    // Invalid input. If we get here somehow we managed to store invalid _from/_to
    // values or the traverser did a let an illegal start through
    TRI_ASSERT(false);
    return basics::VelocyPackHelper::NullValue();
  }
  Result res = _trx->documentFastPathLocal(id.substr(0, pos).toString(),
                                           id.substr(pos + 1), *_mmdr, true);
  if (res.ok()) {
    ++_insertedDocuments;
    return VPackSlice(_mmdr->vpack());
  } else if (res.errorNumber() == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    ++_insertedDocuments;
    // This is expected, we may have dangling edges. Interpret as NULL
    return basics::VelocyPackHelper::NullValue();
  } else {
    // ok we are in a rather bad state. Better throw and abort.
    THROW_ARANGO_EXCEPTION(res);
  }
}

VPackSlice TraverserCache::lookupInCollection(SingleServerEdgeDocumentToken const* idToken) const {
  auto col = _trx->vocbase()->lookupCollection(idToken->cid());
  if (!col->readDocument(_trx, idToken->token(), *_mmdr.get())) {
    TRI_ASSERT(false);
    // We already had this token, inconsistent state. Return NULL in Production
    LOG_TOPIC(ERR, arangodb::Logger::GRAPHS) << "Could not extract indexed Edge Document, return 'null' instead. This is most likely a caching issue. Try: '" << col->name() <<".unload(); " << col->name() << ".load()' in arangosh to fix this."; 
    return basics::VelocyPackHelper::NullValue();
  }
  return VPackSlice(_mmdr->vpack());
}

void TraverserCache::insertIntoResult(EdgeDocumentToken const* idToken,
                                      VPackBuilder& builder) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  builder.add(lookupInCollection(static_cast<SingleServerEdgeDocumentToken const*>(idToken)));
}

void TraverserCache::insertIntoResult(StringRef idString,
                                      VPackBuilder& builder) {
  builder.add(lookupInCollection(idString));
}

aql::AqlValue TraverserCache::fetchAqlResult(StringRef idString) {
  return aql::AqlValue(lookupInCollection(idString));
}

aql::AqlValue TraverserCache::fetchAqlResult(EdgeDocumentToken const* idToken) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  return aql::AqlValue(lookupInCollection(static_cast<SingleServerEdgeDocumentToken const*>(idToken)));
}

void TraverserCache::insertDocument(StringRef idString, arangodb::velocypack::Slice const& document) {
  return;
}

bool TraverserCache::validateFilter(
    StringRef idString,
    std::function<bool(VPackSlice const&)> filterFunc) {
  VPackSlice slice = lookupInCollection(idString);
  return filterFunc(slice);
}

StringRef TraverserCache::persistString(
    StringRef const idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  StringRef res = _stringHeap->registerString(idString.begin(), idString.length());
  _persistedStrings.emplace(res);
  return res;
}
