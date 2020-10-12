////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_DISTRIBUTE_EXECUTOR_H
#define ARANGOD_AQL_DISTRIBUTE_EXECUTOR_H

#include "Aql/BlocksWithClients.h"
#include "Aql/DistributeClientBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/RegisterInfos.h"
#include "Basics/ResultT.h"

namespace arangodb {
namespace aql {

class AqlItemBlockManager;
class DistributeClientBlock;
class DistributeNode;

class DistributeExecutorInfos : public ClientsExecutorInfos {
 public:
  DistributeExecutorInfos(std::vector<std::string> clientIds, Collection const* collection,
                          RegisterId regId, RegisterId alternativeRegId,
                          bool allowSpecifiedKeys, bool allowKeyConversionToObject,
                          bool createKeys, bool fixupGraphInput, ScatterNode::ScatterType type);

  auto registerId() const noexcept -> RegisterId;
  auto hasAlternativeRegister() const noexcept -> bool;
  auto alternativeRegisterId() const noexcept -> RegisterId;
  auto allowKeyConversionToObject() const noexcept -> bool;
  auto createKeys() const noexcept -> bool;
  auto usesDefaultSharding() const noexcept -> bool;
  auto allowSpecifiedKeys() const noexcept -> bool;
  auto scatterType() const noexcept -> ScatterNode::ScatterType;

  auto getResponsibleClient(arangodb::velocypack::Slice value) const
      -> ResultT<std::string>;

  auto createKey(VPackSlice input) const -> std::string;

  auto needsToFixGraphInput() const -> bool;

 private:
  RegisterId _regId;
  RegisterId _alternativeRegId;
  bool _allowKeyConversionToObject;
  bool _createKeys;
  bool _usesDefaultSharding;
  bool _allowSpecifiedKeys;
  bool _fixupGraphInput;

  /// @brief _colectionName: the name of the sharded collection
  Collection const* _collection;

  /// @brief Cache for the Logical Collection. This way it is not refetched
  /// on every document.
  std::shared_ptr<arangodb::LogicalCollection> _logCol;

  /// @brief type of distribution that this nodes follows.
  ScatterNode::ScatterType _type;
};

// The DistributeBlock is actually implemented by specializing
// ExecutionBlockImpl, so this class only exists to identify the specialization.
class DistributeExecutor {
 public:
  using Infos = DistributeExecutorInfos;

  using ClientBlockData = DistributeClientBlock;

  DistributeExecutor(DistributeExecutorInfos const& infos);
  ~DistributeExecutor() = default;

  /**
   * @brief Distribute the rows of the given block into the blockMap
   *        NOTE: Has SideEffects
   *        If the input value does not contain an object, it is modified inplace with
   *        a new Object containing a key value!
   *        Hence this method is not const ;(
   *
   * @param block The block to be distributed
   * @param skipped The rows that have been skipped from upstream
   * @param blockMap Map client => Data. Will provide the required data to the correct client.
   */
  auto distributeBlock(SharedAqlItemBlockPtr block, SkipResult skipped,
                       std::unordered_map<std::string, ClientBlockData>& blockMap) -> void;

 private:
  /**
   * @brief Compute which client needs to get this row
   *        NOTE: Has SideEffects
   *        If the input value does not contain an object, it is modified inplace with
   *        a new Object containing a key value!
   *        Hence this method is not const ;(
   *
   * @param block The input block
   * @param rowIndex
   * @return std::string Identifier used by the client
   */
  auto getClient(SharedAqlItemBlockPtr block, size_t rowIndex) -> std::string;

  auto getClientByIdSlice(arangodb::velocypack::Slice input) -> std::string;

 private:
  DistributeExecutorInfos const& _infos;

  // a reusable Builder object for building _key values
  arangodb::velocypack::Builder _keyBuilder;

  // a reusable Builder object for building document objects
  arangodb::velocypack::Builder _objectBuilder;
};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ExecutionBlockImpl<DistributeExecutor>
    : public BlocksWithClientsImpl<DistributeExecutor> {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, DistributeNode const* node,
                     RegisterInfos registerInfos, DistributeExecutorInfos&& executorInfos);

  ~ExecutionBlockImpl() override = default;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_DISTRIBUTE_EXECUTOR_H
