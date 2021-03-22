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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "MutexExecutor.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IdExecutor.h"
#include "Aql/MutexNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedQueryState.h"
#include "Aql/Stats.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"

#include "Logger/LogMacros.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

MutexExecutorInfos::MutexExecutorInfos(
    std::vector<std::string> clientIds)
    : ClientsExecutorInfos(std::move(clientIds)) {}


MutexExecutor::MutexExecutor(MutexExecutorInfos const& infos)
  : _infos(infos), _numClient(0) {}

auto MutexExecutor::distributeBlock(SharedAqlItemBlockPtr block, SkipResult skipped,
                                    std::unordered_map<std::string, ClientBlockData>& blockMap)
    -> void {

  TRI_IF_FAILURE("MutexExecutor::distributeBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (block != nullptr) {
    std::unordered_map<std::string, std::vector<std::size_t>> choosenMap;
    choosenMap.reserve(blockMap.size());

    for (size_t i = 0; i < block->numRows(); ++i) {
      if (block->isShadowRow(i)) {
        // ShadowRows need to be added to all Clients
        for (auto const& [key, value] : blockMap) {
          choosenMap[key].emplace_back(i);
        }
      } else {
        auto const& client = getClient(block, i);
        // We can only have clients we are prepared for
        TRI_ASSERT(blockMap.find(client) != blockMap.end());
        choosenMap[client].emplace_back(i);
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

std::string const& MutexExecutor::getClient(SharedAqlItemBlockPtr /*block*/, size_t rowIndex) {
  TRI_ASSERT(_infos.nrClients() > 0);
  // round-robin distribution
  return _infos.clientIds()[(_numClient++) % _infos.nrClients()];
}

ExecutionBlockImpl<MutexExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                      MutexNode const* node,
                                                      RegisterInfos registerInfos,
                                                      MutexExecutorInfos&& executorInfos)
    : BlocksWithClientsImpl(engine, node, std::move(registerInfos),
                            std::move(executorInfos)) {}
