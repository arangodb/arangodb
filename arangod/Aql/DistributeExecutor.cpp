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

DistributeExecutorInfos::DistributeExecutorInfos(std::vector<std::string> clientIds,
                                                 Collection const* collection, RegisterId regId,
                                                 ScatterNode::ScatterType type,
                                                 std::vector<aql::Collection*> satellites)
    : ClientsExecutorInfos(std::move(clientIds)),
      _regId(regId),
      _collection(collection),
      _logCol(collection->getCollection()),
      _type(type),
      _satellites(std::move(satellites)) {}

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

auto DistributeExecutorInfos::shouldDistributeToAll(arangodb::velocypack::Slice value) const
    -> bool {
  if (_satellites.empty()) {
    // We can only distribute to all on Satellite Collections
    return false;
  }
  auto id = value.get(StaticStrings::IdString);
  if (!id.isString()) {
    // We can only distribute to all if we can detect the collection name
    return false;
  }

  // NOTE: Copy Paste code, shall be unified
  std::string_view vid = id.stringView();
  size_t pos = vid.find('/');
  if (pos == std::string::npos) {
    // Invalid input. Let the sharding take care of it, one server shall complain
    return false;
  }
  vid = vid.substr(0, pos);
  for (auto const& it : _satellites) {
    if (vid == it->name()) {
      // This vertex is from a satellite collection start everywhere!
      return true;
    }
  }
  return false;
}

DistributeExecutor::DistributeExecutor(DistributeExecutorInfos const& infos)
    : _infos(infos) {}

auto DistributeExecutor::distributeBlock(SharedAqlItemBlockPtr const& block, SkipResult skipped,
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
        // check first input register
        AqlValue val = InputAqlItemRow{block, i}.getValue(_infos.registerId());

        VPackSlice input = val.slice();  // will throw when wrong type
        if (input.isNone()) {
          continue;
        }

        if (!input.isObject()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL, "DistributeExecutor requires an object as input");
        }
        // NONE is ignored.
        // Object is processd
        // All others throw.
        TRI_ASSERT(input.isObject());
        if (_infos.shouldDistributeToAll(input)) {
          // This input should be added to all clients
          for (auto const& [key, value] : blockMap) {
            choosenMap[key].emplace_back(i);
          }
        } else {
          auto client = getClient(input);
          if (!client.empty()) {
            // We can only have clients we are prepared for
            TRI_ASSERT(blockMap.find(client) != blockMap.end());
            if (ADB_UNLIKELY(blockMap.find(client) == blockMap.end())) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                  std::string("unexpected client id '") + client + "' found in blockMap");
            }
            choosenMap[client].emplace_back(i);
          }
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

auto DistributeExecutor::getClient(VPackSlice input) const -> std::string {
  auto res = _infos.getResponsibleClient(input);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(std::move(res).result());
  }
  return res.get();
}

ExecutionBlockImpl<DistributeExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                           DistributeNode const* node,
                                                           RegisterInfos registerInfos,
                                                           DistributeExecutorInfos&& executorInfos)
    : BlocksWithClientsImpl(engine, node, std::move(registerInfos),
                            std::move(executorInfos)) {}
