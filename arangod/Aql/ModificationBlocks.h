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
#include "Utils/AqlTransaction.h"

namespace arangodb {
namespace aql {
struct Collection;

class ExecutionEngine;

class ModificationBlock : public ExecutionBlock {
 public:
  ModificationBlock(ExecutionEngine*, ModificationNode const*);

  virtual ~ModificationBlock();

  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

 protected:
  /// @brief the actual work horse
  virtual AqlItemBlock* work(std::vector<AqlItemBlock*>&) = 0;

  /// @brief extract a key from the AqlValue passed
  int extractKey(AqlValue const&, std::string&);

  /// @brief process the result of a data-modification operation
  void handleResult(int, bool, std::string const* errorMessage = nullptr);

  void handleBabyResult(std::unordered_map<int, size_t> const&, size_t, bool,
                        std::string const* errorMessage = nullptr);

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
};

class RemoveBlock : public ModificationBlock {
 public:
  RemoveBlock(ExecutionEngine*, RemoveNode const*);
  ~RemoveBlock() = default;

 protected:
  /// @brief the actual work horse for removing data
  AqlItemBlock* work(std::vector<AqlItemBlock*>&) override final;
};

class InsertBlock : public ModificationBlock {
 public:
  InsertBlock(ExecutionEngine*, InsertNode const*);
  ~InsertBlock() = default;

 protected:
  /// @brief the actual work horse for inserting data
  AqlItemBlock* work(std::vector<AqlItemBlock*>&) override final;
};

class UpdateBlock : public ModificationBlock {
 public:
  UpdateBlock(ExecutionEngine*, UpdateNode const*);
  ~UpdateBlock() = default;

 protected:
  /// @brief the actual work horse for updating data
  AqlItemBlock* work(std::vector<AqlItemBlock*>&) override final;
};

class ReplaceBlock : public ModificationBlock {
 public:
  ReplaceBlock(ExecutionEngine*, ReplaceNode const*);
  ~ReplaceBlock() = default;

 protected:
  /// @brief the actual work horse for replacing data
  AqlItemBlock* work(std::vector<AqlItemBlock*>&) override final;
};

class UpsertBlock : public ModificationBlock {
 public:
  UpsertBlock(ExecutionEngine*, UpsertNode const*);
  ~UpsertBlock() = default;

 protected:
  /// @brief the actual work horse for updating data
  AqlItemBlock* work(std::vector<AqlItemBlock*>&) override final;

 private:
  bool isShardKeyError(arangodb::velocypack::Slice const) const;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
