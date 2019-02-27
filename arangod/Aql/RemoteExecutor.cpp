////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "RemoteExecutor.h"

#include "Aql/ExecutorInfos.h"

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<RemoteExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                       ExecutionNode const* node,
                                                       ExecutorInfos&& infos)
    : ExecutionBlock(engine, node),
      _infos(std::move(infos)),
      _query(*engine->getQuery()) {}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ExecutionBlockImpl<RemoteExecutor>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  auto result = getSomeWithoutTrace(atMost);
  return traceGetSomeEnd(result.first, std::move(result.second));
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlockImpl<RemoteExecutor>::getSomeWithoutTrace(size_t atMost) {
  // silence tests -- we need to introduce new failure tests for fetchers
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  // TODO implement
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<RemoteExecutor>::skipSome(size_t atMost) {
  // TODO IMPLEMENT ME, this is a stub!

  traceSkipSomeBegin(atMost);

  auto res = getSomeWithoutTrace(atMost);

  size_t skipped = 0;
  if (res.second != nullptr) {
    skipped = res.second->size();
    AqlItemBlock* resultPtr = res.second.get();
    returnBlock(resultPtr);
    res.second.release();
  }

  return traceSkipSomeEnd(res.first, skipped);
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ExecutionBlockImpl<RemoteExecutor>::traceGetSomeEnd(
    ExecutionState state, std::unique_ptr<AqlItemBlock> result) {
  ExecutionBlock::traceGetSomeEnd(result.get(), state);
  return {state, std::move(result)};
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<RemoteExecutor>::traceSkipSomeEnd(
    ExecutionState state, size_t skipped) {
  ExecutionBlock::traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<RemoteExecutor>::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<RemoteExecutor>::shutdown(int errorCode) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
