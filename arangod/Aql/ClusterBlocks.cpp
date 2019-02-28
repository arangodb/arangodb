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

#include "ClusterBlocks.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlTransaction.h"
#include "Aql/AqlValue.h"
#include "Aql/BlockCollector.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionStats.h"
#include "Aql/Query.h"
#include "Aql/WakeupQueryCallback.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;
using StringBuffer = arangodb::basics::StringBuffer;

namespace {

/// @brief OurLessThan: comparison method for elements of SortingGatherBlock
class OurLessThan {
 public:
  OurLessThan(arangodb::transaction::Methods* trx,
              std::vector<std::deque<AqlItemBlock*>> const& gatherBlockBuffer,
              std::vector<SortRegister>& sortRegisters) noexcept
      : _trx(trx), _gatherBlockBuffer(gatherBlockBuffer), _sortRegisters(sortRegisters) {}

  bool operator()(std::pair<size_t, size_t> const& a,
                  std::pair<size_t, size_t> const& b) const;

 private:
  arangodb::transaction::Methods* _trx;
  std::vector<std::deque<AqlItemBlock*>> const& _gatherBlockBuffer;
  std::vector<SortRegister>& _sortRegisters;
};  // OurLessThan

bool OurLessThan::operator()(std::pair<size_t, size_t> const& a,
                             std::pair<size_t, size_t> const& b) const {
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
    auto const& lhs =
        _gatherBlockBuffer[a.first].front()->getValueReference(a.second, reg.reg);
    auto const& rhs =
        _gatherBlockBuffer[b.first].front()->getValueReference(b.second, reg.reg);
    auto const& attributePath = reg.attributePath;

    // Fast path if there is no attributePath:
    int cmp;

    if (attributePath.empty()) {
      cmp = AqlValue::Compare(_trx, lhs, rhs, true);
    } else {
      // Take attributePath into consideration:
      bool mustDestroyA;
      auto resolver = _trx->resolver();
      TRI_ASSERT(resolver != nullptr);
      AqlValue aa = lhs.get(*resolver, attributePath, mustDestroyA, false);
      AqlValueGuard guardA(aa, mustDestroyA);
      bool mustDestroyB;
      AqlValue bb = rhs.get(*resolver, attributePath, mustDestroyB, false);
      AqlValueGuard guardB(bb, mustDestroyB);
      cmp = AqlValue::Compare(_trx, aa, bb, true);
    }

    if (cmp < 0) {
      return reg.asc;
    } else if (cmp > 0) {
      return !reg.asc;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @class HeapSorting
/// @brief "Heap" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class HeapSorting final : public SortingStrategy, private OurLessThan {
 public:
  HeapSorting(arangodb::transaction::Methods* trx,
              std::vector<std::deque<AqlItemBlock*>> const& gatherBlockBuffer,
              std::vector<SortRegister>& sortRegisters) noexcept
      : OurLessThan(trx, gatherBlockBuffer, sortRegisters) {}

  virtual ValueType nextValue() override {
    TRI_ASSERT(!_heap.empty());
    std::push_heap(_heap.begin(), _heap.end(), *this);  // re-insert element
    std::pop_heap(_heap.begin(), _heap.end(),
                  *this);  // remove element from _heap but not from vector
    return _heap.back();
  }

  virtual void prepare(std::vector<ValueType>& blockPos) override {
    TRI_ASSERT(!blockPos.empty());

    if (_heap.size() == blockPos.size()) {
      return;
    }

    _heap.clear();
    std::copy(blockPos.begin(), blockPos.end(), std::back_inserter(_heap));
    std::make_heap(_heap.begin(), _heap.end() - 1,
                   *this);  // remain last element out of heap to maintain invariant
    TRI_ASSERT(!_heap.empty());
  }

  virtual void reset() noexcept override { _heap.clear(); }

  bool operator()(std::pair<size_t, size_t> const& lhs,
                  std::pair<size_t, size_t> const& rhs) const {
    return OurLessThan::operator()(rhs, lhs);
  }

 private:
  std::vector<std::reference_wrapper<ValueType>> _heap;
};  // HeapSorting

////////////////////////////////////////////////////////////////////////////////
/// @class MinElementSorting
/// @brief "MinElement" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class MinElementSorting final : public SortingStrategy, public OurLessThan {
 public:
  MinElementSorting(arangodb::transaction::Methods* trx,
                    std::vector<std::deque<AqlItemBlock*>> const& gatherBlockBuffer,
                    std::vector<SortRegister>& sortRegisters) noexcept
      : OurLessThan(trx, gatherBlockBuffer, sortRegisters), _blockPos(nullptr) {}

  virtual ValueType nextValue() override {
    TRI_ASSERT(_blockPos);
    return *(std::min_element(_blockPos->begin(), _blockPos->end(), *this));
  }

  virtual void prepare(std::vector<ValueType>& blockPos) override {
    _blockPos = &blockPos;
  }

  virtual void reset() noexcept override { _blockPos = nullptr; }

 private:
  std::vector<ValueType> const* _blockPos;
};

}  // namespace

