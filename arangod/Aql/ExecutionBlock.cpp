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
#include "Aql/AqlItemBlock.h"
#include "Aql/Ast.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

std::string const doneString = "DONE";
std::string const hasMoreString = "HASMORE";
std::string const waitingString = "WAITING";

static std::string const& stateToString(ExecutionState state) {
  switch (state) {
    case ExecutionState::DONE:
      return doneString;
    case ExecutionState::HASMORE:
      return hasMoreString;
    case ExecutionState::WAITING:
      return waitingString;
  }
}

}  // namespace

ExecutionBlock::ExecutionBlock(ExecutionEngine* engine, ExecutionNode const* ep)
    : _engine(engine),
    _trx(engine->getQuery()->trx()),
      _shutdownResult(TRI_ERROR_NO_ERROR),
      _done(false),
      _exeNode(ep),
      _profile(engine->getQuery()->queryOptions().getProfileLevel()),
      _getSomeBegin(0.0),
      _upstreamState(ExecutionState::HASMORE),
      _dependencyPos(_dependencies.end()),
      _collector(&engine->itemBlockManager()) {}

ExecutionBlock::~ExecutionBlock() = default;

void ExecutionBlock::throwIfKilled() { // TODO Scatter & DistributeExecutor still using
  if (_engine->getQuery()->killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

// Trace the start of a getSome call
void ExecutionBlock::traceGetSomeBegin(size_t atMost) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    if (_getSomeBegin <= 0.0) {
      _getSomeBegin = TRI_microtime();
    }
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto node = getPlanNode();
      LOG_TOPIC("ca7db", INFO, Logger::QUERIES)
      << "getSome type=" << node->getTypeString() << " atMost = " << atMost
      << " this=" << (uintptr_t)this << " id=" << node->id();
    }
  }
}

// Trace the end of a getSome call, potentially with result
void ExecutionBlock::traceGetSomeEnd(AqlItemBlock const* result, ExecutionState state) {
  TRI_ASSERT(result != nullptr || state != ExecutionState::HASMORE);
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    ExecutionNode const* en = getPlanNode();
    ExecutionStats::Node stats;
    stats.calls = 1;
    stats.items = result != nullptr ? result->size() : 0;
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
      LOG_TOPIC("07a60", INFO, Logger::QUERIES)
      << "getSome done type=" << node->getTypeString() << " this=" << (uintptr_t)this
      << " id=" << node->id() << " state=" << ::stateToString(state);

      if (_profile >= PROFILE_LEVEL_TRACE_2) {
        if (result == nullptr) {
          LOG_TOPIC("daa64", INFO, Logger::QUERIES)
          << "getSome type=" << node->getTypeString() << " result: nullptr";
        } else {
          VPackBuilder builder;
          {
            VPackObjectBuilder guard(&builder);
            result->toVelocyPack(transaction(), builder);
          }
          LOG_TOPIC("fcd9c", INFO, Logger::QUERIES) << "getSome type=" << node->getTypeString()
                                                    << " result: " << builder.toJson();
        }
      }
    }
  }
}

void ExecutionBlock::traceSkipSomeBegin(size_t atMost) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    if (_getSomeBegin <= 0.0) {
      _getSomeBegin = TRI_microtime();
    }
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto node = getPlanNode();
      LOG_TOPIC("dba8a", INFO, Logger::QUERIES)
      << "skipSome type=" << node->getTypeString() << " atMost = " << atMost
      << " this=" << (uintptr_t)this << " id=" << node->id();
    }
  }
}

void ExecutionBlock::traceSkipSomeEnd(size_t skipped, ExecutionState state) {
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
      _engine->_stats.nodes.emplace(en->id(), stats);
    }

    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      ExecutionNode const* node = getPlanNode();
      LOG_TOPIC("d1950", INFO, Logger::QUERIES)
      << "skipSome done type=" << node->getTypeString() << " this=" << (uintptr_t)this
      << " id=" << node->id() << " state=" << ::stateToString(state);
    }
  }
}

