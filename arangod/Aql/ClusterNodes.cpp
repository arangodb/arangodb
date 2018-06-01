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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterNodes.h"
#include "Aql/Ast.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/IndexNode.h"
#include "Aql/GraphNode.h"
#include "Transaction/Methods.h"

#include <type_traits>

using namespace arangodb::basics;
using namespace arangodb::aql;

namespace {

arangodb::velocypack::StringRef const SortModeUnset("unset");
arangodb::velocypack::StringRef const SortModeMinElement("minelement");
arangodb::velocypack::StringRef const SortModeHeap("heap");

bool toSortMode(
    arangodb::velocypack::StringRef const& str,
    GatherNode::SortMode& mode
) noexcept {
  // std::map ~25-30% faster than std::unordered_map for small number of elements
  static std::map<arangodb::velocypack::StringRef, GatherNode::SortMode> const NameToValue {
    { SortModeMinElement, GatherNode::SortMode::MinElement},
    { SortModeHeap, GatherNode::SortMode::Heap}
  };

  auto const it = NameToValue.find(str);

  if (it == NameToValue.end()) {
    TRI_ASSERT(false);
    return false;
  }

  mode = it->second;
  return true;
}

arangodb::velocypack::StringRef toString(GatherNode::SortMode mode) noexcept {
  switch (mode) {
    case GatherNode::SortMode::MinElement:
      return SortModeMinElement;
    case GatherNode::SortMode::Heap:
      return SortModeHeap;
    default:
      TRI_ASSERT(false);
      return {};
  }
}

/// @brief OurLessThan: comparison method for elements of _gatherBlockPos
class OurLessThan {
 public:
  OurLessThan(
      arangodb::transaction::Methods* trx,
      std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : _trx(trx),
      _gatherBlockBuffer(gatherBlockBuffer),
      _sortRegisters(sortRegisters) {
  }

  bool operator()(
    std::pair<size_t, size_t> const& a,
    std::pair<size_t, size_t> const& b
  ) const;

 private:
  arangodb::transaction::Methods* _trx;
  std::vector<std::deque<AqlItemBlock*>>& _gatherBlockBuffer;
  std::vector<SortRegister>& _sortRegisters;
}; // OurLessThan

bool OurLessThan::operator()(
    std::pair<size_t, size_t> const& a,
    std::pair<size_t, size_t> const& b
) const {
  // nothing in the buffer is maximum!
  if (_gatherBlockBuffer[a.first].empty()) {
    return false;
  }

  if (_gatherBlockBuffer[b.first].empty()) {
    return true;
  }

  TRI_ASSERT(!_gatherBlockBuffer[a.first].empty());
  TRI_ASSERT(!_gatherBlockBuffer[b.first].empty());

  for (auto const& reg : _sortRegisters) {
    auto const& lhs = _gatherBlockBuffer[a.first].front()->getValueReference(a.second, reg.reg);
    auto const& rhs = _gatherBlockBuffer[b.first].front()->getValueReference(b.second, reg.reg);
    auto const& attributePath = reg.attributePath;

    // Fast path if there is no attributePath:
    int cmp;

    if (attributePath.empty()) {
#ifdef USE_IRESEARCH
      TRI_ASSERT(reg.comparator);
      cmp = (*reg.comparator)(reg.scorer.get(), _trx, lhs, rhs);
#else
      cmp = AqlValue::Compare(_trx, lhs, rhs, true);
#endif
    } else {
      // Take attributePath into consideration:
      bool mustDestroyA;
      AqlValue aa = lhs.get(_trx, attributePath, mustDestroyA, false);
      AqlValueGuard guardA(aa, mustDestroyA);
      bool mustDestroyB;
      AqlValue bb = rhs.get(_trx, attributePath, mustDestroyB, false);
      AqlValueGuard guardB(bb, mustDestroyB);
      cmp = AqlValue::Compare(_trx, aa, bb, true);
    }

    if (cmp == -1) {
      return reg.asc;
    } else if (cmp == 1) {
      return !reg.asc;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @class UnsortingGatherBlock
/// @brief Execution block for gathers without order
////////////////////////////////////////////////////////////////////////////////
class UnsortingGatherBlock final : public ExecutionBlock {
 public:
  UnsortingGatherBlock(ExecutionEngine& engine, GatherNode const& en) noexcept
    : ExecutionBlock(&engine, &en) {
    TRI_ASSERT(en.elements().empty());
  }

  /// @brief shutdown: need our own method since our _buffer is different
  int shutdown(int errorCode) override final {
    DEBUG_BEGIN_BLOCK();
    // don't call default shutdown method since it does the wrong thing to
    // _gatherBlockBuffer
    int ret = TRI_ERROR_NO_ERROR;
    for (auto* dependency : _dependencies) {
      int res = dependency->shutdown(errorCode);

      if (res != TRI_ERROR_NO_ERROR) {
        ret = res;
      }
    }

    if (ret != TRI_ERROR_NO_ERROR) {
      return ret;
    }

    return TRI_ERROR_NO_ERROR;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override final {
    DEBUG_BEGIN_BLOCK();
    int res = ExecutionBlock::initializeCursor(items, pos);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    _atDep = 0;
    _done = _dependencies.empty();

    return TRI_ERROR_NO_ERROR;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief hasMore: true if any position of _buffer hasMore and false
  /// otherwise.
  bool hasMore() override final {
    DEBUG_BEGIN_BLOCK();
    if (_done || _dependencies.empty()) {
      return false;
    }

    for (auto* dependency : _dependencies) {
      if (dependency->hasMore()) {
        return true;
      }
    }

    _done = true;
    return false;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) override final {
    DEBUG_BEGIN_BLOCK();
    traceGetSomeBegin(atMost);

    _done = _dependencies.empty();

    if (_done) {
      traceGetSomeEnd(nullptr);
      return nullptr;
    }

    // the simple case . . .
    auto* res = _dependencies[_atDep]->getSome(atMost);

    while (!res && _atDep < _dependencies.size() - 1) {
      _atDep++;
      res = _dependencies[_atDep]->getSome(atMost);
    }

    _done = (nullptr == res);

    traceGetSomeEnd(res);

    return res;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief skipSome
  size_t skipSome(size_t atMost) override final {
    DEBUG_BEGIN_BLOCK();

    if (_done) {
      return 0;
    }

    // the simple case . . .
    auto skipped = _dependencies[_atDep]->skipSome(atMost);

    while (skipped == 0 && _atDep < _dependencies.size() - 1) {
      _atDep++;
      skipped = _dependencies[_atDep]->skipSome(atMost);
    }

    _done = (skipped == 0);

    return skipped;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

 private:
  /// @brief _atDep: currently pulling blocks from _dependencies.at(_atDep),
  size_t _atDep{};
}; // UnsortingGatherBlock

////////////////////////////////////////////////////////////////////////////////
/// @struct SortingStrategy
////////////////////////////////////////////////////////////////////////////////
struct SortingStrategy {
  typedef std::pair<
    size_t, // dependecy index
    size_t // position within a dependecy
  > ValueType;

  virtual ~SortingStrategy() = default;

  /// @brief returns next value
  virtual ValueType nextValue() = 0;

  /// @brief prepare strategy before transaction
  virtual void prepare(std::vector<ValueType>& /*blockPos*/) { }

  /// @brief resets state
  virtual void reset() = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @class HeapSorting
/// @brief "Heap" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class HeapSorting final : public SortingStrategy, private OurLessThan  {
 public:
  HeapSorting(
      arangodb::transaction::Methods* trx,
      std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : OurLessThan(trx, gatherBlockBuffer, sortRegisters) {
  }

  virtual ValueType nextValue() override {
    TRI_ASSERT(!_heap.empty());
    std::push_heap(_heap.begin(), _heap.end(), *this); // re-insert element
    std::pop_heap(_heap.begin(), _heap.end(), *this); // remove element from _heap but not from vector
    return _heap.back();
  }

  virtual void prepare(std::vector<ValueType>& blockPos) override {
    TRI_ASSERT(!blockPos.empty());

    if (_heap.size() == blockPos.size()) {
      return;
    }

    _heap.clear();
    std::copy(blockPos.begin(), blockPos.end(), std::back_inserter(_heap));
    std::make_heap(_heap.begin(), _heap.end()-1, *this); // remain last element out of heap to maintain invariant
    TRI_ASSERT(!_heap.empty());
  }

  virtual void reset() noexcept override {
    _heap.clear();
  }

  bool operator()(
      std::pair<size_t, size_t> const& lhs,
      std::pair<size_t, size_t> const& rhs
  ) const {
    return OurLessThan::operator()(rhs, lhs);
  }

 private:
  std::vector<std::reference_wrapper<ValueType>> _heap;
}; // HeapSorting

////////////////////////////////////////////////////////////////////////////////
/// @class MinElementSorting
/// @brief "MinElement" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class MinElementSorting final : public SortingStrategy, public OurLessThan {
 public:
  MinElementSorting(
      arangodb::transaction::Methods* trx,
      std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
      std::vector<SortRegister>& sortRegisters) noexcept
    : OurLessThan(trx, gatherBlockBuffer, sortRegisters) {
  }

  virtual ValueType nextValue() override {
    TRI_ASSERT(_blockPos);
    return *(std::min_element(_blockPos->begin(), _blockPos->end(), *this));
  }

  virtual void prepare(std::vector<ValueType>& blockPos) override {
    _blockPos = &blockPos;
  }

  virtual void reset() noexcept override {
    _blockPos = nullptr;
  }

 private:
  std::vector<ValueType> const* _blockPos;
};

////////////////////////////////////////////////////////////////////////////////
/// @class SortingGatherBlock
/// @brief Execution block for gathers with order
////////////////////////////////////////////////////////////////////////////////
class SortingGatherBlock final : public ExecutionBlock {
 public:
  SortingGatherBlock(
      ExecutionEngine& engine,
      GatherNode const& en)
    : ExecutionBlock(&engine, &en) {
    TRI_ASSERT(!en.elements().empty());

    switch (en.sortMode()) {
      case GatherNode::SortMode::MinElement:
        _strategy = std::make_unique<HeapSorting>(
          _trx, _gatherBlockBuffer, _sortRegisters
        );
        break;
      case GatherNode::SortMode::Heap:
        _strategy = std::make_unique<MinElementSorting>(
          _trx, _gatherBlockBuffer, _sortRegisters
        );
        break;
      default:
        TRI_ASSERT(false);
        break;
    }
    TRI_ASSERT(_strategy);

    // We know that planRegisters has been run, so
    // getPlanNode()->_registerPlan is set up
    SortRegister::fill(
      *en.plan(),
      *en.getRegisterPlan(),
      en.elements(),
      _sortRegisters
    );
  }

  /// @brief shutdown: need our own method since our _buffer is different
  int shutdown(int errorCode) override final {
    DEBUG_BEGIN_BLOCK();
    // don't call default shutdown method since it does the wrong thing to
    // _gatherBlockBuffer
    int ret = TRI_ERROR_NO_ERROR;
    for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
      int res = (*it)->shutdown(errorCode);

      if (res != TRI_ERROR_NO_ERROR) {
        ret = res;
      }
    }

    if (ret != TRI_ERROR_NO_ERROR) {
      return ret;
    }

    for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
      for (AqlItemBlock* y : x) {
        delete y;
      }
      x.clear();
    }
    _gatherBlockBuffer.clear();
    _gatherBlockPos.clear();

    return TRI_ERROR_NO_ERROR;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override final {
    DEBUG_BEGIN_BLOCK();
    int res = ExecutionBlock::initializeCursor(items, pos);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    for (std::deque<AqlItemBlock*>& x : _gatherBlockBuffer) {
      for (AqlItemBlock* y : x) {
        delete y;
      }
      x.clear();
    }
    _gatherBlockBuffer.clear();
    _gatherBlockPos.clear();
    _gatherBlockBuffer.reserve(_dependencies.size());
    _gatherBlockPos.reserve(_dependencies.size());
    for (size_t i = 0; i < _dependencies.size(); i++) {
      _gatherBlockBuffer.emplace_back();
      _gatherBlockPos.emplace_back(i, 0);
    }

    _strategy->reset();

    _done = _dependencies.empty();
    return TRI_ERROR_NO_ERROR;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief hasMore: true if any position of _buffer hasMore and false
  /// otherwise.
  bool hasMore() override final {
    DEBUG_BEGIN_BLOCK();
    if (_done || _dependencies.empty()) {
      return false;
    }

    for (size_t i = 0; i < _gatherBlockBuffer.size(); i++) {
      if (!_gatherBlockBuffer[i].empty()) {
        return true;
      } else if (getBlock(i, DefaultBatchSize())) {
        _gatherBlockPos[i] = std::make_pair(i, 0);
        return true;
      }
    }

    _done = true;
    return false;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) override final {
    DEBUG_BEGIN_BLOCK();
    traceGetSomeBegin(atMost);

    if (_dependencies.empty()) {
      _done = true;
    }

    if (_done) {
      traceGetSomeEnd(nullptr);
      return nullptr;
    }

    // the non-simple case . . .
    size_t available = 0;  // nr of available rows
    size_t index = 0;      // an index of a non-empty buffer

    // pull more blocks from dependencies . . .
    TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
    TRI_ASSERT(_gatherBlockBuffer.size() == _gatherBlockPos.size());

    for (size_t i = 0; i < _dependencies.size(); ++i) {
      if (_gatherBlockBuffer[i].empty()) {
        if (getBlock(i, atMost)) {
          index = i;
          _gatherBlockPos[i] = std::make_pair(i, 0);
        }
      } else {
        index = i;
      }

      auto const& cur = _gatherBlockBuffer[i];
      if (!cur.empty()) {
        TRI_ASSERT(cur[0]->size() >= _gatherBlockPos[i].second);
        available += cur[0]->size() - _gatherBlockPos[i].second;
        for (size_t j = 1; j < cur.size(); ++j) {
          available += cur[j]->size();
        }
      }
    }

    if (available == 0) {
      _done = true;
      traceGetSomeEnd(nullptr);
      return nullptr;
    }

    size_t toSend = (std::min)(available, atMost);  // nr rows in outgoing block

    // the following is similar to AqlItemBlock's slice method . . .
    std::vector<std::unordered_map<AqlValue, AqlValue>> cache;
    cache.resize(_gatherBlockBuffer.size());

    TRI_ASSERT(!_gatherBlockBuffer.at(index).empty());
    AqlItemBlock* example = _gatherBlockBuffer[index].front();
    size_t nrRegs = example->getNrRegs();

    // automatically deleted if things go wrong
    std::unique_ptr<AqlItemBlock> res(requestBlock(toSend, static_cast<arangodb::aql::RegisterId>(nrRegs)));

    _strategy->prepare(_gatherBlockPos);

    for (size_t i = 0; i < toSend; i++) {
      // get the next smallest row from the buffer . . .
      auto const val = _strategy->nextValue();
      auto& blocks = _gatherBlockBuffer[val.first];
      auto& blocksPos = _gatherBlockPos[val.first];

      // copy the row in to the outgoing block . . .
      for (RegisterId col = 0; col < nrRegs; col++) {
        TRI_ASSERT(!blocks.empty());
        AqlValue const& x = blocks.front()->getValueReference(val.second, col);
        if (!x.isEmpty()) {
          if (x.requiresDestruction()) {
            // complex value, with ownership transfer
            auto it = cache[val.first].find(x);

            if (it == cache[val.first].end()) {
              AqlValue y = x.clone();
              try {
                res->setValue(i, col, y);
              } catch (...) {
                y.destroy();
                throw;
              }
              cache[val.first].emplace(x, y);
            } else {
              res->setValue(i, col, (*it).second);
            }
          } else {
            // simple value, no ownership transfer needed
            res->setValue(i, col, x);
          }
        }
      }

      // renew the _gatherBlockPos and clean up the buffer if necessary
      if (++blocksPos.second == blocks.front()->size()) {
        TRI_ASSERT(!blocks.empty());
        AqlItemBlock* cur = blocks.front();
        returnBlock(cur);
        blocks.pop_front();
        blocksPos.second = 0; // reset position within a dependency

        if (blocks.empty()) {
          // if we pulled everything from the buffer, we need to fetch
          // more data for the shard for which we have no more local
          // values.
          getBlock(val.first, atMost);
          cache[val.first].clear();
          // note that if getBlock() returns false here, this is not
          // a problem, because the sort function used takes care of
          // this
        }
      }
    }

    traceGetSomeEnd(res.get());
    return res.release();

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief skipSome
  size_t skipSome(size_t atMost) override final {
    DEBUG_BEGIN_BLOCK();

    if (_done) {
      return 0;
    }

    // the non-simple case . . .
    size_t available = 0;  // nr of available rows
    TRI_ASSERT(_dependencies.size() != 0);

    // pull more blocks from dependencies . . .
    for (size_t i = 0; i < _dependencies.size(); i++) {
      if (_gatherBlockBuffer[i].empty()) {
        if (getBlock(i, atMost)) {
          _gatherBlockPos[i] = std::make_pair(i, 0);
        }
      }

      auto cur = _gatherBlockBuffer[i];
      if (!cur.empty()) {
        available += cur[0]->size() - _gatherBlockPos[i].second;
        for (size_t j = 1; j < cur.size(); j++) {
          available += cur[j]->size();
        }
      }
    }

    if (available == 0) {
      _done = true;
      return 0;
    }

    size_t const skipped = (std::min)(available, atMost);  // nr rows in outgoing block

    _strategy->prepare(_gatherBlockPos);

    for (size_t i = 0; i < skipped; i++) {
      // get the next smallest row from the buffer . . .
      auto const val = _strategy->nextValue();
      auto& blocks = _gatherBlockBuffer[val.first];
      auto& blocksPos = _gatherBlockPos[val.first];

      // renew the _gatherBlockPos and clean up the buffer if necessary
      if (++blocksPos.second == blocks.front()->size()) {
        TRI_ASSERT(!blocks.empty());
        AqlItemBlock* cur = blocks.front();
        returnBlock(cur);
        blocks.pop_front();
        blocksPos.second = 0; // reset position within a dependency
      }
    }

    return skipped;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

 private:
  /// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
  /// non-simple case only
  bool getBlock(size_t i, size_t atMost) {
    DEBUG_BEGIN_BLOCK();
    TRI_ASSERT(i < _dependencies.size());

    std::unique_ptr<AqlItemBlock> docs(_dependencies[i]->getSome(atMost));
    if (docs && docs->size() > 0) {
      _gatherBlockBuffer[i].emplace_back(docs.get());
      docs.release();
      return true;
    }

    return false;

    // cppcheck-suppress style
    DEBUG_END_BLOCK();
  }

  /// @brief _gatherBlockBuffer: buffer the incoming block from each dependency
  /// separately
  std::vector<std::deque<AqlItemBlock*>> _gatherBlockBuffer;

  /// @brief _gatherBlockPos: pairs (i, _pos in _buffer.at(i)), i.e. the same as
  /// the usual _pos but one pair per dependency
  std::vector<std::pair<size_t, size_t>> _gatherBlockPos;

  /// @brief sort elements for this block
  std::vector<SortRegister> _sortRegisters;

  /// @brief sorting strategy
  std::unique_ptr<SortingStrategy> _strategy;
}; // SortingGatherBlock

}

/// @brief constructor for RemoteNode 
RemoteNode::RemoteNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query()->vocbase())),
      _server(base.get("server").copyString()),
      _ownName(base.get("ownName").copyString()),
      _queryId(base.get("queryId").copyString()),
      _isResponsibleForInitializeCursor(base.get("isResponsibleForInitializeCursor").getBoolean()) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> RemoteNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const&
) const {
  return std::make_unique<RemoteBlock>(
    &engine, this, server(), ownName(), queryId()
  );
}

/// @brief toVelocyPack, for RemoteNode
void RemoteNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("server", VPackValue(_server));
  nodes.add("ownName", VPackValue(_ownName));
  nodes.add("queryId", VPackValue(_queryId));
  nodes.add("isResponsibleForInitializeCursor",
            VPackValue(_isResponsibleForInitializeCursor));

  // And close it:
  nodes.close();
}

/// @brief estimateCost
double RemoteNode::estimateCost(size_t& nrItems) const {
  if (_dependencies.size() == 1) {
    // This will usually be the case, however, in the context of the
    // instantiation it is possible that there is no dependency...
    double depCost = _dependencies[0]->estimateCost(nrItems);
    return depCost + nrItems;  // we need to process them all
  }
  // We really should not get here, but if so, do something bordering on
  // sensible:
  nrItems = 1;
  return 1.0;
}

/// @brief construct a scatter node
ScatterNode::ScatterNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query()->vocbase())),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ScatterNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const& includedShards
) const {
  auto const shardIds = collection()->shardIds(includedShards);

  return std::make_unique<ScatterBlock>(
    &engine, this, *shardIds
  );
}

/// @brief toVelocyPack, for ScatterNode
void ScatterNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));