BlockWithClients::BlockWithClients(ExecutionEngine* engine, ExecutionNode const* ep,
                                   std::vector<std::string> const& shardIds)
    : ExecutionBlock(engine, ep), _nrClients(shardIds.size()), _wasShutdown(false) {
  _shardIdMap.reserve(_nrClients);
  for (size_t i = 0; i < _nrClients; i++) {
    _shardIdMap.emplace(std::make_pair(shardIds[i], i));
  }
}

std::pair<ExecutionState, Result> BlockWithClients::initializeCursor(AqlItemBlock* items,
                                                                     size_t pos) {
  return ExecutionBlock::initializeCursor(items, pos);
}

/// @brief shutdown
std::pair<ExecutionState, Result> BlockWithClients::shutdown(int errorCode) {
  if (_wasShutdown) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  auto res = ExecutionBlock::shutdown(errorCode);
  if (res.first == ExecutionState::WAITING) {
    return res;
  }
  _wasShutdown = true;
  return res;
}

/// @brief getClientId: get the number <clientId> (used internally)
/// corresponding to <shardId>
size_t BlockWithClients::getClientId(std::string const& shardId) const {
  if (shardId.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "got empty shard id");
  }

  auto it = _shardIdMap.find(shardId);
  if (it == _shardIdMap.end()) {
    std::string message("AQL: unknown shard id ");
    message.append(shardId);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }
  return ((*it).second);
}

// -----------------------------------------------------------------------------
// -- SECTION --                                            UnsortingGatherBlock
// -----------------------------------------------------------------------------

/// @brief initializeCursor
std::pair<ExecutionState, arangodb::Result> UnsortingGatherBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || !res.second.ok()) {
    return res;
  }

  _atDep = 0;
  _done = _dependencies.empty();

  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> UnsortingGatherBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  _done = _dependencies.empty();

  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  // the simple case ...
  auto res = _dependencies[_atDep]->getSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceGetSomeEnd(nullptr, ExecutionState::WAITING);
    return res;
  }

  while (res.second == nullptr && _atDep < _dependencies.size() - 1) {
    _atDep++;
    res = _dependencies[_atDep]->getSome(atMost);
    if (res.first == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return res;
    }
  }

  _done = (nullptr == res.second);

  traceGetSomeEnd(res.second.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res.second)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> UnsortingGatherBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  // the simple case . . .
  auto res = _dependencies[_atDep]->skipSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceSkipSomeEnd(res.second, ExecutionState::WAITING);
    return res;
  }

  while (res.second == 0 && _atDep < _dependencies.size() - 1) {
    _atDep++;
    res = _dependencies[_atDep]->skipSome(atMost);
    if (res.first == ExecutionState::WAITING) {
      traceSkipSomeEnd(res.second, ExecutionState::WAITING);
      return res;
    }
  }

  _done = (res.second == 0);

  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(res.second, state);
  return {state, res.second};
}

