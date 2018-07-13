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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MODIFICATION_BLOCKS_H
#define ARANGOD_AQL_MODIFICATION_BLOCKS_H 1

#include "Basics/Common.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ModificationNodes.h"

namespace arangodb {
namespace aql {
struct Collection;

class ExecutionEngine;

class ModificationBlock : public ExecutionBlock {
 public:
  ModificationBlock(ExecutionEngine*, ModificationNode const*);

  virtual ~ModificationBlock();

  /// @brief getSome
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(
      size_t atMost) override final;

  std::pair<ExecutionState, arangodb::Result> initializeCursor(
      AqlItemBlock* items, size_t pos) override final;

 protected:
  /// @brief the actual work horse
  virtual std::unique_ptr<AqlItemBlock> work() = 0;

  /// @brief extract a key from the AqlValue passed
  int extractKey(AqlValue const&, std::string& key);

  /// @brief extract a key and rev from the AqlValue passed
  int extractKeyAndRev(AqlValue const&, std::string& key, std::string& rev);

  /// @brief process the result of a data-modification operation
  void handleResult(int, bool, std::string const* errorMessage = nullptr);

  void handleBabyResult(std::unordered_map<int, size_t> const&, size_t,
                        bool ignoreAllErrors,
                        bool ignoreDocumentNotFound = false);

  /// @brief determine the number of rows in a vector of blocks
  size_t countBlocksRows() const;

  /// @brief Returns the success return start of this block.
  ///        Can either be HASMORE or DONE.
  ///        Guarantee is that if DONE is returned every subsequent call
  ///        to get/skipSome will NOT find mor documents.
  ///        HASMORE is allowed to lie, so a next call to get/skipSome could return
  ///        no more results.
  ExecutionState getHasMoreState() override;


 protected:
  /// @brief output register ($OLD)
  RegisterId _outRegOld;

  /// @brief output register ($NEW)
  RegisterId _outRegNew;

  /// @brief collection
  Collection const* _collection;

  /// @brief whether or not we're a DB server in a cluster
  bool _isDBServer;

  /// @brief whether or not the collection uses the default sharding attributes
  bool _usesDefaultSharding;

  /// @brief whether this block contributes to statistics.
  ///        Will only be disabled in SmartGraphCase.
  bool _countStats;
 
  /// @brief A list of AQL Itemblocks fetched from upstream
  std::vector<std::unique_ptr<AqlItemBlock>> _blocks;

  /// @brief a Builder object, reused for various tasks to save a few memory allocations
  velocypack::Builder _tempBuilder;
};

class RemoveBlock : public ModificationBlock {
 public:
  RemoveBlock(ExecutionEngine*, RemoveNode const*);
  ~RemoveBlock() = default;

  Type getType() const override final {
    return Type::REMOVE;
  }

 protected:
  /// @brief the actual work horse for removing data
  std::unique_ptr<AqlItemBlock> work() override final;
};

class InsertBlock : public ModificationBlock {
 public:
  InsertBlock(ExecutionEngine*, InsertNode const*);
  ~InsertBlock() = default;

  Type getType() const override final {
    return Type::INSERT;
  }

 protected:
  /// @brief the actual work horse for inserting data
  std::unique_ptr<AqlItemBlock> work() override final;
};

class UpdateBlock : public ModificationBlock {
 public:
  UpdateBlock(ExecutionEngine*, UpdateNode const*);
  ~UpdateBlock() = default;

  Type getType() const override final {
    return Type::UPDATE;
  }

 protected:
  /// @brief the actual work horse for updating data
  std::unique_ptr<AqlItemBlock> work() override final;
};

class ReplaceBlock : public ModificationBlock {
 public:
  ReplaceBlock(ExecutionEngine*, ReplaceNode const*);
  ~ReplaceBlock() = default;

  Type getType() const override final {
    return Type::REPLACE;
  }

 protected:
  /// @brief the actual work horse for replacing data
  std::unique_ptr<AqlItemBlock> work() override final;
};

class UpsertBlock : public ModificationBlock {
 public:
  UpsertBlock(ExecutionEngine*, UpsertNode const*);
  ~UpsertBlock() = default;

  Type getType() const override final {
    return Type::UPSERT;
  }

 protected:
  /// @brief the actual work horse for updating data
  std::unique_ptr<AqlItemBlock> work() override final;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
