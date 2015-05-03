////////////////////////////////////////////////////////////////////////////////
/// @brief V8 Traverser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-collection.h"
#include "Utils/transactions.h"
#include "Utils/V8ResolverGuard.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"
#include "V8Traverser.h"
#include "VocBase/key-generator.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::basics::traverser;
using namespace triagens::arango;

namespace std {
  template<>
  struct hash<VertexId> {
    public:
      size_t operator()(VertexId const& s) const {
        size_t h1 = std::hash<TRI_voc_cid_t>()(s.cid);
        size_t h2 = TRI_FnvHashString(s.key);
        return h1 ^ ( h2 << 1 );
      }
  };

  template<>
  struct equal_to<VertexId> {
    public:
      bool operator()(VertexId const& s, VertexId const& t) const {
        return s.cid == t.cid && strcmp(s.key, t.key) == 0;
      }
  };
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief callback to weight an edge
////////////////////////////////////////////////////////////////////////////////

typedef function<double(TRI_doc_mptr_copy_t& edge)> WeightCalculatorFunction;

class EdgeCollectionInfo {
  private:
    
////////////////////////////////////////////////////////////////////////////////
/// @brief Edge direction for this collection
////////////////////////////////////////////////////////////////////////////////

    TRI_edge_direction_e _direction;

////////////////////////////////////////////////////////////////////////////////
/// @brief prefix for edge collection id
////////////////////////////////////////////////////////////////////////////////

    string _edgeIdPrefix;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
////////////////////////////////////////////////////////////////////////////////

    TRI_document_collection_t* _edgeCollection;

  public:

    EdgeCollectionInfo(
      TRI_edge_direction_e direction,
      string& edgeCollectionName,
      TRI_document_collection_t* edgeCollection
    )  : _direction(direction), 
       _edgeCollection(edgeCollection) {
      _edgeIdPrefix = edgeCollectionName + "/";
    }

    std::string extractEdgeId(TRI_doc_mptr_copy_t& ptr) {
      char const* key = TRI_EXTRACT_MARKER_KEY(&ptr);
      return _edgeIdPrefix + key;
    }

    vector<TRI_doc_mptr_copy_t> getEdges (VertexId& vertexId) {
      return TRI_LookupEdgesDocumentCollection(_edgeCollection,
                   _direction, vertexId.cid, const_cast<char*>(vertexId.key));
    }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief Define edge weight by the number of hops.
///        Respectively 1 for any edge.
////////////////////////////////////////////////////////////////////////////////

class HopWeightCalculator {
  public: 
    HopWeightCalculator() {};

////////////////////////////////////////////////////////////////////////////////
/// @brief Callable weight calculator for edge
////////////////////////////////////////////////////////////////////////////////

