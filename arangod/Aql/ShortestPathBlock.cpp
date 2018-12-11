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

#include "ShortestPathBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Cluster/ClusterComm.h"
#include "Graph/AttributeWeightShortestPathFinder.h"
#include "Graph/ConstantWeightShortestPathFinder.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathResult.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

ShortestPathBlock::ShortestPathBlock(ExecutionEngine* engine,
                                     ShortestPathNode const* ep)
    : ExecutionBlock(engine, ep),
      _vertexVar(nullptr),
      _vertexReg(ExecutionNode::MaxRegisterId),
      _edgeVar(nullptr),
      _edgeReg(ExecutionNode::MaxRegisterId),
      _opts(static_cast<ShortestPathOptions*>(ep->options())),
      _posInPath(0),
      _pathLength(0),
      _path(nullptr),
      _startReg(ExecutionNode::MaxRegisterId),
      _useStartRegister(false),
      _targetReg(ExecutionNode::MaxRegisterId),
      _useTargetRegister(false),
      _usedConstant(false),
      _engines(nullptr) {
  TRI_ASSERT(_opts != nullptr);

  if (!ep->usesStartInVariable()) {
    _startVertexId = ep->getStartVertex();
  } else {
    auto it = ep->getRegisterPlan()->varInfo.find(ep->startInVariable()->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    _startReg = it->second.registerId;
    _useStartRegister = true;
  }

  if (!ep->usesTargetInVariable()) {
    _targetVertexId = ep->getTargetVertex();
  } else {
    auto it = ep->getRegisterPlan()->varInfo.find(ep->targetInVariable()->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    _targetReg = it->second.registerId;
    _useTargetRegister = true;
  }

  if (ep->usesVertexOutVariable()) {
    _vertexVar = ep->vertexOutVariable();
  }

  if (ep->usesEdgeOutVariable()) {
    _edgeVar = ep->edgeOutVariable();
  }
  _path = std::make_unique<arangodb::graph::ShortestPathResult>();

  if (_opts->useWeight()) {
    _finder.reset(
        new arangodb::graph::AttributeWeightShortestPathFinder(_opts));
  } else {
    _finder.reset(
        new arangodb::graph::ConstantWeightShortestPathFinder(_opts));
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
}

std::pair<ExecutionState, arangodb::Result> ShortestPathBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);
  
  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }
  _posInPath = 0;
  _pathLength = 0;
  _usedConstant = false;

  return res;
}

