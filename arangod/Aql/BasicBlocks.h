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

#ifndef ARANGOD_AQL_BASIC_BLOCKS_H
#define ARANGOD_AQL_BASIC_BLOCKS_H 1

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class SingletonBlock final : public ExecutionBlock {
 public:
  SingletonBlock(ExecutionEngine* engine, SingletonNode const* ep);

  /// @brief initializeCursor, store a copy of the register values coming from
  /// above
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

 private:
  std::pair<ExecutionState, arangodb::Result> getOrSkipSome(size_t atMost, bool skipping,
                                                            AqlItemBlock*& result,
                                                            size_t& skipped) override;

  /// @brief _inputRegisterValues
  std::unique_ptr<AqlItemBlock> _inputRegisterValues;

  std::unordered_set<RegisterId> _whitelist;
};


class NoResultsBlock final : public ExecutionBlock {
 public:
  NoResultsBlock(ExecutionEngine* engine, ExecutionNode const* ep)
      : ExecutionBlock(engine, ep) {}

  /// @brief initializeCursor, store a copy of the register values coming from
  /// above
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

 private:
  std::pair<ExecutionState, arangodb::Result> getOrSkipSome(size_t atMost, bool skipping,
                                                            AqlItemBlock*& result,
                                                            size_t& skipped) override;
};

}  // namespace aql
}  // namespace arangodb

#endif
