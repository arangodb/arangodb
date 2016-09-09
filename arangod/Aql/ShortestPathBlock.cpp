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

#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Utils/AqlTransaction.h"
#include "Utils/OperationCursor.h"
#include "Utils/Transaction.h"
#include "VocBase/EdgeCollectionInfo.h"
#include "VocBase/MasterPointer.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instantiation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef arangodb::basics::DynamicDistanceFinder<
    arangodb::velocypack::Slice, arangodb::velocypack::Slice, double,
    arangodb::traverser::ShortestPath> ArangoDBPathFinder;

typedef arangodb::basics::ConstDistanceFinder<arangodb::velocypack::Slice,
                                              arangodb::velocypack::Slice,
                                              arangodb::basics::VelocyPackHelper::VPackStringHash, 
                                              arangodb::basics::VelocyPackHelper::VPackStringEqual,
                                              arangodb::traverser::ShortestPath>
    ArangoDBConstDistancePathFinder;



using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief Local class to expand edges.
///        Will be handed over to the path finder
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace aql {
struct ConstDistanceExpanderLocal {
 private:
  /// @brief reference to the Block
  ShortestPathBlock const* _block; 

  /// @brief Defines if this expander follows the edges in reverse
  bool _isReverse;

  /// @brief Local cursor vector
  std::vector<TRI_doc_mptr_t*> _cursor;

 public:
  ConstDistanceExpanderLocal(ShortestPathBlock const* block,
                             bool isReverse)
      : _block(block), _isReverse(isReverse) {}

  void operator()(VPackSlice const& v, std::vector<VPackSlice>& resEdges,
                  std::vector<VPackSlice>& neighbors) {
    std::shared_ptr<arangodb::OperationCursor> edgeCursor;
    for (auto const& edgeCollection : _block->_collectionInfos) {
      TRI_ASSERT(edgeCollection != nullptr);
      if (_isReverse) {
        edgeCursor = edgeCollection->getReverseEdges(v);
      } else {
        edgeCursor = edgeCollection->getEdges(v);
      }
      // Clear the local cursor before using the
      // next edge cursor.
      // While iterating over the edge cursor, _cursor
      // has to stay intact.
      _cursor.clear();
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(_cursor, UINT64_MAX);
        for (auto const& mptr : _cursor) {
          VPackSlice edge(mptr->vpack());
          VPackSlice from =
              arangodb::Transaction::extractFromFromDocument(edge);
          if (from == v) {
            VPackSlice to = arangodb::Transaction::extractToFromDocument(edge);
            if (to != v) {
              resEdges.emplace_back(edge);
              neighbors.emplace_back(to);
            }
          } else {
            resEdges.emplace_back(edge);
            neighbors.emplace_back(from);
          }
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster class to expand edges.
///        Will be handed over to the path finder
////////////////////////////////////////////////////////////////////////////////

struct ConstDistanceExpanderCluster {
 private:

  /// @brief reference to the Block
  ShortestPathBlock* _block; 

  /// @brief Defines if this expander follows the edges in reverse
  bool _isReverse;

 public:
  ConstDistanceExpanderCluster(ShortestPathBlock* block, bool isReverse)
      : _block(block), _isReverse(isReverse) {}

  void operator()(VPackSlice const& v, std::vector<VPackSlice>& resEdges,
                  std::vector<VPackSlice>& neighbors) {
    int res = TRI_ERROR_NO_ERROR;
    for (auto const& edgeCollection : _block->_collectionInfos) {
      VPackBuilder result;
      TRI_ASSERT(edgeCollection != nullptr);
      if (_isReverse) {
        res = edgeCollection->getReverseEdgesCoordinator(v, result);
      } else {
        res = edgeCollection->getEdgesCoordinator(v, result);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      VPackSlice edges = result.slice().get("edges");
      for (auto const& edge : VPackArrayIterator(edges)) {
        VPackSlice from = arangodb::Transaction::extractFromFromDocument(edge);
        if (from == v) {
          VPackSlice to = arangodb::Transaction::extractToFromDocument(edge);
          if (to != v) {
            resEdges.emplace_back(edge);
            neighbors.emplace_back(to);
          }
        } else {
          resEdges.emplace_back(edge);
          neighbors.emplace_back(from);
        }
      }
      // Make sure the data Slices are pointing to is not running out of scope.
      // This is not thread-safe!
      _block->_coordinatorCache.emplace_back(result.steal());
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Expander for weighted edges
////////////////////////////////////////////////////////////////////////////////

struct EdgeWeightExpanderLocal {

 private:

  /// @brief reference to the Block
  ShortestPathBlock const* _block; 

  /// @brief Defines if this expander follows the edges in reverse
  bool _reverse;

 public:
  EdgeWeightExpanderLocal(
      ShortestPathBlock const* block, bool reverse)
      : _block(block), _reverse(reverse) {}

  void inserter(std::unordered_map<VPackSlice, size_t>& candidates,
                std::vector<ArangoDBPathFinder::Step*>& result,
                VPackSlice const& s, VPackSlice const& t, double currentWeight,
                VPackSlice edge) {
    auto cand = candidates.find(t);
    if (cand == candidates.end()) {
      // Add weight
      auto step = std::make_unique<ArangoDBPathFinder::Step>(
          t, s, currentWeight, edge);
      result.emplace_back(step.release());
      candidates.emplace(t, result.size() - 1);
    } else {
      // Compare weight
      auto old = result[cand->second];
      auto oldWeight = old->weight();
      if (currentWeight < oldWeight) {
        old->setWeight(currentWeight);
        old->_predecessor = s;
        old->_edge = edge;
      }
    }
  }

  void operator()(VPackSlice const& source,
                  std::vector<ArangoDBPathFinder::Step*>& result) {
    std::vector<TRI_doc_mptr_t*> cursor;
    std::unique_ptr<arangodb::OperationCursor> edgeCursor;
    std::unordered_map<VPackSlice, size_t> candidates;
    for (auto const& edgeCollection : _block->_collectionInfos) {
      TRI_ASSERT(edgeCollection != nullptr);
      if (_reverse) {
        edgeCursor = edgeCollection->getReverseEdges(source);
      } else {
        edgeCursor = edgeCollection->getEdges(source);
      }
      
      candidates.clear();

      // Clear the local cursor before using the
      // next edge cursor.
      // While iterating over the edge cursor, _cursor
      // has to stay intact.
      cursor.clear();
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(cursor, UINT64_MAX);
        for (auto const& mptr : cursor) {
          VPackSlice edge(mptr->vpack());
          VPackSlice from =
              arangodb::Transaction::extractFromFromDocument(edge);
          VPackSlice to = arangodb::Transaction::extractToFromDocument(edge);
          double currentWeight = edgeCollection->weightEdge(edge);
          if (from == source) {
            inserter(candidates, result, from, to, currentWeight, edge);
          } else {
            inserter(candidates, result, to, from, currentWeight, edge);
          }
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Expander for weighted edges
////////////////////////////////////////////////////////////////////////////////

struct EdgeWeightExpanderCluster {

 private:

  /// @brief reference to the Block
  ShortestPathBlock* _block;

  /// @brief Defines if this expander follows the edges in reverse
  bool _reverse;

 public:
  EdgeWeightExpanderCluster(ShortestPathBlock* block, bool reverse)
      : _block(block), _reverse(reverse) {}

  void operator()(VPackSlice const& source,
                  std::vector<ArangoDBPathFinder::Step*>& result) {
    int res = TRI_ERROR_NO_ERROR;
    std::unordered_map<VPackSlice, size_t> candidates;

    for (auto const& edgeCollection : _block->_collectionInfos) {
      TRI_ASSERT(edgeCollection != nullptr);
      VPackBuilder edgesBuilder;
      if (_reverse) {
        res = edgeCollection->getReverseEdgesCoordinator(source, edgesBuilder);
      } else {
        res = edgeCollection->getEdgesCoordinator(source, edgesBuilder);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }

      candidates.clear();

      auto inserter = [&](VPackSlice const& s, VPackSlice const& t,
                          double currentWeight, VPackSlice const& edge) {
        auto cand = candidates.find(t);
        if (cand == candidates.end()) {
          // Add weight
          auto step = std::make_unique<ArangoDBPathFinder::Step>(
              t, s, currentWeight, edge);
          result.emplace_back(step.release());
          candidates.emplace(t, result.size() - 1);
        } else {
          // Compare weight
          auto old = result[cand->second];
          auto oldWeight = old->weight();
          if (currentWeight < oldWeight) {
            old->setWeight(currentWeight);
            old->_predecessor = s;
            old->_edge = edge;
          }
        }
      };

      VPackSlice edges = edgesBuilder.slice().get("edges");
      for (auto const& edge : VPackArrayIterator(edges)) {
        VPackSlice from = arangodb::Transaction::extractFromFromDocument(edge);
        VPackSlice to = arangodb::Transaction::extractToFromDocument(edge);
        double currentWeight = edgeCollection->weightEdge(edge);
        if (from == source) {
          inserter(from, to, currentWeight, edge);
        } else {
          inserter(to, from, currentWeight, edge);
        }
      }
      _block->_coordinatorCache.emplace_back(edgesBuilder.steal());
    }
  }
};
}
}

ShortestPathBlock::ShortestPathBlock(ExecutionEngine* engine,
                                     ShortestPathNode const* ep)
    : ExecutionBlock(engine, ep),
      _vertexVar(nullptr),
      _edgeVar(nullptr),
      _opts(_trx),
      _posInPath(0),
      _pathLength(0),
      _path(nullptr),
      _useStartRegister(false),
      _useTargetRegister(false),
      _usedConstant(false) {

  ep->fillOptions(_opts);

  size_t count = ep->_edgeColls.size();
  TRI_ASSERT(ep->_directions.size());
  _collectionInfos.reserve(count);

  for (size_t j = 0; j < count; ++j) {
    auto info = std::make_unique<arangodb::traverser::EdgeCollectionInfo>(
        _trx, ep->_edgeColls[j], ep->_directions[j], _opts.weightAttribute,
        _opts.defaultWeight);
    _collectionInfos.emplace_back(info.get());
    info.release();
  }

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
  _path = std::make_unique<arangodb::traverser::ShortestPath>();

  if (arangodb::ServerState::instance()->isCoordinator()) {
    if (_opts.useWeight) {
      _finder.reset(new arangodb::basics::DynamicDistanceFinder<
                    arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                    double, arangodb::traverser::ShortestPath>(
          EdgeWeightExpanderCluster(this, false),
          EdgeWeightExpanderCluster(this, true), _opts.bidirectional));
    } else {
      _finder.reset(new arangodb::basics::ConstDistanceFinder<
                    arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                    arangodb::basics::VelocyPackHelper::VPackStringHash,
                    arangodb::basics::VelocyPackHelper::VPackStringEqual,
                    arangodb::traverser::ShortestPath>(
          ConstDistanceExpanderCluster(this, false),
          ConstDistanceExpanderCluster(this, true)));
    }
  } else {
    if (_opts.useWeight) {
      _finder.reset(new arangodb::basics::DynamicDistanceFinder<
                    arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                    double, arangodb::traverser::ShortestPath>(
          EdgeWeightExpanderLocal(this, false),
          EdgeWeightExpanderLocal(this, true), _opts.bidirectional));
    } else {
      _finder.reset(new arangodb::basics::ConstDistanceFinder<
                    arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                    arangodb::basics::VelocyPackHelper::VPackStringHash,
                    arangodb::basics::VelocyPackHelper::VPackStringEqual,
                    arangodb::traverser::ShortestPath>(
          ConstDistanceExpanderLocal(this, false),
          ConstDistanceExpanderLocal(this, true)));
    }
  }
}

ShortestPathBlock::~ShortestPathBlock() {
  for (auto& it : _collectionInfos) {
    delete it;
  }
}

int ShortestPathBlock::initialize() {
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

  return res;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

int ShortestPathBlock::initializeCursor(AqlItemBlock* items, size_t pos) {
  _posInPath = 0;
  _pathLength = 0;
  _usedConstant = false;
  return ExecutionBlock::initializeCursor(items, pos);
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
    auto pos = _startVertexId.find("/");
    if (pos == std::string::npos) {
      _engine->getQuery()->registerWarning(TRI_ERROR_BAD_PARAMETER,
                                           "Invalid input for Shortest Path: "
                                           "Only id strings or objects with "
                                           "_id are allowed");
      return false;
    } else {
      _opts.setStart(_startVertexId);
    }
  } else {
    AqlValue const& in = items->getValueReference(_pos, _startReg);
    if (in.isObject()) {
      try {
        _opts.setStart(_trx->extractIdString(in.slice()));
      }
      catch (...) {
        // _id or _key not present... ignore this error and fall through
        // returning no path
        return false;
      }
    } else if (in.isString()) {
      _startVertexId = in.slice().copyString();
      _opts.setStart(_startVertexId);
    } else {
      _engine->getQuery()->registerWarning(
          TRI_ERROR_BAD_PARAMETER, "Invalid input for Shortest Path: Only "
                                       "id strings or objects with _id are "
                                       "allowed");
      return false;
    }

  }

  if (!_useTargetRegister) {
    auto pos = _targetVertexId.find("/");
    if (pos == std::string::npos) {
      _engine->getQuery()->registerWarning(
          TRI_ERROR_BAD_PARAMETER, "Invalid input for Shortest Path: "
                                       "Only id strings or objects with "
                                       "_id are allowed");
      return false;
    } else {
      _opts.setEnd(_targetVertexId);
    }
  } else {
    AqlValue const& in = items->getValueReference(_pos, _targetReg);
    if (in.isObject()) {
      try {
        std::string idString = _trx->extractIdString(in.slice());
        _opts.setEnd(idString);
      }
      catch (...) {
        // _id or _key not present... ignore this error and fall through
        // returning no path
        return false;
      }
    } else if (in.isString()) {
      _targetVertexId = in.slice().copyString();
      _opts.setEnd(_targetVertexId);
    } else {
      _engine->getQuery()->registerWarning(
          TRI_ERROR_BAD_PARAMETER, "Invalid input for Shortest Path: Only "
                                       "id strings or objects with _id are "
                                       "allowed");
      return false;
    }
  }

  VPackSlice start = _opts.getStart();
  VPackSlice end = _opts.getEnd();
  TRI_ASSERT(_finder != nullptr);
  // We do not need this data anymore. Result has been processed.
  // Save some memory.
  _coordinatorCache.clear();
  bool hasPath = _finder->shortestPath(start, end, *_path);

  if (hasPath) {
    _posInPath = 0;
    _pathLength = _path->length();
  }

  return hasPath;
}

AqlItemBlock* ShortestPathBlock::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  if (_done) {
    return nullptr;
  }

  if (_buffer.empty()) {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
      _done = true;
      return nullptr;
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  AqlItemBlock* cur = _buffer.front();
  size_t const curRegs = cur->getNrRegs();

  // Collect the next path:
  if (_posInPath >= _pathLength) {
    if (!nextPath(cur)) {
      // This input does not have any path. maybe the next one has.
      // we can only return nullptr iff the buffer is empty.
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        delete cur;
        _pos = 0;
      }
      return getSome(atMost, atMost);
    }
  }

  size_t available = _pathLength - _posInPath;
  size_t toSend = (std::min)(atMost, available);

  RegisterId nrRegs =
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];
  std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, nrRegs));
  // automatically freed if we throw
  TRI_ASSERT(curRegs <= res->getNrRegs());

  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur, res.get(), _pos);

  // TODO this might be optimized in favor of direct mptr.
  // TODO: lease builder?
  VPackBuilder resultBuilder;
  for (size_t j = 0; j < toSend; j++) {
    if (usesVertexOutput()) {
      // TODO this might be optimized in favor of direct mptr.
      resultBuilder.clear();
      _path->vertexToVelocyPack(_trx, _posInPath, resultBuilder);
      res->setValue(j, _vertexReg, AqlValue(resultBuilder.slice()));
    }
    if (usesEdgeOutput()) {
      // TODO this might be optimized in favor of direct mptr.
      resultBuilder.clear();
      _path->edgeToVelocyPack(_trx, _posInPath, resultBuilder);
      res->setValue(j, _edgeReg, AqlValue(resultBuilder.slice()));
    }
    if (j > 0) {
      // re-use already copied aqlvalues
      res->copyValuesFromFirstRow(j, static_cast<RegisterId>(curRegs));
    }
    ++_posInPath;
  }

  if (_posInPath >= _pathLength) {
    // Advance read position for next call
    if (++_pos >= cur->size()) {
      _buffer.pop_front();  // does not throw
      delete cur;
      _pos = 0;
    }
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  return res.release();

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

size_t ShortestPathBlock::skipSome(size_t, size_t atMost) {
  return 0;
}

