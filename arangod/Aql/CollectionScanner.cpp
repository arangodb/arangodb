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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CollectionScanner.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/OperationCursor.h"
#include "Utils/Transaction.h"

using namespace arangodb::aql;

CollectionScanner::CollectionScanner(arangodb::Transaction* trx,
                                     arangodb::ManagedDocumentResult* mmdr,
                                     std::string const& collection,
                                     bool readRandom)
    : _cursor(trx->indexScan(collection,
                             (readRandom ? arangodb::Transaction::CursorType::ANY
                                         : arangodb::Transaction::CursorType::ALL),
                             Transaction::IndexHandle(), VPackSlice(), mmdr, 0, 
                             UINT64_MAX, 1000, false)) {
  TRI_ASSERT(_cursor->successful());
}

CollectionScanner::~CollectionScanner() {}

void CollectionScanner::scan(std::vector<arangodb::DocumentIdentifierToken>& result, size_t batchSize) {
  result.clear();

  if (!_cursor->hasMore()) {
    return;
  }

  _cursor->getMoreMptr(result, batchSize); 
}

int CollectionScanner::forward(size_t batchSize, uint64_t& skipped) {
  return _cursor->skip(batchSize, skipped);
}

void CollectionScanner::reset() {
  _cursor->reset();
}
