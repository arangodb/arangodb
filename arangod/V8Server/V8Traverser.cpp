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
#include "Traverser.h"
#include "VocBase/key-generator.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to weight an edge
////////////////////////////////////////////////////////////////////////////////

typedef function<double(TRI_doc_mptr_copy_t& edge)> WeightCalculatorFunction;


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
    };
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
    };

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
    };
};

class SimpleEdgeExpander {

////////////////////////////////////////////////////////////////////////////////
/// @brief direction of expansion
////////////////////////////////////////////////////////////////////////////////

    TRI_edge_direction_e direction;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
////////////////////////////////////////////////////////////////////////////////

    TRI_document_collection_t* edgeCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name and /
////////////////////////////////////////////////////////////////////////////////

    string edgeIdPrefix;

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
                       CollectionNameResolver* resolver,
                       WeightCalculatorFunction weighter)
      : direction(direction), edgeCollection(edgeCollection),
        resolver(resolver), weighter(weighter)
    {
      edgeIdPrefix = edgeCollectionName + "/";
    };

    Traverser::VertexId extractFromId(TRI_doc_mptr_copy_t& ptr) {
      char const* key = TRI_EXTRACT_MARKER_FROM_KEY(&ptr);
      TRI_voc_cid_t cid = TRI_EXTRACT_MARKER_FROM_CID(&ptr);
      string col = resolver->getCollectionName(cid);
      col.append("/");
      col.append(key);
      return col;
    };

    Traverser::VertexId extractToId(TRI_doc_mptr_copy_t& ptr) {
      char const* key = TRI_EXTRACT_MARKER_TO_KEY(&ptr);
      TRI_voc_cid_t cid = TRI_EXTRACT_MARKER_TO_CID(&ptr);
      string col = resolver->getCollectionName(cid);
      col.append("/");
      col.append(key);
      return col;
    };

    Traverser::EdgeId extractEdgeId(TRI_doc_mptr_copy_t& ptr) {
      char const* key = TRI_EXTRACT_MARKER_KEY(&ptr);
      return edgeIdPrefix + key;
    };

    void operator() (Traverser::VertexId source,
                     vector<Traverser::Step>& result) {
      std::vector<TRI_doc_mptr_copy_t> edges;
      TransactionBase fake(true); // Fake a transaction to please checks. 
                                  // This is due to multi-threading

      // Process Vertex Id!
      size_t split;
      char const* str = source.c_str();
      if (!TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
        // TODO Error Handling
        return;
      }
      string collectionName = source.substr(0, split);

      auto col = resolver->getCollectionStruct(collectionName);
      if (col == nullptr) {
        // collection not found
        throw TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }
      auto collectionCId = col->_cid;
      edges = TRI_LookupEdgesDocumentCollection(edgeCollection, direction, collectionCId, const_cast<char*>(str + split + 1));

      unordered_map<Traverser::VertexId, size_t> candidates;
      Traverser::VertexId from;
      Traverser::VertexId to;
      for (size_t j = 0;  j < edges.size(); ++j) {
        from = extractFromId(edges[j]);
        to = extractToId(edges[j]);
        double currentWeight = weighter(edges[j]);
        auto inserter = [&](Traverser::VertexId s, Traverser::VertexId t) {
          auto cand = candidates.find(t);
          if (cand == candidates.end()) {
            // Add weight
            result.emplace_back(t, s, currentWeight, extractEdgeId(edges[j]));
            candidates.emplace(t, result.size() - 1);
          } else {
            // Compare weight
            auto oldWeight = result[cand->second].weight();
            if (currentWeight < oldWeight) {
              result[cand->second].setWeight(currentWeight);
            }
          }
        };
        if (from != source) {
          inserter(to, from);
        } 
        else if (to != source) {
          inserter(from, to);
        }
      }
    } 
};

static v8::Handle<v8::Value> pathIdsToV8(v8::Isolate* isolate, Traverser::Path& p) {
  v8::EscapableHandleScope scope(isolate);
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  uint32_t const vn = static_cast<uint32_t>(p.vertices.size());
  v8::Handle<v8::Array> vertices = v8::Array::New(isolate, static_cast<int>(vn));

  for (size_t j = 0;  j < vn;  ++j) {
    vertices->Set(static_cast<uint32_t>(j), TRI_V8_STRING(p.vertices[j].c_str()));
  }
  result->Set(TRI_V8_STRING("vertices"), vertices);

  uint32_t const en = static_cast<uint32_t>(p.edges.size());
  v8::Handle<v8::Array> edges = v8::Array::New(isolate, static_cast<int>(en));

  for (size_t j = 0;  j < en;  ++j) {
    edges->Set(static_cast<uint32_t>(j), TRI_V8_STRING(p.edges[j].c_str()));
  }
  result->Set(TRI_V8_STRING("edges"), edges);
  result->Set(TRI_V8_STRING("distance"), v8::Number::New(isolate, p.weight));

  return scope.Escape<v8::Value>(result);
};

struct LocalCollectionGuard {
  LocalCollectionGuard (TRI_vocbase_col_t* collection)
    : _collection(collection) {
  }

  ~LocalCollectionGuard () {
    if (_collection != nullptr && ! _collection->_isLocal) {
      FreeCoordinatorCollection(_collection);
    }
  }

  TRI_vocbase_col_t* _collection;
};