  // And close it:
  nodes.close();
}

/// @brief estimateCost
double ScatterNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  auto shardIds = _collection->shardIds();
  size_t nrShards = shardIds->size();
  return depCost + nrItems * nrShards;
}

/// @brief construct a distribute node
DistributeNode::DistributeNode(ExecutionPlan* plan,
                               arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query()->vocbase())),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())),
      _variable(nullptr),
      _alternativeVariable(nullptr),
      _createKeys(base.get("createKeys").getBoolean()),
      _allowKeyConversionToObject(base.get("allowKeyConversionToObject").getBoolean()),
      _allowSpecifiedKeys(false) {
  if (base.hasKey("variable") && base.hasKey("alternativeVariable")) {     
    _variable = Variable::varFromVPack(plan->getAst(), base, "variable");
    _alternativeVariable = Variable::varFromVPack(plan->getAst(), base, "alternativeVariable");
  } else {
    _variable = plan->getAst()->variables()->getVariable(base.get("varId").getNumericValue<VariableId>());
    _alternativeVariable = plan->getAst()->variables()->getVariable(base.get("alternativeVarId").getNumericValue<VariableId>());
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> DistributeNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const& includedShards
) const {
  auto const shardIds = collection()->shardIds(includedShards);

  return std::make_unique<DistributeBlock>(
    &engine, this, *shardIds, collection()
  );
}

/// @brief toVelocyPack, for DistributedNode
void DistributeNode::toVelocyPackHelper(VPackBuilder& nodes,
                                        unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));
  nodes.add("createKeys", VPackValue(_createKeys));
  nodes.add("allowKeyConversionToObject",
            VPackValue(_allowKeyConversionToObject));
  nodes.add(VPackValue("variable"));
  _variable->toVelocyPack(nodes);
  nodes.add(VPackValue("alternativeVariable"));
  _alternativeVariable->toVelocyPack(nodes);
  
  // legacy format, remove in 3.4
  nodes.add("varId", VPackValue(static_cast<int>(_variable->id)));
  nodes.add("alternativeVarId",
            VPackValue(static_cast<int>(_alternativeVariable->id)));

  // And close it:
  nodes.close();
}
  