/// @brief shutdown: Inform all traverser Engines to destroy themselves
std::pair<ExecutionState, Result> ShortestPathBlock::shutdown(int errorCode) {
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
        "/_db/"
        + arangodb::basics::StringUtils::urlEncode(_trx->vocbase().name())
        + "/_internal/traverser/"
      );

      for (auto const& it : *_engines) {
        arangodb::CoordTransactionID coordTransactionID = TRI_NewTickServer();
        std::unordered_map<std::string, std::string> headers;
        auto res = cc->syncRequest(
            coordTransactionID, "server:" + it.first, RequestType::DELETE_REQ,
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

  return {state, result};
}

bool ShortestPathBlock::nextPath(AqlItemBlock const* items) {
  if (_usedConstant) {
    // Both source and target are constant.
    // Just one path to compute
    return false;
  }
  _path->clear();
  if (!_useStartRegister && !_useTargetRegister) {
    // Both are constant, after this computation we are done
    _usedConstant = true;
  }
  if (!_useStartRegister) {
    auto pos = _startVertexId.find('/');
    if (pos == std::string::npos) {
      _engine->getQuery()->registerWarning(TRI_ERROR_BAD_PARAMETER,
                                           "Invalid input for Shortest Path: "
                                           "Only id strings or objects with "
                                           "_id are allowed");
      return false;
    } else {
      _opts->setStart(_startVertexId);
    }
  } else {
    AqlValue const& in = items->getValueReference(_pos, _startReg);
    if (in.isObject()) {
      try {
        _opts->setStart(_trx->extractIdString(in.slice()));
      } catch (...) {
        // _id or _key not present... ignore this error and fall through
        // returning no path
        return false;
      }
    } else if (in.isString()) {
      _startVertexId = in.slice().copyString();
      _opts->setStart(_startVertexId);
    } else {
      _engine->getQuery()->registerWarning(
          TRI_ERROR_BAD_PARAMETER,
          "Invalid input for Shortest Path: Only "
          "id strings or objects with _id are "
          "allowed");
      return false;
    }
  }

  if (!_useTargetRegister) {
    auto pos = _targetVertexId.find('/');
    if (pos == std::string::npos) {
      _engine->getQuery()->registerWarning(TRI_ERROR_BAD_PARAMETER,
                                           "Invalid input for Shortest Path: "
                                           "Only id strings or objects with "
                                           "_id are allowed");
      return false;
    } else {
      _opts->setEnd(_targetVertexId);
    }
  } else {
    AqlValue const& in = items->getValueReference(_pos, _targetReg);
    if (in.isObject()) {
      try {
        std::string idString = _trx->extractIdString(in.slice());
        _opts->setEnd(idString);
      } catch (...) {
        // _id or _key not present... ignore this error and fall through
        // returning no path
        return false;
      }
    } else if (in.isString()) {
      _targetVertexId = in.slice().copyString();
      _opts->setEnd(_targetVertexId);
    } else {
      _engine->getQuery()->registerWarning(
          TRI_ERROR_BAD_PARAMETER,
          "Invalid input for Shortest Path: Only "
          "id strings or objects with _id are "
          "allowed");
      return false;
    }
  }

  VPackSlice start = _opts->getStart();
  VPackSlice end = _opts->getEnd();
  TRI_ASSERT(_finder != nullptr);

  bool hasPath =
      _finder->shortestPath(start, end, *_path, [this]() { throwIfKilled(); });

  if (hasPath) {
    _posInPath = 0;
    _pathLength = _path->length();
  }

  return hasPath;
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ShortestPathBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  RegisterId const nrInRegs = getNrInputRegisters();
  RegisterId const nrOutRegs = getNrOutputRegisters();
  while (true) {
    if (_done) {
      TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
      traceGetSomeEnd(nullptr, ExecutionState::DONE);
      return {ExecutionState::DONE, nullptr};
    }

    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      ExecutionState state;
      bool blockAppended;
      std::tie(state, blockAppended) = ExecutionBlock::getBlock(toFetch);
      if (state == ExecutionState::WAITING) {
        traceGetSomeEnd(nullptr, ExecutionState::WAITING);
        return {ExecutionState::WAITING, nullptr};
      }
      if (!blockAppended) {
        _done = true;
        TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
        traceGetSomeEnd(nullptr, ExecutionState::DONE);
        return {ExecutionState::DONE, nullptr};
      }
      _pos = 0;  // this is in the first block
    }

    // If we get here, we do have _buffer.front()
    AqlItemBlock* cur = _buffer.front();
    TRI_ASSERT(cur != nullptr);
    TRI_ASSERT(nrInRegs == cur->getNrRegs());

    // Collect the next path:
    if (_posInPath >= _pathLength) {
      if (!nextPath(cur)) {
        // This input does not have any path. maybe the next one has.
        // we can only return nullptr iff the buffer is empty.
        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        }
        continue;
      }
    }

    size_t available = _pathLength - _posInPath;
    size_t toSend = (std::min)(atMost, available);

    std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrOutRegs));
    // automatically freed if we throw
    TRI_ASSERT(nrInRegs <= nrOutRegs);

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    for (size_t j = 0; j < toSend; j++) {
      if (usesVertexOutput()) {
        res->setValue(j, _vertexReg,
            _path->vertexToAqlValue(_opts->cache(), _posInPath));
      }
      if (usesEdgeOutput()) {
        res->setValue(j, _edgeReg,
            _path->edgeToAqlValue(_opts->cache(), _posInPath));
      }
      if (j > 0) {
        // re-use already copied aqlvalues
        res->copyValuesFromFirstRow(j, nrInRegs);
      }
      ++_posInPath;
    }

    if (_posInPath >= _pathLength) {
      // Advance read position for next call
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      }
    }

    // Clear out registers no longer needed later:
    clearRegisters(res.get());
    traceGetSomeEnd(res.get(), getHasMoreState());
    return {getHasMoreState(), std::move(res)};
  }
}

std::pair<ExecutionState, size_t> ShortestPathBlock::skipSome(size_t atMost) {
  // TODO implement without data reading
  // There is a regression test for this:
  // testShortestPathDijkstraOutboundSkipFirst in aql-graph.js
  auto res = getSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    return {res.first, 0};
  }
  return {res.first, res.second->size()};
}
