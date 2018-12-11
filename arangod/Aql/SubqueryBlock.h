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

#ifndef ARANGOD_AQL_SUBQUERY_BLOCK_H
#define ARANGOD_AQL_SUBQUERY_BLOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class SubqueryBlock final : public ExecutionBlock {
 public:
  SubqueryBlock(ExecutionEngine*, SubqueryNode const*, ExecutionBlock*);
  ~SubqueryBlock() = default;

  /// @brief getSome
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(
      size_t atMost) override final;

  /// @brief shutdown, tell dependency and the subquery
  std::pair<ExecutionState, Result> shutdown(int errorCode) override final;

  /// @brief getter for the pointer to the subquery
  ExecutionBlock* getSubquery() { return _subquery; }

 private:
  /// @brief execute the subquery
  /// is repeatable in case of WAITING
  /// Fills result in _subqueryResults
  ExecutionState executeSubquery();

  /// @brief destroy the results of a subquery
  void destroySubqueryResults();

  /// @brief initialize the subquery,
  /// is repeatable in case of WAITING
  ExecutionState initSubquery(size_t position);

  /// @brief forward getSome to const subquery
  /// is repeatable in case of WAITING
  ExecutionState getSomeConstSubquery(size_t atMost);

  /// @brief forward getSome to non-const subquery
  /// is repeatable in case of WAITING
  ExecutionState getSomeNonConstSubquery(size_t atMost);

 private:

  /// @brief output register
  RegisterId _outReg;

  /// @brief we need to have an executionblock and where to write the result
  ExecutionBlock* _subquery;

  /// @brief whether the subquery is const and will always return the same values
  /// when invoked multiple times
  bool const _subqueryIsConst;

  /// @brief whether the subquery returns data
  bool const _subqueryReturnsData;

  /// @brief a unique_ptr to hold temporary results if thread gets suspended
  ///        guaranteed to be cleared out after a DONE/HASMORE of get/skip-some
  std::unique_ptr<AqlItemBlock> _result;

  /// @brief the list of results from a single subquery
  ///        NOTE: Responsibilty here is a bit tricky, it is handed over to the result
  std::unique_ptr<std::vector<std::unique_ptr<AqlItemBlock>>> _subqueryResults;

  /// @brief the current subquery in process, used if this thread gets suspended.
  size_t _subqueryPos;

  /// @brief track if we have already initialized this subquery.
  bool _subqueryInitialized;

  /// @brief track if we have completely executed the subquery.
  bool _subqueryCompleted;

  /// @brief track if we have completely shutdown the main query.
  bool _hasShutdownMainQuery;

  /// @brief result of the main query shutdown. is only valid if _hasShutdownMainQuery == true.
  Result _mainQueryShutdownResult;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
