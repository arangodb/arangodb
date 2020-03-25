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
/// @author Michael Hackstein
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionBlock.h"

#include "Aql/AqlCallStack.h"
#include "Aql/Ast.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

#define LOG_QUERY(logId, level)            \
  LOG_TOPIC(logId, level, Logger::QUERIES) \
      << "[query#" << this->_engine->getQuery()->id() << "] "

namespace {

std::string const doneString = "DONE";
std::string const hasMoreString = "HASMORE";
std::string const waitingString = "WAITING";
std::string const unknownString = "UNKNOWN";

std::string const& stateToString(aql::ExecutionState state) {
  switch (state) {
    case aql::ExecutionState::DONE:
      return doneString;
    case aql::ExecutionState::HASMORE:
      return hasMoreString;
    case aql::ExecutionState::WAITING:
      return waitingString;
    default:
      // just to suppress a warning ..
      return unknownString;
  }
}

}  // namespace

#ifdef ARANGODB_USE_GOOGLE_TESTS
size_t ExecutionBlock::DefaultBatchSize = ExecutionBlock::ProductionDefaultBatchSize;
#endif

ExecutionBlock::ExecutionBlock(ExecutionEngine* engine, ExecutionNode const* ep)
    : _engine(engine),
      _trxVpackOptions(engine->getQuery()->trx()->transactionContextPtr()->getVPackOptions()),
      _shutdownResult(TRI_ERROR_NO_ERROR),
      _done(false),
      _isInSplicedSubquery(ep != nullptr ? ep->isInSplicedSubquery() : false),
      _exeNode(ep),
      _dependencyPos(_dependencies.end()),
      _profile(engine->getQuery()->queryOptions().getProfileLevel()),
      _getSomeBegin(0.0),
      _upstreamState(ExecutionState::HASMORE),
      _pos(0),
      _collector(&engine->itemBlockManager()) {}

ExecutionBlock::~ExecutionBlock() = default;

std::pair<ExecutionState, Result> ExecutionBlock::initializeCursor(InputAqlItemRow const& input) {
  if (_dependencyPos == _dependencies.end()) {
    // We need to start again.
    _dependencyPos = _dependencies.begin();
  }
  for (; _dependencyPos != _dependencies.end(); ++_dependencyPos) {
    auto res = (*_dependencyPos)->initializeCursor(input);
    if (res.first == ExecutionState::WAITING || !res.second.ok()) {
      // If we need to wait or get an error we return as is.
      return res;
    }
  }

  _buffer.clear();

  _done = false;
  _upstreamState = ExecutionState::HASMORE;
  _pos = 0;
  _collector.clear();

  TRI_ASSERT(getHasMoreState() == ExecutionState::HASMORE);
  TRI_ASSERT(_dependencyPos == _dependencies.end());
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, Result> ExecutionBlock::shutdown(int errorCode) {
  if (_dependencyPos == _dependencies.end()) {
    _shutdownResult.reset(TRI_ERROR_NO_ERROR);
    _dependencyPos = _dependencies.begin();
  }

  for (; _dependencyPos != _dependencies.end(); ++_dependencyPos) {
    Result res;
    ExecutionState state;
    try {
      std::tie(state, res) = (*_dependencyPos)->shutdown(errorCode);
      if (state == ExecutionState::WAITING) {
        return {state, TRI_ERROR_NO_ERROR};
      }
    } catch (...) {
      _shutdownResult.reset(TRI_ERROR_INTERNAL);
    }

    if (res.fail()) {
      _shutdownResult = res;
    }
  }

  _buffer.clear();

  return {ExecutionState::DONE, _shutdownResult};
}

void ExecutionBlock::traceGetSomeBegin(size_t atMost) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    if (_getSomeBegin <= 0.0) {
      _getSomeBegin = TRI_microtime();
    }
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto const node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      LOG_TOPIC("ca7db", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "getSome type=" << node->getTypeString() << " atMost=" << atMost
          << " this=" << (uintptr_t)this << " id=" << node->id();
    }
  }
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlock::traceGetSomeEnd(
    ExecutionState state, SharedAqlItemBlockPtr result) {
  TRI_ASSERT(result != nullptr || state != ExecutionState::HASMORE);
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    ExecutionNode const* en = getPlanNode();
    ExecutionStats::Node stats;
    auto const items = result != nullptr ? result->size() : 0;
    stats.calls = 1;
    stats.items = items;
    if (state != ExecutionState::WAITING) {
      stats.runtime = TRI_microtime() - _getSomeBegin;
      _getSomeBegin = 0.0;
    }

    auto it = _engine->_stats.nodes.find(en->id());
    if (it != _engine->_stats.nodes.end()) {
      it->second += stats;
    } else {
      _engine->_stats.nodes.try_emplace(en->id(), stats);
    }

    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      ExecutionNode const* node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      LOG_TOPIC("07a60", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "getSome done type=" << node->getTypeString()
          << " this=" << (uintptr_t)this << " id=" << node->id()
          << " state=" << stateToString(state) << " items=" << items;

      if (_profile >= PROFILE_LEVEL_TRACE_2) {
        if (result == nullptr) {
          LOG_TOPIC("daa64", INFO, Logger::QUERIES)
              << "[query#" << queryId << "] "
              << "getSome type=" << node->getTypeString() << " result: nullptr";
        } else {
          VPackBuilder builder;
          auto const options = trxVpackOptions();
          result->toSimpleVPack(options, builder);
          LOG_TOPIC("fcd9c", INFO, Logger::QUERIES)
              << "[query#" << queryId << "] "
              << "getSome type=" << node->getTypeString()
              << " result: " << VPackDumper::toString(builder.slice(), options);
        }
      }
    }
  }
  return {state, std::move(result)};
}

