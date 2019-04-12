////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionBlockMock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 ExecutionNodeMock
// -----------------------------------------------------------------------------

ExecutionNodeMock::ExecutionNodeMock(size_t id /*= 0*/)
    : ExecutionNode(nullptr, id) {
  setVarUsageValid();
  planRegisters();
}

arangodb::aql::ExecutionNode::NodeType ExecutionNodeMock::getType() const {
  return arangodb::aql::ExecutionNode::NodeType::SINGLETON;
}

std::unique_ptr<arangodb::aql::ExecutionBlock> ExecutionNodeMock::createBlock(
    arangodb::aql::ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, arangodb::aql::ExecutionBlock*> const& cache) const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "cannot create a block of ExecutionNodeMock");
}

arangodb::aql::ExecutionNode* ExecutionNodeMock::clone(arangodb::aql::ExecutionPlan* plan,
                                                       bool withDependencies,
                                                       bool withProperties) const {
  return new ExecutionNodeMock(id());
}

void ExecutionNodeMock::toVelocyPackHelper(arangodb::velocypack::Builder& nodes,
                                           unsigned flags) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);  // call base class method
  nodes.close();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                ExecutionBlockMock
// -----------------------------------------------------------------------------

ExecutionBlockMock::ExecutionBlockMock(arangodb::aql::AqlItemBlock const& data,
                                       arangodb::aql::ExecutionEngine& engine,
                                       arangodb::aql::ExecutionNode const& node)
    : arangodb::aql::ExecutionBlock(&engine, &node), _data(&data), _inflight(0) {}

std::pair<arangodb::aql::ExecutionState, arangodb::Result> ExecutionBlockMock::initializeCursor(
    arangodb::aql::InputAqlItemRow const& input) {
  const auto res = ExecutionBlock::initializeCursor(input);

  if (res.first == arangodb::aql::ExecutionState::WAITING || !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _pos_in_data = 0;
  _upstreamState = arangodb::aql::ExecutionState::HASMORE;
  _inflight = 0;

  return res;
}

std::pair<arangodb::aql::ExecutionState, arangodb::aql::SharedAqlItemBlockPtr>
ExecutionBlockMock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_done) {
    TRI_ASSERT(getHasMoreState() == arangodb::aql::ExecutionState::DONE);
    traceGetSomeEnd(nullptr, arangodb::aql::ExecutionState::DONE);
    return {arangodb::aql::ExecutionState::DONE, nullptr};
  }

  bool needMore;
  arangodb::aql::SharedAqlItemBlockPtr cur = nullptr;
  arangodb::aql::SharedAqlItemBlockPtr res;

  do {
    needMore = false;

    if (_buffer.empty()) {
      if (_upstreamState == arangodb::aql::ExecutionState::DONE) {
        return {arangodb::aql::ExecutionState::DONE, nullptr};
      }
      size_t const toFetch = (std::min)(DefaultBatchSize(), atMost);
      auto res = ExecutionBlock::getBlock(toFetch);
      if (res.first == arangodb::aql::ExecutionState::WAITING) {
        return {res.first, nullptr};
      }
      _upstreamState = res.first;
      if (!res.second) {
        _done = true;
        TRI_ASSERT(getHasMoreState() == arangodb::aql::ExecutionState::DONE);
        traceGetSomeEnd(nullptr, arangodb::aql::ExecutionState::DONE);
        return {arangodb::aql::ExecutionState::DONE, nullptr};
      }
      _pos = 0;  // this is in the first block
    }

    TRI_ASSERT(!_buffer.empty());
    cur = _buffer.front();

    if (_pos_in_data == _data->size()) {
      needMore = true;
      _pos_in_data = 0;

      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        cur = nullptr;
        _pos = 0;
      }
    }
  } while (needMore);

  TRI_ASSERT(cur != nullptr);

  auto const from = std::min(_pos_in_data, _data->size());
  auto const to = std::min(_pos_in_data + atMost, _data->size());
  res = _data->slice(from, to);

  // only copy 1st row of registers inherited from previous frame(s)
  inheritRegisters(cur.get(), res.get(), _pos);

  throwIfKilled();  // check if we were aborted

  TRI_IF_FAILURE("ExecutionBlockMock::moreDocuments") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _pos_in_data = to;
  TRI_ASSERT(res != nullptr);

  if (res->size() < atMost) {
    // The collection did not have enough results
    res->shrink(res->size());
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  traceGetSomeEnd(res.get(), getHasMoreState());
  return {getHasMoreState(), std::move(res)};
}

std::pair<arangodb::aql::ExecutionState, size_t> ExecutionBlockMock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    traceSkipSomeEnd(0, arangodb::aql::ExecutionState::DONE);
    return {arangodb::aql::ExecutionState::DONE, 0};
  }

  while (_inflight < atMost) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      auto upstreamRes = getBlock(toFetch);
      if (upstreamRes.first == arangodb::aql::ExecutionState::WAITING) {
        return {upstreamRes.first, 0};
      }
      _upstreamState = upstreamRes.first;
      if (!upstreamRes.second) {
        _done = true;
        size_t skipped = _inflight;
        _inflight = 0;
        return {arangodb::aql::ExecutionState::DONE, skipped};
      }
      _pos = 0;  // this is in the first block
      _pos_in_data = 0;
    }

    TRI_ASSERT(!_buffer.empty());
    arangodb::aql::SharedAqlItemBlockPtr cur = _buffer.front();

    TRI_ASSERT(_data->size() >= _pos_in_data);
    _inflight += std::min(_data->size() - _pos_in_data, atMost - _inflight);
    _pos_in_data += _inflight;

    if (_inflight < atMost) {
      // not skipped enough re-initialize fetching of documents
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        _pos = 0;
      } else {
        // we have exhausted this cursor
        // re-initialize fetching of documents
        _pos_in_data = 0;
      }
    }
  }

  size_t skipped = _inflight;
  _inflight = 0;
  arangodb::aql::ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}
