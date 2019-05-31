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
#include "Aql/PruneExpressionEvaluator.h"
#include "Aql/Query.h"
#include "Basics/StringRef.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterTraverser.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Cluster/SmartGraphTraverser.h"
#endif
#include "Graph/SingleServerTraverser.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
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
          _opts, _mmdr.get(), ep->engines(), _trx->vocbase().name(), _trx));
    } else {
#endif
      _traverser.reset(
          new arangodb::traverser::ClusterTraverser(_opts, _mmdr.get(), ep->engines(),
                                                    _trx->vocbase().name(), _trx));
#ifdef USE_ENTERPRISE
    }
#endif
  } else {
    _traverser.reset(
        new arangodb::traverser::SingleServerTraverser(_opts, _trx, _mmdr.get()));
  }
  if (!ep->usesEdgeOutVariable() && !ep->usesPathOutVariable() && _opts->useBreadthFirst &&
      _opts->uniqueVertices == traverser::TraverserOptions::UniquenessLevel::GLOBAL) {
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

  if (ep->pruneExpression() != nullptr) {
    std::vector<Variable const*> pruneVars;
    ep->getPruneVariables(pruneVars);
    std::vector<RegisterId> pruneRegs;
    // Create List for _pruneVars
    pruneRegs.reserve(pruneVars.size());
    size_t vertexRegIdx = std::numeric_limits<std::size_t>::max();
    size_t edgeRegIdx = std::numeric_limits<std::size_t>::max();
    size_t pathRegIdx = std::numeric_limits<std::size_t>::max();
    for (auto const v : pruneVars) {
      auto it = varInfo.find(v->id);
      TRI_ASSERT(it != varInfo.end());
      if (v == _vertexVar) {
        vertexRegIdx = pruneRegs.size();
      } else if (v == _edgeVar) {
        edgeRegIdx = pruneRegs.size();
      } else if (v == _pathVar) {
        pathRegIdx = pruneRegs.size();
      }
      pruneRegs.emplace_back(it->second.registerId);
    }
    _opts->activatePrune(std::move(pruneVars), std::move(pruneRegs),
                         vertexRegIdx, edgeRegIdx, pathRegIdx, ep->pruneExpression());
  }
}

TraversalBlock::~TraversalBlock() { freeCaches(); }

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

std::pair<ExecutionState, arangodb::Result> TraversalBlock::initializeCursor(AqlItemBlock* items,
                                                                             size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _pos = 0;
  _posInPaths = 0;
  _usedConstant = false;
  freeCaches();
  _traverser->done();
  _skipped = 0;

  return res;
}