/// @brief getVariablesUsedHere, returning a vector
std::vector<Variable const*> DistributeNode::getVariablesUsedHere() const {
  std::vector<Variable const*> vars;
  vars.emplace_back(_variable);
  if (_variable != _alternativeVariable) {
    vars.emplace_back(_alternativeVariable);
  }
  return vars;
}
  
/// @brief getVariablesUsedHere, modifying the set in-place
void DistributeNode::getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const {
  vars.emplace(_variable);
  vars.emplace(_alternativeVariable);
}

/// @brief estimateCost
double DistributeNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}

/*static*/ Collection const* GatherNode::findCollection(
    GatherNode const& root
) noexcept {
  ExecutionNode const* node = root.getFirstDependency();

  while (node) {
    switch (node->getType()) {
      case ENUMERATE_COLLECTION:
        return castTo<EnumerateCollectionNode const*>(node)->collection();
      case INDEX:
        return castTo<IndexNode const*>(node)->collection();
      case TRAVERSAL:
      case SHORTEST_PATH:
        return castTo<GraphNode const*>(node)->collection();
      case SCATTER:
      case SCATTER_IRESEARCH_VIEW:
        return nullptr; // diamond boundary
      default:
        node = node->getFirstDependency();
        break;
    }
  }

  return nullptr;
}