// -----------------------------------------------------------------------------
// -- SECTION --                                              SortingGatherBlock
// -----------------------------------------------------------------------------

SortingGatherBlock::SortingGatherBlock(ExecutionEngine& engine, GatherNode const& en)
    : ExecutionBlock(&engine, &en) {
  TRI_ASSERT(!en.elements().empty());

  switch (en.sortMode()) {
    case GatherNode::SortMode::MinElement:
      _strategy = std::make_unique<MinElementSorting>(_trx, _gatherBlockBuffer, _sortRegisters);
      break;
    case GatherNode::SortMode::Heap:
    case GatherNode::SortMode::Default:  // use heap by default
      _strategy = std::make_unique<HeapSorting>(_trx, _gatherBlockBuffer, _sortRegisters);
      break;
    default:
      TRI_ASSERT(false);
      break;
  }
  TRI_ASSERT(_strategy);

  // We know that planRegisters has been run, so
  // getPlanNode()->_registerPlan is set up
  SortRegister::fill(*en.plan(), *en.getRegisterPlan(), en.elements(), _sortRegisters);
}

SortingGatherBlock::~SortingGatherBlock() { clearBuffers(); }

void SortingGatherBlock::clearBuffers() noexcept {
  for (std::deque<AqlItemBlock*>& it : _gatherBlockBuffer) {
    for (AqlItemBlock* b : it) {
      delete b;
    }
    it.clear();
  }
}

/// @brief initializeCursor
std::pair<ExecutionState, arangodb::Result> SortingGatherBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING || !res.second.ok()) {
    return res;
  }

  clearBuffers();

  TRI_ASSERT(!_dependencies.empty());

  if (_gatherBlockBuffer.empty()) {
    // only do this initialization once
    _gatherBlockBuffer.reserve(_dependencies.size());
    _gatherBlockPos.reserve(_dependencies.size());
    _gatherBlockPosDone.reserve(_dependencies.size());

    for (size_t i = 0; i < _dependencies.size(); ++i) {
      _gatherBlockBuffer.emplace_back();
      _gatherBlockPos.emplace_back(i, 0);
      _gatherBlockPosDone.push_back(false);
    }
  } else {
    for (size_t i = 0; i < _dependencies.size(); i++) {
      TRI_ASSERT(_gatherBlockBuffer[i].empty());
      _gatherBlockPos[i].second = 0;
      _gatherBlockPosDone[i] = false;
    }
  }

  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPosDone.size() == _dependencies.size());

  _strategy->reset();

  _done = _dependencies.empty();

  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

/**
 * @brief Fills all _gatherBlockBuffer entries. Is repeatable during WAITING.
 *
 *
 * @param atMost The amount of data requested per block.
 * @param nonEmptyIndex an index of a non-empty GatherBlock buffer
 *
 * @return Will return {WAITING, 0} if it had to request new data from upstream.
 *         If everything is in place: all buffers are either filled with at
 *         least "atMost" rows, or the upstream block is DONE.
 *         Will return {DONE, SUM(_gatherBlockBuffer)} on success.
 */
std::pair<ExecutionState, size_t> SortingGatherBlock::fillBuffers(size_t atMost) {
  size_t available = 0;

  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());

  // In the future, we should request all blocks in parallel. But not everything
  // is yet thread safe for that to work, so we have to return immediately on
  // the first WAITING we encounter.
  for (size_t i = 0; i < _dependencies.size(); i++) {
    // reset position to 0 if we're going to fetch a new block.
    // this doesn't hurt, even if we don't get one.
    if (_gatherBlockBuffer[i].empty()) {
      _gatherBlockPos[i].second = 0;
    }
    ExecutionState state;
    bool blockAppended;
    std::tie(state, blockAppended) = getBlocks(i, atMost);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, 0};
    }

    available += availableRows(i);
  }

  return {ExecutionState::DONE, available};
}