void ExecutionBlock::traceSkipSomeBegin(size_t atMost) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    if (_getSomeBegin <= 0.0) {
      _getSomeBegin = TRI_microtime();
    }
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      LOG_TOPIC("dba8a", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "skipSome type=" << node->getTypeString() << " atMost=" << atMost
          << " this=" << (uintptr_t)this << " id=" << node->id();
    }
  }
}

std::pair<ExecutionState, size_t> ExecutionBlock::traceSkipSomeEnd(
    std::pair<ExecutionState, size_t> const res) {
  ExecutionState const state = res.first;
  size_t const skipped = res.second;

  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    ExecutionNode const* en = getPlanNode();
    ExecutionStats::Node stats;
    stats.calls = 1;
    stats.items = skipped;
    if (state != ExecutionState::WAITING) {
      stats.runtime = TRI_microtime() - _getSomeBegin;
      _getSomeBegin = 0.0;
    }

    auto it = _engine->_stats.nodes.find(en->id());
    if (it != _engine->_stats.nodes.end()) {
      it->second += stats;
    } else {
      _engine->_stats.nodes.try_emplace(en->id(), stats);
    }

    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      ExecutionNode const* node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      LOG_TOPIC("d1950", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "skipSome done type=" << node->getTypeString()
          << " this=" << (uintptr_t)this << " id=" << node->id()
          << " state=" << stateToString(state) << " skipped=" << skipped;
    }
  }
  return res;
}

std::pair<ExecutionState, size_t> ExecutionBlock::traceSkipSomeEnd(ExecutionState state,
                                                                   size_t skipped) {
  return traceSkipSomeEnd({state, skipped});
}

ExecutionState ExecutionBlock::getHasMoreState() {
  if (_done) {
    return ExecutionState::DONE;
  }
  if (_buffer.empty() && _upstreamState == ExecutionState::DONE) {
    _done = true;
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

ExecutionNode const* ExecutionBlock::getPlanNode() const { return _exeNode; }

velocypack::Options const* ExecutionBlock::trxVpackOptions() const noexcept {
  return _trxVpackOptions;
}

void ExecutionBlock::addDependency(ExecutionBlock* ep) {
  TRI_ASSERT(ep != nullptr);
  // We can never have the same dependency twice
  TRI_ASSERT(std::find(_dependencies.begin(), _dependencies.end(), ep) ==
             _dependencies.end());
  _dependencies.emplace_back(ep);
  _dependencyPos = _dependencies.end();
}

bool ExecutionBlock::isInSplicedSubquery() const noexcept {
  return _isInSplicedSubquery;
}

void ExecutionBlock::traceExecuteBegin(AqlCallStack const& stack, std::string const& clientId) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    if (_getSomeBegin <= 0.0) {
      _getSomeBegin = TRI_microtime();
    }
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto const node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      auto const& call = stack.peek();
      LOG_TOPIC("1e717", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "execute type=" << node->getTypeString() << " call= " << call
          << " this=" << (uintptr_t)this << " id=" << node->id()
          << (clientId.empty() ? "" : " clientId=" + clientId);
    }
  }
}

auto ExecutionBlock::traceExecuteEnd(std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> const& result,
                                     std::string const& clientId)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    auto const& [state, skipped, block] = result;
    auto const items = block != nullptr ? block->size() : 0;
    ExecutionNode const* en = getPlanNode();
    ExecutionStats::Node stats;
    stats.calls = 1;
    stats.items = skipped.getSkipCount() + items;
    if (state != ExecutionState::WAITING) {
      stats.runtime = TRI_microtime() - _getSomeBegin;
      _getSomeBegin = 0.0;
    }

    auto it = _engine->_stats.nodes.find(en->id());
    if (it != _engine->_stats.nodes.end()) {
      it->second += stats;
    } else {
      _engine->_stats.nodes.emplace(en->id(), stats);
    }

    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      ExecutionNode const* node = getPlanNode();
      LOG_QUERY("60bbc", INFO)
          << "execute done " << printBlockInfo() << " state=" << stateToString(state)
          << " skipped=" << skipped.getSkipCount() << " produced=" << items
          << (clientId.empty() ? "" : " clientId=" + clientId);
      ;

      if (_profile >= PROFILE_LEVEL_TRACE_2) {
        if (block == nullptr) {
          LOG_QUERY("9b3f4", INFO)
              << "execute type=" << node->getTypeString() << " result: nullptr";
        } else {
          VPackBuilder builder;
          auto const options = trxVpackOptions();
          block->toSimpleVPack(options, builder);
          LOG_QUERY("f12f9", INFO)
              << "execute type=" << node->getTypeString()
              << " result: " << VPackDumper::toString(builder.slice(), options);
        }
      }
    }
  }

  return result;
}

auto ExecutionBlock::printTypeInfo() const -> std::string const {
  std::stringstream stream;
  ExecutionNode const* node = getPlanNode();
  stream << "type=" << node->getTypeString();
  ;
  return stream.str();
}

auto ExecutionBlock::printBlockInfo() const -> std::string const {
  std::stringstream stream;
  ExecutionNode const* node = getPlanNode();
  stream << printTypeInfo() << " this=" << (uintptr_t)this << " id=" << node->id();
  return stream.str();
}
