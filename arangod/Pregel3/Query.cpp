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
#include "Utils/CollectionNameResolver.h"
#include "Transaction/Helpers.h"

namespace arangodb::pregel3 {

void Query::loadGraph() {
  // todo clean up

  auto start = std::chrono::steady_clock::now();

  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollectionsForRead = true;
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  transaction::Methods trx(ctx, {}, {}, {}, trxOpts);
  Result res = trx.begin();
  CollectionNameResolver collectionNameResolver(_vocbase);

  auto addVertex = [&](LocalDocumentId const& token, VPackSlice slice) {
    // LOG_DEVEL << "Adding a vertex" << slice.toJson();
    // todo add dependence on _graphSpec (other graphs)
    // todo add dependence on slice (currently empty properties are added)

    // get the _id (assume the slice is correct)
    // todo is _id stored in the db always a custom type?

    std::string vertexId = transaction::helpers::extractIdString(
        &collectionNameResolver, slice, VPackSlice());
    size_t const newVertexIdx = _graph->vertexProperties.size();
    _graph->vertexIdToIdx.emplace(vertexId, newVertexIdx);
    _graph->idxToVertexId.emplace_back(vertexId);
    _graph->vertexProperties.emplace_back(BaseGraph::VertexProps(token));

    return true;  // this is needed to make cursor->nextDocument(cb, batchSize)
                  // happy which expects a callback function returning a bool
  };

  auto addEdge = [&](LocalDocumentId const& token, VPackSlice slice) -> bool {
    // LOG_DEVEL << "Adding an edge" << slice.toJson();
    _graph->edgeProperties.emplace_back(BaseGraph::EdgeProps());
    // get _to and _from
    std::string to = slice.get("_to").copyString();
    std::string from = slice.get("_from").copyString();
    size_t const toIdx = _graph->vertexIdToIdx.find(to)->second;
    size_t const fromIdx = _graph->vertexIdToIdx.find(from)->second;
    _graph->vertexProperties[fromIdx].neighbors.push_back(toIdx);
    if (!_graph->graphProperties.isDirected) {
      _graph->vertexProperties[toIdx].neighbors.push_back(fromIdx);
    }
    return true;
  };

  if (std::holds_alternative<
          GraphSpecification::GraphSpecificationByCollections>(
          _graphSpec._graphSpec)) {
    // todo choose one of standard graph types (define some) or CustomGraph
    _graph = std::make_shared<BaseGraph>();
    // todo add graph properties

    // add vertices
    for (auto const& collName :
         std::get<GraphSpecification::GraphSpecificationByCollections>(
             _graphSpec._graphSpec)
             .vertexCollectionNames) {
      auto cursor = trx.indexScan(
          collName, transaction::Methods::CursorType::ALL, ReadOwnWrites::no);
      uint64_t sizeColl = cursor->collection()->numberDocuments(
          &trx, transaction::CountType::Normal);
      _graph->vertexProperties.reserve(_graph->vertexProperties.size() +
                                       sizeColl);
      _graph->idxToVertexId.reserve(_graph->idxToVertexId.size() + sizeColl);
      while (cursor->nextDocument(
          addVertex, Utils::standardBatchSize)) {  // todo let user input it
      }
    }

    // add edges
    for (auto const& collName :
         std::get<GraphSpecification::GraphSpecificationByCollections>(
             _graphSpec._graphSpec)
             .edgeCollectionNames) {
      auto cursor = trx.indexScan(
          collName, transaction::Methods::CursorType::ALL, ReadOwnWrites::no);
      uint64_t sizeColl = cursor->collection()->numberDocuments(
          &trx, transaction::CountType::Normal);
      _graph->vertexProperties.reserve(_graph->vertexProperties.size() +
                                       sizeColl);
      while (cursor->nextDocument(addEdge, Utils::standardBatchSize)) {
      }
    }
    // test
    //    for (auto const& [v, idx] : _graph->vertexIdToIdx) {
    //      LOG_DEVEL << "Neighbours of " << v << " ";
    //      std::string neighbs;
    //      for (auto const& w : _graph->vertexProperties.at(idx).neighbors) {
    //        neighbs += " " + _graph->idxToVertexId.at(w);
    //      }
    //      LOG_DEVEL << neighbs;
    //    }
    LOG_DEVEL << "finished in "
              << std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now() - start)
                     .count()
              << " sec";
  }
}

};  // namespace arangodb::pregel3
