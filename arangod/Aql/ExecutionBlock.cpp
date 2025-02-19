////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/Timing.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>

#include <string_view>

using namespace arangodb;
using namespace arangodb::aql;

#define LOG_QUERY(logId, level)            \
  LOG_TOPIC(logId, level, Logger::QUERIES) \
      << "[query#" << this->_engine->getQuery().id() << "] "

namespace {

std::string_view stateToString(aql::ExecutionState state) {
  switch (state) {
    case aql::ExecutionState::DONE:
      return "DONE";
    case aql::ExecutionState::HASMORE:
      return "HASMORE";
    case aql::ExecutionState::WAITING:
      return "WAITING";
    default:
      // just to suppress a warning ..
      return "UNKNOWN";
  }
}

}  // namespace

#ifdef ARANGODB_USE_GOOGLE_TESTS
size_t ExecutionBlock::DefaultBatchSize =
    ExecutionBlock::ProductionDefaultBatchSize;
#endif

ExecutionBlock::ExecutionBlock(ExecutionEngine* engine, ExecutionNode const* ep)
    : _engine(engine),
      _upstreamState(ExecutionState::HASMORE),
      _profileLevel(engine->getQuery().queryOptions().getProfileLevel()),
      _exeNode(ep),
      _dependencyPos(_dependencies.end()),
      _startOfExecution(-1.0),
      _done(false) {}

ExecutionBlock::~ExecutionBlock() = default;

std::pair<ExecutionState, Result> ExecutionBlock::initializeCursor(
    InputAqlItemRow const& input) {
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
  return {ExecutionState::DONE, {}};
}

ExecutionState ExecutionBlock::getHasMoreState() noexcept {
  if (!_done && _upstreamState == ExecutionState::DONE) {
    _done = true;
  }
  return _done ? ExecutionState::DONE : ExecutionState::HASMORE;
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

void ExecutionBlock::collectExecStats(ExecutionStats& stats) {
  if (_profileLevel >= ProfileLevel::Blocks) {
    stats.addNode(getPlanNode()->id(), _execNodeStats);
  }
}

void ExecutionBlock::traceExecuteBegin(AqlCallStack const& stack,
                                       std::string_view clientId) {
  // add timing for block in case profiling is turned on.
  if (_profileLevel >= ProfileLevel::Blocks) {
    // only if profiling is turned on, get current time
    _startOfExecution = currentSteadyClockValue();
    TRI_ASSERT(_startOfExecution > 0.0);

    if (_profileLevel >= ProfileLevel::TraceOne) {
      LOG_TOPIC("1e717", INFO, Logger::QUERIES)
          << "[query#" << this->_engine->getQuery().id()
          << "] execute type=" << getPlanNode()->getTypeString()
          << " callStack= " << stack.toString() << " this=" << (uintptr_t)this
          << " id=" << getPlanNode()->id()
          << (clientId.empty() ? "" : " clientId=") << clientId;
    }
  }
}

void ExecutionBlock::traceExecuteEnd(
    std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> const& result,
    std::string_view clientId) {
  if (_profileLevel >= ProfileLevel::Blocks) {
    TRI_ASSERT(_startOfExecution > 0.0);
    _execNodeStats.runtime += currentSteadyClockValue() - _startOfExecution;
    _startOfExecution = -1.0;

    auto [state, skipped, block] = result;
    auto items = block != nullptr ? block->numRows() : 0;

    _execNodeStats.calls += 1;
    _execNodeStats.items += skipped.getSkipCount() + items;

    if (_profileLevel >= ProfileLevel::TraceOne) {
      size_t rows = 0;
      size_t shadowRows = 0;
      if (block != nullptr) {
        shadowRows = block->numShadowRows();
        rows = block->numRows() - shadowRows;
      }
      LOG_QUERY("60bbc", INFO)
          << "execute done " << printBlockInfo()
          << " state=" << stateToString(state)
          << " skipped=" << skipped.getSkipCount() << " produced=" << rows
          << " shadowRows=" << shadowRows
          << (clientId.empty() ? "" : " clientId=") << clientId;

      if (_profileLevel >= ProfileLevel::TraceTwo) {
        ExecutionNode const* node = getPlanNode();
        LOG_QUERY("f12f9", INFO)
            << "execute type=" << node->getTypeString() << " id=" << node->id()
            << (clientId.empty() ? "" : " clientId=") << clientId
            << " result: " << std::invoke([&, &block = block]() -> std::string {
                 if (block == nullptr) {
                   return "nullptr";
                 } else {
                   auto const* opts = &_engine->getQuery().vpackOptions();
                   VPackBuilder builder;
                   block->toSimpleVPack(opts, builder);
                   return VPackDumper::toString(builder.slice(), opts);
                 }
               });
      }
    }
  }
}

ExecutionNodeStats& ExecutionBlock::stats() noexcept { return _execNodeStats; }

auto ExecutionBlock::printTypeInfo() const -> std::string const {
  return absl::StrCat("type=", getPlanNode()->getTypeString());
}

auto ExecutionBlock::printBlockInfo() const -> std::string const {
  return absl::StrCat(printTypeInfo(), " this=", (uintptr_t)this,
                      " id=", getPlanNode()->id().id());
}
