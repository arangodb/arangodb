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

#include "Aql/Ast.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Functions.h"
#include "Aql/TraversalBlock.h"
#include "Aql/ExecutionNode.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterTraverser.h"
#include "V8/v8-globals.h"
#include "V8Server/V8Traverser.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::aql;

using VertexId = triagens::arango::traverser::VertexId;

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
    _pathReg(0),
    _expressions(ep->expressions()),
    _hasV8Expression(false) {

  triagens::arango::traverser::TraverserOptions opts;
  ep->fillTraversalOptions(opts);
  auto edgeColls = ep->edgeColls();
  auto ast = ep->_plan->getAst();

  for (auto& map : *_expressions) {
    for (size_t i = 0; i < map.second.size(); ++i) {
      SimpleTraverserExpression* it = dynamic_cast<SimpleTraverserExpression*>(map.second.at(i));
      std::unique_ptr<Expression> e(new Expression(ast, it->toEvaluate));
      _hasV8Expression |= e->isV8();
      std::unordered_set<Variable const*> inVars;
      e->variables(inVars);
      it->expression = e.release();

      // Prepare _inVars and _inRegs:
      _inVars.emplace_back();
      std::vector<Variable const*>& inVarsCur = _inVars.back();
      _inRegs.emplace_back();
      std::vector<RegisterId>& inRegsCur = _inRegs.back();

      for (auto const& v : inVars) {
        inVarsCur.emplace_back(v);
        auto it = ep->getRegisterPlan()->varInfo.find(v->id);
        TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
        TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
        inRegsCur.emplace_back(it->second.registerId);
      }
    }
  }

  _resolver = new CollectionNameResolver(_trx->vocbase());
  if (triagens::arango::ServerState::instance()->isCoordinator()) {
    _traverser.reset(new triagens::arango::traverser::ClusterTraverser(
      edgeColls,
      opts,
      std::string(_trx->vocbase()->_name, strlen(_trx->vocbase()->_name)),
      _resolver,
      _expressions
    ));
  } else {
    std::vector<TRI_document_collection_t*> edgeCollections;
    for (auto const& coll : edgeColls) {
      TRI_voc_cid_t cid = _resolver->getCollectionId(coll);
      edgeCollections.push_back(_trx->documentCollection(cid));
    }
    _traverser.reset(new triagens::arango::traverser::DepthFirstTraverser(edgeCollections,
                                                                          opts,
                                                                          _resolver,
                                                                          _trx,
                                                                          _expressions));
  }
  if (!ep->usesInVariable()) {
    _vertexId = ep->getStartVertex();
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
  _calculationNodeId = ep->getCalculationNodeId();
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

void TraversalBlock::executeExpressions () {
  AqlItemBlock* cur = _buffer.front();
  for (auto& map : *_expressions) {
    for (size_t i = 0; i < map.second.size(); ++i) {
      // Right now no inVars are allowed.
      SimpleTraverserExpression* it = dynamic_cast<SimpleTraverserExpression*>(map.second.at(i));
      if (it != nullptr && it->expression != nullptr) {
        // inVars and inRegs needs fixx
        TRI_document_collection_t const* myCollection = nullptr; 
        AqlValue a = it->expression->execute(_trx, cur, _pos, _inVars[i], _inRegs[i], &myCollection);
        it->compareTo.reset(new Json(a.toJson(_trx, myCollection, true)));
        a.destroy();
      }
    }
  }
}

void TraversalBlock::executeFilterExpressions () {
  if (! _expressions->empty()) {
    if (_hasV8Expression) {
      bool const isRunningInCluster = triagens::arango::ServerState::instance()->isRunningInCluster();

      // must have a V8 context here to protect Expression::execute()
      auto engine = _engine;
      triagens::basics::ScopeGuard guard{
        [&engine]() -> void { 
          engine->getQuery()->enterContext(); 
        },
        [&]() -> void {
          if (isRunningInCluster) {
            // must invalidate the expression now as we might be called from
            // different threads
            for (auto const& map : *_expressions) {
              for (auto const& e : map.second) {
                auto casted = dynamic_cast<SimpleTraverserExpression*>(e);
                if (casted != nullptr) {
                  casted->expression->invalidate();
                }
              }
            }
          
            engine->getQuery()->exitContext(); 
          }
        }
      };

      ISOLATE;
      v8::HandleScope scope(isolate); // do not delete this!
    
      executeExpressions();
      TRI_IF_FAILURE("TraversalBlock::executeV8")  {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }
    else {
      // no V8 context required!

      Functions::InitializeThreadContext();
      try {
        executeExpressions();
        TRI_IF_FAILURE("TraversalBlock::executeExpression")  {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        Functions::DestroyThreadContext();
      }
      catch (...) {
        Functions::DestroyThreadContext();
        throw;
      }
    }
  }
}

int TraversalBlock::initializeCursor (AqlItemBlock* items, 
                                      size_t pos) {
  return ExecutionBlock::initializeCursor(items, pos);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read more paths
////////////////////////////////////////////////////////////////////////////////

bool TraversalBlock::morePaths (size_t hint) {
  freeCaches();
  _posInPaths = 0;
  if (! _traverser->hasMore()) {
    _engine->_stats.scannedIndex += _traverser->getAndResetReadDocuments();
    _engine->_stats.filtered += _traverser->getAndResetFilteredPaths();
    return false;
  }
  auto en = static_cast<TraversalNode const*>(getPlanNode());

  for (size_t j = 0; j < hint; ++j) {
    std::unique_ptr<triagens::arango::traverser::TraversalPath> p(_traverser->next());

    if (p == nullptr) {
      // There are no further paths available.
      break;
    }

    AqlValue pathValue;
    
    if (usesPathOutput() || (en->condition() != nullptr)) {
      pathValue = AqlValue(p->pathToJson(_trx, _resolver));
    }

    if ( usesVertexOutput() ) {
      _vertices.emplace_back(p->lastVertexToJson(_trx, _resolver));
    }
    if ( usesEdgeOutput() ) {
      _edges.emplace_back(p->lastEdgeToJson(_trx, _resolver));
    }
    if ( usesPathOutput() ) {
      _paths.emplace_back(pathValue);
    }
    _engine->_stats.scannedIndex += p->getReadDocuments();
  }

  _engine->_stats.scannedIndex += _traverser->getAndResetReadDocuments();
  _engine->_stats.filtered += _traverser->getAndResetFilteredPaths();
  // This is only save as long as _vertices is still build
  return _vertices.size() > 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skip the next paths
////////////////////////////////////////////////////////////////////////////////

size_t TraversalBlock::skipPaths (size_t hint) {
  freeCaches();
  _posInPaths = 0;
  if (! _traverser->hasMore()) {
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
  if (! _useRegister) {
    if (! _usedConstant) {
      _usedConstant = true;
      auto pos = _vertexId.find("/");
      auto v(triagens::arango::traverser::VertexId(
        _resolver->getCollectionIdCluster(_vertexId.substr(0, pos).c_str()),
        _vertexId.c_str() + pos + 1
      ));

      _traverser->setStartVertex(v);
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
      if (input.has(TRI_VOC_ATTRIBUTE_ID) ) {
        Json _idJson = input.get(TRI_VOC_ATTRIBUTE_ID);
        if (_idJson.isString()) {
          _vertexId = JsonHelper::getStringValue(_idJson.json(), "");
          VertexId v = triagens::arango::traverser::IdStringToVertexId (
            _resolver,
            _vertexId
          );
          _traverser->setStartVertex(v);
        }
      }
      else if (input.has("vertex")) {
        // This is used whenever the input is the result of another traversal.
        Json vertexJson = input.get("vertex");
        if (vertexJson.has(TRI_VOC_ATTRIBUTE_ID) ) {
          Json _idJson = vertexJson.get(TRI_VOC_ATTRIBUTE_ID);
          if (_idJson.isString()) {
            _vertexId = JsonHelper::getStringValue(_idJson.json(), "");
            VertexId v = triagens::arango::traverser::IdStringToVertexId (
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
      TRI_ASSERT(in.getTypeString() == "");
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
    executeFilterExpressions();
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
    executeFilterExpressions();
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