void TRI_RunDijkstraSearch (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || args.Length() > 5) {
    TRI_V8_THROW_EXCEPTION_USAGE("AQL_SHORTEST_PATH(<vertexcollection>, <edgecollection>, <start>, <end>, <options>)");
  }

  std::unique_ptr<char[]> key;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  vocbase = GetContextVocBase(isolate);

  vector<string> readCollections;
  vector<string> writeCollections;

  double lockTimeout = (double) (TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  bool embed = true;
  bool waitForSync = false;

  // get the vertex collection
  if (! args[0]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <vertexcollection>");
  }
  string vertexCollectionName = TRI_ObjectToString(args[0]);

  // get the edge collection
  if (! args[1]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <edgecollection>");
  }
  string const edgeCollectionName = TRI_ObjectToString(args[1]);

  vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  V8ResolverGuard resolver(vocbase);

  readCollections.push_back(vertexCollectionName);
  readCollections.push_back(edgeCollectionName);
  
  if (! args[2]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <startVertex>");
  }
  string const startVertex = TRI_ObjectToString(args[2]);

  if (! args[3]->IsString()) {
    TRI_V8_THROW_TYPE_ERROR("expecting string for <targetVertex>");
  }
  string const targetVertex = TRI_ObjectToString(args[3]);

  string direction = "outbound";
  bool useWeight = false;
  string weightAttribute = "";
  double defaultWeight = 1;
  bool bidirectional = true;
  bool multiThreaded = false;

  if (args.Length() == 5) {
    if (! args[4]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("expecting json for <options>");
    }
    v8::Handle<v8::Object> options = args[4]->ToObject();
    v8::Local<v8::String> keyDirection = TRI_V8_ASCII_STRING("direction");
    v8::Local<v8::String> keyWeight= TRI_V8_ASCII_STRING("distance");
    v8::Local<v8::String> keyDefaultWeight= TRI_V8_ASCII_STRING("defaultDistance");

    if (options->Has(keyDirection) ) {
      direction = TRI_ObjectToString(options->Get(keyDirection));
      if (   direction != "outbound"
          && direction != "inbound"
          && direction != "any"
         ) {
        TRI_V8_THROW_TYPE_ERROR("expecting direction to be 'outbound', 'inbound' or 'any'");
      }
    }
    if (options->Has(keyWeight) && options->Has(keyDefaultWeight) ) {
      useWeight = true;
      weightAttribute = TRI_ObjectToString(options->Get(keyWeight));
      defaultWeight = TRI_ObjectToDouble(options->Get(keyDefaultWeight));
    }
    v8::Local<v8::String> keyBidirectional = TRI_V8_ASCII_STRING("bidirectional");
    if (options->Has(keyBidirectional)) {
      bidirectional = TRI_ObjectToBoolean(options->Get(keyBidirectional));
    }
    v8::Local<v8::String> keyMultiThreaded = TRI_V8_ASCII_STRING("multiThreaded");
    if (options->Has(keyMultiThreaded)) {
      multiThreaded = TRI_ObjectToBoolean(options->Get(keyMultiThreaded));
    }
  } 

  // IHHF isCoordinator

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
    TRI_V8_THROW_EXCEPTION(res);
  }

  col = resolver.getResolver()->getCollectionStruct(vertexCollectionName);
  if (col == nullptr) {
    // collection not found
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  if (trx.orderBarrier(trx.trxCollection(col->_cid)) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  col = resolver.getResolver()->getCollectionStruct(edgeCollectionName);
  if (col == nullptr) {
    // collection not found
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  if (trx.orderBarrier(trx.trxCollection(col->_cid)) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_document_collection_t* ecol = trx.trxCollection(col->_cid)->_collection->_collection;
  CollectionNameResolver resolver1(vocbase);
  CollectionNameResolver resolver2(vocbase);
  TRI_edge_direction_e forward;
  TRI_edge_direction_e backward;
  if (direction == "outbound") {
    forward = TRI_EDGE_OUT;
    backward = TRI_EDGE_IN;
  } else if (direction == "inbound") {
    forward = TRI_EDGE_IN;
    backward = TRI_EDGE_OUT;
  } else {
    forward = TRI_EDGE_ANY;
    backward = TRI_EDGE_ANY;
  }

  unique_ptr<SimpleEdgeExpander> forwardExpander;
  unique_ptr<SimpleEdgeExpander> backwardExpander;
  WeightCalculatorFunction weighter;
  if (useWeight) {
    weighter = AttributeWeightCalculator(
      weightAttribute, defaultWeight, ecol->getShaper()
    );
  } else {
    weighter = HopWeightCalculator();
  }
  forwardExpander.reset(new SimpleEdgeExpander(forward, ecol, edgeCollectionName, &resolver1, weighter));
  backwardExpander.reset(new SimpleEdgeExpander(backward, ecol, edgeCollectionName, &resolver2, weighter));


  Traverser traverser(*forwardExpander, *backwardExpander, bidirectional);
  unique_ptr<Traverser::Path> path;
  if (multiThreaded) {
    path.reset(traverser.shortestPathTwoThreads(startVertex, targetVertex));
  }
  else {
    path.reset(traverser.shortestPath(startVertex, targetVertex));
  } 
  if (path.get() == nullptr) {
    res = trx.finish(res);
    v8::EscapableHandleScope scope(isolate);
    TRI_V8_RETURN(scope.Escape<v8::Value>(v8::Object::New(isolate)));
  }
  auto result = pathIdsToV8(isolate, *path);
  res = trx.finish(res);

  TRI_V8_RETURN(result);
}