/// @brief Returns the number of unprocessed rows in the buffer i.
size_t SortingGatherBlock::availableRows(size_t i) const {
  size_t available = 0;

  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(i < _dependencies.size());

  auto const& blocks = _gatherBlockBuffer[i];
  size_t curRowIdx = _gatherBlockPos[i].second;

  if (!blocks.empty()) {
    TRI_ASSERT(blocks[0]->size() >= curRowIdx);
    // the first block may already be partially processed
    available += blocks[0]->size() - curRowIdx;
  }

  // add rows from all additional blocks
  for (size_t j = 1; j < blocks.size(); ++j) {
    available += blocks[j]->size();
  }

  return available;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> SortingGatherBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_dependencies.empty()) {
    _done = true;
  }

  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  // the non-simple case . . .

  // pull more blocks from dependencies . . .
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _gatherBlockPos.size());

  size_t available = 0;
  {
    ExecutionState blockState;
    std::tie(blockState, available) = fillBuffers(atMost);
    if (blockState == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return {blockState, nullptr};
    }
  }

  if (available == 0) {
    _done = true;
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  size_t toSend = (std::min)(available, atMost);  // nr rows in outgoing block

  // the following is similar to AqlItemBlock's slice method . . .
  std::vector<std::unordered_map<AqlValue, AqlValue>> cache;
  cache.resize(_dependencies.size());

  size_t nrRegs = getNrInputRegisters();

  // automatically deleted if things go wrong
  std::unique_ptr<AqlItemBlock> res(
      requestBlock(toSend, static_cast<arangodb::aql::RegisterId>(nrRegs)));

  _strategy->prepare(_gatherBlockPos);

  for (size_t i = 0; i < toSend; i++) {
    // get the next smallest row from the buffer . . .
    auto const val = _strategy->nextValue();
    auto const& blocks = _gatherBlockBuffer[val.first];

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

    nextRow(val.first);
  }

  traceGetSomeEnd(res.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> SortingGatherBlock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  // the non-simple case . . .
  TRI_ASSERT(!_dependencies.empty());

  size_t available = 0;
  {
    ExecutionState blockState;
    std::tie(blockState, available) = fillBuffers(atMost);
    if (blockState == ExecutionState::WAITING) {
      traceSkipSomeEnd(0, ExecutionState::WAITING);
      return {blockState, 0};
    }
  }

  if (available == 0) {
    _done = true;
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  size_t const skipped = (std::min)(available, atMost);  // nr rows in outgoing block

  _strategy->prepare(_gatherBlockPos);

  for (size_t i = 0; i < skipped; i++) {
    // get the next smallest row from the buffer . . .
    auto const val = _strategy->nextValue();

    nextRow(val.first);
  }

  // Maybe we can optimize here DONE/HASMORE
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

/// @brief Step to the next row in line in the buffers of dependency i, i.e.,
/// updates _gatherBlockBuffer and _gatherBlockPos. If necessary, steps to the
/// next block and removes the previous one. Will not fetch more blocks.
void SortingGatherBlock::nextRow(size_t i) {
  TRI_ASSERT(i < _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());

  auto& blocks = _gatherBlockBuffer[i];
  auto& blocksPos = _gatherBlockPos[i];
  if (++blocksPos.second == blocks.front()->size()) {
    TRI_ASSERT(!blocks.empty());
    AqlItemBlock* cur = blocks.front();
    returnBlock(cur);
    blocks.pop_front();
    blocksPos.second = 0;  // reset position within a dependency
  }
}

/// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
/// non-simple case only
/// Assures that either atMost rows are actually available in buffer i, or
/// the dependency is DONE.
std::pair<ExecutionState, bool> SortingGatherBlock::getBlocks(size_t i, size_t atMost) {
  TRI_ASSERT(i < _dependencies.size());
  TRI_ASSERT(_gatherBlockBuffer.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPos.size() == _dependencies.size());
  TRI_ASSERT(_gatherBlockPosDone.size() == _dependencies.size());

  if (_gatherBlockPosDone[i]) {
    return {ExecutionState::DONE, false};
  }

  bool blockAppended = false;
  size_t rowsAvailable = availableRows(i);
  ExecutionState state = ExecutionState::HASMORE;

  // repeat until either
  // - enough rows are fetched
  // - dep[i] is DONE
  // - dep[i] is WAITING
  while (state == ExecutionState::HASMORE && rowsAvailable < atMost) {
    std::unique_ptr<AqlItemBlock> itemBlock;
    std::tie(state, itemBlock) = _dependencies[i]->getSome(atMost);

    // Assert that state == WAITING => itemBlock == nullptr
    TRI_ASSERT(state != ExecutionState::WAITING || itemBlock == nullptr);

    if (state == ExecutionState::DONE) {
      _gatherBlockPosDone[i] = true;
    }

    if (itemBlock && itemBlock->size() > 0) {
      rowsAvailable += itemBlock->size();
      _gatherBlockBuffer[i].emplace_back(itemBlock.get());
      itemBlock.release();
      blockAppended = true;
    }
  }

  TRI_ASSERT(state == ExecutionState::WAITING ||
             state == ExecutionState::DONE || rowsAvailable >= atMost);

  return {state, blockAppended};
}

/// @brief timeout
double const SingleRemoteOperationBlock::defaultTimeOut = 3600.0;

/// @brief creates a remote block
SingleRemoteOperationBlock::SingleRemoteOperationBlock(ExecutionEngine* engine,
                                                       SingleRemoteOperationNode const* en)
    : ExecutionBlock(engine, static_cast<ExecutionNode const*>(en)),
      _collection(en->collection()),
      _key(en->key()) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
}

namespace {
std::unique_ptr<VPackBuilder> merge(VPackSlice document, std::string const& key,
                                    TRI_voc_rid_t revision) {
  auto builder = std::make_unique<VPackBuilder>();
  {
    VPackObjectBuilder guard(builder.get());
    TRI_SanitizeObject(document, *builder);
    VPackSlice keyInBody = document.get(StaticStrings::KeyString);

    if (keyInBody.isNone() || keyInBody.isNull() ||
        (keyInBody.isString() && keyInBody.copyString() != key) ||
        ((revision != 0) && (TRI_ExtractRevisionId(document) != revision))) {
      // We need to rewrite the document with the given revision and key:
      builder->add(StaticStrings::KeyString, VPackValue(key));
      if (revision != 0) {
        builder->add(StaticStrings::RevString, VPackValue(TRI_RidToString(revision)));
      }
    }
  }
  return builder;
}
}  // namespace

bool SingleRemoteOperationBlock::getOne(arangodb::aql::AqlItemBlock* aqlres,
                                        size_t outputCounter) {
  int possibleWrites = 0;  // TODO - get real statistic values!
  auto node = ExecutionNode::castTo<SingleRemoteOperationNode const*>(getPlanNode());
  auto out = node->_outVariable;
  auto in = node->_inVariable;
  auto OLD = node->_outVariableOld;
  auto NEW = node->_outVariableNew;

  RegisterId inRegId = ExecutionNode::MaxRegisterId;
  RegisterId outRegId = ExecutionNode::MaxRegisterId;
  RegisterId oldRegId = ExecutionNode::MaxRegisterId;
  RegisterId newRegId = ExecutionNode::MaxRegisterId;

  if (in != nullptr) {
    auto itIn = node->getRegisterPlan()->varInfo.find(in->id);
    TRI_ASSERT(itIn != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itIn).second.registerId < ExecutionNode::MaxRegisterId);
    inRegId = (*itIn).second.registerId;
  }

  if (_key.empty() && in == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                   "missing document reference");
  }

  if (out != nullptr) {
    auto itOut = node->getRegisterPlan()->varInfo.find(out->id);
    TRI_ASSERT(itOut != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOut).second.registerId < ExecutionNode::MaxRegisterId);
    outRegId = (*itOut).second.registerId;
  }

  if (OLD != nullptr) {
    auto itOld = node->getRegisterPlan()->varInfo.find(OLD->id);
    TRI_ASSERT(itOld != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itOld).second.registerId < ExecutionNode::MaxRegisterId);
    oldRegId = (*itOld).second.registerId;
  }

  if (NEW != nullptr) {
    auto itNew = node->getRegisterPlan()->varInfo.find(NEW->id);
    TRI_ASSERT(itNew != node->getRegisterPlan()->varInfo.end());
    TRI_ASSERT((*itNew).second.registerId < ExecutionNode::MaxRegisterId);
    newRegId = (*itNew).second.registerId;
  }

  VPackBuilder inBuilder;
  VPackSlice inSlice = VPackSlice::emptyObjectSlice();
  if (in) {  // IF NOT REMOVE OR SELECT
    if (_buffer.size() < 1) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                     "missing document reference in Register");
    }
    AqlValue const& inDocument = _buffer.front()->getValueReference(_pos, inRegId);
    inBuilder.add(inDocument.slice());
    inSlice = inBuilder.slice();
  }

  auto const& nodeOps = node->_options;

  OperationOptions opOptions;
  opOptions.ignoreRevs = nodeOps.ignoreRevs;
  opOptions.keepNull = !nodeOps.nullMeansRemove;
  opOptions.mergeObjects = nodeOps.mergeObjects;
  opOptions.returnNew = !!NEW;
  opOptions.returnOld = (!!OLD) || out;
  opOptions.waitForSync = nodeOps.waitForSync;
  opOptions.silent = false;
  opOptions.overwrite = nodeOps.overwrite;

  std::unique_ptr<VPackBuilder> mergedBuilder;
  if (!_key.empty()) {
    mergedBuilder = merge(inSlice, _key, 0);
    inSlice = mergedBuilder->slice();
  }

  OperationResult result;
  if (node->_mode == ExecutionNode::NodeType::INDEX) {
    result = _trx->document(_collection->name(), inSlice, opOptions);
  } else if (node->_mode == ExecutionNode::NodeType::INSERT) {
    if (opOptions.returnOld && !opOptions.overwrite) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN,
          "OLD is only available when using INSERT with the overwrite option");
    }
    result = _trx->insert(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::REMOVE) {
    result = _trx->remove(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::REPLACE) {
    if (node->_replaceIndexNode && in == nullptr) {
      // we have a FOR .. IN FILTER doc._key == ... REPLACE - no WITH.
      // in this case replace needs to behave as if it was UPDATE.
      result = _trx->update(_collection->name(), inSlice, opOptions);
    } else {
      result = _trx->replace(_collection->name(), inSlice, opOptions);
    }
    possibleWrites = 1;
  } else if (node->_mode == ExecutionNode::NodeType::UPDATE) {
    result = _trx->update(_collection->name(), inSlice, opOptions);
    possibleWrites = 1;
  }

  // check operation result
  if (!result.ok()) {
    if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
        ((node->_mode == ExecutionNode::NodeType::INDEX) ||
         (node->_mode == ExecutionNode::NodeType::UPDATE && node->_replaceIndexNode) ||
         (node->_mode == ExecutionNode::NodeType::REMOVE && node->_replaceIndexNode) ||
         (node->_mode == ExecutionNode::NodeType::REPLACE && node->_replaceIndexNode))) {
      // document not there is not an error in this situation.
      // FOR ... FILTER ... REMOVE wouldn't invoke REMOVE in first place, so
      // don't throw an excetpion.
      return false;
    } else if (!nodeOps.ignoreErrors) {  // TODO remove if
      THROW_ARANGO_EXCEPTION_MESSAGE(result.errorNumber(), result.errorMessage());
    }

    if (node->_mode == ExecutionNode::NodeType::INDEX) {
      return false;
    }
  }

  _engine->_stats.writesExecuted += possibleWrites;
  _engine->_stats.scannedIndex++;

  if (!(out || OLD || NEW)) {
    return node->hasParent();
  }

  // Fill itemblock
  // create block that can hold a result with one entry and a number of
  // variables corresponding to the amount of out variables

  // only copy 1st row of registers inherited from previous frame(s)
  TRI_ASSERT(result.ok());
  VPackSlice outDocument = VPackSlice::nullSlice();
  if (result.buffer) {
    outDocument = result.slice().resolveExternal();
  }

  VPackSlice oldDocument = VPackSlice::nullSlice();
  VPackSlice newDocument = VPackSlice::nullSlice();
  if (outDocument.isObject()) {
    if (NEW && outDocument.hasKey("new")) {
      newDocument = outDocument.get("new");
    }
    if (outDocument.hasKey("old")) {
      outDocument = outDocument.get("old");
      if (OLD) {
        oldDocument = outDocument;
      }
    }
  }

  TRI_ASSERT(out || OLD || NEW);

  // place documents as in the out variable slots of the result
  if (out) {
    aqlres->emplaceValue(outputCounter,
                         static_cast<arangodb::aql::RegisterId>(outRegId), outDocument);
  }

  if (OLD) {
    TRI_ASSERT(opOptions.returnOld);
    aqlres->emplaceValue(outputCounter,
                         static_cast<arangodb::aql::RegisterId>(oldRegId), oldDocument);
  }

  if (NEW) {
    TRI_ASSERT(opOptions.returnNew);
    aqlres->emplaceValue(outputCounter,
                         static_cast<arangodb::aql::RegisterId>(newRegId), newDocument);
  }

  throwIfKilled();  // check if we were aborted

  TRI_IF_FAILURE("SingleRemoteOperationBlock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return true;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> SingleRemoteOperationBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_done) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  RegisterId nrRegs =
      getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()];
  std::unique_ptr<AqlItemBlock> aqlres(requestBlock(atMost, nrRegs));

  int outputCounter = 0;
  if (_buffer.empty()) {
    size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
    ExecutionState state = ExecutionState::HASMORE;
    bool blockAppended = false;

    std::tie(state, blockAppended) = ExecutionBlock::getBlock(toFetch);
    if (state == ExecutionState::WAITING) {
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return {state, nullptr};
    }
    if (!blockAppended) {
      _done = true;
      traceGetSomeEnd(nullptr, ExecutionState::DONE);
      return {ExecutionState::DONE, nullptr};
    }
    _pos = 0;  // this is in the first block
  }

  // If we get here, we do have _buffer.front()
  arangodb::aql::AqlItemBlock* cur = _buffer.front();
  TRI_ASSERT(cur != nullptr);
  size_t n = cur->size();
  for (size_t i = 0; i < n; i++) {
    inheritRegisters(cur, aqlres.get(), _pos);
    if (getOne(aqlres.get(), outputCounter)) {
      outputCounter++;
    }
    _done = true;
    _pos++;
  }
  _buffer.pop_front();  // does not throw
  returnBlock(cur);
  _pos = 0;
  if (outputCounter == 0) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }
  aqlres->shrink(outputCounter);

  // Clear out registers no longer needed later:
  clearRegisters(aqlres.get());
  traceGetSomeEnd(aqlres.get(), ExecutionState::DONE);
  return {ExecutionState::DONE, std::move(aqlres)};
}

/// @brief skipSome
std::pair<ExecutionState, size_t> SingleRemoteOperationBlock::skipSome(size_t atMost) {
  TRI_ASSERT(false);  // as soon as we need to support LIMIT change me.
  return {ExecutionState::DONE, 0};
}
