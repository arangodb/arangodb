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

#ifndef ARANGOD_AQL_ENUMERATE_LIST_BLOCK_H
#define ARANGOD_AQL_ENUMERATE_LIST_BLOCK_H 1

#include "ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;

class ExecutionEngine;

class EnumerateListBlock final : public ExecutionBlock {
 public:
  EnumerateListBlock(ExecutionEngine*, EnumerateListNode const*);
  ~EnumerateListBlock();

  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(
      size_t atMost) override final;

  // skip atMost documents, returns the number actually skipped . . .
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final;

 private:
  // cppcheck-suppress *
  AqlValue getAqlValue(AqlValue const& inVarReg, size_t n, bool& mustDestroy);

  // cppcheck-suppress *
  void throwArrayExpectedException(AqlValue const& value);

 private:
  // current position in the _inVariable
  size_t _index;

  // total number of elements in DOCVEC
  size_t _docVecSize;

  // the register index containing the inVariable of the
  // EnumerateListNode
  RegisterId _inVarRegId;

  // @brief number of requests in flight in the moment we hit WAITING
  size_t _inflight;
};

}
}

#endif
