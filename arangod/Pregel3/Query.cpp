////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Query.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Methods.h"

namespace arangodb::pregel3 {

void Query::loadGraph() {
  // todo clean up
  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollectionsForRead = true;
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  transaction::Methods trx(ctx, {}, {}, {}, trxOpts);
  Result res = trx.begin();

  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    LOG_DEVEL << slice.toJson();
    return true;  // this is needed to make cursor->nextDocument(cb, batchSize)
                  // happy
  };

  if (std::holds_alternative<
          GraphSpecification::GraphSpecificationByCollections>(
          _graphSpec._graphSpec)) {
    for (auto const& collName :
         std::get<GraphSpecification::GraphSpecificationByCollections>(
             _graphSpec._graphSpec)
             .vertexCollectionNames) {
      auto cursor = trx.indexScan(
          collName, transaction::Methods::CursorType::ALL, ReadOwnWrites::no);
      constexpr uint64_t batchSize = 10000;
      while (cursor->nextDocument(cb, batchSize)) {
      }
    }
    // tell the formatter the number of docs we are about to load
    //    LogicalCollection* coll = cursor->collection();
    //    uint64_t numVertices =
    //        coll->numberDocuments(&trx, transaction::CountType::Normal);
  }
}

};  // namespace arangodb::pregel3