    double operator() (TRI_doc_mptr_copy_t& edge) {
      return 1;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Define edge weight by ony special attribute.
///        Respectively 1 for any edge.
////////////////////////////////////////////////////////////////////////////////

class AttributeWeightCalculator {

  TRI_shape_pid_t _shape_pid;
  double _defaultWeight;
  TRI_shaper_t* _shaper;

  public: 
    AttributeWeightCalculator(
      string keyWeight,
      double defaultWeight,
      TRI_shaper_t* shaper
    ) : _defaultWeight(defaultWeight),
        _shaper(shaper)
    {
      _shape_pid = _shaper->lookupAttributePathByName(_shaper, keyWeight.c_str());
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief Callable weight calculator for edge
////////////////////////////////////////////////////////////////////////////////

    double operator() (TRI_doc_mptr_copy_t& edge) {
      if (_shape_pid == 0) {
        return _defaultWeight;
      }
      TRI_shape_sid_t sid;
      TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, edge.getDataPtr());
      TRI_shape_access_t const* accessor = TRI_FindAccessorVocShaper(_shaper, sid, _shape_pid);
      TRI_shaped_json_t shapedJson;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, edge.getDataPtr());
      TRI_shaped_json_t resultJson;
      TRI_ExecuteShapeAccessor(accessor, &shapedJson, &resultJson);
      if (resultJson._sid != TRI_SHAPE_NUMBER) {
        return _defaultWeight;
      }
      TRI_json_t* json = TRI_JsonShapedJson(_shaper, &resultJson);
      if (json == nullptr) {
        return _defaultWeight;
      }
      double realResult = json->_value._number;
      TRI_FreeJson(_shaper->_memoryZone, json);
      return realResult ;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _from Id out of mptr, we return an RValue reference
/// to explicitly allow move semantics.
////////////////////////////////////////////////////////////////////////////////

static inline VertexId extractFromId(TRI_doc_mptr_copy_t& ptr) {
  return VertexId(TRI_EXTRACT_MARKER_FROM_CID(&ptr),
                  TRI_EXTRACT_MARKER_FROM_KEY(&ptr));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _to Id out of mptr, we return an RValue reference
/// to explicitly allow move semantics.
////////////////////////////////////////////////////////////////////////////////

static inline VertexId extractToId(TRI_doc_mptr_copy_t& ptr) {
  return VertexId(TRI_EXTRACT_MARKER_TO_CID(&ptr),
                  TRI_EXTRACT_MARKER_TO_KEY(&ptr));
};


////////////////////////////////////////////////////////////////////////////////
/// @brief Expander for Multiple edge collections
////////////////////////////////////////////////////////////////////////////////

class MultiCollectionEdgeExpander {

////////////////////////////////////////////////////////////////////////////////
/// @brief all info required for edge collection
////////////////////////////////////////////////////////////////////////////////

    vector<EdgeCollectionInfo*> _edgeCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief the weight calculation function
////////////////////////////////////////////////////////////////////////////////

    WeightCalculatorFunction weighter;

  public: 

    MultiCollectionEdgeExpander(TRI_edge_direction_e direction,
                       vector<TRI_document_collection_t*> edgeCollections,
                       vector<string> edgeCollectionNames,
                       WeightCalculatorFunction weighter)
      : weighter(weighter)
    {
      for(size_t i = 0; i != edgeCollectionNames.size(); ++i) {
        _edgeCollections.push_back(new EdgeCollectionInfo(
          direction,
          edgeCollectionNames[i],
          edgeCollections[i]
        ));
      }
    };

    void operator() (VertexId& source,
                     vector<ArangoDBPathFinder::Step*>& result) {
      TransactionBase fake(true); // Fake a transaction to please checks. 
                                  // This is due to multi-threading

      equal_to<VertexId> eq;
      for (auto edgeCollection : _edgeCollections) { 
        auto edges = edgeCollection->getEdges(source); 

        unordered_map<VertexId, size_t> candidates;
        for (size_t j = 0;  j < edges.size(); ++j) {
          VertexId from = extractFromId(edges[j]);
          VertexId to = extractToId(edges[j]);
          double currentWeight = weighter(edges[j]);
          auto inserter = [&](VertexId& s, VertexId& t) {
            auto cand = candidates.find(t);
            if (cand == candidates.end()) {
              // Add weight
              result.push_back(new ArangoDBPathFinder::Step(t, s, currentWeight,
                               edgeCollection->extractEdgeId(edges[j])));
              candidates.emplace(t, result.size() - 1);
            } else {
              // Compare weight
              auto oldWeight = result[cand->second]->weight();
              if (currentWeight < oldWeight) {
                result[cand->second]->setWeight(currentWeight);
              }
            }
          };
          if (!eq(from, source)) {
            inserter(to, from);
          } 
          else if (!eq(to, source)) {
            inserter(from, to);
          }
        }
      }
    } 
};

class SimpleEdgeExpander {

////////////////////////////////////////////////////////////////////////////////
/// @brief all info required for edge collection
////////////////////////////////////////////////////////////////////////////////

    EdgeCollectionInfo* _edgeCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the collection name resolver
////////////////////////////////////////////////////////////////////////////////

    CollectionNameResolver* resolver;

////////////////////////////////////////////////////////////////////////////////
/// @brief the weight calculation function
////////////////////////////////////////////////////////////////////////////////

    WeightCalculatorFunction weighter;

  public: 

    SimpleEdgeExpander(TRI_edge_direction_e direction,
                       TRI_document_collection_t* edgeCollection,
                       string edgeCollectionName,
                       WeightCalculatorFunction weighter)
      : weighter(weighter) {
      _edgeCollection = new EdgeCollectionInfo(direction, edgeCollectionName, edgeCollection);
    };

    void operator() (VertexId& source,
                     vector<ArangoDBPathFinder::Step*>& result) {
      TransactionBase fake(true); // Fake a transaction to please checks. 
                                  // This is due to multi-threading
      auto edges = _edgeCollection->getEdges(source); 

      equal_to<VertexId> eq;
      unordered_map<VertexId, size_t> candidates;
      for (size_t j = 0;  j < edges.size(); ++j) {
        VertexId from = extractFromId(edges[j]);
        VertexId to = extractToId(edges[j]);
        double currentWeight = weighter(edges[j]);
        auto inserter = [&](VertexId& s, VertexId& t) {
          auto cand = candidates.find(t);
          if (cand == candidates.end()) {
            // Add weight
            result.push_back(new ArangoDBPathFinder::Step(t, s, currentWeight, 
                             _edgeCollection->extractEdgeId(edges[j])));
            candidates.emplace(t, result.size() - 1);
          } else {
            // Compare weight
            auto oldWeight = result[cand->second]->weight();
            if (currentWeight < oldWeight) {
              result[cand->second]->setWeight(currentWeight);
            }
          }
        };
        if (!eq(from, source)) {
          inserter(to, from);
        } 
        else if (!eq(to, source)) {
          inserter(from, to);
        }
      }
    } 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch (
    v8::Isolate* isolate,
    TRI_vocbase_t* vocbase,
    string const& vertexCollectionName,
    string const& edgeCollectionName,
    string const& startVertex,
    string const& targetVertex,
    ShortestPathOptions& opts) {

  vector<string> readCollections;
  vector<string> writeCollections;

  readCollections.push_back(vertexCollectionName);
  readCollections.push_back(edgeCollectionName);


  // IHHF isCoordinator
  double lockTimeout = (double) (TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  bool embed = true;
  bool waitForSync = false;

  V8ResolverGuard resolver(vocbase);

  // Start Transaction to collect all parts of the path
  ExplicitTransaction trx(
    vocbase,
    readCollections,
    writeCollections,
    lockTimeout,
    waitForSync,
    embed
  );
  
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    trx.finish(res);
    throw res;
  }

  TRI_vocbase_col_t const* col = resolver.getResolver()->getCollectionStruct(vertexCollectionName);
  if (col == nullptr) {
    trx.finish(res);
    throw TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  if (trx.orderBarrier(trx.trxCollection(col->_cid)) == nullptr) {
    trx.finish(res);
    throw TRI_ERROR_OUT_OF_MEMORY;
  }

  col = resolver.getResolver()->getCollectionStruct(edgeCollectionName);
  if (col == nullptr) {
    trx.finish(res);
    throw TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  if (trx.orderBarrier(trx.trxCollection(col->_cid)) == nullptr) {
    trx.finish(res);
    throw TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_document_collection_t* ecol 
    = trx.trxCollection(col->_cid)->_collection->_collection;
  CollectionNameResolver resolver1(vocbase);
  CollectionNameResolver resolver2(vocbase);
  TRI_edge_direction_e forward;
  TRI_edge_direction_e backward;
  if (opts.direction == "outbound") {
    forward = TRI_EDGE_OUT;
    backward = TRI_EDGE_IN;
  } else if (opts.direction == "inbound") {
    forward = TRI_EDGE_IN;
    backward = TRI_EDGE_OUT;
  } else {
    forward = TRI_EDGE_ANY;
    backward = TRI_EDGE_ANY;
  }

  unique_ptr<SimpleEdgeExpander> forwardExpander;
  unique_ptr<SimpleEdgeExpander> backwardExpander;
  WeightCalculatorFunction weighter;
  if (opts.useWeight) {
    weighter = AttributeWeightCalculator(
      opts.weightAttribute, opts.defaultWeight, ecol->getShaper()
    );
  } else {
    weighter = HopWeightCalculator();
  }
  forwardExpander.reset(new SimpleEdgeExpander(forward, ecol, 
                                           edgeCollectionName, weighter));
  backwardExpander.reset(new SimpleEdgeExpander(backward, ecol,
                                           edgeCollectionName, weighter));

  // Transform string ids to VertexIds
  // Needs refactoring!
  size_t split;
  char const* str = startVertex.c_str();
  if (!TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
    // TODO Error Handling
    trx.finish(res);
    return nullptr;
  }
  string collectionName = startVertex.substr(0, split);

  auto coli = resolver1.getCollectionStruct(collectionName);
  if (coli == nullptr) {
    // collection not found
    trx.finish(res);
    throw TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  VertexId sv(coli->_cid, const_cast<char*>(str + split + 1));

  str = targetVertex.c_str();
  if (!TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
    // TODO Error Handling
    trx.finish(res);
    return nullptr;
  }
  collectionName = targetVertex.substr(0, split);

  coli = resolver2.getCollectionStruct(collectionName);
  if (coli == nullptr) {
    // collection not found
    trx.finish(res);
    throw TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  VertexId tv(coli->_cid, str + split + 1);

  ArangoDBPathFinder pathFinder(*forwardExpander, 
                              *backwardExpander,
                              opts.bidirectional);
  unique_ptr<ArangoDBPathFinder::Path> path;
  if (opts.multiThreaded) {
    path.reset(pathFinder.shortestPathTwoThreads(sv, tv));
  }
  else {
    path.reset(pathFinder.shortestPath(sv, tv));
  } 
  res = trx.finish(res);
  return path;
}