/// @brief construct a gather node
GatherNode::GatherNode(
    ExecutionPlan* plan,
    arangodb::velocypack::Slice const& base,
    SortElementVector const& elements)
  : ExecutionNode(plan, base),
    _elements(elements),
    _sortmode(SortMode::MinElement) {
  if (!_elements.empty()) {
    auto const sortModeSlice = base.get("sortmode");

    if (!toSortMode(VelocyPackHelper::getStringRef(sortModeSlice, ""), _sortmode)) {
      LOG_TOPIC(ERR, Logger::AQL)
          << "invalid sort mode detected while creating 'GatherNode' from vpack";
    }
  }
}

GatherNode::GatherNode(
    ExecutionPlan* plan,
    size_t id,
    SortMode sortMode) noexcept
  : ExecutionNode(plan, id),
    _sortmode(sortMode) {
}

/// @brief toVelocyPack, for GatherNode
void GatherNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  if (_elements.empty()) {
    nodes.add("sortmode", VPackValue(SortModeUnset.data()));
  } else {
    nodes.add("sortmode", VPackValue(toString(_sortmode).data()));
  }

  nodes.add(VPackValue("elements"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _elements) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("inVariable"));
      it.var->toVelocyPack(nodes);
      nodes.add("ascending", VPackValue(it.ascending));
      if (!it.attributePath.empty()) {
        nodes.add(VPackValue("path"));
        VPackArrayBuilder arr(&nodes);
        for (auto const& a : it.attributePath) {
          nodes.add(VPackValue(a));
        }
      }
    }
  }

  // And close it:
  nodes.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> GatherNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const&
) const {
  if (elements().empty()) {
    return std::make_unique<UnsortingGatherBlock>(engine, *this);
  }

  return std::make_unique<SortingGatherBlock>(engine, *this);
}

/// @brief estimateCost
double GatherNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}
