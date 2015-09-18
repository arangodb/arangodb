////////////////////////////////////////////////////////////////////////////////
/// @brief Implementation of the ExecutionBlock for Traversals
///
/// @file arangod/Aql/TraversalBlock.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/TraversalBlock.h"
#include "Utils/ShapedJsonTransformer.h"
#include "Utils/SingleCollectionReadOnlyTransaction.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                              class TraversalBlock
// -----------------------------------------------------------------------------

TraversalBlock::TraversalBlock (ExecutionEngine* engine,
                                TraversalNode const* ep)
  : ExecutionBlock(engine, ep),
    _posInPaths(0),
    _useRegister(false),
    _usedConstant(false),
    _vertexReg(0),
    _edgeReg(0),
    _pathReg(0) {

  basics::traverser::TraverserOptions opts;
  ep->fillTraversalOptions(opts);
  std::vector<TRI_document_collection_t*> edgeCollections;
  auto cids = ep->edgeCids();
  for (auto const& cid : cids) {
    edgeCollections.push_back(_trx->documentCollection(cid));
  }
  _traverser.reset(new basics::traverser::DepthFirstTraverser(edgeCollections, opts));
  _resolver = new CollectionNameResolver(_trx->vocbase());
  if (!ep->usesInVariable()) {
    _vertexId = ep->getStartVertex();
    auto pos = _vertexId.find("/");
    
    _startId = VertexId(
      _resolver->getCollectionId(_vertexId.substr(0, pos).c_str()),
      _vertexId.c_str() + pos + 1
    );
  }
  else {
    auto it = ep->getRegisterPlan()->varInfo.find(ep->inVariable()->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    _reg = it->second.registerId;
    _useRegister = true;
  }
  _vertexVar = nullptr;
  _edgeVar = nullptr;
  _pathVar = nullptr;
  if (ep->usesVertexOutVariable()) {
    _vertexVar = ep->vertexOutVariable();
  }

  if (ep->usesEdgeOutVariable()) {
    _edgeVar = ep->edgeOutVariable();
  }

  if (ep->usesPathOutVariable()) {
    _pathVar = ep->pathOutVariable();
  }

  /*
  auto pruner = [] (const TraversalPath<TRI_doc_mptr_copy_t, VertexId>& path) -> bool {
    if (strcmp(path.vertices.back().key, "1") == 0) {
      return true;
    }
    if (strcmp(path.vertices.back().key, "31") == 0) {
      return true;
    }
    return false;
  };
  opts.setPruningFunction(pruner);
  */
}

TraversalBlock::~TraversalBlock () {
  delete _resolver;
  freeCaches();
}

void TraversalBlock::freeCaches () {
  for (auto& v : _vertices) {
    v.destroy();
  }
  _vertices.clear();
  for (auto& e : _edges) {
    e.destroy();
  }
  _edges.clear();
  for (auto& p : _paths) {
    p.destroy();
  }
  _paths.clear();
}

int TraversalBlock::initialize () {
  int res = ExecutionBlock::initialize();
  
  auto varInfo = getPlanNode()->getRegisterPlan()->varInfo;
  if (usesVertexOutput()) {
    auto it = varInfo.find(_vertexVar->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _vertexReg = it->second.registerId;
  }
  if (usesEdgeOutput()) {
    auto it = varInfo.find(_edgeVar->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _edgeReg = it->second.registerId;
  }
  if (usesPathOutput()) {
    auto it = varInfo.find(_pathVar->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _pathReg = it->second.registerId;
  }
  

  return res;
}

int TraversalBlock::initializeCursor (AqlItemBlock* items, 
                                      size_t pos) {
  int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

Json TraversalBlock::extractVertexJson (
  VertexId const& v
) {
  auto collection = _trx->trxCollection(v.cid);
  if (collection == nullptr) {
    SingleCollectionReadOnlyTransaction intTrx(new StandaloneTransactionContext(), _trx->vocbase(), v.cid);
    int res = intTrx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    collection = intTrx.trxCollection();
    TRI_doc_mptr_copy_t mptr;
    intTrx.read(&mptr, v.key);
    Json tmp = TRI_ExpandShapedJson(
      collection->_collection->_collection->getShaper(),
      _resolver,
      v.cid,
      &mptr
    );
    intTrx.finish(res);
    return tmp;
  }
  TRI_doc_mptr_copy_t mptr;
  _trx->readSingle(collection, &mptr, v.key);
  return TRI_ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    _resolver,
    v.cid,
    &mptr
  );
}


Json TraversalBlock::extractEdgeJson (
  EdgeInfo const& e
) {
  auto cid = e.cid;
  auto collection = _trx->trxCollection(cid);
  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, &e.mptr);
  return TRI_ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    _resolver,
    cid,
    &e.mptr
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform the VertexId to AQLValue object
////////////////////////////////////////////////////////////////////////////////

AqlValue TraversalBlock::vertexToAqlValue (
  VertexId const& v
) {
  // This json is freed by the AqlValue. No unique_ptr here.
  Json* result = new Json(extractVertexJson(v)); 
  return AqlValue(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform the EdgeInfo to AQLValue object
////////////////////////////////////////////////////////////////////////////////

AqlValue TraversalBlock::edgeToAqlValue (
  EdgeInfo const& e
) {
  // This json is freed by the AqlValue. No unique_ptr here.
  Json* result = new Json(extractEdgeJson(e)); 
  return AqlValue(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Transform the path to AQLValue object
////////////////////////////////////////////////////////////////////////////////

AqlValue TraversalBlock::pathToAqlValue (
  const TraversalPath<EdgeInfo, VertexId>& p
) {
  // This json is freed by the AqlValue. No unique_ptr here.
  Json* path = new Json(Json::Object, 2); 
  Json vertices(Json::Array);
  for (size_t i = 0; i < p.vertices.size(); ++i) {
    vertices(extractVertexJson(p.vertices[i]));
  }
  Json edges(Json::Array);
  for (size_t i = 0; i < p.edges.size(); ++i) {
    edges(extractEdgeJson(p.edges[i]));
  }
  (*path)("vertices", vertices)
         ("edges", edges);

  return AqlValue(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read more paths
////////////////////////////////////////////////////////////////////////////////

bool TraversalBlock::morePaths (size_t hint) {
  freeCaches();
  _posInPaths = 0;
  if (!_traverser->hasMore()) {
    return false;
  }
  for (size_t j = 0; j < hint; ++j) {
    auto p = _traverser->next();
    if (p.edges.size() == 0) {
      // There are no further paths available.
      break;
    }
    if ( usesVertexOutput() ) {
      _vertices.push_back(vertexToAqlValue(p.vertices.back()));
    }
    if ( usesEdgeOutput() ) {
      _edges.push_back(edgeToAqlValue(p.edges.back()));
    }
    if ( usesPathOutput() ) {
      _paths.push_back(pathToAqlValue(p));
    }
  }
  // This is only save as long as _vertices is still build
  return _vertices.size() > 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skip the next paths
////////////////////////////////////////////////////////////////////////////////

size_t TraversalBlock::skipPaths (size_t hint) {
  freeCaches();
  _posInPaths = 0;
  if (!_traverser->hasMore()) {
    return 0;
  }
  return _traverser->skip(hint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the list of paths
////////////////////////////////////////////////////////////////////////////////

void TraversalBlock::initializePaths (AqlItemBlock const* items) {
  if (_vertices.size() > 0) {
    // No Initialisation required.
    return;
  }
  if (!_useRegister) {
    if (!_usedConstant) {
      _usedConstant = true;
      _traverser->setStartVertex(_startId);
    }
  }
  else {
    auto in = items->getValueReference(_pos, _reg);
    if (in.isShaped()) {
      auto col = items->getDocumentCollection(_reg);
      VertexId v(col->_info._cid, TRI_EXTRACT_MARKER_KEY(in.getMarker()));
      _traverser->setStartVertex(v);
    }
    else if (in.isObject()) {
      Json input = in.toJson(_trx, nullptr, false);
      if (input.has("_id") ) {
        Json _idJson = input.get("_id");
        if (_idJson.isString()) {
          _vertexId = JsonHelper::getStringValue(_idJson.json(), "");
          VertexId v = triagens::basics::traverser::IdStringToVertexId (
            _resolver,
            _vertexId
          );
          _traverser->setStartVertex(v);
        }
      }
      else if (input.has("vertex")) {
        // This is used whenever the input is the result of another traversal.
        Json vertexJson = input.get("vertex");
        if (vertexJson.has("_id") ) {
          Json _idJson = vertexJson.get("_id");
          if (_idJson.isString()) {
            _vertexId = JsonHelper::getStringValue(_idJson.json(), "");
            VertexId v = triagens::basics::traverser::IdStringToVertexId (
              _resolver,
              _vertexId
            );
            _traverser->setStartVertex(v);
          }
        }
      }
    }
    else if (in._type == AqlValue::DOCVEC) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, 
                                     std::string("Only one start vertex allowed. Embed it in a FOR loop."));
    }
    else {
      std::cout << "FOUND Type: " << in.getTypeString() << std::endl;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* TraversalBlock::getSome (size_t, // atLeast,
                                       size_t atMost) {
  if (_done) {
    return nullptr;
  }

  if (_buffer.empty()) {

    size_t toFetch = (std::min)(DefaultBatchSize, atMost);
    if (! ExecutionBlock::getBlock(toFetch, toFetch)) {
      _done = true;
      return nullptr;
    }
    _pos = 0;           // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  size_t const curRegs = cur->getNrRegs();

  if (_pos == 0) {
    // Initial initialisation
    initializePaths(cur);
  }

  // Iterate more paths:
  if (_posInPaths >= _vertices.size()) {
    if (! morePaths(atMost)) {
      // This input does not have any more paths. maybe the next one has.
      // we can only return nullptr iff the buffer is empty.
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        // returnBlock(cur);
        delete cur;
        _pos = 0;
      } else {
        initializePaths(cur);
      }
      return getSome(atMost, atMost);
    }
  }

  size_t available = _vertices.size() - _posInPaths;
  size_t toSend = (std::min)(atMost, available);

  RegisterId nrRegs = getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

  std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrRegs));
  // std::unique_ptr<AqlItemBlock> res(new AqlItemBlock(toSend, nrRegs));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());
  
  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur, res.get(), _pos);

  for (size_t j = 0; j < toSend; j++) {
    if (j > 0) {
      // re-use already copied aqlvalues
      for (RegisterId i = 0; i < curRegs; i++) {
        res->setValue(j, i, res->getValueReference(0, i));
        // Note: if this throws, then all values will be deleted
        // properly since the first one is.
      }
    }
    if ( usesVertexOutput() ) {
      res->setValue(j, _vertexReg, _vertices[_posInPaths].clone());
    }
    if ( usesEdgeOutput() ) {
      res->setValue(j, _edgeReg, _edges[_posInPaths].clone());
    }
    if ( usesPathOutput() ) {
      res->setValue(j,
        _pathReg,
        _paths[_posInPaths].clone()
      );
    }
    ++_posInPaths;
  }
  // Advance read position:
  if (_posInPaths >= _vertices.size()) {
    // we have exhausted our local paths buffer
    // fetch more paths into our buffer
    if (! morePaths(atMost)) {
      // nothing more to read, re-initialize fetching of paths
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        // returnBlock(cur);
        delete cur;
        _pos = 0;
      } else {
        initializePaths(cur);
      }
    }
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();
}

size_t TraversalBlock::skipSome (size_t atLeast, size_t atMost) {
  size_t skipped = 0;

  if (_done) {
    return skipped;
  }

  if (_buffer.empty()) {

    size_t toFetch = (std::min)(DefaultBatchSize, atMost);
    if (! ExecutionBlock::getBlock(toFetch, toFetch)) {
      _done = true;
      return skipped;
    }
    _pos = 0;           // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();

  if (_pos == 0) {
    // Initial initialisation
    initializePaths(cur);
  }

  size_t available = _vertices.size() - _posInPaths;
  // We have not yet fetched any paths. We can skip the next atMost many
  if (available == 0) {
    return skipPaths(atMost);
  }
  // We have fewer paths available in our list, so we clear the list and thereby skip these.
  if (available <= atMost) {
    freeCaches();
    _posInPaths = 0;
    return available;
  }
  _posInPaths += atMost;
  // Skip the next atMost many paths.
  return atMost;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
