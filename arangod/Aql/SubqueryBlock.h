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
#include "Utils/AqlTransaction.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class SubqueryBlock : public ExecutionBlock {
 public:
  SubqueryBlock(ExecutionEngine*, SubqueryNode const*, ExecutionBlock*);
  ~SubqueryBlock() = default;

  /// @brief initialize, tell dependency and the subquery
  int initialize() override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  /// @brief shutdown, tell dependency and the subquery
  int shutdown(int errorCode) override final;

  /// @brief getter for the pointer to the subquery
  ExecutionBlock* getSubquery() { return _subquery; }

 private:
  /// @brief execute the subquery and return its results
  std::vector<AqlItemBlock*>* executeSubquery();

  /// @brief destroy the results of a subquery
  void destroySubqueryResults(std::vector<AqlItemBlock*>*);

  /// @brief output register
  RegisterId _outReg;

  /// @brief we need to have an executionblock and where to write the result
  ExecutionBlock* _subquery;

  /// @brief whether the subquery is const and will always return the same values
  /// when invoked multiple times
  bool _subqueryIsConst;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