/// @brief shutdown: Inform all traverser Engines to destroy themselves
std::pair<ExecutionState, Result> TraversalBlock::shutdown(int errorCode) {
  ExecutionState state;
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {state, result};
  }

  // We have to clean up the engines in Coordinator Case.
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto cc = arangodb::ClusterComm::instance();

    if (cc != nullptr) {
      // nullptr only happens on controlled server shutdown
      std::string const url(
          "/_db/" + arangodb::basics::StringUtils::urlEncode(_trx->vocbase().name()) +
          "/_internal/traverser/");

      for (auto const& it : *_engines) {
        arangodb::CoordTransactionID coordTransactionID = TRI_NewTickServer();
        std::unordered_map<std::string, std::string> headers;
        auto res = cc->syncRequest("", coordTransactionID, "server:" + it.first,
                                   RequestType::DELETE_REQ,
                                   url + arangodb::basics::StringUtils::itoa(it.second),
                                   "", headers, 30.0);

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

  return {state, result};
}

/// @brief read more paths from _traverser. returns true if there are more
/// paths.
bool TraversalBlock::getSomePaths(size_t hint) {
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
}

/// @brief skip the next paths
size_t TraversalBlock::skipPaths(size_t hint) {
  freeCaches();
  _posInPaths = 0;
  if (!_traverser->hasMore()) {
    return 0;
  }
  return _traverser->skip(hint);
}

void TraversalBlock::initializeExpressions(AqlItemBlock const* items, size_t pos) {
  // Initialize the Expressions within the options.
  // We need to find the variable and read its value here. Everything is computed right now.
  _opts->clearVariableValues();
  TRI_ASSERT(_inVars.size() == _inRegs.size());
  for (size_t i = 0; i < _inVars.size(); ++i) {
    _opts->setVariableValue(_inVars[i], items->getValueReference(pos, _inRegs[i]));
  }
  if (_opts->usesPrune()) {
    auto* evaluator = _opts->getPruneEvaluator();
    // Replace by inputRow
    evaluator->prepareContext(items, pos);
  }
  // IF cluster => Transfer condition.
}

/// @brief initialize the list of paths
void TraversalBlock::initializePaths(AqlItemBlock const* items, size_t pos) {
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
        _engine->getQuery()->registerWarning(TRI_ERROR_BAD_PARAMETER,
                                             "Invalid input for traversal: "
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
      } catch (...) {
        // _id or _key not present... ignore this error and fall through
      }
    } else if (in.isString()) {
      _vertexId = in.slice().copyString();
      _traverser->setStartVertex(_vertexId);
    } else {
      _engine->getQuery()->registerWarning(TRI_ERROR_BAD_PARAMETER,
                                           "Invalid input for traversal: Only "
                                           "id strings or objects with _id are "
                                           "allowed");
    }
  }
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> TraversalBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  RegisterId const nrOutRegs = getNrOutputRegisters();
  RegisterId const nrInRegs = getNrInputRegisters();

  while (!_done && _skipped < atMost) {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    BufferState bufferState = getBlockIfNeeded(toFetch);

    if (bufferState == BufferState::WAITING) {
      traceGetSomeEnd(nullptr, bufferState);
      return {ExecutionState::WAITING, nullptr};
    }
    if (bufferState == BufferState::NO_MORE_BLOCKS) {
      break;
    }
    TRI_ASSERT(bufferState == BufferState::HAS_BLOCKS ||
               bufferState == BufferState::HAS_NEW_BLOCK);

    TRI_ASSERT(!_buffer.empty());

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();
    TRI_ASSERT(cur != nullptr);
    TRI_ASSERT(nrInRegs == cur->getNrRegs());

    // Initialization on the first row of each new block
    if (bufferState == BufferState::HAS_NEW_BLOCK) {
      // A new row (and therefore block) should only be fetched at the very
      // beginning, or after the traverser is completely processed; in either
      // case, the traverser should be done.
      TRI_ASSERT(_pos == 0 && !_traverser->hasMore());
      initializePaths(cur, _pos);
    }

    if (!_vertices.empty()) {
      TRI_ASSERT(_posInPaths < _vertices.size());

      size_t available = _vertices.size() - _posInPaths;
      size_t toSend = (std::min)(atMost - _skipped, available);

      // automatically freed if we throw
      std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrOutRegs));
      TRI_ASSERT(nrInRegs <= res->getNrRegs());

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
          res->copyValuesFromFirstRow(j, nrInRegs);
        }
        ++_posInPaths;
      }

      _collector.add(std::move(res));

      advanceCursor(0, toSend);
    }

    // if there are no more paths left, reset traverser (in getSomePaths),
    // move to the next input row and re-initialize the paths unless we
    // switched to the next input block. In case we processed the current block
    // fully, we can't initialize the paths yet as we need the row for this:
    // this will be done after the next block is fetched.
    if (_posInPaths >= _vertices.size() && !getSomePaths(atMost)) {
      _usedConstant = false;
      AqlItemBlock* removedBlock = advanceCursor(1, 0);
      if (removedBlock == nullptr) {
        initializePaths(cur, _pos);
      }
      returnBlockUnlessNull(removedBlock);
    }
  }

  std::unique_ptr<AqlItemBlock> result(_collector.steal());
  _skipped = 0;

  // Clear out registers no longer needed later:
  clearRegisters(result.get());
  traceGetSomeEnd(result.get(), getHasMoreState());
  return {getHasMoreState(), std::move(result)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> TraversalBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  // eat as much as possible from _vertices first
  if (_posInPaths < _vertices.size()) {
    size_t const skip = (std::min)(atMost, _vertices.size() - _posInPaths);
    advanceCursor(0, skip);
    _posInPaths += skip;
  }
  // now, _vertices is either empty, or _skipped == atMost.
  TRI_ASSERT(_vertices.empty() || _skipped == atMost);

  while (_skipped < atMost) {
    BufferState bufferState = getBlockIfNeeded(atMost);

    if (bufferState == BufferState::WAITING) {
      traceSkipSomeEnd(0, ExecutionState::WAITING);
      return {ExecutionState::WAITING, 0};
    }
    if (bufferState == BufferState::NO_MORE_BLOCKS) {
      break;
    }

    TRI_ASSERT(bufferState == BufferState::HAS_BLOCKS ||
               bufferState == BufferState::HAS_NEW_BLOCK);
    TRI_ASSERT(!_buffer.empty());

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();

    // Initialization on the first row of each new block
    if (bufferState == BufferState::HAS_NEW_BLOCK) {
      // A new row (and therefore block) should only be fetched at the very
      // beginning, or after the traverser is completely processed; in either
      // case, the traverser should be done.
      TRI_ASSERT(_pos == 0 && !_traverser->hasMore());
      initializePaths(cur, _pos);
    }

    TRI_ASSERT(atMost >= _skipped);
    size_t const skip = skipPaths(atMost - _skipped);
    advanceCursor(0, skip);

    TRI_ASSERT(skip != 0 || !_traverser->hasMore());

    if (!_traverser->hasMore()) {
      AqlItemBlock* removedBlock = advanceCursor(1, 0);
      if (removedBlock == nullptr) {
        initializePaths(cur, _pos);
      }
      returnBlockUnlessNull(removedBlock);
    }
  }

  size_t skipped = _skipped;
  _skipped = 0;
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}
