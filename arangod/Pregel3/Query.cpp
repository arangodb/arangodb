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

ResultT<double> getCapacity(VPackSlice slice) {
  if (slice.hasKey(Utils::capacityProp)) {
    VPackSlice cap = slice.get(Utils::capacityProp);
    if (cap.isDouble()) {
      return slice.get(Utils::capacityProp).getDouble();
    } else {
      return ResultT<double>::error(TRI_ERROR_BAD_PARAMETER,
                                    "Capacity should be a double.");
    }
  } else {
    return ResultT<double>::error(
        TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE,
        "Edge has no capacity: " + slice.copyString());
  }
}

ResultT<double> extractDefaultCapacity(AlgorithmSpecification& algSpec) {
  if (algSpec.defaultCapacity.has_value()) {
    return algSpec.defaultCapacity.value();
  } else {
    return -1.0;
  }
}

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
    //    auto graphVerticesEdges =
    //    _getCastedGraphVerticesEdges(_algSpec.algName);
    //    std::get<0>(std::get<0>(graphVerticesEdges))->vertices
    if (_algSpec.algName == "MinCut") {
      // todo the graph is casted each time a vertex is inserted
      auto graph = dynamic_cast<MinCutGraph*>(_graph.get());
      graph->vertices.emplace_back();
      // trust that each vertex id appears only once
      _vertexIdToIdx[vertexId] = graph->vertexIds.size();
      graph->vertexIds.push_back(vertexId);
    } else {
      auto graph = dynamic_cast<EmptyPropertiesGraph*>(_graph.get());
      graph->vertices.emplace_back();
      _vertexIdToIdx[vertexId] = graph->vertexIds.size();
      graph->vertexIds.push_back(vertexId);
    }
    return true;  // this is needed to make cursor->nextDocument(cb, batchSize)
                  // happy which expects a callback function returning a bool
  };

  // adding an edge if we know that the graph has no multiple edges (but can
  // have self-loops). In this case, we don't check if a neighbor has already
  // been added. todo check this lambda, it may be wrong
  auto addSingleEdge = [&](LocalDocumentId const& token,
                           VPackSlice slice) -> bool {
    // LOG_DEVEL << "Adding an edge" << slice.toJson();
    std::string to = slice.get("_to").copyString();
    std::string from = slice.get("_from").copyString();
    size_t const toIdx = _vertexIdToIdx.find(to)->second;
    size_t const fromIdx = _vertexIdToIdx.find(from)->second;

    if (_algSpec.algName == "MinCut") {
      // todo the graph is casted each time a vertex is inserted
      auto graph = dynamic_cast<MinCutGraph*>(_graph.get());
      size_t const edgeIdx = graph->edges.size();
      // for the reverse edge (if any)
      _vertexVertexToEdge[std::make_pair(toIdx, fromIdx)] = edgeIdx;
      // get the capacity
      double capacity = _defaultCapacity;
      auto resCapacity = getCapacity(slice);
      if (resCapacity.fail()) {
        // todo print a warning
      } else {
        capacity = resCapacity.get();
      }

      auto revEdge = _vertexVertexToEdge.find(std::make_pair(toIdx, fromIdx));
      if (revEdge != _vertexVertexToEdge.end()) {
        graph->edges.emplace_back(fromIdx, toIdx, capacity, revEdge->second);
      } else {
        graph->edges.emplace_back(fromIdx, toIdx, capacity);
      }
    } else {
      auto graph = dynamic_cast<EmptyPropertiesGraph*>(_graph.get());
      size_t const edgeIdx = graph->edges.size();
      // for the reverse edge (if any)
      _vertexVertexToEdge[std::make_pair(toIdx, fromIdx)] = edgeIdx;
      graph->edges.emplace_back(fromIdx, toIdx);
    }
    return true;
  };

  if (std::holds_alternative<
          GraphSpecification::GraphSpecificationByCollections>(
          _graphSpec._graphSpec)) {
    // todo choose one of standard graph types (define some) or CustomGraph
    _graph =
        std::make_shared<Graph<EmptyVertexProperties, EmptyEdgeProperties>>();
    if (_algSpec.algName == "MinCut") {
      _graph = std::make_shared<MinCutGraph>();
      auto resDefCap = extractDefaultCapacity(_algSpec);
      if (resDefCap.fail()) {
        // todo print a warning
      } else {
        _defaultCapacity = resDefCap.get();
      }
    } else {
      _graph =
          std::make_shared<Graph<EmptyVertexProperties, EmptyEdgeProperties>>();
    }
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

      if (_algSpec.algName == "MinCut") {
        auto const graph = dynamic_cast<MinCutGraph*>(_graph.get());
        graph->vertices.reserve(graph->vertices.size() + sizeColl);
      } else {
        auto const graph = dynamic_cast<EmptyPropertiesGraph*>(_graph.get());
        graph->vertices.reserve(graph->vertices.size() + sizeColl);
      }
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
      if (_algSpec.algName == "MinCut") {
        auto const graph = dynamic_cast<MinCutGraph*>(_graph.get());
        graph->edges.reserve(graph->edges.size() + sizeColl);
        while (cursor->nextDocument(addSingleEdge, Utils::standardBatchSize)) {
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
}

}  // namespace arangodb::pregel3
