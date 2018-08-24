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

#ifndef ARANGOD_AQL_ENUMERATE_COLLECTION_BLOCK_H
#define ARANGOD_AQL_ENUMERATE_COLLECTION_BLOCK_H 1

#include "Aql/DocumentProducingBlock.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace arangodb {

class LocalDocumentId;
struct OperationCursor;

namespace aql {
class AqlItemBlock;
struct Collection;
class ExecutionEngine;

class EnumerateCollectionBlock final : public ExecutionBlock, public DocumentProducingBlock {
 public:
  EnumerateCollectionBlock(ExecutionEngine* engine,
                           EnumerateCollectionNode const* ep);

  /// @brief initializeCursor
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief getSome
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override final;

  // skip atMost documents, returns the number actually skipped . . .
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final;

 private:
  /// @brief collection
  Collection const* _collection;
  
  /// @brief cursor
  std::unique_ptr<OperationCursor> _cursor;

  /// @brief Persistent counter of elements that are in flight during WAITING
  ///        has to be resetted as soon as we return with DONE/HASMORE
  size_t _inflight;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
