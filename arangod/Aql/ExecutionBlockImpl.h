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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_BLOCK_IMPL_H
#define ARANGOD_AQL_EXECUTION_BLOCK_IMPL_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/BlockFetcherInterfaces.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
class AqlItemRow;
class ExecutionNode;
class ExecutionEngine;

template<class Executor>
class ExecutionBlockImpl : public ExecutionBlock, public SingleRowFetcher {
 public:

  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node);
  ~ExecutionBlockImpl();

  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(
      size_t atMost) override;

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  std::pair<ExecutionState, AqlItemRow const*> fetchRow() override;

 private:

  ExecutionState fetchBlock();

 private:

  Executor& _executor;
};


} // aql
} // arangodb
#endif
