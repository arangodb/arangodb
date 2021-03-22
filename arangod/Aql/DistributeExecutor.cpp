////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "DistributeExecutor.h"

#include "Aql/AqlCallStack.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/IdExecutor.h"
#include "Aql/RegisterPlan.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Aql/SkipResult.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

DistributeExecutorInfos::DistributeExecutorInfos(
    std::vector<std::string> clientIds, Collection const* collection,
    RegisterId regId, ScatterNode::ScatterType type)
    : ClientsExecutorInfos(std::move(clientIds)),
      _regId(regId),
      _collection(collection),
      _logCol(collection->getCollection()),
      _type(type) {}

auto DistributeExecutorInfos::registerId() const noexcept -> RegisterId {
  TRI_ASSERT(_regId.isValid());
  return _regId;
}

auto DistributeExecutorInfos::scatterType() const noexcept -> ScatterNode::ScatterType {
  return _type;
}

auto DistributeExecutorInfos::getResponsibleClient(arangodb::velocypack::Slice value) const
    -> ResultT<std::string> {
  std::string shardId;
  auto res = _logCol->getResponsibleShard(value, true, shardId);

  if (res != TRI_ERROR_NO_ERROR) {
    return Result{res};
  }

  TRI_ASSERT(!shardId.empty());
  if (_type == ScatterNode::ScatterType::SERVER) {
    // Special case for server based distribution.
    shardId = _collection->getServerForShard(shardId);
    TRI_ASSERT(!shardId.empty());
  }
  return shardId;
}

DistributeExecutor::DistributeExecutor(DistributeExecutorInfos const& infos)
    : _infos(infos) {}

auto DistributeExecutor::distributeBlock(SharedAqlItemBlockPtr block, SkipResult skipped,
                                         std::unordered_map<std::string, ClientBlockData>& blockMap)
    -> void {
  std::unordered_map<std::string, std::vector<std::size_t>> choosenMap;
  choosenMap.reserve(blockMap.size());

  if (block != nullptr) {
    for (size_t i = 0; i < block->numRows(); ++i) {
      if (block->isShadowRow(i)) {
        // ShadowRows need to be added to all Clients
        for (auto const& [key, value] : blockMap) {
          choosenMap[key].emplace_back(i);
        }
      } else {
        auto client = getClient(block, i);
        if (!client.empty()) {
          // We can only have clients we are prepared for
          TRI_ASSERT(blockMap.find(client) != blockMap.end());
          choosenMap[client].emplace_back(i);
        }
      }
    }

    // We cannot have more in choosen than we have blocks
    TRI_ASSERT(choosenMap.size() <= blockMap.size());
    for (auto& [key, target] : blockMap) {
      auto value = choosenMap.find(key);
      if (value != choosenMap.end()) {
        // we have data in this block
        target.addBlock(skipped, block, std::move(value->second));
      } else {
        // No data, add skipped
        target.addBlock(skipped, nullptr, {});
      }
    }
  } else {
    for (auto& [key, target] : blockMap) {
      // No data, add skipped
      target.addBlock(skipped, nullptr, {});
    }
  }
}

auto DistributeExecutor::getClient(SharedAqlItemBlockPtr block, size_t rowIndex)
    -> std::string {
  InputAqlItemRow row{block, rowIndex};
   
  // check first input register
  AqlValue val = row.getValue(_infos.registerId());

  VPackSlice input = val.slice();  // will throw when wrong type
  if (input.isNone()) {
    return {};
  }
  
  if (!input.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "DistributeExecutor requires an object as input");
  }
  auto res = _infos.getResponsibleClient(input);
  THROW_ARANGO_EXCEPTION_IF_FAIL(res.result());
  return res.get();
}

ExecutionBlockImpl<DistributeExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                           DistributeNode const* node,
                                                           RegisterInfos registerInfos,
                                                           DistributeExecutorInfos&& executorInfos)
    : BlocksWithClientsImpl(engine, node, std::move(registerInfos), std::move(executorInfos)) {}
