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

#include "Aql/CollectionScanner.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

struct TRI_doc_mptr_t;

namespace arangodb {
namespace aql {
class AqlItemBlock;
struct Collection;
class CollectionScanner;
class ExecutionEngine;

class EnumerateCollectionBlock : public ExecutionBlock {
 public:
  EnumerateCollectionBlock(ExecutionEngine* engine,
                           EnumerateCollectionNode const* ep);

  ~EnumerateCollectionBlock() = default;

  /// @brief initialize fetching of documents
  void initializeDocuments();

  /// @brief skip instead of fetching
  bool skipDocuments(size_t toSkip, size_t& skipped);

  /// @brief continue fetching of documents
  bool moreDocuments(size_t hint);

  /// @brief initialize, here we fetch all docs from the database
  int initialize() override final;

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  // skip between atLeast and atMost, returns the number actually skipped . . .
  // will only return less than atLeast if there aren't atLeast many
  // things to skip overall.
  size_t skipSome(size_t atLeast, size_t atMost) override final;

 private:
  /// @brief collection
  Collection* _collection;

  /// @brief collection scanner
  CollectionScanner _scanner;
  
  /// @brief iterator over documents
  arangodb::velocypack::ArrayIterator _iterator;

  /// @brief document buffer
  arangodb::velocypack::Slice _documents;

  /// @brief whether or not the enumerated documents need to be stored
  bool _mustStoreResult;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
