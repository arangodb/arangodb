////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Timing.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

#define LOG_QUERY(logId, level)            \
  LOG_TOPIC(logId, level, Logger::QUERIES) \
  << "[query#" << this->_engine->getQuery().id() << "] "

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
      _upstreamState(ExecutionState::HASMORE),
      _exeNode(ep),
      _dependencies(),
      _dependencyPos(_dependencies.end()),
      _profileLevel(engine->getQuery().queryOptions().getProfileLevel()),
      _done(false) {}

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

  _done = false;
  _upstreamState = ExecutionState::HASMORE;

  TRI_ASSERT(getHasMoreState() == ExecutionState::HASMORE);
  TRI_ASSERT(_dependencyPos == _dependencies.end());
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

ExecutionState ExecutionBlock::getHasMoreState() {
  if (_done) {
    return ExecutionState::DONE;
  }
  if (_upstreamState == ExecutionState::DONE) {
    _done = true;
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

ExecutionNode const* ExecutionBlock::getPlanNode() const { return _exeNode; }

void ExecutionBlock::addDependency(ExecutionBlock* ep) {
  TRI_ASSERT(ep != nullptr);
  // We can never have the same dependency twice
  TRI_ASSERT(std::find(_dependencies.begin(), _dependencies.end(), ep) ==
             _dependencies.end());
  _dependencies.emplace_back(ep);
  _dependencyPos = _dependencies.end();
}

void ExecutionBlock::collectExecStats(ExecutionStats& stats) const {
  if (_profileLevel >= ProfileLevel::Blocks) {
    stats.addNode(getPlanNode()->id(), _execNodeStats);
  }
}

bool ExecutionBlock::isInSplicedSubquery() const noexcept {
  return _exeNode != nullptr ? _exeNode->isInSplicedSubquery() : false;
}

void ExecutionBlock::traceExecuteBegin(AqlCallStack const& stack, std::string const& clientId) {
  if (_profileLevel >= ProfileLevel::Blocks) {
    if (_execNodeStats.runtime >= 0.0) {
      _execNodeStats.runtime -= currentSteadyClockValue();
      TRI_ASSERT(_execNodeStats.runtime < 0.0);
    }
    
    if (_profileLevel >= ProfileLevel::TraceOne) {
      auto const node = getPlanNode();
      auto const queryId = this->_engine->getQuery().id();
      LOG_TOPIC("1e717", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "execute type=" << node->getTypeString()
          << " callStack= " << stack.toString() << " this=" << (uintptr_t)this
          << " id=" << node->id() << (clientId.empty() ? "" : " clientId=" + clientId);
    }
  }
}

void ExecutionBlock::traceExecuteEnd(std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> const& result,
                                     std::string const& clientId)  {

  if (_profileLevel >= ProfileLevel::Blocks) {
    auto const& [state, skipped, block] = result;
    auto const items = block != nullptr ? block->numRows() : 0;

    _execNodeStats.calls += 1;
    _execNodeStats.items += skipped.getSkipCount() + items;
    if (state != ExecutionState::WAITING) {
      TRI_ASSERT(_execNodeStats.runtime < 0.0);
      _execNodeStats.runtime += currentSteadyClockValue();
      TRI_ASSERT(_execNodeStats.runtime >= 0.0);
    }

    if (_profileLevel >= ProfileLevel::TraceOne) {
      size_t rows = 0;
      size_t shadowRows = 0;
      if (block != nullptr) {
        shadowRows = block->numShadowRows();
        rows = block->numRows() - shadowRows;
      }
      ExecutionNode const* node = getPlanNode();
      LOG_QUERY("60bbc", INFO)
          << "execute done " << printBlockInfo() << " state=" << stateToString(state)
          << " skipped=" << skipped.getSkipCount() << " produced=" << rows
          << " shadowRows=" << shadowRows
          << (clientId.empty() ? "" : " clientId=" + clientId);
      ;

      if (_profileLevel >= ProfileLevel::TraceTwo) {
        if (block == nullptr) {
          LOG_QUERY("9b3f4", INFO)
              << "execute type=" << node->getTypeString() << " result: nullptr";
        } else {
          auto const* opts = &_engine->getQuery().vpackOptions();
          VPackBuilder builder;
          block->toSimpleVPack(opts, builder);
          LOG_QUERY("f12f9", INFO)
              << "execute type=" << node->getTypeString()
              << " result: " << VPackDumper::toString(builder.slice(), opts);
        }
      }
    }
  }
}

auto ExecutionBlock::printTypeInfo() const -> std::string const {
  std::stringstream stream;
  ExecutionNode const* node = getPlanNode();
  stream << "type=" << node->getTypeString();
  return stream.str();
}

auto ExecutionBlock::printBlockInfo() const -> std::string const {
  std::stringstream stream;
  ExecutionNode const* node = getPlanNode();
  stream << printTypeInfo() << " this=" << (uintptr_t)this << " id=" << node->id().id();
  return stream.str();
}
