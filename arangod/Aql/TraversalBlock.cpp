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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraversalBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringRef.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterTraverser.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Cluster/SmartGraphTraverser.h"
#endif
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/SingleServerTraverser.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using namespace arangodb::traverser;

TraversalBlock::TraversalBlock(ExecutionEngine* engine, TraversalNode const* ep)
    : ExecutionBlock(engine, ep),
      _posInPaths(0),
      _opts(nullptr),
      _traverser(nullptr),
      _reg(ExecutionNode::MaxRegisterId),
      _useRegister(false),
      _usedConstant(false),
      _vertexVar(nullptr),
      _vertexReg(0),
      _edgeVar(nullptr),
      _edgeReg(0),
      _pathVar(nullptr),
      _pathReg(0),
      _engines(nullptr) {
  auto const& registerPlan = ep->getRegisterPlan()->varInfo;
  ep->getConditionVariables(_inVars);
  for (auto const& v : _inVars) {
    auto it = registerPlan.find(v->id);
    TRI_ASSERT(it != registerPlan.end());
    _inRegs.emplace_back(it->second.registerId);
  }

  _opts = static_cast<TraverserOptions*>(ep->options());
  TRI_ASSERT(_opts != nullptr);
  _mmdr.reset(new ManagedDocumentResult);

  if (arangodb::ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    if (ep->isSmart()) {
      _traverser.reset(new arangodb::traverser::SmartGraphTraverser(
          _opts,
          _mmdr.get(),
          ep->engines(),
          _trx->vocbase()->name(),
          _trx));
    } else {
#endif
      _traverser.reset(new arangodb::traverser::ClusterTraverser(
          _opts,
          _mmdr.get(),
          ep->engines(),
          _trx->vocbase()->name(),
          _trx));
#ifdef USE_ENTERPRISE
    }
#endif
  } else {
    _traverser.reset(
        new arangodb::traverser::SingleServerTraverser(_opts, _trx, _mmdr.get()));
  }
  if (!ep->usesEdgeOutVariable() && !ep->usesPathOutVariable() &&
      _opts->useBreadthFirst &&
      _opts->uniqueVertices ==
          traverser::TraverserOptions::UniquenessLevel::GLOBAL) {
    _traverser->allowOptimizedNeighbors();
  }
  if (!ep->usesInVariable()) {
    _vertexId = ep->getStartVertex();
  } else {
    auto it = ep->getRegisterPlan()->varInfo.find(ep->inVariable()->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    _reg = it->second.registerId;
    _useRegister = true;
  }
  if (ep->usesVertexOutVariable()) {
    _vertexVar = ep->vertexOutVariable();
  }

  if (ep->usesEdgeOutVariable()) {
    _edgeVar = ep->edgeOutVariable();
  }

  if (ep->usesPathOutVariable()) {
    _pathVar = ep->pathOutVariable();
  }

  if (arangodb::ServerState::instance()->isCoordinator()) {
    _engines = ep->engines();
  }
}

TraversalBlock::~TraversalBlock() {
  freeCaches();
}

void TraversalBlock::freeCaches() {
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

int TraversalBlock::initialize() {
  DEBUG_BEGIN_BLOCK();
  int res = ExecutionBlock::initialize();
  auto varInfo = getPlanNode()->getRegisterPlan()->varInfo;

  if (usesVertexOutput()) {
    TRI_ASSERT(_vertexVar != nullptr);
    auto it = varInfo.find(_vertexVar->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _vertexReg = it->second.registerId;
  }
  if (usesEdgeOutput()) {
    TRI_ASSERT(_edgeVar != nullptr);
    auto it = varInfo.find(_edgeVar->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _edgeReg = it->second.registerId;
  }
  if (usesPathOutput()) {
    TRI_ASSERT(_pathVar != nullptr);
    auto it = varInfo.find(_pathVar->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < ExecutionNode::MaxRegisterId);
    _pathReg = it->second.registerId;
  }

  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int TraversalBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  _pos = 0;
  _posInPaths = 0;
  _usedConstant = false;
  freeCaches();
  _traverser->done();
  return ExecutionBlock::initializeCursor(items, pos);
}

/// @brief shutdown: Inform all traverser Engines to destroy themselves
int TraversalBlock::shutdown(int errorCode) {
  DEBUG_BEGIN_BLOCK();
  // We have to clean up the engines in Coordinator Case.
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto cc = arangodb::ClusterComm::instance();
    if (cc != nullptr) {
      // nullptr only happens on controlled server shutdown
      std::string const url(
          "/_db/" + arangodb::basics::StringUtils::urlEncode(_trx->vocbase()->name()) +
          "/_internal/traverser/");
      for (auto const& it : *_engines) {
        arangodb::CoordTransactionID coordTransactionID = TRI_NewTickServer();
        std::unordered_map<std::string, std::string> headers;
        auto res = cc->syncRequest(
            "", coordTransactionID, "server:" + it.first, RequestType::DELETE_REQ,
            url + arangodb::basics::StringUtils::itoa(it.second), "", headers,
            30.0);
        if (res->status != CL_COMM_SENT) {
          // Note If there was an error on server side we do not have CL_COMM_SENT
          std::string message("Could not destroy all traversal engines");
          if (!res->errorMessage.empty()) {
            message += std::string(": ") + res->errorMessage;
          }
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << message;
        }
      }
    }
  }

  return ExecutionBlock::shutdown(errorCode);

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief read more paths
bool TraversalBlock::morePaths(size_t hint) {
  DEBUG_BEGIN_BLOCK();
  
  freeCaches();
  _posInPaths = 0;
  if (!_traverser->hasMore()) {
    _engine->_stats.scannedIndex += _traverser->getAndResetReadDocuments();
    _engine->_stats.filtered += _traverser->getAndResetFilteredPaths();
    return false;
  }
  
  if (usesVertexOutput()) {
    _vertices.reserve(hint);
  }
  if (usesEdgeOutput()) {
    _edges.reserve(hint);
  }
  if (usesPathOutput()) {
    _paths.reserve(hint);
  }

  transaction::BuilderLeaser tmp(_trx);
  for (size_t j = 0; j < hint; ++j) {
    if (!_traverser->next()) {
      // There are no further paths available.
      break;
    }

    if (usesVertexOutput()) {
      _vertices.emplace_back(_traverser->lastVertexToAqlValue());
    }
    if (usesEdgeOutput()) {
      _edges.emplace_back(_traverser->lastEdgeToAqlValue());
    }
    if (usesPathOutput()) {
      tmp->clear();
      _paths.emplace_back(_traverser->pathToAqlValue(*tmp.builder()));
    }
    throwIfKilled();  // check if we were aborted
  }

  _engine->_stats.scannedIndex += _traverser->getAndResetReadDocuments();
  _engine->_stats.filtered += _traverser->getAndResetFilteredPaths();
  return !_vertices.empty();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skip the next paths
size_t TraversalBlock::skipPaths(size_t hint) {
  DEBUG_BEGIN_BLOCK();
  freeCaches();
  _posInPaths = 0;
  if (!_traverser->hasMore()) {
    return 0;
  }
  return _traverser->skip(hint);

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

void TraversalBlock::initializeExpressions(AqlItemBlock const* items, size_t pos) {
  // Initialize the Expressions within the options.
  // We need to find the variable and read its value here. Everything is computed right now.
  _opts->clearVariableValues();
  TRI_ASSERT(_inVars.size() == _inRegs.size());
  for (size_t i = 0; i < _inVars.size(); ++i) {
    _opts->setVariableValue(_inVars[i], items->getValueReference(pos, _inRegs[i]));
  }
  // IF cluster => Transfer condition.
}

/// @brief initialize the list of paths
void TraversalBlock::initializePaths(AqlItemBlock const* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  if (!_vertices.empty()) {
    // No Initialization required.
    return;
  }
        
  initializeExpressions(items, pos);

  if (!_useRegister) {
    if (!_usedConstant) {
      _usedConstant = true;
      auto pos = _vertexId.find('/');
      if (pos == std::string::npos) {
        _engine->getQuery()->registerWarning(
            TRI_ERROR_BAD_PARAMETER, "Invalid input for traversal: "
                                         "Only id strings or objects with "
                                         "_id are allowed");
      } else {
        _traverser->setStartVertex(_vertexId);
      }
    }
  } else {
    AqlValue const& in = items->getValueReference(_pos, _reg);
    if (in.isObject()) {
      try {
        _traverser->setStartVertex(_trx->extractIdString(in.slice()));
      }
      catch (...) {
        // _id or _key not present... ignore this error and fall through
      }
    } else if (in.isString()) {
      _vertexId = in.slice().copyString();
      _traverser->setStartVertex(_vertexId);
    } else {
      _engine->getQuery()->registerWarning(
          TRI_ERROR_BAD_PARAMETER, "Invalid input for traversal: Only "
                                       "id strings or objects with _id are "
                                       "allowed");
    }
  }

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief getSome
AqlItemBlock* TraversalBlock::getSome(size_t,  // atLeast,
                                      size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();
  while (true) {
    if (_done) {
      traceGetSomeEnd(nullptr);
      return nullptr;
    }

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        traceGetSomeEnd(nullptr);
        return nullptr;
      }
      _pos = 0;  // this is in the first block
    }

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();
    size_t const curRegs = cur->getNrRegs();

    if (_pos == 0 && !_traverser->hasMore()) {
      // Initial initialization
      initializePaths(cur, _pos);
    }

    // Iterate more paths:
    if (_posInPaths >= _vertices.size()) {
      if (!morePaths(atMost)) {
        // This input does not have any more paths. maybe the next one has.
        // we can only return nullptr iff the buffer is empty.
        _usedConstant = false; // must reset this variable because otherwise the traverser's start vertex may not be reset properly
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        } else {
          initializePaths(cur, _pos);
        }
        continue;
      }
    }

    size_t available = _vertices.size() - _posInPaths;
    size_t toSend = (std::min)(atMost, available);

    RegisterId nrRegs =
        getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];

    std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrRegs));
    // automatically freed if we throw
    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    for (size_t j = 0; j < toSend; j++) {
      if (usesVertexOutput()) {
        res->setValue(j, _vertexReg, _vertices[_posInPaths].clone());
      }
      if (usesEdgeOutput()) {
        res->setValue(j, _edgeReg, _edges[_posInPaths].clone());
      }
      if (usesPathOutput()) {
        res->setValue(j, _pathReg, _paths[_posInPaths].clone());
      }
      if (j > 0) {
        // re-use already copied AqlValues
        res->copyValuesFromFirstRow(j, static_cast<RegisterId>(curRegs));
      }
      ++_posInPaths;
    }
    // Advance read position:
    if (_posInPaths >= _vertices.size()) {
      // we have exhausted our local paths buffer
      // fetch more paths into our buffer
      if (!morePaths(atMost)) {
        // nothing more to read, re-initialize fetching of paths

        _usedConstant = false; // must reset this variable because otherwise the traverser's start vertex may not be reset properly
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        } else {
          initializePaths(cur, _pos);
        }
      }
    }

    // Clear out registers no longer needed later:
    clearRegisters(res.get());
    traceGetSomeEnd(res.get());
    return res.release();
  }

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief skipSome
size_t TraversalBlock::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;

  if (_done) {
    return skipped;
  }

  if (_posInPaths < _vertices.size()) {
    skipped += (std::min)(atMost, _vertices.size() - _posInPaths);
    _posInPaths += skipped;
  }

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block
    }

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();
    initializePaths(cur, _pos);
  
    while (atMost > skipped) {
      TRI_ASSERT(atMost >= skipped);
      skipped += skipPaths(atMost - skipped);
  
      if (_traverser->hasMore()) {
        continue;
      }

      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        delete cur;
        _pos = 0;
        break;
      }
    
      initializePaths(cur, _pos);
    }
  }
  
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}
